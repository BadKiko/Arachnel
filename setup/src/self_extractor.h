#pragma once

#include <QString>
#include <functional>

namespace arachnel::setup {

using ExtractProgressFn = std::function<void(int percent, const QString& status)>;

bool extractZipSlice(const QString& containerPath, quint64 offset, quint64 size,
                     const QString& destinationDir, QString* errorOut = nullptr,
                     ExtractProgressFn onProgress = {});

bool extractZipArchive(const QString& zipPath, const QString& destinationDir,
                       QString* errorOut = nullptr);

bool ensureRuntimeBootstrapped(const QString& executablePath, QString* errorOut = nullptr);

} // namespace arachnel::setup
