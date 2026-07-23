namespace {

constexpr auto kSteamCmdInfo = "https://api.steamcmd.net/v1/info/";

#if defined(Q_OS_WIN)
void hideProcessWindow(QProcess& process)
{
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
}
#endif

// Blocking GET that must NOT run a nested event loop on the GUI/QML thread.
// Nested QEventLoop::exec() from Button.onClicked / property bindings aborts with:
// "Object destroyed while one of its QML signal handlers is in progress".
QByteArray httpGetBlocking(QNetworkAccessManager* /*network*/, const QUrl& url, int timeoutMs,
                           QString* errorOut)
{
    struct Result {
        QMutex mutex;
        QWaitCondition done;
        QByteArray body;
        QString error;
        bool finished = false;
    };
    const auto result = std::make_shared<Result>();

    QThread* thread = QThread::create([result, url, timeoutMs]() {
        QNetworkAccessManager nam;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
        request.setTransferTimeout(timeoutMs);

        QEventLoop loop;
        QNetworkReply* reply = nam.get(request);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(timeoutMs + 2000, &loop, &QEventLoop::quit);
        loop.exec();

        QByteArray body;
        QString error;
        if (!reply->isFinished()) {
            reply->abort();
            error = QStringLiteral("Request timed out");
        } else if (reply->error() != QNetworkReply::NoError) {
            error = reply->errorString();
        } else {
            body = reply->readAll();
        }
        reply->deleteLater();

        QMutexLocker lock(&result->mutex);
        result->body = std::move(body);
        result->error = std::move(error);
        result->finished = true;
        result->done.wakeAll();
    });

    thread->start();
    {
        QMutexLocker lock(&result->mutex);
        while (!result->finished)
            result->done.wait(&result->mutex);
    }
    thread->wait();
    delete thread;

    if (errorOut && !result->error.isEmpty())
        *errorOut = result->error;
    return result->body;
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
