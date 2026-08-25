#include "install_heuristics.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace arachnel::core {

namespace {

QStringList archiveSuffixes()
{
    return {QStringLiteral(".zip"), QStringLiteral(".7z"), QStringLiteral(".rar")};
}

bool pathHasArchive(const QString& rootDir)
{
    QDirIterator it(rootDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        for (const QString& suffix : archiveSuffixes()) {
            if (path.endsWith(suffix, Qt::CaseInsensitive))
                return true;
        }
    }
    return false;
}

} // namespace

bool isExcludedGameExecutable(const QString& fileName)
{
    const QString lower = fileName.toLower();
    // Shell stubs / scripts / helpers
    if (lower == QStringLiteral("cmd.exe") || lower == QStringLiteral("powershell.exe")
        || lower == QStringLiteral("cmd_stub.exe") || lower == QStringLiteral("conhost.exe"))
        return true;

    // Uninstaller, setup, redistributables, crash handlers
    if (lower == QStringLiteral("unins000.exe") || lower == QStringLiteral("uninstall.exe")
        || lower.contains(QStringLiteral("unins"))
        || lower.contains(QStringLiteral("setup")) || lower.contains(QStringLiteral("redist"))
        || lower.contains(QStringLiteral("vcredist")) || lower.contains(QStringLiteral("dxsetup"))
        || lower.contains(QStringLiteral("dxwebsetup")) || lower.contains(QStringLiteral("oalinst"))
        || lower.contains(QStringLiteral("unitycrashhandler"))
        || lower.contains(QStringLiteral("crashpad")) || lower.contains(QStringLiteral("crashreport"))
        || lower.contains(QStringLiteral("crashhandler")) || lower.contains(QStringLiteral("prereq")))
        return true;

    // Common tools, media utilities, streaming helpers, archives, downloaders
    if (lower == QStringLiteral("ffmpeg.exe") || lower == QStringLiteral("ffprobe.exe")
        || lower == QStringLiteral("ffplay.exe") || lower == QStringLiteral("yt-dlp.exe")
        || lower == QStringLiteral("youtube-dl.exe") || lower == QStringLiteral("texconv.exe")
        || lower.startsWith(QStringLiteral("easyhook")) || lower.contains(QStringLiteral("easyhook"))
        || lower == QStringLiteral("7z.exe") || lower == QStringLiteral("7za.exe")
        || lower == QStringLiteral("7zr.exe") || lower == QStringLiteral("rar.exe")
        || lower == QStringLiteral("unrar.exe") || lower == QStringLiteral("curl.exe")
        || lower == QStringLiteral("wget.exe") || lower == QStringLiteral("quickhash.exe")
        || lower == QStringLiteral("openvr_api.exe") || lower == QStringLiteral("steam.exe")
        || lower.startsWith(QStringLiteral("steamless")) || lower.startsWith(QStringLiteral("steamstub"))
        || lower == QStringLiteral("gdb.exe") || lower == QStringLiteral("gdbserver.exe")
        || lower == QStringLiteral("lldb.exe") || lower == QStringLiteral("elevate.exe")
        || lower == QStringLiteral("launcher_helper.exe") || lower == QStringLiteral("register.exe")
        || lower == QStringLiteral("dotnet.exe") || lower.startsWith(QStringLiteral("windowsdesktop-runtime")))
        return true;

    // Tools Steam ships next to the game (Craft The World Editor.exe, dedicated servers, …).
    QString stem = lower;
    if (stem.endsWith(QStringLiteral(".exe")))
        stem.chop(4);
    if (stem == QStringLiteral("editor") || stem.endsWith(QStringLiteral("editor"))
        || stem == QStringLiteral("dedicated") || stem.contains(QStringLiteral("dedicatedserver"))
        || stem == QStringLiteral("server") || stem.endsWith(QStringLiteral("server"))
        || stem == QStringLiteral("unrealversionselector") || stem.endsWith(QStringLiteral("config"))
        || stem.endsWith(QStringLiteral("configuration")) || stem.endsWith(QStringLiteral("settings")))
        return true;

    return false;
}

