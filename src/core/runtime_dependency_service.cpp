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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace arachnel::core {

namespace {

constexpr auto kSteamCmdInfo = "https://api.steamcmd.net/v1/info/";

#if defined(Q_OS_WIN)
void hideProcessWindow(QProcess& process)
{
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
}
#endif

QByteArray httpGetBlocking(QNetworkAccessManager* network, const QUrl& url, int timeoutMs,
                           QString* errorOut)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    request.setTransferTimeout(timeoutMs);

    QEventLoop loop;
    QNetworkReply* reply = network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        if (errorOut)
            *errorOut = reply->errorString();
        reply->deleteLater();
        return {};
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();
    return body;
}

bool downloadFileBlocking(QNetworkAccessManager* network, const QUrl& url,
                          const QString& destination, QString* errorOut)
{
    const QByteArray body = httpGetBlocking(network, url, 120000, errorOut);
    if (body.isEmpty())
        return false;

    QFile file(destination);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Could not write file: %1")
                            .arg(destination);
        return false;
    }
    file.write(body);
    return true;
}

bool installerExitOk(int exitCode, const QString& installerPath = {})
{
    if (exitCode == 0 || exitCode == 1638 || exitCode == 3010)
        return true;
    // DXSETUP often returns 247 under Proton/Wine even when DLLs were registered.
    if (exitCode == 247 && installerPath.toLower().contains(QStringLiteral("dxsetup.exe")))
        return true;
    return false;
}

bool runSilentInstaller(const QString& program, const QStringList& args, const QString& workDir,
                        const QProcessEnvironment& env, int* exitCodeOut, QString* errorOut,
                        const QString& errorLabel = {})
{
    const QString label = errorLabel.isEmpty() ? program : errorLabel;
    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    process.setProcessEnvironment(env);
    if (!workDir.isEmpty())
        process.setWorkingDirectory(workDir);
#if defined(Q_OS_WIN)
    hideProcessWindow(process);
#endif
    process.start();
    if (!process.waitForStarted(15000)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Could not start installer: %1")
                            .arg(label);
        return false;
    }
    if (!process.waitForFinished(600000)) {
        process.kill();
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Installer timed out: %1").arg(label);
        return false;
    }
    const int code = process.exitCode();
    if (exitCodeOut)
        *exitCodeOut = code;
    if (!installerExitOk(code, label)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Installer failed (%1): %2")
                            .arg(code)
                            .arg(label);
        return false;
    }
    return true;
}

bool isWindowsRuntimeDepot(const RuntimeDepotRef& depot)
{
    const QString os = depot.osList.trimmed().toLower();
    return os.isEmpty() || os == QStringLiteral("windows");
}

#if defined(Q_OS_WIN)
bool isVcRedist2015Installed(bool wantX64)
{
    const QStringList roots = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"),
        QStringLiteral(
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"),
    };
    const QString needle =
        wantX64 ? QStringLiteral("Microsoft Visual C++ 2015-2022 Redistributable (x64)")
                : QStringLiteral("Microsoft Visual C++ 2015-2022 Redistributable (x86)");

    for (const QString& root : roots) {
        QSettings settings(root, QSettings::NativeFormat);
        for (const QString& key : settings.childGroups()) {
            settings.beginGroup(key);
            const QString name = settings.value(QStringLiteral("DisplayName")).toString();
            settings.endGroup();
            if (name.contains(needle))
                return true;
        }
    }
    return false;
}

bool isDepotInstalledInRegistry(const QString& depotId)
{
    QSettings commonRedist(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam\\Apps\\CommonRedist"),
        QSettings::NativeFormat);
    if (commonRedist.contains(depotId))
        return commonRedist.value(depotId).toInt() != 0;

    QSettings commonRedistNative(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam\\Apps\\CommonRedist"),
        QSettings::NativeFormat);
    if (commonRedistNative.contains(depotId))
        return commonRedistNative.value(depotId).toInt() != 0;
    return false;
}
#endif

