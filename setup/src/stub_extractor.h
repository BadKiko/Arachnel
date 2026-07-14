#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace arachnel::setup {

bool extractZipSliceNative(const std::filesystem::path& containerPath, std::uint64_t offset,
                           std::uint64_t size, const std::filesystem::path& destinationDir,
                           std::wstring* errorOut = nullptr);

std::filesystem::path runtimeCacheDirNative();

bool runtimeIsCurrentNative(const std::filesystem::path& dir, std::uint64_t runtimeOffset,
                            std::uint64_t runtimeSize);

bool writeRuntimeManifestNative(const std::filesystem::path& dir, std::uint64_t runtimeOffset,
                                std::uint64_t runtimeSize);

bool isRuntimeReadyNative(const std::filesystem::path& dir);

} // namespace arachnel::setup
