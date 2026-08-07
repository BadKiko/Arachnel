#pragma once

#include <QString>

#include <functional>

namespace arachnel::core {

using FileProgressCallback = std::function<void(qint64 bytesDone, qint64 bytesTotal)>;

qint64 pathByteSize(const QString& path);
bool removePathRecursive(const QString& path, QString* errorOut = nullptr);
bool copyPathRecursive(const QString& src, const QString& dst, QString* errorOut = nullptr,
                       const FileProgressCallback& onProgress = {}, qint64* copiedInOut = nullptr,
                       qint64 totalHint = 0);
bool movePathRecursive(const QString& src, const QString& dst, QString* errorOut = nullptr,
                       const FileProgressCallback& onProgress = {});
QString relocatePathPrefix(const QString& path, const QString& oldRoot, const QString& newRoot);
/** Rewrite absolute path prefixes inside a text/JSON file (install markers). */
bool rewritePathPrefixInFile(const QString& filePath, const QString& oldRoot,
                             const QString& newRoot);

} // namespace arachnel::core
