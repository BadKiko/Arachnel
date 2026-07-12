#include "archive_installer.h"

#include "file_utils.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

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

#if defined(Q_OS_WIN)

QString quoteWindowsArg(const QString& text)
{
    if (text.isEmpty())
        return QStringLiteral("\"\"");

    if (!text.contains(QLatin1Char(' ')) && !text.contains(QLatin1Char('\t'))
        && !text.contains(QLatin1Char('"')))
        return text;

    QString escaped;
    escaped.reserve(text.size() + 4);
    escaped += QLatin1Char('"');
    int backslashes = 0;
    for (const QChar ch : text) {
        if (ch == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        if (ch == QLatin1Char('"')) {
            escaped += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            backslashes = 0;
            escaped += QLatin1Char('"');
            continue;
        }
        if (backslashes > 0) {
            escaped += QString(backslashes, QLatin1Char('\\'));
            backslashes = 0;
        }
        escaped += ch;
    }
    if (backslashes > 0)
        escaped += QString(backslashes * 2, QLatin1Char('\\'));
    escaped += QLatin1Char('"');
    return escaped;
}

QString formatWindowsParameters(const QStringList& arguments)
{
    QStringList parts;
    for (const QString& argument : arguments)
        parts << quoteWindowsArg(argument);
    return parts.join(QLatin1Char(' '));
}

QString describeWin32Error(DWORD error)
{
    if (error == ERROR_CANCELLED)
        return QStringLiteral("запуск отменён (UAC)");
    if (error == ERROR_ELEVATION_REQUIRED)
        return QStringLiteral("требуются права администратора");
    return QStringLiteral("Win32 %1").arg(error);
}

bool runWindowsProcess(const QString& program, const QStringList& arguments, int timeoutMs,
                       QString* errorOut, const QString& workingDirectory)
{
    if (!QFileInfo::exists(program)) {
        if (errorOut)
            *errorOut = QStringLiteral("Файл не найден: %1").arg(program);
        return false;
    }

    const QString nativeProgram = QDir::toNativeSeparators(program);
    const QString parameters = formatWindowsParameters(arguments);
    const QString nativeWorkDir =
        workingDirectory.isEmpty() ? QString() : QDir::toNativeSeparators(workingDirectory);

    SHELLEXECUTEINFOW executeInfo{};
    executeInfo.cbSize = sizeof(executeInfo);
    executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOZONECHECKS;
    executeInfo.lpVerb = L"open";
    executeInfo.lpFile = reinterpret_cast<LPCWSTR>(nativeProgram.utf16());
    executeInfo.lpParameters = parameters.isEmpty()
                                   ? nullptr
                                   : reinterpret_cast<LPCWSTR>(parameters.utf16());
    executeInfo.lpDirectory = nativeWorkDir.isEmpty()
                                  ? nullptr
                                  : reinterpret_cast<LPCWSTR>(nativeWorkDir.utf16());
    executeInfo.nShow = SW_HIDE;

    if (!ShellExecuteExW(&executeInfo)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Не удалось запустить %1: %2")
                            .arg(nativeProgram, describeWin32Error(GetLastError()));
        }
        return false;
    }

    if (!executeInfo.hProcess) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось отследить процесс установки");
        return false;
    }

    const DWORD waitResult =
        WaitForSingleObject(executeInfo.hProcess, static_cast<DWORD>(timeoutMs));
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(executeInfo.hProcess, 1);
        CloseHandle(executeInfo.hProcess);
        if (errorOut)
            *errorOut = QStringLiteral("Таймаут: %1").arg(nativeProgram);
        return false;
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(executeInfo.hProcess, &exitCode);
    CloseHandle(executeInfo.hProcess);

    if (exitCode != 0) {
        if (errorOut)
            *errorOut = QStringLiteral("%1 завершился с кодом %2").arg(nativeProgram).arg(exitCode);
        return false;
    }

    return true;
}

#endif

} // namespace

