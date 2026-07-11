#include "archive_installer.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

namespace freetp {

namespace {

QStringList archiveSuffixes()
{
    return {QStringLiteral(".zip"), QStringLiteral(".7z"), QStringLiteral(".rar")};
}

bool runProcess(QProcess& process, int timeoutMs, QString* errorOut)
{
    if (!process.waitForStarted(15000)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось запустить: %1").arg(process.program());
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        if (errorOut)
            *errorOut = QStringLiteral("Таймаут: %1").arg(process.program());
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorOut = stderrText.isEmpty()
                            ? QStringLiteral("%1 завершился с кодом %2")
                                  .arg(process.program())
                                  .arg(process.exitCode())
                            : stderrText;
        }
        return false;
    }
    return true;
}

QString find7zExecutable()
{
    const QStringList candidates = {
#if defined(Q_OS_WIN)
        QStringLiteral("7z"),
        QStringLiteral("7z.exe"),
        QStringLiteral("C:/Program Files/7-Zip/7z.exe"),
#else
        QStringLiteral("7z"),
        QStringLiteral("7za"),
#endif
    };
    for (const QString& candidate : candidates) {
        QProcess probe;
        probe.start(candidate, {QStringLiteral("--help")});
        if (probe.waitForFinished(3000) && probe.exitCode() == 0)
            return candidate;
    }
    return {};
}

bool extractWith7z(const QString& sevenZip, const QString& archive, const QString& destDir,
                   QString* errorOut)
{
    QDir().mkpath(destDir);
    QProcess process;
    process.setProgram(sevenZip);
    process.setArguments({QStringLiteral("x"), QStringLiteral("-y"),
                          QStringLiteral("-o") + destDir, archive});
    return runProcess(process, 3600000, errorOut);
}

bool extractZipWithTar(const QString& archive, const QString& destDir, QString* errorOut)
{
    QDir().mkpath(destDir);
    QProcess process;
    process.setProgram(QStringLiteral("tar"));
    process.setArguments({QStringLiteral("-xf"), archive, QStringLiteral("-C"), destDir});
    return runProcess(process, 3600000, errorOut);
}

bool isExcludedExecutable(const QString& fileName)
{
    static const QSet<QString> excluded = {
        QStringLiteral("unins000.exe"), QStringLiteral("uninstall.exe"),
        QStringLiteral("unitycrashhandler64.exe"), QStringLiteral("easyanticheat"),
        QStringLiteral("redist"), QStringLiteral("directx"), QStringLiteral("vcredist"),
        QStringLiteral("installscript"), QStringLiteral("setup.exe"),
    };
    const QString lower = fileName.toLower();
    for (const QString& token : excluded) {
        if (lower.contains(token))
            return true;
    }
    return false;
}

} // namespace

QString findDownloadContentRoot(const QString& downloadPath)
{
    QDir dir(downloadPath);
    if (!dir.exists())
        return {};

    if (!findGameExecutable(downloadPath).isEmpty())
        return downloadPath;

    QDirIterator archives(downloadPath, QDir::Files, QDirIterator::Subdirectories);
    while (archives.hasNext()) {
        const QString path = archives.next();
        for (const QString& suffix : archiveSuffixes()) {
            if (path.endsWith(suffix, Qt::CaseInsensitive))
                return downloadPath;
        }
    }

    const QStringList children = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (children.size() == 1)
        return dir.absoluteFilePath(children.constFirst());

    return downloadPath;
}

QString findGameExecutable(const QString& rootDir)
{
    QString bestPath;
    int bestDepth = 9999;
    qint64 bestSize = 0;

    QDirIterator it(rootDir, {QStringLiteral("*.exe")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (isExcludedExecutable(info.fileName()))
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

bool extractArchivesInDirectory(const QString& downloadDir, const QString& destDir,
                                QString* errorOut)
{
    QDir dir(downloadDir);
    if (!dir.exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("Папка загрузки не найдена");
        return false;
    }

    QStringList archives;
    QDirIterator it(downloadDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        for (const QString& suffix : archiveSuffixes()) {
            if (path.endsWith(suffix, Qt::CaseInsensitive)) {
                archives.append(path);
                break;
            }
        }
    }

    if (archives.isEmpty())
        return true;

    const QString sevenZip = find7zExecutable();
  for (const QString& archive : archives) {
        bool ok = false;
        if (!sevenZip.isEmpty())
            ok = extractWith7z(sevenZip, archive, destDir, errorOut);
        if (!ok && archive.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
            ok = extractZipWithTar(archive, destDir, errorOut);
        if (!ok) {
            if (errorOut && errorOut->isEmpty())
                *errorOut = QStringLiteral("Установите 7-Zip для распаковки %1").arg(archive);
            return false;
        }
    }

    return true;
}

QString installPortableFromDownload(const QString& downloadPath, const QString& targetPath,
                                    QString* errorOut)
{
    QDir targetDir(targetPath);
    if (targetDir.exists()) {
        if (!targetDir.removeRecursively()) {
            if (errorOut)
                *errorOut = QStringLiteral("Не удалось очистить папку установки");
            return {};
        }
    }
    if (!QDir().mkpath(targetPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось создать папку установки");
        return {};
    }

    QFileInfo downloadInfo(downloadPath);
    if (!downloadInfo.exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("Загрузка не найдена: %1").arg(downloadPath);
        return {};
    }

    QString workRoot = downloadPath;
    if (downloadInfo.isFile()) {
        workRoot = downloadInfo.absolutePath();
    }

    QString extractRoot = targetPath;
    QString extractError;
    if (!extractArchivesInDirectory(workRoot, extractRoot, &extractError)) {
        if (errorOut)
            *errorOut = extractError;
        return {};
    }

    QString exe = findGameExecutable(targetPath);
    if (exe.isEmpty())
        exe = findGameExecutable(workRoot);

    if (exe.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Исполняемый файл игры не найден после распаковки");
        return {};
    }

    return QFileInfo(exe).absolutePath();
}

} // namespace freetp