QString steamCommonRedistRoot()
{
#if defined(Q_OS_WIN)
    QSettings steam(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"),
                    QSettings::NativeFormat);
    const QString steamPath = steam.value(QStringLiteral("SteamPath")).toString();
    if (steamPath.isEmpty())
        return {};
    return QDir(steamPath).filePath(
        QStringLiteral("steamapps/common/Steamworks Shared/_CommonRedist"));
#else
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QStringList candidates = {
        home + QStringLiteral("/.steam/debian-installation/steamapps/common/Steamworks "
                              "Shared/_CommonRedist"),
        home + QStringLiteral("/.steam/root/steamapps/common/Steamworks Shared/_CommonRedist"),
        home + QStringLiteral("/.local/share/Steam/steamapps/common/Steamworks "
                              "Shared/_CommonRedist"),
    };
    for (const QString& path : candidates) {
        if (QDir(path).exists())
            return path;
    }
    return {};
#endif
}

bool downloadCdnFallbackInstaller(QNetworkAccessManager* network, const QString& depotId,
                                  const QString& destination, QString* errorOut)
{
    QUrl url;
    if (RuntimeDepotCatalog::isX64VcDepotId(depotId))
        url = QUrl(QStringLiteral("https://aka.ms/vs/17/release/vc_redist.x64.exe"));
    else if (RuntimeDepotCatalog::isVcDepotId(depotId))
        url = QUrl(QStringLiteral("https://aka.ms/vs/17/release/vc_redist.x86.exe"));
    else
        return false;

    return downloadFileBlocking(network, url, destination, errorOut);
}

QProcessEnvironment protonEnvForGame(ProtonManager* protonManager, SettingsStore* settings,
                                     const QString& gameId)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"),
               protonManager->steamCompatClientPath());
    env.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"),
               protonManager->compatDataPathForGame(gameId));
    env.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
    const QString protonId = settings->resolvedProtonId(QString(), *protonManager);
    const QString installDir = protonManager->installDirForId(protonId);
    if (!installDir.isEmpty())
        env.insert(QStringLiteral("PROTON_PATH"), installDir);
    QDir().mkpath(protonManager->compatDataPathForGame(gameId) + QStringLiteral("/pfx"));
    return env;
}

QStringList silentArgsForInstaller(const QString& installerPath)
{
    const QString lower = installerPath.toLower();
    if (lower.contains(QStringLiteral("dxsetup.exe")))
        return {QStringLiteral("/silent")};
    if (lower.contains(QStringLiteral("websetup"))
        || lower.contains(QStringLiteral("dxwebsetup")))
        return {};
    if (lower.contains(QStringLiteral("ndp48")) || lower.contains(QStringLiteral("dotnet")))
        return {QStringLiteral("/q"), QStringLiteral("/norestart")};
    return {QStringLiteral("/install"), QStringLiteral("/quiet"), QStringLiteral("/norestart")};
}

QString resolveSteamAppIdFromTitle(QNetworkAccessManager* network, const QString& title)
{
    if (title.trimmed().isEmpty())
        return {};

    QUrl url(QStringLiteral("https://store.steampowered.com/api/storesearch/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), title.trimmed());
    query.addQueryItem(QStringLiteral("l"), QStringLiteral("english"));
    query.addQueryItem(QStringLiteral("cc"), QStringLiteral("US"));
    url.setQuery(query);

    QString error;
    const QByteArray body = httpGetBlocking(network, url, 15000, &error);
    if (body.isEmpty())
        return {};

    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    const QString titleLower = title.trimmed().toLower();
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("name")).toString().trimmed().toLower() == titleLower)
            return QString::number(item.value(QStringLiteral("id")).toInt());
    }
    if (!items.isEmpty())
        return QString::number(items.first().toObject().value(QStringLiteral("id")).toInt());
    return {};
}

QStringList installerNamesForDepot(const QString& depotId)
{
    if (depotId == QStringLiteral("228990"))
        return {QStringLiteral("DXSETUP.exe")};
    if (RuntimeDepotCatalog::isX64VcDepotId(depotId))
        return {QStringLiteral("vc_redist.x64.exe")};
    if (RuntimeDepotCatalog::isVcDepotId(depotId))
        return {QStringLiteral("vc_redist.x86.exe"), QStringLiteral("vcredist_x86.exe")};
    if (depotId == QStringLiteral("229020"))
        return {QStringLiteral("ndp48-x86-x64.exe"), QStringLiteral("NDP48-x86-x64-AllOS-ENU.exe")};
    return {};
}

