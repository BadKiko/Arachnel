#include "runtime_dependency_service.h"

#include "proton_manager.h"
#include "runtime_container_manager.h"
#include "install_heuristics.h"
#include "runtime_depot_catalog.h"
#include "runtime_manifest_probe.h"
#include "settings_store.h"

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

    QString installerPath = findInstallerInTree(cacheDir, depot.depotId);
    if (installerPath.isEmpty())
        installerPath = findSteamCommonRedistInstaller(depot.depotId);

    if (installerPath.isEmpty()) {
        if (depot.depotId == QStringLiteral("229000")) {
            containers.markDepotInstalled(request.gameId, request.steamAppId, depot.depotId);
            return true;
        }

        if (RuntimeDepotCatalog::isVcDepotId(depot.depotId)) {
            const QStringList names = installerNamesForDepot(depot.depotId);
            if (!names.isEmpty()) {
                const QString cdnPath = cacheDir + QLatin1Char('/') + names.first();
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
                installerPath = cdnPath;
            }
        }
    }

    if (installerPath.isEmpty()) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Core", "Installer not found for %1")
                            .arg(depot.label);
        }
        return false;
    }

    if (onStatus) {
        onStatus(QCoreApplication::translate("Core", "Installing runtime: %1").arg(depot.label));
    }

    const QStringList silentArgs = silentArgsForInstaller(installerPath);
    if (silentArgs.isEmpty()) {
        containers.markDepotInstalled(request.gameId, request.steamAppId, depot.depotId);
        return true;
    }

#if defined(Q_OS_LINUX)
    if (!protonManager || !settings) {
        if (errorOut)
            *errorOut = QCoreApplication::translate(
                "Core", "Proton is required to install runtime dependencies");
        return false;
    }
    const QString protonId = settings->resolvedProtonId(QString(), *protonManager);
    const QString protonExecutable = protonManager->executableForId(protonId);
    if (protonExecutable.isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate(
                "Core", "Proton not found. Install Proton-GE in Settings → Launch.");
        return false;
    }

    const QProcessEnvironment env = protonEnvForGame(protonManager, settings, request.gameId);
    QStringList args = {QStringLiteral("run"), installerPath};
    args += silentArgs;

    int exitCode = -1;
    bool installOk =
        runSilentInstaller(protonExecutable, args, QFileInfo(installerPath).absolutePath(), env,
                           &exitCode, errorOut, installerPath);
    if (!installOk && isDepotInstalledInPrefix(depot, prefixDir))
        installOk = true;
    if (!installOk)
        return false;
#else
    int exitCode = -1;
    if (!runSilentInstaller(installerPath, silentArgs, QFileInfo(installerPath).absolutePath(),
                            QProcessEnvironment::systemEnvironment(), &exitCode, errorOut,
                            installerPath))
        return false;
#endif

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
