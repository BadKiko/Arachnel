#include "steamless_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>

#include <utility>

namespace arachnel::core {

namespace {

quint16 readU16Le(const QByteArray& data, int offset)
{
    if (offset < 0 || offset + 2 > data.size())
        return 0;
    return static_cast<quint16>(static_cast<uchar>(data.at(offset)))
        | static_cast<quint16>(static_cast<uchar>(data.at(offset + 1))) << 8;
}

quint32 readU32Le(const QByteArray& data, int offset)
{
    if (offset < 0 || offset + 4 > data.size())
        return 0;
    return static_cast<quint32>(static_cast<uchar>(data.at(offset)))
        | static_cast<quint32>(static_cast<uchar>(data.at(offset + 1))) << 8
        | static_cast<quint32>(static_cast<uchar>(data.at(offset + 2))) << 16
        | static_cast<quint32>(static_cast<uchar>(data.at(offset + 3))) << 24;
}

bool downloadToFile(const QString& url, const QString& path, QString* errorOut)
{
    if (url.isEmpty())
        return false;

    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel"));
    request.setTransferTimeout(120000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        if (errorOut)
            *errorOut = reply->errorString();
        reply->deleteLater();
        return false;
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = file.errorString();
        reply->deleteLater();
        return false;
    }
    file.write(reply->readAll());
    file.close();
    reply->deleteLater();
    return true;
}

bool extractZipArchive(const QString& archivePath, const QString& destDir, QString* errorOut)
{
    QDir().mkpath(destDir);
    {
        QFile file(archivePath);
        if (!file.open(QIODevice::ReadOnly)
            || !file.read(4).startsWith(QByteArray("PK\x03\x04", 4))) {
            if (errorOut)
                *errorOut =
                    QCoreApplication::translate("Core", "Downloaded file is not a ZIP archive");
            return false;
        }
    }

    QProcess process;
#if defined(Q_OS_WIN)
    QString escapedArchive = archivePath;
    escapedArchive.replace(QLatin1Char('\''), QStringLiteral("''"));
    QString escapedDest = destDir;
    escapedDest.replace(QLatin1Char('\''), QStringLiteral("''"));
    process.setProgram(QStringLiteral("powershell"));
    process.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        QStringLiteral(
            "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "[System.IO.Compression.ZipFile]::ExtractToDirectory('%1', '%2')")
            .arg(escapedArchive, escapedDest),
    });
#else
    process.setProgram(QStringLiteral("unzip"));
    process.setArguments({QStringLiteral("-q"), archivePath, QStringLiteral("-d"), destDir});
#endif

    process.start();
    if (!process.waitForStarted(15000)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Could not start archive extraction");
        return false;
    }

    qint64 waitedMs = 0;
    while (!process.waitForFinished(100)) {
        waitedMs += 100;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        if (waitedMs >= 300000) {
            process.kill();
            if (errorOut)
                *errorOut = QCoreApplication::translate("Core", "Archive extraction timed out");
            return false;
        }
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            const QByteArray stderrBytes = process.readAllStandardError();
            *errorOut = stderrBytes.isEmpty()
                            ? QCoreApplication::translate("Core", "Archive extraction failed")
                            : QString::fromUtf8(stderrBytes).trimmed();
        }
        return false;
    }
    return true;
}

QString findFileInTree(const QString& root, const QString& fileName)
{
    if (root.isEmpty() || !QDir(root).exists())
        return {};
    QDirIterator it(root, {fileName}, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

bool copyFileIfMissing(const QString& source, const QString& dest)
{
    if (QFileInfo::exists(dest))
        return true;
    if (!QFileInfo::exists(source))
        return false;
    QDir().mkpath(QFileInfo(dest).absolutePath());
    QFile::remove(dest);
    return QFile::copy(source, dest);
}

bool plantResourceFile(const QString& resourcePath, const QString& dest)
{
    if (QFileInfo::exists(dest))
        return true;
    QFile src(resourcePath);
    if (!src.open(QIODevice::ReadOnly))
        return false;
    QDir().mkpath(QFileInfo(dest).absolutePath());
    QFile::remove(dest);
    QFile out(dest);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    out.write(src.readAll());
    return true;
}

#if !defined(Q_OS_WIN)
QString wineExecutable()
{
    const QString found = QStandardPaths::findExecutable(QStringLiteral("wine"));
    return found.isEmpty() ? QStringLiteral("wine") : found;
}

QString toWinePath(const QString& path)
{
    QString win = QFileInfo(path).absoluteFilePath();
    win.replace(QLatin1Char('/'), QLatin1Char('\\'));
    if (!win.startsWith(QStringLiteral("Z:\\")))
        win.prepend(QStringLiteral("Z:\\"));
    return win;
}
#endif

} // namespace

SteamlessService::SteamlessService(NoticeFn notice, QObject* parent)
    : QObject(parent), m_notice(std::move(notice))
{
}

QString SteamlessService::toolRoot()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/steamless");
    QDir().mkpath(root);
    return root;
}

