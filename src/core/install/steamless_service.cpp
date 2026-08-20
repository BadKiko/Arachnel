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
#include <QRegularExpression>
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

bool isZipArchive(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray magic = file.read(4);
    return magic.startsWith(QByteArray("PK\x03\x04", 4));
}

QString escapePowerShellSingleQuotedLiteral(const QString& value)
{
    QString out = value;
    out.replace(QLatin1Char('\''), QStringLiteral("''"));
    return out;
}

bool extractZipArchive(const QString& archivePath, const QString& destDir, QString* errorOut)
{
    QDir().mkpath(destDir);
    if (!isZipArchive(archivePath)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Downloaded file is not a ZIP archive");
        return false;
    }

    QProcess process;
#if defined(Q_OS_WIN)
    process.setProgram(QStringLiteral("powershell"));
    const QString escapedArchive = escapePowerShellSingleQuotedLiteral(archivePath);
    const QString escapedDest = escapePowerShellSingleQuotedLiteral(destDir);
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

// Wine Mono on Fedora-family distros links libmono-2.0-x86.dll against the
// 32-bit MinGW runtime (libgcc_s_dw2-1.dll, libwinpthread-1.dll), but those
// DLLs are not shipped with the wine-mono package. Steamless.CLI.exe is a
// 32-bit .NET app, so mono cannot start without them. Place them next to the
// CLI (wine searches the exe's directory first) so the runtime resolves.
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

// Compare package versions like "16.2.0-3" or "14.0.0.r248.g7735a1a63-1" by
// numeric segments; returns true when a < b.
bool versionLess(const QString& a, const QString& b)
{
    static const QRegularExpression tokenRe(QStringLiteral("[0-9]+|[a-z]+"));
    QStringList ta;
    QStringList tb;
    for (auto it = tokenRe.globalMatch(a); it.hasNext();)
        ta.append(it.next().captured(0));
    for (auto it = tokenRe.globalMatch(b); it.hasNext();)
        tb.append(it.next().captured(0));
    const int n = qMin(ta.size(), tb.size());
    for (int i = 0; i < n; ++i) {
        bool aNum = false;
        bool bNum = false;
        const qlonglong av = ta.at(i).toLongLong(&aNum);
        const qlonglong bv = tb.at(i).toLongLong(&bNum);
        if (aNum && bNum) {
            if (av != bv)
                return av < bv;
        } else if (ta.at(i) != tb.at(i)) {
            return ta.at(i) < tb.at(i);
        }
    }
    return ta.size() < tb.size();
}

// Extract a MSYS2-style .pkg.tar.zst archive (plain zstd-compressed tar).
bool extractTarZst(const QString& archivePath, const QString& destDir, QString* errorOut)
{
    QDir().mkpath(destDir);
    QStringList tried;
    for (const QString& program : {QStringLiteral("tar"), QStringLiteral("bsdtar")}) {
        const QString exe = QStandardPaths::findExecutable(program);
        if (exe.isEmpty())
            continue;
        QProcess process;
        if (program == QLatin1String("tar")) {
            process.setProgram(exe);
            process.setArguments({QStringLiteral("--zstd"), QStringLiteral("-xf"),
                                  archivePath, QStringLiteral("-C"), destDir});
        } else {
            process.setProgram(exe);
            process.setArguments({QStringLiteral("-xf"), archivePath,
                                  QStringLiteral("-C"), destDir});
        }
        process.start();
        if (!process.waitForStarted(15000))
            continue;
        if (process.waitForFinished(120000)
            && process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0)
            return true;
        tried.append(program);
    }
    if (errorOut) {
        *errorOut = QCoreApplication::translate(
                         "Core", "Could not extract %1 (tried %2); install tar with zstd support")
                         .arg(QFileInfo(archivePath).fileName(), tried.join(QLatin1String(", ")));
    }
    return false;
}

QString versionOfMsys2Package(const QString& fileName)
{
    // mingw-w64-i686-gcc-libs-16.2.0-3-any.pkg.tar.zst -> 16.2.0-3
    QString name = fileName;
    name.remove(QStringLiteral(".pkg.tar.zst"));
    const int dash = name.lastIndexOf(QLatin1Char('-')); // -any suffix
    if (dash >= 0)
        name.truncate(dash);
    const int dash2 = name.lastIndexOf(QLatin1Char('-')); // -release
    if (dash2 >= 0)
        name.truncate(dash2);
    return name;
}

// Fetch the newest MSYS2 package filename matching pkgPrefix from the listing.
QString latestMsys2Package(const QString& listing, const QString& pkgPrefix)
{
    const QRegularExpression re(QStringLiteral("href=\"(%1-[^\"]*\\.pkg\\.tar\\.zst)\"")
                                   .arg(QRegularExpression::escape(pkgPrefix)));
    QString best;
    for (auto it = re.globalMatch(listing); it.hasNext();) {
        const QString name = it.next().captured(1);
        if (best.isEmpty() || versionLess(versionOfMsys2Package(best), versionOfMsys2Package(name)))
            best = name;
    }
    return best;
}

// Fetch a DLL by downloading the small MSYS2 package that ships it and
// extracting the file next to the Steamless CLI.
bool downloadMsys2Dll(const QString& dllName, const QString& pkgPrefix, const QString& cliDir,
                      QString* errorOut)
{
    QNetworkAccessManager nam;
    QNetworkRequest listingRequest(
        QUrl(QStringLiteral("https://repo.msys2.org/mingw/mingw32/")));
    listingRequest.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel"));
    listingRequest.setTransferTimeout(30000);
    QNetworkReply* listingReply = nam.get(listingRequest);
    QEventLoop loop;
    QObject::connect(listingReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (listingReply->error() != QNetworkReply::NoError) {
        if (errorOut)
            *errorOut = listingReply->errorString();
        listingReply->deleteLater();
        return false;
    }
    const QString listing = QString::fromUtf8(listingReply->readAll());
    listingReply->deleteLater();

    const QString pkgName = latestMsys2Package(listing, pkgPrefix);
    if (pkgName.isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "No %1 package found on MSYS2")
                            .arg(pkgPrefix);
        return false;
    }

    const QString downloadDir = QFileInfo(cliDir).absolutePath() + QStringLiteral("/.download");
    QDir().mkpath(downloadDir);
    const QString pkgPath = downloadDir + QLatin1Char('/') + pkgName;
    if (!downloadToFile(QStringLiteral("https://repo.msys2.org/mingw/mingw32/") + pkgName,
                        pkgPath, errorOut))
        return false;

    QString extractError;
    const QString extractDir = downloadDir + QStringLiteral("/mingw");
    if (!extractTarZst(pkgPath, extractDir, &extractError)) {
        QFile::remove(pkgPath);
        if (errorOut)
            *errorOut = extractError;
        return false;
    }
    QFile::remove(pkgPath);

    const QString found = findFileInTree(extractDir, dllName);
    if (found.isEmpty() || !copyFileIfMissing(found, cliDir + QLatin1Char('/') + dllName)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "%1 not found in %2")
                            .arg(dllName, pkgName);
        return false;
    }
    return true;
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

bool SteamlessService::ensureTool(QString* errorOut)
{
    if (isAvailable())
        return true;

#if !defined(Q_OS_WIN)
    // Steamless is a Windows tool - on Linux it must run under Wine. Do not
    // download it before confirming Wine exists.
    if (QStandardPaths::findExecutable(QStringLiteral("wine")).isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate(
                "Core", "Steamless needs Wine on Linux (install wine first)");
        return false;
    }
#endif

    // Locate a downloadable release asset on GitHub.
    QString assetUrl;
    QString assetName;
    {
        QNetworkAccessManager nam;
        QNetworkRequest request(QUrl(
            QStringLiteral("https://api.github.com/repos/atom0s/Steamless/releases/latest")));
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel"));
        request.setTransferTimeout(20000);
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
                assetUrl = value.toObject().value(QStringLiteral("browser_download_url")).toString();
                assetName = name;
                break;
            }
        }
        if (assetUrl.isEmpty()) {
            for (const QJsonValue& value : assets) {
                const QString name = value.toObject().value(QStringLiteral("name")).toString();
                if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                    assetUrl =
                        value.toObject().value(QStringLiteral("browser_download_url")).toString();
                    assetName = name;
                    break;
                }
            }
        }
        reply->deleteLater();
    }

    if (assetUrl.isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "No Steamless release asset found");
        return false;
    }

    const QString root = toolRoot();
    const QString downloadDir = root + QStringLiteral("/.download");
    const QString zipPath = downloadDir + QLatin1Char('/') + assetName;

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
            *errorOut = QCoreApplication::translate("Core", "Steamless.CLI.exe not found in release");
        return false;
    }