QString findInstallerInTree(const QString& root, const QString& depotId)
{
    const QStringList names = installerNamesForDepot(depotId);
    if (names.isEmpty())
        return {};

    for (const QString& name : names) {
        QDirIterator it(root, {name}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext())
            return it.next();
    }
    return {};
}

QString findSteamCommonRedistInstaller(const QString& depotId)
{
    // Bundled Steam _CommonRedist VC is 2015-era; depots 228986–228989 need unified 2015-2022.
    if (depotId == QStringLiteral("228986") || depotId == QStringLiteral("228987")
        || depotId == QStringLiteral("228988") || depotId == QStringLiteral("228989"))
        return {};

    const QString root = steamCommonRedistRoot();
    if (root.isEmpty())
        return {};

    for (const QString& name : installerNamesForDepot(depotId)) {
        QDirIterator it(root, {name}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext())
            return it.next();
    }
    return {};
}

bool prefixFileExists(const QString& prefixDir, const QString& wineRelativePath)
{
    const QString base = prefixDir + QStringLiteral("/drive_c/");
    const QStringList candidates = {
        base + wineRelativePath,
        base + wineRelativePath.toLower(),
    };
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path))
            return true;
    }

    const QString fileName = QFileInfo(wineRelativePath).fileName();
    if (fileName.isEmpty())
        return false;

    QDirIterator it(prefixDir + QStringLiteral("/drive_c"), {fileName}, QDir::Files,
                    QDirIterator::Subdirectories);
    return it.hasNext();
}

bool isVcRedist2015InstalledInPrefix(const QString& prefixDir, bool wantX64)
{
    const QStringList needles =
        wantX64 ? QStringList{QStringLiteral("Microsoft Visual C++ 2015-2022 Redistributable (x64)"),
                              QStringLiteral("Visual C++ 2022 X64 Minimum Runtime")}
                : QStringList{QStringLiteral("Microsoft Visual C++ 2015-2022 Redistributable (x86)"),
                              QStringLiteral("Visual C++ 2022 X86 Minimum Runtime")};

    for (const QString& regName :
         {QStringLiteral("user.reg"), QStringLiteral("system.reg")}) {
        QFile file(prefixDir + QLatin1Char('/') + regName);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QString regText = QString::fromUtf8(file.readAll());
        for (const QString& needle : needles) {
            if (regText.contains(needle))
                return true;
        }
    }

    const QString dllRel =
        wantX64 ? QStringLiteral("Windows/System32/vcruntime140.dll")
                : QStringLiteral("Windows/SysWOW64/vcruntime140.dll");
    return prefixFileExists(prefixDir, dllRel);
}

bool isDirectXRedistInstalledInPrefix(const QString& prefixDir)
{
    return prefixFileExists(prefixDir, QStringLiteral("Windows/System32/d3dx9_43.dll"));
}

bool isDepotInstalledInPrefix(const RuntimeDepotRef& depot, const QString& prefixDir)
{
    if (RuntimeDepotCatalog::isVcDepotId(depot.depotId))
        return isVcRedist2015InstalledInPrefix(prefixDir,
                                               RuntimeDepotCatalog::isX64VcDepotId(depot.depotId));
    if (depot.depotId == QStringLiteral("228990"))
        return isDirectXRedistInstalledInPrefix(prefixDir);
    return false;
}

} // namespace

RuntimeDependencyService::RuntimeDependencyService(QObject* parent)
    : QObject(parent)
{
}

QNetworkAccessManager* RuntimeDependencyService::network() const
{
    if (!m_network)
        m_network = new QNetworkAccessManager(const_cast<RuntimeDependencyService*>(this));
    return m_network;
}