QString SteamlessService::cliPath() const
{
    const QString envPath = qEnvironmentVariable("ARACHNEL_STEAMLESS_PATH").trimmed();
    if (!envPath.isEmpty() && QFileInfo::exists(envPath))
        return envPath;

    if (!m_cachedCliPath.isEmpty() && QFileInfo::exists(m_cachedCliPath))
        return m_cachedCliPath;

    const QString root = toolRoot();
    const QString direct = root + QStringLiteral("/Steamless.CLI.exe");
    if (QFileInfo::exists(direct)) {
        m_cachedCliPath = direct;
        return direct;
    }

    const QString found = findFileInTree(root, QStringLiteral("Steamless.CLI.exe"));
    if (!found.isEmpty())
        m_cachedCliPath = found;
    return found;
}

bool SteamlessService::isAvailable() const
{
    return !cliPath().isEmpty() && QFileInfo::exists(cliPath());
}

QStringList SteamlessService::collectExeCandidates(const QString& installPath)
{
    QStringList candidates;
    QDirIterator it(installPath, {QStringLiteral("*.exe")}, QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    int seen = 0;
    while (it.hasNext() && seen < 4000) {
        it.next();
        ++seen;
        const QString relative = QDir(installPath).relativeFilePath(it.filePath());
        const int depth = relative.count(QLatin1Char('/')) + relative.count(QLatin1Char('\\'));
        if (depth > 8)
            continue;
        candidates.append(it.filePath());
        if (candidates.size() >= 48)
            break;
    }
    return candidates;
}

bool SteamlessService::ensureTool(QString* errorOut)
{
#if !defined(Q_OS_WIN)
    if (QStandardPaths::findExecutable(QStringLiteral("wine")).isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate(
                "Core", "Steamless needs Wine on Linux (install wine first)");
        return false;
    }
#endif

    if (!isAvailable()) {
        QString assetUrl;
        QString assetName;
        {
            QNetworkAccessManager nam;
            QNetworkRequest request(QUrl(
                QStringLiteral("https://api.github.com/repos/atom0s/Steamless/releases/latest")));
            request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel"));
            request.setTransferTimeout(20000);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            QNetworkReply* reply = nam.get(request);
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            if (reply->error() != QNetworkReply::NoError) {
                if (errorOut)
                    *errorOut = reply->errorString();
                reply->deleteLater();
                return false;
            }

            const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
            for (const QJsonValue& value : assets) {
                const QString name = value.toObject().value(QStringLiteral("name")).toString();
                if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)
                    && name.contains(QStringLiteral("win-x64"), Qt::CaseInsensitive)) {
                    assetUrl =
                        value.toObject().value(QStringLiteral("browser_download_url")).toString();
                    assetName = name;
                    break;
                }
            }
            if (assetUrl.isEmpty()) {
                for (const QJsonValue& value : assets) {
                    const QString name = value.toObject().value(QStringLiteral("name")).toString();
                    if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                        assetUrl = value.toObject()
                                       .value(QStringLiteral("browser_download_url"))
                                       .toString();
                        assetName = name;
                        break;
                    }
                }
            }
            reply->deleteLater();
        }

        if (assetUrl.isEmpty()) {
            if (errorOut)
                *errorOut =
                    QCoreApplication::translate("Core", "No Steamless release asset found");
            return false;
        }

        const QString root = toolRoot();
        const QString zipPath = root + QStringLiteral("/.download/") + assetName;
        QString downloadError;
        if (!downloadToFile(assetUrl, zipPath, &downloadError)) {
            if (errorOut)
                *errorOut = downloadError;
            return false;
        }

        QString extractError;
        if (!extractZipArchive(zipPath, root, &extractError)) {
            QFile::remove(zipPath);
            if (errorOut)
                *errorOut = extractError;
            return false;
        }
        QFile::remove(zipPath);

        m_cachedCliPath = findFileInTree(root, QStringLiteral("Steamless.CLI.exe"));
        if (m_cachedCliPath.isEmpty()) {
            if (errorOut)
                *errorOut = QCoreApplication::translate(
                    "Core", "Steamless.CLI.exe not found in release");
            return false;
        }
        emit toolAvailableChanged();
    }

#if !defined(Q_OS_WIN)
    QString runtimeError;
    if (!prepareLinuxRuntime(cliPath(), &runtimeError)) {
        if (errorOut)
            *errorOut = runtimeError;
        return false;
    }