#if !defined(Q_OS_WIN)
    QString runtimeError;
    if (!prepareLinuxRuntime(m_cachedCliPath, &runtimeError)) {
        if (errorOut)
            *errorOut = runtimeError;
        return false;
    }
#endif

    emit toolAvailableChanged();
    return true;
}

#if !defined(Q_OS_WIN)
bool SteamlessService::prepareLinuxRuntime(const QString& cliPath, QString* errorOut)
{
    const QString cliDir = QFileInfo(cliPath).absolutePath();

    // Mono resolves Steamless.API from the exe's directory (unlike Windows
    // .NET which tolerates the Plugins/ subfolder). Mirror the plugin DLLs
    // next to the CLI so the assembly reference resolves under Wine Mono.
    const QString pluginsDir = cliDir + QStringLiteral("/Plugins");
    if (QDir(pluginsDir).exists()) {
        QDirIterator it(pluginsDir, {QStringLiteral("*.dll")}, QDir::Files);
        while (it.hasNext()) {
            it.next();
            copyFileIfMissing(it.filePath(), cliDir + QLatin1Char('/') + it.fileName());
        }
    }

    // Wine Mono on Fedora-family distros needs the 32-bit MinGW runtime.
    const QStringList runtimeDlls = {QStringLiteral("libgcc_s_dw2-1.dll"),
                                     QStringLiteral("libwinpthread-1.dll")};
    const QStringList sysrootCandidates = {
        QStringLiteral("/usr/i686-w64-mingw32/sys-root/mingw/bin"),
        QStringLiteral("/usr/i686-w64-mingw32/bin"),
        QStringLiteral("/opt/mingw32/bin"),
    };

    for (const QString& dll : runtimeDlls) {
        if (QFileInfo::exists(cliDir + QLatin1Char('/') + dll))
            continue;

        bool copied = false;
        for (const QString& dir : sysrootCandidates) {
            if (copyFileIfMissing(dir + QLatin1Char('/') + dll,
                                  cliDir + QLatin1Char('/') + dll)) {
                copied = true;
                break;
            }
        }
        if (copied)
            continue;

        const QString pkg = dll == QLatin1String("libgcc_s_dw2-1.dll")
                                ? QStringLiteral("mingw-w64-i686-gcc-libs")
                                : QStringLiteral("mingw-w64-i686-libwinpthread");
        QString downloadError;
        if (!downloadMsys2Dll(dll, pkg, cliDir, &downloadError)) {
            if (errorOut) {
                *errorOut = QCoreApplication::translate(
                                 "Core",
                                 "Steamless needs the 32-bit MinGW runtime for Wine Mono "
                                 "(%1 missing); auto-download failed: %2")
                                 .arg(dll, downloadError);
            }
            return false;
        }
    }
    return true;
}
#endif