QVector<RuntimeDepotRef> RuntimeDependencyService::resolveFromSteamCmd(const QString& steamAppId,
                                                                       QString* errorOut) const
{
    QVector<RuntimeDepotRef> result;
    const QUrl url(QString::fromUtf8(kSteamCmdInfo) + steamAppId);
    const QByteArray body = httpGetBlocking(network(), url, 20000, errorOut);
    if (body.isEmpty())
        return result;

    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonObject app = data.value(steamAppId).toObject();
    const QJsonObject depots = app.value(QStringLiteral("depots")).toObject();

    for (auto it = depots.begin(); it != depots.end(); ++it) {
        const QString depotId = it.key();
        if (!depotId.toInt())
            continue;
        const QJsonObject block = it.value().toObject();
        const bool shared = block.value(QStringLiteral("sharedinstall")).toString()
                                == QStringLiteral("1")
                            || block.value(QStringLiteral("sharedinstall")).toBool();
        const QString fromApp = block.value(QStringLiteral("depotfromapp")).toString();
        if (!shared && !RuntimeDepotCatalog::isSteamworksSharedDepot(depotId))
            continue;
        if (!fromApp.isEmpty() && fromApp != QStringLiteral("228980")
            && !RuntimeDepotCatalog::isSteamworksSharedDepot(depotId))
            continue;

        RuntimeDepotRef ref;
        ref.depotId = depotId;
        ref.label = RuntimeDepotCatalog::labelForDepotId(depotId);
        const QJsonObject config = block.value(QStringLiteral("config")).toObject();
        ref.osList = config.value(QStringLiteral("oslist")).toString();
        result.append(ref);
    }
    return result;
}

QVector<RuntimeDepotRef> RuntimeDependencyService::mergeDependencies(
    const QVector<RuntimeDepotRef>& primary, const QVector<RuntimeDepotRef>& extra) const
{
    QVector<RuntimeDepotRef> merged = primary;
    for (const RuntimeDepotRef& ref : extra) {
        bool found = false;
        for (const RuntimeDepotRef& existing : merged) {
            if (existing.depotId == ref.depotId) {
                found = true;
                break;
            }
        }
        if (!found)
            merged.append(ref);
    }
    return merged;
}

QVector<RuntimeDepotRef> RuntimeDependencyService::resolveDependencies(const QString& steamAppId,
                                                                       QString* errorOut) const
{
    if (steamAppId.trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Steam App ID is missing");
        return {};
    }

    QString steamCmdError;
    QVector<RuntimeDepotRef> deps = resolveFromSteamCmd(steamAppId, &steamCmdError);

    if (deps.isEmpty() && !steamCmdError.isEmpty() && errorOut)
        *errorOut = steamCmdError;

    return deps;
}

bool RuntimeDependencyService::isDepotSatisfied(const RuntimeDepotRef& depot,
                                                const QString& gameId) const
{
    RuntimeContainerManager containers;
    const QString prefixDir = containers.prefixDirForGame(gameId);

#if defined(Q_OS_WIN)
    if (RuntimeDepotCatalog::isVcDepotId(depot.depotId)) {
        if (isVcRedist2015Installed(RuntimeDepotCatalog::isX64VcDepotId(depot.depotId)))
            return true;
    } else if (depot.depotId == QStringLiteral("228990")) {
        if (isDepotInstalledInRegistry(depot.depotId))
            return true;
    } else if (containers.isDepotInstalledForGame(gameId, depot.depotId)) {
        return true;
    }
#else
    if (isDepotInstalledInPrefix(depot, prefixDir))
        return true;
#endif

    if (containers.isDepotInstalledForGame(gameId, depot.depotId))
        containers.unmarkDepotInstalled(gameId, depot.depotId);

    return false;
}

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
#if defined(Q_OS_LINUX)
        if (!isWindowsRuntimeDepot(depot)) {
            result.skippedLabels.append(depot.label);
            continue;
        }
#endif
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

    QString steamAppId = request.steamAppId.trimmed();
    if (steamAppId.isEmpty())
        steamAppId = resolveSteamAppIdFromTitle(network(), request.title);
    out.insert(QStringLiteral("steamAppId"), steamAppId);

    QVector<RuntimeDepotRef> deps;
    if (!steamAppId.isEmpty())
        deps = resolveFromSteamCmd(steamAppId, nullptr);

    if (!request.installPath.isEmpty()) {
        const QString executable = findGameExecutableInTree(request.installPath);
        if (!executable.isEmpty()) {
            const ManifestRuntimeNeeds needs = probeExecutableManifest(executable);
            deps = mergeDependencies(deps, depotsFromManifestNeeds(needs));
        }
    }

    // Also surface depots already recorded in state.json.
    for (const QString& depotId : containers.installedDepotIds(gameId)) {
        RuntimeDepotRef ref;
        ref.depotId = depotId;
        ref.label = RuntimeDepotCatalog::labelForDepotId(depotId);
        ref.osList = QStringLiteral("windows");
        deps = mergeDependencies(deps, {ref});
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
