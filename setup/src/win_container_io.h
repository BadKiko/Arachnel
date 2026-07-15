#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace arachnel::setup {

// Opens a container file for read while the same image may still be executing.
HANDLE openContainerForRead(const std::filesystem::path& path);

bool queryContainerSize(HANDLE file, std::uint64_t* sizeOut);

bool readContainerAt(HANDLE file, std::uint64_t offset, void* buffer, std::size_t size);

bool copyContainerSlice(const std::filesystem::path& containerPath, std::uint64_t offset,
                        std::uint64_t size, const std::filesystem::path& outputPath,
                        std::wstring* errorOut);

} // namespace arachnel::setup