#endif
    return true;
}

#if !defined(Q_OS_WIN)
bool SteamlessService::prepareLinuxRuntime(const QString& cliPath, QString* errorOut)
{
    const QString cliDir = QFileInfo(cliPath).absolutePath();

    // Wine Mono resolves Steamless.API from the exe directory.
    const QString pluginsDir = cliDir + QStringLiteral("/Plugins");
    if (QDir(pluginsDir).exists()) {
        QDirIterator it(pluginsDir, {QStringLiteral("*.dll")}, QDir::Files);
        while (it.hasNext()) {
            it.next();
            copyFileIfMissing(it.filePath(), cliDir + QLatin1Char('/') + it.fileName());
        }
    }

    // Bundled MinGW runtime - no MSYS2 download.
    const QStringList runtimeDlls = {QStringLiteral("libgcc_s_dw2-1.dll"),
                                     QStringLiteral("libwinpthread-1.dll")};
    for (const QString& dll : runtimeDlls) {
        const QString dest = cliDir + QLatin1Char('/') + dll;
        if (QFileInfo::exists(dest))
            continue;
        if (plantResourceFile(QStringLiteral(":/steamless/") + dll, dest))
            continue;
        if (errorOut) {
            *errorOut = QCoreApplication::translate(
                             "Core",
                             "Steamless needs %1 next to the CLI (bundled runtime missing)")
                             .arg(dll);
        }
        return false;
    }
    return true;
}
#endif

void SteamlessService::ensureSetup()
{
    ensureTool(nullptr);
}

bool SteamlessService::hasSteamStub(const QString& exePath)
{
    QFile file(exePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray dos = file.read(0x40);
    if (dos.size() < 0x40 || dos.at(0) != 'M' || dos.at(1) != 'Z')
        return false;

    const qint64 peOffset = static_cast<qint64>(readU32Le(dos, 0x3C));
    if (peOffset <= 0 || peOffset > 0x40000000 || !file.seek(peOffset))
        return false;

    const QByteArray signature = file.read(4);
    if (signature.size() != 4 || signature != QByteArray("PE\0\0", 4))
        return false;

    const QByteArray coff = file.read(20);
    if (coff.size() < 20)
        return false;

    const int sectionCount = readU16Le(coff, 2);
    const int optionalSize = readU16Le(coff, 16);
    if (sectionCount <= 0 || sectionCount > 128 || optionalSize <= 0)
        return false;

    if (file.read(optionalSize).size() < optionalSize)
        return false;

    for (int i = 0; i < sectionCount; ++i) {
        const QByteArray section = file.read(40);
        if (section.size() < 40)
            break;
        QByteArray name = section.left(8);
        const int nul = name.indexOf('\0');
        if (nul >= 0)
            name.truncate(nul);
        if (name == ".bind")
            return true;
    }
    return false;
}

bool SteamlessService::stripExecutable(const QString& exePath, const QString& cliPath,
                                       QString* errorOut)
{
    const QString unpackedPath = exePath + QStringLiteral(".unpacked.exe");
    QFile::remove(unpackedPath);

    QProcess process;
#if defined(Q_OS_WIN)
    process.setProgram(cliPath);
    process.setArguments({QStringLiteral("--quiet"), exePath});
#else
    if (QStandardPaths::findExecutable(QStringLiteral("wine")).isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate(
                "Core", "Steamless needs Wine on Linux (install wine first)");
        return false;
    }
    process.setProgram(wineExecutable());
    process.setArguments({cliPath, QStringLiteral("--quiet"), toWinePath(exePath)});
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
    process.setProcessEnvironment(env);
#endif
    process.setWorkingDirectory(QFileInfo(cliPath).absolutePath());
    process.start();

    if (!process.waitForStarted(15000)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Steamless failed to start");
        return false;
    }
    if (!process.waitForFinished(180000)) {
        process.kill();
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Steamless timed out");
        return false;
    }

    if (!QFileInfo::exists(unpackedPath)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "No unpacked output produced");
        return false;
    }

    const QString backupPath = exePath + QStringLiteral(".steamstub.bak");
    QFile::remove(backupPath);
    if (!QFile::rename(exePath, backupPath)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Could not back up original executable");
        return false;
    }
    if (!QFile::rename(unpackedPath, exePath)) {
        QFile::rename(backupPath, exePath);
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Could not replace executable");
        return false;
    }
    return true;
}

