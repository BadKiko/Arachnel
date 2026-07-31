#include "runtime_dependency_service.h"

#include "installscript_vdf.h"
#include "proton_manager.h"
#include "runtime_container_manager.h"
#include "install_heuristics.h"
#include "runtime_depot_catalog.h"
#include "runtime_manifest_probe.h"
#include "settings_store.h"
#include "steamworks_install_scripts.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWaitCondition>

#include <memory>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace arachnel::core {


#include "runtime_dependency_service_helpers.h"

bool RuntimeDependencyService::installDepotIntoContainer(
    const RuntimeDepotRef& depot, const RuntimeEnsureRequest& request,
    ProtonManager* protonManager, SettingsStore* settings,
    const std::function<void(const QString&)>& onStatus, QString* errorOut) const
{
#if defined(Q_OS_LINUX)
    if (!isWindowsRuntimeDepot(depot))
        return true;
    // Windows .NET Framework redists are flaky under Proton; wine-mono covers Stardew-class
    // titles. Don't block launch on Steamworks depots like 229002 (.NET 4.0).
    if (RuntimeDepotCatalog::isDotNetDepotId(depot.depotId))
        return true;
#endif

    RuntimeContainerManager containers;
    const QString cacheDir =
        containers.cacheDirForGame(request.gameId) + QStringLiteral("/redist/") + depot.depotId;
    QDir().mkpath(cacheDir);

    const QString prefixDir = containers.prefixDirForGame(request.gameId);
    if (isDepotInstalledInPrefix(depot, prefixDir)) {
        containers.markDepotInstalled(request.gameId, request.steamAppId, depot.depotId);
        return true;
    }

    RedistInstallPlan plan = buildRedistInstallPlan(depot.depotId);
    QVector<RedistInstallStep> steps = plan.steps;

    // Steam's bundled VC for 228986–228989 is 2015-era; always prefer aka.ms unified redist.
    const bool preferVcCdn = depot.depotId == QStringLiteral("228986")
                             || depot.depotId == QStringLiteral("228987")
                             || depot.depotId == QStringLiteral("228988")
                             || depot.depotId == QStringLiteral("228989");
    if (preferVcCdn)
        steps.clear();

    // VC without Steamworks Shared content (or forced CDN): Microsoft CDN into cache.
    if (steps.isEmpty() && depotHasCdnFallback(depot.depotId)) {
        const QStringList names = installerNamesForDepot(depot.depotId);
        if (!names.isEmpty()) {
            const QString cdnPath = cacheDir + QLatin1Char('/') + names.first();
            if (!QFileInfo::exists(cdnPath)) {
                if (onStatus) {
                    onStatus(QCoreApplication::translate("Core", "Downloading runtime: %1")
                                 .arg(depot.label));
                }
                QString downloadError;
                if (!downloadCdnFallbackInstaller(network(), depot.depotId, cdnPath, &downloadError)) {
                    if (errorOut)
                        *errorOut = downloadError;
                    return false;
                }
            }
            RedistInstallStep step;
            step.processPath = cdnPath;
            step.arguments = silentArgsForInstaller(cdnPath);
            steps.append(step);
        }
    }

    // Secondary: tree-walk cache / local _CommonRedist by exe name (non-VC scripts missing).
    if (steps.isEmpty()) {
        QString installerPath = findInstallerInTree(cacheDir, depot.depotId);
        if (installerPath.isEmpty())
            installerPath = findSteamCommonRedistInstaller(depot.depotId);
        if (!installerPath.isEmpty()) {
            RedistInstallStep step;
            step.processPath = installerPath;
            step.arguments = silentArgsForInstaller(installerPath);
            steps.append(step);
        }
    }

    // Soft-skip when no installer content (OpenAL / XNA / unknown without Steamworks Shared).
    if (steps.isEmpty())
        return true;

    if (onStatus) {
        onStatus(QCoreApplication::translate("Core", "Installing runtime: %1").arg(depot.label));
    }

#if defined(Q_OS_LINUX)
    if (!protonManager || !settings) {
        if (errorOut)
            *errorOut = QCoreApplication::translate(
                "Core", "Proton is required to install runtime dependencies");
        return false;
    }
    const QString protonId = settings->resolvedProtonId(request.protonId, *protonManager);
    const QString protonExecutable = protonManager->executableForId(protonId);
    if (protonExecutable.isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate(
                "Core", "Proton not found. Install Proton-GE in Settings → Launch.");
        return false;
    }
    const QProcessEnvironment env =
        protonEnvForGame(protonManager, settings, request.gameId, request.protonId);
#else
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#endif

    for (const RedistInstallStep& step : steps) {
        if (step.processPath.isEmpty() || !QFileInfo::exists(step.processPath))
            continue;

        QStringList silentArgs = step.arguments;
        if (silentArgs.isEmpty())
            silentArgs = silentArgsForInstaller(step.processPath);
        // Empty silent args = interactive installer (e.g. some DX web setups) - skip.
        if (silentArgs.isEmpty()
            && step.processPath.toLower().contains(QStringLiteral("websetup"))) {
            continue;
        }

        int exitCode = -1;
        QString stepError;
#if defined(Q_OS_LINUX)
        QStringList args = {QStringLiteral("run"), step.processPath};
        args += silentArgs;
        runSilentInstaller(protonExecutable, args, QFileInfo(step.processPath).absolutePath(),
                           env, &exitCode, &stepError, step.processPath);
#else
        runSilentInstaller(step.processPath, silentArgs,
                           QFileInfo(step.processPath).absolutePath(), env, &exitCode, &stepError,
                           step.processPath);
#endif
        // Continue remaining steps (x86 then x64); prefix probe decides final success.
        Q_UNUSED(exitCode);
        Q_UNUSED(stepError);
    }

    if (!isDepotInstalledInPrefix(depot, prefixDir)) {
        // HasRunKey alone is enough when CRT/DX probes don't apply (OpenAL / XNA).
        bool hasRunOk = false;
        for (const RedistInstallStep& step : steps) {
            if (!step.hasRunKey.isEmpty() && prefixHasRunKey(prefixDir, step.hasRunKey)) {
                hasRunOk = true;
                break;
            }
        }
        if (!hasRunOk) {
            if (errorOut && errorOut->isEmpty()) {
                *errorOut = QCoreApplication::translate(
                                "Core", "Runtime install did not register in the Proton prefix: %1")
                                .arg(depot.label);
            }
            return false;
        }
    }

    containers.markDepotInstalled(request.gameId, request.steamAppId, depot.depotId);
    return true;
}

