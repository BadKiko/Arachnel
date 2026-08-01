#include "stub_extractor.h"

#include "win_container_io.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ShlObj.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace arachnel::setup {

namespace {

std::wstring system32Path(const wchar_t* exeName)
{
    wchar_t systemDir[MAX_PATH] = {};
    if (GetSystemDirectoryW(systemDir, MAX_PATH) == 0)
        return {};
    return std::wstring(systemDir) + L'\\' + exeName;
}

bool runTarExpand(const std::filesystem::path& zipPath, const std::filesystem::path& destinationDir,
                  std::wstring* errorOut)
{
    const std::wstring tar = system32Path(L"tar.exe");
    if (tar.empty() || GetFileAttributesW(tar.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (errorOut)
            *errorOut = L"tar.exe not found (Windows 10 or newer required)";
        return false;
    }

    std::filesystem::create_directories(destinationDir);

    // Quote paths for CreateProcess; use tar only (no PowerShell - Defender ML signal).
    std::wstring command = L"\"" + tar + L"\" -xf \"" + zipPath.wstring() + L"\" -C \""
                           + destinationDir.wstring() + L"\"";

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    // CREATE_NO_WINDOW: avoid console flash; tar is a normal system binary, not a script host.
    if (!CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startupInfo, &processInfo)) {
        if (errorOut)
            *errorOut = L"Could not start tar.exe";
        return false;
    }

    WaitForSingleObject(processInfo.hProcess, 600000);
    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    if (exitCode != 0) {
        if (errorOut)
            *errorOut = L"Archive extraction failed (tar)";
        return false;
    }
    return true;
}

bool directoryHasAnyFile(const std::filesystem::path& dir)
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec))
        return false;
    const auto end = std::filesystem::directory_iterator{};
    for (auto it = std::filesystem::directory_iterator(dir, ec); !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) || it->is_directory(ec))
            return true;
    }
    return false;
}

} // namespace

bool extractZipSliceNative(const std::filesystem::path& containerPath, std::uint64_t offset,
                           std::uint64_t size, const std::filesystem::path& destinationDir,
                           std::wstring* errorOut)
{
    if (size == 0) {
        if (errorOut)
            *errorOut = L"Installer runtime payload is empty";
        return false;
    }

    wchar_t tempPath[MAX_PATH] = {};
    if (GetTempPathW(MAX_PATH, tempPath) == 0) {
        if (errorOut)
            *errorOut = L"Could not resolve temp directory";
        return false;
    }

    const std::filesystem::path tempZip =
        std::filesystem::path(tempPath) / L"arachnel-setup-slice.zip";
    std::error_code ec;
    std::filesystem::remove(tempZip, ec);

    if (!copyContainerSlice(containerPath, offset, size, tempZip, errorOut))
        return false;

    const auto zipSize = std::filesystem::file_size(tempZip, ec);
    if (ec || zipSize != size) {
        std::filesystem::remove(tempZip, ec);
        if (errorOut)
            *errorOut = L"Temporary archive size mismatch (incomplete copy or AV interference)";
        return false;
    }

    if (!runTarExpand(tempZip, destinationDir, errorOut)) {
        std::filesystem::remove(tempZip, ec);
        return false;
    }
    std::filesystem::remove(tempZip, ec);

    if (!directoryHasAnyFile(destinationDir)) {
        if (errorOut) {
            *errorOut = L"Archive extraction produced no files.\n"
                        L"Windows tar could not unpack the installer runtime.";
        }
        return false;
    }
    return true;
}

std::filesystem::path runtimeCacheDirNative()
{
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)))
        return {};

    const std::filesystem::path base(localAppData);
    CoTaskMemFree(localAppData);
    return base / L"Arachnel Setup" / L"cache" / L"runtime";
}

bool isRuntimeReadyNative(const std::filesystem::path& dir)
{
    return std::filesystem::exists(dir / L"arachnel_setup.exe")
           && std::filesystem::exists(dir / L"Qt6Core.dll")
           && std::filesystem::exists(dir / L"uninstall.exe");
}

bool runtimeIsCurrentNative(const std::filesystem::path& dir, std::uint64_t runtimeOffset,
                            std::uint64_t runtimeSize)
{
    if (!isRuntimeReadyNative(dir))
        return false;

    const std::filesystem::path manifestPath = dir / L"runtime-manifest.txt";
    std::ifstream manifest(manifestPath);
    if (!manifest)
        return false;

    std::uint64_t cachedOffset = 0;
    std::uint64_t cachedSize = 0;
    std::string line;
    while (std::getline(manifest, line)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;
        const std::string key = line.substr(0, pos);
        const std::uint64_t value = std::stoull(line.substr(pos + 1));
        if (key == "offset")
            cachedOffset = value;
        else if (key == "size")
            cachedSize = value;
    }

    return cachedOffset == runtimeOffset && cachedSize == runtimeSize;
}

bool writeRuntimeManifestNative(const std::filesystem::path& dir, std::uint64_t runtimeOffset,
                                std::uint64_t runtimeSize)
{
    const std::filesystem::path manifestPath = dir / L"runtime-manifest.txt";
    std::ofstream manifest(manifestPath, std::ios::trunc);
    if (!manifest)
        return false;
    manifest << "offset=" << runtimeOffset << "\n";
    manifest << "size=" << runtimeSize << "\n";
    return static_cast<bool>(manifest);
}

} // namespace arachnel::setup
