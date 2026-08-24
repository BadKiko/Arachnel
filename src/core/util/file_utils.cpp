#include "file_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
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

int peImageBits(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 0x40)
        return 0;

    if (!file.seek(0x3C))
        return 0;
    const QByteArray lfanew = file.read(4);
    if (lfanew.size() < 4)
        return 0;
    const quint32 peOffset = quint32(static_cast<uchar>(lfanew.at(0)))
                             | (quint32(static_cast<uchar>(lfanew.at(1))) << 8)
                             | (quint32(static_cast<uchar>(lfanew.at(2))) << 16)
                             | (quint32(static_cast<uchar>(lfanew.at(3))) << 24);

    if (peOffset < 0x40 || peOffset + 6 > quint32(file.size()) || !file.seek(peOffset))
        return 0;
    if (file.read(4) != QByteArray("PE\0\0", 4))
        return 0;

    const QByteArray machine = file.read(2);
    if (machine.size() < 2)
        return 0;
    const quint16 machineType = quint16(static_cast<uchar>(machine.at(0)))
                                | (quint16(static_cast<uchar>(machine.at(1))) << 8);
    if (machineType == 0x8664) // IMAGE_FILE_MACHINE_AMD64
        return 64;
    if (machineType == 0x14c) // IMAGE_FILE_MACHINE_I386
        return 32;
    return 0;
}

QByteArray firstJsonObject(const QByteArray& data)
{
    const int start = data.indexOf('{');
    if (start < 0)
        return {};
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (int i = start; i < data.size(); ++i) {
        const char c = data.at(i);
        if (inString) {
            if (escape)
                escape = false;
            else if (c == '\\')
                escape = true;
            else if (c == '"')
                inString = false;
            continue;
        }
        if (c == '"')
            inString = true;
        else if (c == '{')
            ++depth;
        else if (c == '}') {
            --depth;
            if (depth == 0)
                return data.mid(start, i - start + 1);
        }
    }
    return {};
}

bool healUnityScriptingAssembliesFile(const QString& jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray raw = file.readAll();
    file.close();
    if (raw.trimmed().isEmpty())
        return false;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    const bool leftoverBytes = parseError.error != QJsonParseError::NoError || !doc.isObject();
    if (leftoverBytes) {
        const QByteArray first = firstJsonObject(raw);
        if (first.isEmpty())
            return false;
        doc = QJsonDocument::fromJson(first, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            return false;
    }

    QJsonObject root = doc.object();
    QJsonArray names = root.value(QStringLiteral("names")).toArray();
    const QJsonArray types = root.value(QStringLiteral("types")).toArray();
    if (names.isEmpty() || names.size() != types.size())
        return false;

    const QDir managed(QFileInfo(jsonPath).absolutePath());
    const bool hasWin64 =
        QFileInfo::exists(managed.filePath(QStringLiteral("Facepunch.Steamworks.Win64.dll")));
    bool swappedSteamworks = false;
    if (hasWin64) {
        for (int i = 0; i < names.size(); ++i) {
            if (names.at(i).toString() == QLatin1String("Facepunch.Steamworks.Posix.dll")) {
                names[i] = QStringLiteral("Facepunch.Steamworks.Win64.dll");
                swappedSteamworks = true;
            }
        }
        if (swappedSteamworks)
            root.insert(QStringLiteral("names"), names);
    }

    if (!leftoverBytes && !swappedSteamworks)
        return false;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return true;
}

int healUnityScriptingAssemblies(const QString& installPath)
{
    if (installPath.isEmpty() || !QFileInfo::exists(installPath))
        return 0;

    int repaired = 0;
    QDirIterator it(installPath, {QStringLiteral("ScriptingAssemblies.json")}, QDir::Files,
                    QDirIterator::Subdirectories);
    int seen = 0;
    while (it.hasNext() && seen < 32) {
        it.next();
        ++seen;
        const QString rel = QDir(installPath).relativeFilePath(it.filePath());
        if (rel.count(QLatin1Char('/')) + rel.count(QLatin1Char('\\')) > 4)
            continue;
        if (healUnityScriptingAssembliesFile(it.filePath()))
            ++repaired;
    }
    return repaired;
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