RuntimeEnsureResult RuntimeDependencyService::ensureInstalled(
    const RuntimeEnsureRequest& request, ProtonManager* protonManager, SettingsStore* settings,
    const std::function<void(const QString&)>& onStatus) const
{
    RuntimeEnsureResult result;

#if !defined(Q_OS_LINUX)
    // Native Windows: no Proton prefix / Wine redist container to prepare.
    Q_UNUSED(request);
    Q_UNUSED(protonManager);
    Q_UNUSED(settings);
    Q_UNUSED(onStatus);
    result.success = true;
    return result;
#else
    QString steamAppId = request.steamAppId.trimmed();
    if (steamAppId.isEmpty())
        steamAppId = resolveSteamAppIdFromTitle(network(), request.title);

    QString resolveError;
    QVector<RuntimeDepotRef> deps;
    if (!steamAppId.isEmpty())
        deps = resolveFromSteamCmd(steamAppId, &resolveError);

    if (!request.installPath.isEmpty()) {
        const QString executable = findGameExecutableInTree(request.installPath);
        if (!executable.isEmpty()) {
            const ManifestRuntimeNeeds needs = probeExecutableManifest(executable);
            deps = mergeDependencies(deps, depotsFromManifestNeeds(needs));
            // Windows .exe under Proton: always ensure unified VC++ 2015-2022 x64
            // (Steam shared depots / PE probe often miss it; games then show MSVC dialog).
            if (executable.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
                ManifestRuntimeNeeds fallback;
                fallback.needsVc2015x64 = true;
                deps = mergeDependencies(deps, depotsFromManifestNeeds(fallback));
            }
        }
    }

    if (deps.isEmpty()) {
        result.success = true;
        return result;
    }

    RuntimeEnsureRequest effectiveRequest = request;
    effectiveRequest.steamAppId = steamAppId;

    for (const RuntimeDepotRef& depot : deps) {
        if (!isWindowsRuntimeDepot(depot)) {
            result.skippedLabels.append(depot.label);
            continue;
        }
        if (isDepotSatisfied(depot, request.gameId)) {
            result.skippedLabels.append(depot.label);
            continue;
        }

        QString installError;
        if (!installDepotIntoContainer(depot, effectiveRequest, protonManager, settings, onStatus,
                                       &installError)) {
            result.success = false;
            result.error = installError;
            return result;
        }
        result.installedLabels.append(depot.label);
    }

    result.success = true;
    return result;
#endif
}

QVariantMap RuntimeDependencyService::containerInfoForGame(const RuntimeEnsureRequest& request) const
{
    QVariantMap out;
#if !defined(Q_OS_LINUX)
    (void)request;
    return out;
#else
    RuntimeContainerManager containers;
    const QString gameId = request.gameId;
    out.insert(QStringLiteral("containerPath"), containers.containerRootForGame(gameId));
    out.insert(QStringLiteral("prefixPath"), containers.prefixDirForGame(gameId));
    out.insert(QStringLiteral("cachePath"), containers.cacheDirForGame(gameId));
    out.insert(QStringLiteral("prefixExists"),
               QDir(containers.prefixDirForGame(gameId)).exists());

    // UI-only: never hit the network here. Binding/settings must stay sync-safe on the
    // GUI thread (no nested event loops / steamcmd / store search).
    const QString steamAppId = request.steamAppId.trimmed();
    out.insert(QStringLiteral("steamAppId"), steamAppId);

    QVector<RuntimeDepotRef> deps;
    for (const QString& depotId : containers.installedDepotIds(gameId)) {
        RuntimeDepotRef ref;
        ref.depotId = depotId;
        ref.label = RuntimeDepotCatalog::labelForDepotId(depotId);
        ref.osList = QStringLiteral("windows");
        deps.append(ref);
    }

    if (!request.installPath.isEmpty()) {
        const QString executable = findGameExecutableInTree(request.installPath);
        if (!executable.isEmpty()) {
            const ManifestRuntimeNeeds needs = probeExecutableManifest(executable);
            deps = mergeDependencies(deps, depotsFromManifestNeeds(needs));
        }
    }

    QVariantList depRows;
    int installedCount = 0;
    for (const RuntimeDepotRef& depot : deps) {
        if (!isWindowsRuntimeDepot(depot))
            continue;
        const bool installed = isDepotSatisfied(depot, gameId);
        if (installed)
            ++installedCount;
        QVariantMap row;
        row.insert(QStringLiteral("depotId"), depot.depotId);
        row.insert(QStringLiteral("label"), depot.label);
        row.insert(QStringLiteral("installed"), installed);
        depRows.append(row);
    }
    out.insert(QStringLiteral("dependencies"), depRows);
    out.insert(QStringLiteral("installedCount"), installedCount);
    out.insert(QStringLiteral("totalCount"), depRows.size());
    return out;
#endif
}

} // namespace arachnel::core
