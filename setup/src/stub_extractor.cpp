#include "stub_extractor.h"

#include "win_container_io.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ShlObj.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace arachnel::setup {

namespace {

std::wstring quoteForPowerShell(const std::wstring& value)
{
    std::wstring escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back(L'\'');
    for (wchar_t ch : value) {
        if (ch == L'\'')
            escaped.append(L"''");
        else
            escaped.push_back(ch);
    }
    escaped.push_back(L'\'');
    return escaped;
}

bool runPowerShellExpand(const std::filesystem::path& zipPath,
                         const std::filesystem::path& destinationDir, std::wstring* errorOut)
{
    std::filesystem::create_directories(destinationDir);

    wchar_t tempPath[MAX_PATH] = {};
    if (GetTempPathW(MAX_PATH, tempPath) == 0) {
        if (errorOut)
            *errorOut = L"Could not resolve temp directory";
        return false;
    }

    const std::filesystem::path scriptPath =
        std::filesystem::path(tempPath) / L"arachnel-expand.ps1";

    {
        std::wofstream script(scriptPath, std::ios::trunc);
        if (!script) {
            if (errorOut)
                *errorOut = L"Could not create PowerShell script";
            return false;
        }
        script << L"$ErrorActionPreference = 'Stop'\n";
        script << L"Expand-Archive -LiteralPath " << quoteForPowerShell(zipPath.wstring())
               << L" -DestinationPath " << quoteForPowerShell(destinationDir.wstring())
               << L" -Force\n";
    }

    wchar_t systemDir[MAX_PATH] = {};
    if (GetSystemDirectoryW(systemDir, MAX_PATH) == 0) {
        if (errorOut)
            *errorOut = L"Could not resolve System32 directory";
        return false;
    }

    const std::wstring powershell =
        std::wstring(systemDir) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring command = L"\"" + powershell
                           + L"\" -NoProfile -ExecutionPolicy Bypass -File \""
                           + scriptPath.wstring() + L"\"";

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    if (!CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startupInfo, &processInfo)) {
        std::error_code ec;
        std::filesystem::remove(scriptPath, ec);
        if (errorOut)
            *errorOut = L"Could not start PowerShell";
        return false;
    }

    WaitForSingleObject(processInfo.hProcess, 600000);
    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    std::error_code ec;
    std::filesystem::remove(scriptPath, ec);

    if (exitCode != 0) {
        if (errorOut)
            *errorOut = L"Archive extraction failed";
        return false;
    }
    return true;
}

bool writeSliceToTempZip(const std::filesystem::path& containerPath, std::uint64_t offset,
                         std::uint64_t size, const std::filesystem::path& tempZipPath,
                         std::wstring* errorOut)
{
    return copyContainerSlice(containerPath, offset, size, tempZipPath, errorOut);
}

} // namespace

bool extractZipSliceNative(const std::filesystem::path& containerPath, std::uint64_t offset,
                           std::uint64_t size, const std::filesystem::path& destinationDir,
                           std::wstring* errorOut)
{
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

    if (!writeSliceToTempZip(containerPath, offset, size, tempZip, errorOut))
        return false;

    const bool ok = runPowerShellExpand(tempZip, destinationDir, errorOut);
    std::filesystem::remove(tempZip, ec);
    return ok;
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
