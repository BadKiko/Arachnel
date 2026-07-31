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

namespace {

bool copyDirContents(const QString& srcDir, const QString& destDir)
{
    QDir().mkpath(destDir);
    QDir source(srcDir);
    if (!source.exists())
        return false;

    const QFileInfoList entries =
        source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        const QString destPath = destDir + QLatin1Char('/') + entry.fileName();
        if (entry.isDir()) {
            if (!copyDirContents(entry.absoluteFilePath(), destPath))
                return false;
            continue;
        }
        if (QFileInfo::exists(destPath)) {
            const QFileInfo existing(destPath);
            if (existing.size() == entry.size())
                continue;
            QFile::remove(destPath);
        }
        if (!QFile::copy(entry.absoluteFilePath(), destPath))
            return false;
    }
    return true;
}

/**
 * Proton/Wine often break on installer paths that contain spaces
 * (Steamworks Shared, game folders with spaces). Stage into our cache (no spaces).
 */
QString stageInstallerForProton(const QString& cacheDir, const QString& depotId,
                                const QString& installerPath)
{
    const QFileInfo src(installerPath);
    if (!src.isFile())
        return {};

    const QString abs = QDir::cleanPath(src.absoluteFilePath());
    if (!abs.contains(QLatin1Char(' ')))
        return abs;

    const QString stagedRoot =
        QDir::cleanPath(cacheDir + QStringLiteral("/staged/") + depotId);
    const QString srcDir = src.absolutePath();
    if (!copyDirContents(srcDir, stagedRoot)) {
        // Last resort: single-file copy.
        QDir().mkpath(stagedRoot);
        const QString dest = stagedRoot + QLatin1Char('/') + src.fileName();
        QFile::remove(dest);
        if (!QFile::copy(abs, dest))
            return abs;
        return dest;
    }
    return stagedRoot + QLatin1Char('/') + src.fileName();
}

bool appendCdnVcSteps(QNetworkAccessManager* network, const QString& cacheDir,
                      const RuntimeDepotRef& depot,
                      const std::function<void(const QString&)>& onStatus,
                      QVector<RedistInstallStep>* steps, QString* errorOut)
{
    if (!steps)
        return false;

    const bool wantBothArches = RuntimeDepotCatalog::isModernVcDepotId(depot.depotId);
    const QStringList packages =
        wantBothArches
            ? QStringList{QStringLiteral("vc_redist.x64.exe"), QStringLiteral("vc_redist.x86.exe")}
            : installerNamesForDepot(depot.depotId);

    for (const QString& name : packages) {
        const QString cdnPath = cacheDir + QLatin1Char('/') + name;
        if (!QFileInfo::exists(cdnPath)) {
            if (onStatus) {
                onStatus(QCoreApplication::translate("Core", "Downloading runtime: %1")
                             .arg(depot.label));
            }
            const QString fakeDepot = name.contains(QStringLiteral("x64"), Qt::CaseInsensitive)
                                          ? QStringLiteral("228986")
                                          : QStringLiteral("228985");
            QString downloadError;
            if (!downloadCdnFallbackInstaller(network, fakeDepot, cdnPath, &downloadError)) {
                if (errorOut)
                    *errorOut = downloadError;
                return false;
            }
        }
        RedistInstallStep step;
        step.processPath = cdnPath;
        step.arguments = silentArgsForInstaller(cdnPath);
        steps->append(step);
    }
    return !steps->isEmpty();
}

