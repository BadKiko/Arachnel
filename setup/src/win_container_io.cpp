#include "win_container_io.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace arachnel::setup {

HANDLE openContainerForRead(const std::filesystem::path& path)
{
    return CreateFileW(path.c_str(), GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool queryContainerSize(HANDLE file, std::uint64_t* sizeOut)
{
    if (!sizeOut)
        return false;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0)
        return false;

    *sizeOut = static_cast<std::uint64_t>(size.QuadPart);
    return true;
}

bool readContainerAt(HANDLE file, std::uint64_t offset, void* buffer, std::size_t size)
{
    if (!buffer || size == 0)
        return false;

    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN))
        return false;

    std::size_t remaining = size;
    auto* cursor = static_cast<std::uint8_t*>(buffer);
    while (remaining > 0) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, 1u << 20));
        DWORD read = 0;
        if (!ReadFile(file, cursor, chunk, &read, nullptr) || read == 0)
            return false;
        cursor += read;
        remaining -= read;
    }
    return true;
}

bool copyContainerSlice(const std::filesystem::path& containerPath, std::uint64_t offset,
                        std::uint64_t size, const std::filesystem::path& outputPath,
                        std::wstring* errorOut)
{
    const HANDLE input = openContainerForRead(containerPath);
    if (input == INVALID_HANDLE_VALUE) {
        if (errorOut)
            *errorOut = L"Could not open installer payload";
        return false;
    }

    const HANDLE output = CreateFileW(outputPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        CloseHandle(input);
        if (errorOut)
            *errorOut = L"Could not create temporary archive";
        return false;
    }

    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(input, position, nullptr, FILE_BEGIN)) {
        CloseHandle(input);
        CloseHandle(output);
        if (errorOut)
            *errorOut = L"Invalid payload offset";
        return false;
    }

    constexpr std::size_t kChunk = 256 * 1024;
    std::vector<char> buffer(kChunk);
    std::uint64_t remaining = size;

    while (remaining > 0) {
        const DWORD toRead =
            static_cast<DWORD>(std::min<std::uint64_t>(kChunk, remaining));
        DWORD read = 0;
        if (!ReadFile(input, buffer.data(), toRead, &read, nullptr) || read == 0) {
            CloseHandle(input);
            CloseHandle(output);
            if (errorOut)
                *errorOut = L"Unexpected end of installer payload";
            return false;
        }
        DWORD written = 0;
        if (!WriteFile(output, buffer.data(), read, &written, nullptr) || written != read) {
            CloseHandle(input);
            CloseHandle(output);
            if (errorOut)
                *errorOut = L"Could not write temporary archive";
            return false;
        }
        remaining -= read;
    }

    CloseHandle(input);
    CloseHandle(output);
    return true;
}

} // namespace arachnel::setup