void SteamlessService::ensureSetup()
{
    // Already installed - nothing to do (this is the normal "once" path).
    if (isAvailable())
        return;

    QString error;
    if (!ensureTool(&error)) {
        if (m_notice) {
            m_notice(QCoreApplication::translate("Core", "Steamless setup skipped: %1")
                         .arg(error.isEmpty()
                                  ? QCoreApplication::translate("Core", "unknown error")
                                  : error));
        }
        return;
    }

    if (m_notice)
        m_notice(QCoreApplication::translate("Core", "Steamless is ready"));
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

    QStringList candidates;
    QDirIterator it(installPath, {QStringLiteral("*.exe")},
                    QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
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

    if (candidates.isEmpty()) {
        result.error = QCoreApplication::translate("Core", "No executables found");
        return result;
    }

    for (const QString& exe : std::as_const(candidates)) {
        if (hasSteamStub(exe)) {
            QString error;
            if (stripExecutable(exe, cliPath, &error)) {
                ++result.stripped;
                result.messages.append(QCoreApplication::translate("Core", "Steamless unpacked %1")
                                           .arg(QFileInfo(exe).fileName()));
            } else {
                result.messages.append(
                    QCoreApplication::translate("Core", "Steamless failed on %1: %2")
                        .arg(QFileInfo(exe).fileName(),
                             error.isEmpty()
                                 ? QCoreApplication::translate("Core", "unknown error")
                                 : error));
            }
            continue;
        }

        // No SteamStub wrapper. Distinguish "already applied earlier" (the
        // backup sibling exists) from "never protected / doesn't need it".
        if (QFileInfo::exists(exe + QStringLiteral(".steamstub.bak")))
            ++result.alreadyApplied;
        else
            ++result.notProtected;
    }
    return result;
}

void SteamlessService::processInstall(const QString& installPath, const QString& title)
{
    if (installPath.isEmpty())
        return;

    if (!isAvailable()) {
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
    }

    const QString cli = cliPath();

#if !defined(Q_OS_WIN)
    // Self-heal an already-downloaded tool (e.g. installed before this logic
    // existed): mirror plugins and provision the MinGW runtime DLLs so Wine
    // Mono can actually run the CLI.
    QString runtimeError;
    if (!prepareLinuxRuntime(cli, &runtimeError)) {
        if (m_notice) {
            m_notice(QCoreApplication::translate("Core", "Steamless is not available: %1")
                         .arg(runtimeError));
        }
        emit finished(installPath, 0);
        return;
    }
#endif

    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this,
            [this, watcher, installPath, title]() {
                const Result result = watcher->result();
                watcher->deleteLater();

                if (!result.error.isEmpty() && result.messages.isEmpty()) {
                    if (m_notice)
                        m_notice(QCoreApplication::translate("Core", "Steamless: %1").arg(result.error));
                } else {
                    for (const QString& message : result.messages) {
                        if (m_notice)
                            m_notice(message);
                    }
                    if (result.stripped > 0 && m_notice) {
                        m_notice(QCoreApplication::translate(
                                     "Core", "Steamless removed SteamStub from %1 file(s) in %2")
                                     .arg(result.stripped)
                                     .arg(title));
                    } else if (result.alreadyApplied > 0 && m_notice) {
                        m_notice(QCoreApplication::translate(
                                     "Core", "Steamless already applied to %1 (%2 file(s)) - nothing to do")
                                     .arg(title)
                                     .arg(result.alreadyApplied));
                    } else if (result.notProtected > 0 && m_notice) {
                        m_notice(QCoreApplication::translate(
                                     "Core", "Steamless not needed - %1 has no SteamStub DRM")
                                     .arg(title));
                    }
                }
                emit finished(installPath, result.stripped);
            });

    watcher->setFuture(QtConcurrent::run([installPath, cli]() {
        return processInstallSync(installPath, cli);
    }));
}

} // namespace arachnel::core