SteamlessService::Result SteamlessService::processInstallSync(const QString& installPath,
                                                              const QString& cliPath)
{
    Result result;
    if (installPath.isEmpty() || !QFileInfo::exists(installPath)) {
        result.error = QCoreApplication::translate("Core", "Game folder not found");
        return result;
    }

    const QStringList candidates = collectExeCandidates(installPath);
    if (candidates.isEmpty()) {
        result.error = QCoreApplication::translate("Core", "No executables found");
        return result;
    }

    for (const QString& exe : candidates) {
        if (!hasSteamStub(exe))
            continue;
        QString error;
        if (stripExecutable(exe, cliPath, &error)) {
            ++result.stripped;
            result.messages.append(QCoreApplication::translate("Core", "Steamless unpacked %1")
                                       .arg(QFileInfo(exe).fileName()));
        } else {
            result.messages.append(QCoreApplication::translate("Core", "Steamless failed on %1: %2")
                                       .arg(QFileInfo(exe).fileName(),
                                            error.isEmpty()
                                                ? QCoreApplication::translate("Core", "unknown error")
                                                : error));
        }
    }
    return result;
}

void SteamlessService::processInstall(const QString& installPath, const QString& title)
{
    if (installPath.isEmpty())
        return;

    QString error;
    if (!ensureTool(&error)) {
        if (m_notice) {
            m_notice(QCoreApplication::translate("Core", "Steamless is not available: %1")
                         .arg(error.isEmpty()
                                  ? QCoreApplication::translate("Core", "unknown error")
                                  : error));
        }
        return;
    }

    const QString cli = cliPath();
    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this,
            [this, watcher, installPath, title]() {
                const Result result = watcher->result();
                watcher->deleteLater();

                if (!result.error.isEmpty() && result.messages.isEmpty()) {
                    if (m_notice)
                        m_notice(QCoreApplication::translate("Core", "Steamless: %1").arg(result.error));
                } else if (result.stripped > 0) {
                    for (const QString& message : result.messages) {
                        if (m_notice)
                            m_notice(message);
                    }
                    if (m_notice) {
                        m_notice(QCoreApplication::translate(
                                     "Core", "Steamless removed SteamStub from %1 file(s) in %2")
                                     .arg(result.stripped)
                                     .arg(title));
                    }
                }
                emit finished(installPath, result.stripped);
            });

    watcher->setFuture(QtConcurrent::run([installPath, cli]() {
        return processInstallSync(installPath, cli);
    }));
}

int SteamlessService::ensureUnpacked(const QString& installPath, QString* errorOut)
{
    if (installPath.isEmpty() || !QFileInfo::exists(installPath)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Game folder not found");
        return -1;
    }

    bool needsStrip = false;
    for (const QString& exe : collectExeCandidates(installPath)) {
        if (hasSteamStub(exe)) {
            needsStrip = true;
            break;
        }
    }
    if (!needsStrip)
        return 0;

    QString toolError;
    if (!ensureTool(&toolError)) {
        if (errorOut) {
            *errorOut = toolError.isEmpty()
                            ? QCoreApplication::translate("Core", "Steamless is not available")
                            : toolError;
        }
        return -1;
    }

    const Result result = processInstallSync(installPath, cliPath());
    if (!result.error.isEmpty() && result.stripped == 0) {
        if (errorOut)
            *errorOut = result.error;
        return -1;
    }
    return result.stripped;
}

QVariantMap SteamlessService::installInfo(const QString& installPath)
{
    QVariantMap info{
        {QStringLiteral("steamlessRelevant"), false},
        {QStringLiteral("steamlessApplied"), false},
        {QStringLiteral("steamlessStubPresent"), false},
        {QStringLiteral("steamlessLabel"),
         QCoreApplication::translate("Core", "Not needed")},
    };
    if (installPath.isEmpty() || !QFileInfo::exists(installPath))
        return info;

    const QStringList candidates = collectExeCandidates(installPath);
    if (candidates.isEmpty())
        return info;

    info.insert(QStringLiteral("steamlessRelevant"), true);
    bool stubPresent = false;
    bool applied = false;
    for (const QString& exe : candidates) {
        if (hasSteamStub(exe))
            stubPresent = true;
        if (QFileInfo::exists(exe + QStringLiteral(".steamstub.bak")))
            applied = true;
    }
    info.insert(QStringLiteral("steamlessStubPresent"), stubPresent);
    info.insert(QStringLiteral("steamlessApplied"), applied && !stubPresent);

    QString label;
    if (stubPresent)
        label = QCoreApplication::translate("Core", "Needed");
    else if (applied)
        label = QCoreApplication::translate("Core", "Applied");
    else
        label = QCoreApplication::translate("Core", "Not needed");
    info.insert(QStringLiteral("steamlessLabel"), label);
    return info;
}

} // namespace arachnel::core
