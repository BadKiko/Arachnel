#include "file_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <algorithm>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace arachnel::core {
namespace {

void clearReadOnlyAttribute(const QString& filePath)
{
#if defined(Q_OS_WIN)
    SetFileAttributesW(reinterpret_cast<LPCWSTR>(filePath.utf16()), FILE_ATTRIBUTE_NORMAL);
#else
    QFile::setPermissions(filePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                        | QFile::ReadUser | QFile::WriteUser);
#endif
}

bool tryRemoveFile(const QString& path)
{
    clearReadOnlyAttribute(path);
    return QFile::remove(path);
}

bool tryRemoveDirectoryOnce(const QString& path)
{
    // Clear read-only on nested files so removeRecursively can succeed on Windows.
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        clearReadOnlyAttribute(it.next());
    }

    QDir dir(path);
    return dir.removeRecursively();
}

void reportProgress(const FileProgressCallback& onProgress, qint64* copiedInOut, qint64 totalHint,
                    qint64 delta)
{
    if (copiedInOut)
        *copiedInOut += delta;
    if (!onProgress)
        return;
    const qint64 done = copiedInOut ? *copiedInOut : delta;
    const qint64 total = totalHint > 0 ? totalHint : done;
    onProgress(done, total);
}

} // namespace

qint64 pathByteSize(const QString& path)
{
    if (path.isEmpty())
        return 0;
    QFileInfo info(path);
    if (!info.exists())
        return 0;
    if (info.isFile())
        return info.size();

    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

bool removePathRecursive(const QString& path, QString* errorOut)
{
    if (path.isEmpty())
        return true;

    QFileInfo info(path);
    if (!info.exists())
        return true;

    constexpr int kAttempts = 5;
    for (int attempt = 0; attempt < kAttempts; ++attempt) {
        if (attempt > 0)
            QThread::msleep(static_cast<unsigned long>(100 * attempt));

        info.refresh();
        if (!info.exists())
            return true;

        const bool ok = info.isFile() ? tryRemoveFile(path) : tryRemoveDirectoryOnce(path);
        if (ok)
            return true;
    }

    if (errorOut) {
        *errorOut = info.isFile()
                        ? QCoreApplication::translate("Core", "Failed to delete file: %1").arg(path)
                        : QCoreApplication::translate("Core", "Failed to delete folder: %1").arg(path);
    }
    return false;
}

bool copyPathRecursive(const QString& src, const QString& dst, QString* errorOut,
                       const FileProgressCallback& onProgress, qint64* copiedInOut, qint64 totalHint)
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
        if (QFile::copy(src, dst)) {
            reportProgress(onProgress, copiedInOut, totalHint, srcInfo.size());
            return true;
        }
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
        if (!copyPathRecursive(srcPath, dstPath, errorOut, onProgress, copiedInOut, totalHint))
            return false;
    }
    return true;
}

bool movePathRecursive(const QString& src, const QString& dst, QString* errorOut,
                       const FileProgressCallback& onProgress)
{
    if (!QFileInfo(src).exists())
        return true;

    const qint64 total = pathByteSize(src);
    if (QDir().rename(src, dst)) {
        if (onProgress)
            onProgress(total > 0 ? total : 1, total > 0 ? total : 1);
        return true;
    }

    qint64 copied = 0;
    if (!copyPathRecursive(src, dst, errorOut, onProgress, &copied, total))
        return false;
    if (onProgress && total > 0)
        onProgress(total, total);
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

bool rewritePathPrefixInFile(const QString& filePath, const QString& oldRoot, const QString& newRoot)
{
    if (filePath.isEmpty() || oldRoot.isEmpty() || !QFileInfo::exists(filePath))
        return true;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QByteArray data = file.readAll();
    file.close();

    const QByteArray oldFwd = QDir::fromNativeSeparators(oldRoot).toUtf8();
    const QByteArray newFwd = QDir::fromNativeSeparators(newRoot).toUtf8();
    const QByteArray oldNat = QDir::toNativeSeparators(oldRoot).toUtf8();
    const QByteArray newNat = QDir::toNativeSeparators(newRoot).toUtf8();

    const QByteArray before = data;
    data.replace(oldFwd, newFwd);
    if (oldNat != oldFwd)
        data.replace(oldNat, newNat);
    if (data == before)
        return true;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(data);
    return true;
}

int healWindowsInstallLayout(const QString& installPath)
{
#if defined(Q_OS_WIN)
    Q_UNUSED(installPath);
    return 0;
#else
    if (installPath.isEmpty() || !QFileInfo::exists(installPath))
        return 0;

    QStringList broken;
    QDirIterator it(installPath, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System
                                     | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileName().contains(QLatin1Char('\\')))
            broken.append(it.filePath());
    }
    std::sort(broken.begin(), broken.end(), [](const QString& a, const QString& b) {
        return a.size() > b.size();
    });

    int moved = 0;
    for (const QString& src : broken) {
        const QFileInfo info(src);
        if (!info.exists())
            continue;
        const QString dest = info.dir().filePath(
            info.fileName().replace(QLatin1Char('\\'), QLatin1Char('/')));
        if (dest == src)
            continue;
        QDir().mkpath(QFileInfo(dest).absolutePath());
        if (QFileInfo::exists(dest)) {
            if (info.isDir())
                QDir(src).removeRecursively();
            else
                QFile::remove(src);
            ++moved;
            continue;
        }
        if (info.isDir() ? QDir().rename(src, dest) : QFile::rename(src, dest))
            ++moved;
    }

    const QString marker = QDir(installPath).filePath(QStringLiteral(".arachnel-steamidra"));
    if (QFileInfo::exists(marker)) {
        QFile file(marker);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray data = file.readAll();
            file.close();
            const QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                QJsonObject launch = root.value(QStringLiteral("launch")).toObject();
                QString path = launch.value(QStringLiteral("path")).toString();
                if (path.contains(QLatin1Char('\\'))) {
                    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
                    launch.insert(QStringLiteral("path"), path);
                    root.insert(QStringLiteral("launch"), launch);
                    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
                        file.close();
                    }
                }
            } else {
                QByteArray raw = data;
                raw.replace(QByteArray("\\\\"), QByteArray("/"));
                raw.replace('\\', '/');
                if (raw != data && file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    file.write(raw);
                    file.close();
                }
            }
        }
    }
    return moved;
#endif
}

} // namespace arachnel::core