QString findGameExecutableInTree(const QString& rootDir, const QString& gameTitle)
{
    QString bestPath;
    qint64 bestScore = -1;

    QString cleanTitleCompact;
    if (!gameTitle.isEmpty()) {
        for (const QChar& c : gameTitle) {
            if (c.isLetterOrNumber())
                cleanTitleCompact.append(c.toLower());
        }
    }

    QDirIterator it(rootDir, {QStringLiteral("*.exe")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (isExcludedGameExecutable(info.fileName()))
            continue;

        const QString relative = QDir(rootDir).relativeFilePath(path);
        const int depth = relative.count(QLatin1Char('/')) + relative.count(QLatin1Char('\\'));
        const qint64 size = info.size();
        const QString stem = info.completeBaseName().toLower();

        // Base score from size (capped to prevent oversized utility tools from overpowering)
        qint64 score = qMin(size, 200LL * 1024 * 1024);

        // Depth penalty (prefer executables located close to root directory)
        score -= static_cast<qint64>(depth) * (20LL * 1024 * 1024);

        // Game title match bonus
        if (!cleanTitleCompact.isEmpty()) {
            QString cleanStem;
            for (const QChar& c : stem) {
                if (c.isLetterOrNumber())
                    cleanStem.append(c);
            }
            if (!cleanStem.isEmpty()) {
                if (cleanStem == cleanTitleCompact) {
                    score += 600LL * 1024 * 1024;
                } else if (cleanTitleCompact.startsWith(cleanStem) && cleanStem.length() >= 4) {
                    score += 500LL * 1024 * 1024;
                } else if (cleanStem.startsWith(cleanTitleCompact) && cleanTitleCompact.length() >= 4) {
                    score += 500LL * 1024 * 1024;
                } else if (cleanTitleCompact.contains(cleanStem) && cleanStem.length() >= 4) {
                    score += 350LL * 1024 * 1024;
                }
            }
        }

        // Unreal Engine / Unity / Steam API heuristics
        if (stem.contains(QStringLiteral("shipping")) || stem.contains(QStringLiteral("-win64-shipping")))
            score += 400LL * 1024 * 1024;
        if (QFileInfo::exists(info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral("_Data")))
            score += 400LL * 1024 * 1024;
        if (QFileInfo::exists(info.absolutePath() + QStringLiteral("/steam_api64.dll"))
            || QFileInfo::exists(info.absolutePath() + QStringLiteral("/steam_api.dll")))
            score += 200LL * 1024 * 1024;
        if (QFileInfo::exists(info.absolutePath() + QStringLiteral("/SteamFix64.dll"))
            || QFileInfo::exists(info.absolutePath() + QStringLiteral("/winmm.dll"))
            || QFileInfo::exists(info.absolutePath() + QStringLiteral("/OnlineFix64.dll")))
            score += 300LL * 1024 * 1024;
        const bool titleMentionsVr = cleanTitleCompact.contains(QStringLiteral("vr"));
        if (titleMentionsVr) {
            if (stem.endsWith(QStringLiteral("_vr")) || stem.endsWith(QStringLiteral("-vr"))
                || stem == QStringLiteral("vr") || stem.contains(QStringLiteral("_vr_"))
                || stem.endsWith(QStringLiteral("vr"))) {
                score += 250LL * 1024 * 1024;
            }
        }

        if (stem.contains(QStringLiteral("launcher")) || stem.contains(QStringLiteral("bootstrap"))
            || stem.contains(QStringLiteral("patcher")))
            score -= 50LL * 1024 * 1024;

        if (score > bestScore) {
            bestScore = score;
            bestPath = path;
        }
    }

    return bestPath;
}

QString findSetupExecutableInTree(const QString& rootDir)
{
    QString bestPath;
    int bestDepth = 9999;

    QDirIterator it(rootDir, {QStringLiteral("*.exe")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        const QString lower = info.fileName().toLower();
        if (lower == QStringLiteral("unins000.exe")
            || lower == QStringLiteral("uninstall.exe"))
            continue;

        const bool isSetup = lower == QStringLiteral("setup.exe")
                             || lower.contains(QStringLiteral("setup"));
        if (!isSetup)
            continue;

        const QString relative = QDir(rootDir).relativeFilePath(path);
        const int depth = relative.count(QLatin1Char('/')) + relative.count(QLatin1Char('\\'));
        if (depth < bestDepth || (depth == bestDepth && lower == QStringLiteral("setup.exe"))) {
            bestDepth = depth;
            bestPath = path;
        }
    }

    return bestPath;
}

bool isInnoSetupExecutable(const QString& setupPath)
{
    QFile file(setupPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray header = file.read(1024 * 1024);
    return header.contains("Inno Setup");
}

QString findDownloadContentRoot(const QString& downloadPath)
{
    QDir dir(downloadPath);
    if (!dir.exists())
        return {};

    if (!findGameExecutableInTree(downloadPath).isEmpty())
        return downloadPath;

    if (pathHasArchive(downloadPath))
        return downloadPath;

    const QStringList children = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (children.size() == 1)
        return dir.absoluteFilePath(children.constFirst());

    return downloadPath;
}

InstallAnalysis analyzeDownloadPath(const QString& downloadPath)
{
    const QString contentRoot = findDownloadContentRoot(downloadPath);
    if (contentRoot.isEmpty() || !QDir(contentRoot).exists()) {
        return makeInstallAnalysis(InstallKind::PortableArchive, QStringLiteral("unknown"), 0,
                                   QStringLiteral("Download path not found"), false);
    }

    const QString setupExe = findSetupExecutableInTree(contentRoot);
    if (!setupExe.isEmpty() && isInnoSetupExecutable(setupExe)) {
        return makeInstallAnalysis(InstallKind::Installer, QStringLiteral("inno-setup"), 95,
                                   QStringLiteral("Inno Setup installer detected"), false);
    }

    if (!setupExe.isEmpty()) {
        return makeInstallAnalysis(InstallKind::Installer, QStringLiteral("setup-exe"), 55,
                                   QStringLiteral("Setup executable found"), false);
    }

    if (!findGameExecutableInTree(contentRoot).isEmpty()) {
        return makeInstallAnalysis(InstallKind::PortableArchive, QStringLiteral("portable-ready"),
                                   90, QStringLiteral("Game executable already present"), true);
    }

    if (pathHasArchive(contentRoot)) {
        return makeInstallAnalysis(InstallKind::PortableArchive, QStringLiteral("portable-archive"),
                                   80, QStringLiteral("Archive files detected"), true);
    }

    return makeInstallAnalysis(InstallKind::PortableArchive, QStringLiteral("unknown"), 15,
                               QStringLiteral("Could not determine install method"), true);
}

InstallAnalysis analyzeTorrentFileNames(const QStringList& fileNames)
{
    bool hasSetup = false;
    bool hasArchive = false;
    bool hasFtpChunk = false;

    for (const QString& path : fileNames) {
        const QString fileName = QFileInfo(path).fileName().toLower();
        if (fileName == QStringLiteral("setup.exe") || fileName.contains(QStringLiteral("setup")))
            hasSetup = true;
        if (fileName.endsWith(QStringLiteral(".ftp")))
            hasFtpChunk = true;
        for (const QString& suffix : archiveSuffixes()) {
            if (fileName.endsWith(suffix))
                hasArchive = true;
        }
    }

    if (hasFtpChunk) {
        return makeInstallAnalysis(InstallKind::Installer, QStringLiteral("chunked-installer"), 85,
                                   QStringLiteral("Multi-part installer torrent"), false);
    }

    if (hasSetup) {
        return makeInstallAnalysis(InstallKind::Installer, QStringLiteral("setup-exe"), 75,
                                   QStringLiteral("Setup executable in torrent"), false);
    }

    if (hasArchive) {
        return makeInstallAnalysis(InstallKind::PortableArchive, QStringLiteral("portable-archive"),
                                   70, QStringLiteral("Archive files in torrent"), true);
    }

    return makeInstallAnalysis(InstallKind::PortableArchive, QStringLiteral("unknown"), 20,
                               QStringLiteral("No install hints in torrent file list"), true);
}

} // namespace arachnel::core
