#include "file_utils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>

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
            *errorOut = QCoreApplication::translate("Core", "Failed to delete file: %1").arg(path);
        return false;
    }

    QDir dir(path);
    if (!dir.removeRecursively()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Failed to delete folder: %1").arg(path);
        return false;
    }
    return true;
}

bool copyPathRecursive(const QString& src, const QString& dst, QString* errorOut)
{
    QFileInfo srcInfo(src);
    if (!srcInfo.exists()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Source not found: %1").arg(src);
        return false;
    }

    if (srcInfo.isFile()) {
        QDir().mkpath(QFileInfo(dst).absolutePath());
        if (QFile::exists(dst) && !QFile::remove(dst)) {
            if (errorOut)
                *errorOut = QCoreApplication::translate("Core", "Failed to replace: %1").arg(dst);
            return false;
        }
        if (QFile::copy(src, dst))
            return true;
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Failed to copy: %1").arg(src);
        return false;
    }

    QDir srcDir(src);
    QDir dstDir(dst);
    if (!dstDir.exists() && !QDir().mkpath(dst)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Failed to create folder: %1").arg(dst);
        return false;
    }

    const QStringList entries =
        srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    const QString cleanSrc = QDir::cleanPath(src);
    const QString cleanDst = QDir::cleanPath(dst);
    for (const QString& entry : entries) {
        const QString srcPath = srcDir.absoluteFilePath(entry);
        const QString dstPath = dstDir.absoluteFilePath(entry);
        // Never copy a destination that lives inside the source tree into itself.
        if (QDir::cleanPath(srcPath).compare(cleanDst, Qt::CaseInsensitive) == 0)
            continue;
        if (cleanDst.startsWith(cleanSrc + QLatin1Char('/'), Qt::CaseInsensitive)
            && QDir::cleanPath(srcPath)
                   .startsWith(cleanDst + QLatin1Char('/'), Qt::CaseInsensitive))
            continue;
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