bool runInstallProcess(const QString& program, const QStringList& arguments, int timeoutMs,
                       QString* errorOut, const QString& workingDirectory)
{
#if defined(Q_OS_WIN)
    return runWindowsProcess(program, arguments, timeoutMs, errorOut, workingDirectory);
#else
    const QString nativeProgram = QDir::toNativeSeparators(program);
    const QString nativeWorkDir =
        workingDirectory.isEmpty() ? QString() : QDir::toNativeSeparators(workingDirectory);

    QProcess process;
    process.setProgram(nativeProgram);
    process.setArguments(arguments);
    if (!nativeWorkDir.isEmpty())
        process.setWorkingDirectory(nativeWorkDir);

    return runProcess(process, timeoutMs, errorOut);
#endif
}

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

namespace {

QString portableSourceRoot(const QString& downloadPath)
{
    if (findGameExecutable(downloadPath).isEmpty())
        return findDownloadContentRoot(downloadPath);
    return downloadPath;
}

bool relocatePortableContent(const QString& sourceRoot, const QString& targetPath,
                             QString* errorOut)
{
    QDir source(sourceRoot);
    QDir target(targetPath);
    if (!source.exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("Исходная папка не найдена: %1").arg(sourceRoot);
        return false;
    }

    const QString canonicalSource = source.canonicalPath();
    const QString canonicalTarget = target.canonicalPath();
    if (canonicalSource == canonicalTarget)
        return true;

    if (canonicalTarget.startsWith(canonicalSource + QLatin1Char('/'), Qt::CaseInsensitive)) {
        if (errorOut)
            *errorOut = QStringLiteral("Некорректный путь установки");
        return false;
    }

    if (!target.exists() && !QDir().mkpath(targetPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось создать папку установки");
        return false;
    }

    const QStringList entries = source.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Нет файлов для установки");
        return false;
    }

    for (const QString& entry : entries) {
        const QString srcPath = source.absoluteFilePath(entry);
        const QString dstPath = target.absoluteFilePath(entry);
        if (!arachnel::core::movePathRecursive(srcPath, dstPath, errorOut))
            return false;
    }

    return true;
}

} // namespace

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

    const QString workRoot =
        downloadInfo.isFile() ? downloadInfo.absolutePath() : downloadPath;
    const QString sourceRoot = portableSourceRoot(workRoot);

    QString extractError;
    if (!extractArchivesInDirectory(workRoot, targetPath, &extractError)) {
        if (errorOut)
            *errorOut = extractError;
        return {};
    }

    QString exe = findGameExecutable(targetPath);
    if (!exe.isEmpty())
        return QFileInfo(exe).absolutePath();

    exe = findGameExecutable(sourceRoot);
    if (exe.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Исполняемый файл игры не найден после распаковки");
        return {};
    }

    const QString gameRoot = QFileInfo(exe).absolutePath();
    if (!relocatePortableContent(gameRoot, targetPath, errorOut))
        return {};

    exe = findGameExecutable(targetPath);
    if (exe.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Игра установлена, но исполняемый файл не найден");
        return {};
    }

    return QFileInfo(exe).absolutePath();
}

bool installAddonOverlay(const QString& downloadPath, const QString& gameInstallPath,
                         QString* errorOut)
{
    const QFileInfo artifact(downloadPath);
    if (!artifact.exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("Файлы дополнения не найдены");
        return false;
    }

    if (!QDir().mkpath(gameInstallPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось открыть папку игры");
        return false;
    }

    if (artifact.isFile()) {
        for (const QString& suffix : archiveSuffixes()) {
            if (!downloadPath.endsWith(suffix, Qt::CaseInsensitive))
                continue;
            const QString sevenZip = find7zExecutable();
            if (!sevenZip.isEmpty())
                return extractWith7z(sevenZip, downloadPath, gameInstallPath, errorOut);
            if (suffix == QStringLiteral(".zip"))
                return extractZipWithTar(downloadPath, gameInstallPath, errorOut);
            if (errorOut)
                *errorOut = QStringLiteral("Установите 7-Zip для распаковки %1").arg(downloadPath);
            return false;
        }
        if (errorOut)
            *errorOut = QStringLiteral("Неподдерживаемый файл дополнения");
        return false;
    }

    return extractArchivesInDirectory(downloadPath, gameInstallPath, errorOut);
}

} // namespace freetp
