#include "linux_fix_launch.h"

#include "archive_installer.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

namespace freetp {

namespace {

constexpr auto kFixMetaDir = ".arachnel";
constexpr auto kFixMetaFile = "fix-launch.json";

bool matchesFixFileName(const QString& fileName)
{
    static const QRegularExpression pattern(
        QStringLiteral("(?i)^(emp|custom)\\.dll$|^win.*\\.dll$|^(online|steam).*\\.(dll|ini|json)$"
                       "|^eos.*\\.dll$|^epicfix.*\\.dll$|^(winmm|dlllist)\\.txt$|^launch_data\\.of.*$"));
    return pattern.match(fileName).hasMatch();
}

QString fixMetaPath(const QString& installPath)
{
    return QDir(installPath).filePath(QStringLiteral("%1/%2").arg(kFixMetaDir, kFixMetaFile));
}

bool isSteamRunning()
{
    QProcess process;
    process.start(QStringLiteral("pidof"), {QStringLiteral("steam")});
    if (!process.waitForFinished(3000))
        return false;
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString steamHome()
{
    const QString home = QDir::homePath();
    const QStringList candidates = {
        home + QStringLiteral("/.steam/steam"),
        home + QStringLiteral("/.local/share/Steam"),
    };
    for (const QString& candidate : candidates) {
        if (QDir(candidate).exists())
            return candidate;
    }
    return home + QStringLiteral("/.steam/steam");
}

QString stubDllPath(const QString& pluginResourceRoot, bool is32Bit)
{
    const QString name =
        is32Bit ? QStringLiteral("ftpPath32.dll") : QStringLiteral("ftpPath64.dll");
    return QDir(pluginResourceRoot).filePath(QStringLiteral("linux/") + name);
}

QString bundledNewtonsoftPath(const QString& pluginResourceRoot)
{
    return QDir(pluginResourceRoot).filePath(QStringLiteral("linux/Newtonsoft.Json.dll"));
}

QString appendOverride(QString overrides, const QString& dllName, const QString& mode)
{
    const QString token = dllName + mode;
    if (overrides.contains(dllName + QStringLiteral("=")))
        return overrides;
    if (!overrides.isEmpty() && !overrides.endsWith(QLatin1Char(';')))
        overrides += QLatin1Char(';');
    return overrides + token;
}

QString readDllListOverrides(const QString& listPath)
{
    QFile file(listPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QString overrides;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive))
            continue;
        const QString dll = QFileInfo(line).fileName().section(QLatin1Char('.'), 0, 0).toLower();
        overrides = appendOverride(overrides, dll, QStringLiteral("=n"));
    }
    return overrides;
}

bool patchFreeTpIni(const QString& iniPath)
{
    QFile file(iniPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if (content.contains(QStringLiteral("[OnlineFix Linux]"), Qt::CaseInsensitive))
        return false;

    QRegularExpression realMain(
        QStringLiteral("\\[Main\\][^\\[]*?RealAppId\\s*=\\s*(\\d+)"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    QRegularExpression fakeMain(
        QStringLiteral("\\[Main\\][^\\[]*?FakeAppId\\s*=\\s*(\\d+)"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch realMatch = realMain.match(content);
    const QRegularExpressionMatch fakeMatch = fakeMain.match(content);
    if (!realMatch.hasMatch() || !fakeMatch.hasMatch())
        return false;

    const QString realAppId = realMatch.captured(1);
    const QString fakeAppId = fakeMatch.captured(1);

    content.replace(realMain, QStringLiteral("[Main]\nRealAppId=%1").arg(fakeAppId));
    content += QStringLiteral("\n[OnlineFix Linux]\nRealAppId=%1\n").arg(realAppId);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(content.toUtf8());
    return true;
}

bool applyPhotonNewtonsoftPatch(const QString& gameDir, const QString& pluginResourceRoot)
{
    const bool wantsPhoton = QFileInfo::exists(QDir(gameDir).filePath(QStringLiteral("Launcher.exe")))
                             && (QDir(gameDir).exists(QStringLiteral("onlinefix.json"))
                                 || !QDir(gameDir)
                                        .entryList({QStringLiteral("launch_data.of*")},
                                                   QDir::Files)
                                        .isEmpty());
    if (!wantsPhoton)
        return false;

    const QString target = QDir(gameDir).filePath(QStringLiteral("Newtonsoft.Json.dll"));
    if (QFileInfo::exists(target))
        return false;

    QDirIterator it(gameDir, {QStringLiteral("Newtonsoft.Json.dll")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString found = it.next();
        if (QFile::link(found, target))
            return true;
    }

    const QString bundled = bundledNewtonsoftPath(pluginResourceRoot);
    if (QFileInfo::exists(bundled))
        return QFile::copy(bundled, target);

    return false;
}

bool applyFakeSteamDllSwap(const QString& fixPath, const QString& pluginResourceRoot,
                           bool enable)
{
    if (fixPath.isEmpty())
        return false;

    QDir fixDir(fixPath);
    const QStringList dlls =
        fixDir.entryList({QStringLiteral("steamfix32.dll"), QStringLiteral("steamfix64.dll")},
                         QDir::Files);
    if (dlls.isEmpty())
        return false;

    bool changed = false;
    for (const QString& dllName : dlls) {
        const QString dllPath = fixDir.filePath(dllName);
        const QString backupPath = dllPath + QStringLiteral(".noarach");

        if (enable) {
            if (QFileInfo::exists(backupPath))
                continue;

            const bool is32 = dllName.contains(QStringLiteral("32"));
            const QString stub = stubDllPath(pluginResourceRoot, is32);
            if (!QFileInfo::exists(stub))
                return false;

            if (!QFile::rename(dllPath, backupPath))
                continue;
            if (!QFile::copy(stub, dllPath)) {
                QFile::rename(backupPath, dllPath);
                continue;
            }
            changed = true;
        } else {
            if (!QFileInfo::exists(backupPath))
                continue;
            QFile::remove(dllPath);
            QFile::rename(backupPath, dllPath);
            changed = true;
        }
    }
    return changed;
}

struct FixScanResult {
    QString wineDllOverrides;
    QString fixPath;
    QString fakeSteamId;
    QString realSteamId;
};

FixScanResult scanFixFiles(const QString& installPath)
{
    FixScanResult result;
    QDirIterator it(installPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString fileName = QFileInfo(path).fileName();
        if (!matchesFixFileName(fileName))
            continue;

        if (fileName.compare(QStringLiteral("winmm.txt"), Qt::CaseInsensitive) == 0
            || fileName.compare(QStringLiteral("dlllist.txt"), Qt::CaseInsensitive) == 0) {
            result.wineDllOverrides += readDllListOverrides(path);
            continue;
        }

        if (fileName.compare(QStringLiteral("onlinefix.ini"), Qt::CaseInsensitive) == 0
            || fileName.compare(QStringLiteral("steamfix.ini"), Qt::CaseInsensitive) == 0) {
            if (fileName.compare(QStringLiteral("steamfix.ini"), Qt::CaseInsensitive) == 0)
                patchFreeTpIni(path);

            QFile iniFile(path);
            if (iniFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString content = QString::fromUtf8(iniFile.readAll());
                QRegularExpression realRe(QStringLiteral("RealAppId\\s*=\\s*(\\d+)"),
                                        QRegularExpression::CaseInsensitiveOption);
                QRegularExpression fakeRe(QStringLiteral("FakeAppId\\s*=\\s*(\\d+)"),
                                        QRegularExpression::CaseInsensitiveOption);
                const QRegularExpressionMatch realMatch = realRe.match(content);
                const QRegularExpressionMatch fakeMatch = fakeRe.match(content);
                if (realMatch.hasMatch())
                    result.realSteamId = realMatch.captured(1);
                if (fakeMatch.hasMatch())
                    result.fakeSteamId = fakeMatch.captured(1);
            }
            continue;
        }

        if (QRegularExpression(QStringLiteral("(?i)^steamfix.*\\.dll$")).match(fileName).hasMatch())
            result.fixPath = QFileInfo(path).absolutePath();

        const QString dll = QFileInfo(path).completeBaseName().toLower();
        if (QRegularExpression(QStringLiteral("(?i)^win.*\\.dll$")).match(fileName).hasMatch())
            result.wineDllOverrides = appendOverride(result.wineDllOverrides, dll, QStringLiteral("=n,b"));
        else
            result.wineDllOverrides = appendOverride(result.wineDllOverrides, dll, QStringLiteral("=n"));
    }

    return result;
}

void saveFixMeta(const QString& installPath, const FixScanResult& scan, bool fakeSteamApplied)
{
    QDir metaDir(QDir(installPath).filePath(kFixMetaDir));
    metaDir.mkpath(QStringLiteral("."));

    QJsonObject obj;
    obj.insert(QStringLiteral("wineDllOverrides"), scan.wineDllOverrides);
    obj.insert(QStringLiteral("fixPath"), scan.fixPath);
    obj.insert(QStringLiteral("fakeSteamId"), scan.fakeSteamId);
    obj.insert(QStringLiteral("realSteamId"), scan.realSteamId);
    obj.insert(QStringLiteral("fakeSteamApplied"), fakeSteamApplied);

    QFile file(fixMetaPath(installPath));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

bool loadFixMeta(const QString& installPath, FixScanResult* scan, bool* fakeSteamApplied)
{
    QFile file(fixMetaPath(installPath));
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    if (scan) {
        scan->wineDllOverrides = obj.value(QStringLiteral("wineDllOverrides")).toString();
        scan->fixPath = obj.value(QStringLiteral("fixPath")).toString();
        scan->fakeSteamId = obj.value(QStringLiteral("fakeSteamId")).toString();
        scan->realSteamId = obj.value(QStringLiteral("realSteamId")).toString();
    }
    if (fakeSteamApplied)
        *fakeSteamApplied = obj.value(QStringLiteral("fakeSteamApplied")).toBool();
    return true;
}

void appendSteamOverlayEnv(arachnel::core::LaunchInfo* info, const QString& fakeSteamId)
{
    const QString home = QDir::homePath();
    const QString steamRoot = steamHome();
    const QString preload =
        QStringLiteral(":%1/ubuntu12_32/gameoverlayrenderer.so:%2/ubuntu12_64/gameoverlayrenderer.so")
            .arg(steamRoot, steamRoot);

    info->environmentExtras.insert(QStringLiteral("LD_PRELOAD"), preload);
    info->environmentExtras.insert(QStringLiteral("ENABLE_VK_LAYER_VALVE_steam_overlay_1"),
                                   QStringLiteral("true"));
    info->environmentExtras.insert(QStringLiteral("SteamOverlayGameId"),
                                   fakeSteamId.isEmpty() ? QStringLiteral("480") : fakeSteamId);
}

} // namespace

bool linuxFixLaunchEnabled()
{
#if defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

QString findBestGameExecutable(const QString& rootDir)
{
    {
        QDirIterator it(rootDir, {QStringLiteral("*.exe")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (QFileInfo(path).fileName().compare(QStringLiteral("eosauthlauncher.exe"),
                                                     Qt::CaseInsensitive) == 0)
                return path;
        }
    }

    {
        QDirIterator it(rootDir, {QStringLiteral("*.exe")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (QFileInfo(path).fileName().compare(QStringLiteral("launcher.exe"),
                                                   Qt::CaseInsensitive) != 0)
                continue;
            if (QFileInfo::exists(QDir(QFileInfo(path).absolutePath())
                                      .filePath(QStringLiteral("onlinefix.json"))))
                return path;
        }
    }

    return findGameExecutable(rootDir);
}

void prepareLinuxFixInstall(const QString& installPath, const QString& pluginResourceRoot)
{
#if !defined(Q_OS_LINUX)
    (void)installPath;
    (void)pluginResourceRoot;
    return;
#else
    if (installPath.isEmpty() || !QDir(installPath).exists())
        return;

    applyPhotonNewtonsoftPatch(installPath, pluginResourceRoot);

    QDirIterator patchIt(installPath, {QStringLiteral("steamfix.ini")}, QDir::Files,
                         QDirIterator::Subdirectories);
    while (patchIt.hasNext())
        patchFreeTpIni(patchIt.next());

    const FixScanResult scan = scanFixFiles(installPath);
    saveFixMeta(installPath, scan, false);
#endif
}

void applyLinuxFixLaunchInfo(const QString& installPath, const QString& pluginResourceRoot,
                             const LinuxFixLaunchOptions& options,
                             arachnel::core::LaunchInfo* info)
{
#if !defined(Q_OS_LINUX)
    (void)installPath;
    (void)pluginResourceRoot;
    (void)options;
    (void)info;
    return;
#else
    if (!info || installPath.isEmpty())
        return;

    FixScanResult scan;
    bool fakeSteamApplied = false;
    if (!loadFixMeta(installPath, &scan, &fakeSteamApplied) || scan.wineDllOverrides.isEmpty())
        scan = scanFixFiles(installPath);

    applyPhotonNewtonsoftPatch(QFileInfo(info->executable).absolutePath(), pluginResourceRoot);

    const bool steamRunning = isSteamRunning();
    if (options.preferSteamOverlay && steamRunning)
        appendSteamOverlayEnv(info, scan.fakeSteamId);

    if (!steamRunning && options.autoFakeSteamWhenSteamDown && !scan.fixPath.isEmpty()) {
        if (!fakeSteamApplied) {
            if (applyFakeSteamDllSwap(scan.fixPath, pluginResourceRoot, true))
                fakeSteamApplied = true;
        }
    } else if (steamRunning && fakeSteamApplied) {
        if (applyFakeSteamDllSwap(scan.fixPath, pluginResourceRoot, false))
            fakeSteamApplied = false;
    }

    info->wineDllOverrides = scan.wineDllOverrides;
    saveFixMeta(installPath, scan, fakeSteamApplied);
#endif
}

} // namespace freetp
