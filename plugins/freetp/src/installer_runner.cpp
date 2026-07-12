#include "installer_runner.h"

#include "archive_installer.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QThread>

namespace freetp {

namespace {

constexpr int kInnoInstallTimeoutMs = 3600000;

QStringList desktopRoots()
{
    QStringList roots;
    const QString userDesktop =
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (!userDesktop.isEmpty())
        roots.append(userDesktop);

#if defined(Q_OS_WIN)
    const QByteArray publicProfile = qgetenv("PUBLIC");
    if (!publicProfile.isEmpty())
        roots.append(QString::fromLocal8Bit(publicProfile) + QStringLiteral("/Desktop"));
#endif

    return roots;
}

QByteArray utf16LeBytes(const QString& text)
{
    QByteArray bytes;
    bytes.reserve(text.size() * 2);
    for (const QChar ch : text) {
        const ushort code = ch.unicode();
        bytes.append(char(code & 0xFF));
        bytes.append(char((code >> 8) & 0xFF));
    }
    return bytes;
}

bool shortcutPointsToInstallPath(const QString& shortcutPath, const QString& installPath,
                                 const QString& gameExecutable)
{
    QFile file(shortcutPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray data = file.readAll();
    QStringList needles;
    const auto addNeedle = [&needles](const QString& value) {
        if (value.isEmpty())
            return;
        needles.append(value);
        needles.append(QDir::toNativeSeparators(value));
        needles.append(QDir::fromNativeSeparators(value));
    };

    addNeedle(installPath);
    addNeedle(gameExecutable);

    for (const QString& needle : needles) {
        if (data.contains(needle.toUtf8()))
            return true;
        if (data.contains(utf16LeBytes(needle)))
            return true;
        if (data.contains(utf16LeBytes(QDir::toNativeSeparators(needle))))
            return true;
    }

    return false;
}

QString innoPathArg(const QString& flag, const QString& path)
{
    return flag + QDir::toNativeSeparators(path);
}

QString tailOfInstallLog(const QString& logPath, int maxLines = 8)
{
    QFile file(logPath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QString text = QString::fromLocal8Bit(file.readAll());
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.isEmpty())
        return {};

    const int start = qMax(0, lines.size() - maxLines);
    return lines.mid(start).join(QLatin1Char('\n'));
}

void clearTargetDirectory(const QString& targetPath, QString* errorOut)
{
    QDir targetDir(targetPath);
    if (!targetDir.exists())
        return;

    if (targetDir.removeRecursively())
        return;

    const QString backupPath =
        targetPath + QStringLiteral(".old-")
        + QString::number(QDateTime::currentMSecsSinceEpoch());
    if (QDir().rename(targetPath, backupPath))
        return;

    if (errorOut)
        *errorOut = QStringLiteral("Не удалось очистить папку установки");
}

QString waitForGameExecutable(const QString& targetPath)
{
    for (int attempt = 0; attempt < 20; ++attempt) {
        const QString exe = findGameExecutable(targetPath);
        if (!exe.isEmpty())
            return exe;
        QThread::msleep(500);
    }
    return {};
}

} // namespace

QString findSetupExecutable(const QString& rootDir)
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

QString installInnoSetup(const QString& setupPath, const QString& targetPath, QString* errorOut)
{
    if (!QFileInfo::exists(setupPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Установщик не найден");
        return {};
    }

    clearTargetDirectory(targetPath, errorOut);
    if (errorOut && !errorOut->isEmpty())
        return {};

    if (!QDir().mkpath(targetPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось создать папку установки");
        return {};
    }

    const QString logPath = QDir(targetPath).absoluteFilePath(QStringLiteral("install.log"));
    const QStringList args = {
        QStringLiteral("/VERYSILENT"),
        QStringLiteral("/SUPPRESSMSGBOXES"),
        QStringLiteral("/NORESTART"),
        QStringLiteral("/SP-"),
        QStringLiteral("/MERGETASKS=!desktopicon"),
        innoPathArg(QStringLiteral("/DIR="), targetPath),
        innoPathArg(QStringLiteral("/LOG="), logPath),
    };

    if (!runInstallProcess(setupPath, args, kInnoInstallTimeoutMs, errorOut,
                           QFileInfo(setupPath).absolutePath())) {
        if (errorOut) {
            const QString logTail = tailOfInstallLog(logPath);
            if (!logTail.isEmpty())
                *errorOut += QStringLiteral("\n") + logTail;
        }
        return {};
    }

    const QString exe = waitForGameExecutable(targetPath);
    if (exe.isEmpty()) {
        if (errorOut) {
            const QString logTail = tailOfInstallLog(logPath);
            *errorOut = QStringLiteral("Игра не найдена после установки Inno Setup");
            if (!logTail.isEmpty())
                *errorOut += QStringLiteral("\n") + logTail;
        }
        return {};
    }

    return QFileInfo(exe).absolutePath();
}

QString installInnoOverlay(const QString& setupPath, const QString& targetPath, QString* errorOut)
{
    if (!QFileInfo::exists(setupPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Установщик не найден");
        return {};
    }

    if (!QDir().mkpath(targetPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось создать папку игры");
        return {};
    }

    const QString logPath = QDir(targetPath).absoluteFilePath(QStringLiteral("addon-install.log"));
    const QStringList args = {
        QStringLiteral("/VERYSILENT"),
        QStringLiteral("/SUPPRESSMSGBOXES"),
        QStringLiteral("/NORESTART"),
        QStringLiteral("/SP-"),
        QStringLiteral("/MERGETASKS=!desktopicon"),
        innoPathArg(QStringLiteral("/DIR="), targetPath),
        innoPathArg(QStringLiteral("/LOG="), logPath),
    };

    if (!runInstallProcess(setupPath, args, kInnoInstallTimeoutMs, errorOut,
                           QFileInfo(setupPath).absolutePath())) {
        if (errorOut) {
            const QString logTail = tailOfInstallLog(logPath);
            if (!logTail.isEmpty())
                *errorOut += QStringLiteral("\n") + logTail;
        }
        return {};
    }

    return targetPath;
}

void cleanupInnoSideEffects(const QString& installPath)
{
    if (installPath.isEmpty())
        return;

    const QString gameExecutable = findGameExecutable(installPath);

    QDirIterator urls(installPath, {QStringLiteral("*.url")}, QDir::Files);
    while (urls.hasNext())
        QFile::remove(urls.next());

    for (const QString& desktop : desktopRoots()) {
        QDirIterator shortcuts(desktop, {QStringLiteral("*.lnk")}, QDir::Files);
        while (shortcuts.hasNext()) {
            const QString shortcutPath = shortcuts.next();
            if (shortcutPointsToInstallPath(shortcutPath, installPath, gameExecutable))
                QFile::remove(shortcutPath);
        }
    }
}

} // namespace freetp
