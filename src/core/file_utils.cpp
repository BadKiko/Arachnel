#include "file_utils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace arachnel::core {

bool removePathRecursive(const QString& path, QString* errorOut)
{
    if (path.isEmpty())
        return true;

    QFileInfo info(path);
    if (!info.exists())
        return true;

    if (info.isFile()) {
        if (QFile::remove(path))
            return true;
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось удалить файл: %1").arg(path);
        return false;
    }

    QDir dir(path);
    if (!dir.removeRecursively()) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось удалить папку: %1").arg(path);
        return false;
    }
    return true;
}

bool copyPathRecursive(const QString& src, const QString& dst, QString* errorOut)
{
    QFileInfo srcInfo(src);
    if (!srcInfo.exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("Источник не найден: %1").arg(src);
        return false;
    }

    if (srcInfo.isFile()) {
        QDir().mkpath(QFileInfo(dst).absolutePath());
        if (QFile::exists(dst) && !QFile::remove(dst)) {
            if (errorOut)
                *errorOut = QStringLiteral("Не удалось заменить: %1").arg(dst);
            return false;
        }
        if (QFile::copy(src, dst))
            return true;
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось скопировать: %1").arg(src);
        return false;
    }

    QDir srcDir(src);
    QDir dstDir(dst);
    if (!dstDir.exists() && !QDir().mkpath(dst)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось создать папку: %1").arg(dst);
        return false;
    }

    const QStringList entries =
        srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        const QString srcPath = srcDir.absoluteFilePath(entry);
        const QString dstPath = dstDir.absoluteFilePath(entry);
        if (!copyPathRecursive(srcPath, dstPath, errorOut))
            return false;
    }
    return true;
}

bool movePathRecursive(const QString& src, const QString& dst, QString* errorOut)
{
    if (!QFileInfo(src).exists())
        return true;

    if (QDir().rename(src, dst))
        return true;

    if (!copyPathRecursive(src, dst, errorOut))
        return false;
    return removePathRecursive(src, errorOut);
}

QString relocatePathPrefix(const QString& path, const QString& oldRoot, const QString& newRoot)
{
    const QString normalizedPath = QDir::fromNativeSeparators(path);
    const QString normalizedOld = QDir::fromNativeSeparators(oldRoot);
    const QString normalizedNew = QDir::fromNativeSeparators(newRoot);

    if (normalizedPath.startsWith(normalizedOld, Qt::CaseInsensitive))
        return normalizedNew + normalizedPath.mid(normalizedOld.size());
    return path;
}

} // namespace arachnel::core