bool runProtonInstallerSteps(const QVector<RedistInstallStep>& steps, const QString& cacheDir,
                             const QString& depotId, const QString& protonExecutable,
                             const QProcessEnvironment& env)
{
    bool ranAny = false;
    for (const RedistInstallStep& step : steps) {
        if (step.processPath.isEmpty() || !QFileInfo::exists(step.processPath))
            continue;

        const QString staged =
            stageInstallerForProton(cacheDir, depotId, step.processPath);
        if (staged.isEmpty() || !QFileInfo::exists(staged))
            continue;

        QStringList silentArgs = step.arguments;
        if (silentArgs.isEmpty())
            silentArgs = silentArgsForInstaller(staged);
        if (silentArgs.isEmpty()
            && staged.toLower().contains(QStringLiteral("websetup"))) {
            continue;
        }

        int exitCode = -1;
        QString stepError;
#if defined(Q_OS_LINUX)
        QStringList args = {QStringLiteral("run"), staged};
        args += silentArgs;
        runSilentInstaller(protonExecutable, args, QFileInfo(staged).absolutePath(), env,
                           &exitCode, &stepError, staged);
#else
        Q_UNUSED(protonExecutable);
        runSilentInstaller(staged, silentArgs, QFileInfo(staged).absolutePath(), env, &exitCode,
                           &stepError, staged);
#endif
        ranAny = true;
        Q_UNUSED(exitCode);
        Q_UNUSED(stepError);
    }
    return ranAny;
}

} // namespace

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

    // Modern Steamworks VC packages are stale; always use aka.ms (both arches).
    if (RuntimeDepotCatalog::isModernVcDepotId(depot.depotId))
        steps.clear();

    if (steps.isEmpty() && depotHasCdnFallback(depot.depotId)) {
        if (!appendCdnVcSteps(network(), cacheDir, depot, onStatus, &steps, errorOut))
            return false;
    }

    // Secondary: tree-walk cache / local _CommonRedist by exe name.
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
    const QString protonExecutable;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#endif

    runProtonInstallerSteps(steps, cacheDir, depot.depotId, protonExecutable, env);

    auto markOk = [&]() {
        containers.markDepotInstalled(request.gameId, request.steamAppId, depot.depotId);
        return true;
    };

    auto probeOk = [&]() {
        if (isDepotInstalledInPrefix(depot, prefixDir))
            return true;
        for (const RedistInstallStep& step : steps) {
            if (!step.hasRunKey.isEmpty() && prefixHasRunKey(prefixDir, step.hasRunKey))
                return true;
        }
        return false;
    };

    if (probeOk())
        return markOk();

    // Steam VC script ran but CRT still stub/missing → retry Microsoft CDN.
    if (RuntimeDepotCatalog::isVcDepotId(depot.depotId)
        && !RuntimeDepotCatalog::isModernVcDepotId(depot.depotId)) {
        QVector<RedistInstallStep> cdnSteps;
        QString cdnError;
        if (appendCdnVcSteps(network(), cacheDir, depot, onStatus, &cdnSteps, &cdnError)
            && !cdnSteps.isEmpty()) {
            if (onStatus) {
                onStatus(QCoreApplication::translate("Core", "Installing runtime: %1")
                             .arg(depot.label));
            }
            runProtonInstallerSteps(cdnSteps, cacheDir, depot.depotId, protonExecutable, env);
            if (probeOk())
                return markOk();
        }
    }

    // Optional shared redists (OpenAL / XNA / DX without content): don't block Play.
    if (!RuntimeDepotCatalog::isVcDepotId(depot.depotId)
        && !depotHasCdnFallback(depot.depotId)) {
        return true;
    }

    if (errorOut && errorOut->isEmpty()) {
        *errorOut = QCoreApplication::translate(
                        "Core", "Runtime install did not register in the Proton prefix: %1")
                        .arg(depot.label);
    }
    return false;
}

RuntimeEnsureResult RuntimeDependencyService::ensureInstalled(
    const RuntimeEnsureRequest& request, ProtonManager* protonManager, SettingsStore* settings,
    const std::function<void(const QString&)>& onStatus) const
{
    RuntimeEnsureResult result;

#if !defined(Q_OS_LINUX)
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
            // Windows .exe under Proton: always ensure unified VC++ 2015-2022 x64.
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

        // Soft-skip (DotNet / missing optional content) returns true without satisfying probe.
        if (isDepotSatisfied(depot, request.gameId))
            result.installedLabels.append(depot.label);
        else
            result.skippedLabels.append(depot.label);
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
