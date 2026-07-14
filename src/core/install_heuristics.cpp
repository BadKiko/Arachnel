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
    return lower == QStringLiteral("unins000.exe") || lower == QStringLiteral("uninstall.exe")
           || lower.contains(QStringLiteral("setup")) || lower.contains(QStringLiteral("redist"))
           || lower.contains(QStringLiteral("vcredist")) || lower.contains(QStringLiteral("dxsetup"))
           || lower.contains(QStringLiteral("unitycrashhandler"));
}

QString findGameExecutableInTree(const QString& rootDir)
{
    QString bestPath;
    int bestDepth = 9999;
    qint64 bestSize = 0;

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

        if (depth < bestDepth || (depth == bestDepth && size > bestSize)) {
            bestDepth = depth;
            bestSize = size;
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
                                   90, QStringLiteral("Game executable already present"), false);
    }

    if (pathHasArchive(contentRoot)) {
        return makeInstallAnalysis(InstallKind::PortableArchive, QStringLiteral("portable-archive"),
                                   80, QStringLiteral("Archive files detected"), false);
    }

    return makeInstallAnalysis(InstallKind::PortableArchive, QStringLiteral("unknown"), 15,
                               QStringLiteral("Could not determine install method"), false);
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
                                   70, QStringLiteral("Archive files in torrent"), false);
    }

    return makeInstallAnalysis(InstallKind::PortableArchive, QStringLiteral("unknown"), 20,
                               QStringLiteral("No install hints in torrent file list"), false);
}

} // namespace arachnel::core
