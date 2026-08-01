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

std::wstring quotePsLiteral(const std::wstring& value)
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

std::wstring system32Path(const wchar_t* exeName)
{
    wchar_t systemDir[MAX_PATH] = {};
    if (GetSystemDirectoryW(systemDir, MAX_PATH) == 0)
        return {};
    return std::wstring(systemDir) + L'\\' + exeName;
}

bool runProcess(const std::wstring& command, std::wstring* errorOut, const wchar_t* failMessage)
{
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    if (!CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startupInfo, &processInfo)) {
        if (errorOut)
            *errorOut = failMessage;
        return false;
    }

    WaitForSingleObject(processInfo.hProcess, 600000);
    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    if (exitCode != 0) {
        if (errorOut)
            *errorOut = failMessage;
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

bool writeUtf8BomFile(const std::filesystem::path& path, const std::string& utf8Body,
                      std::wstring* errorOut)
{
    const HANDLE file =
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (errorOut)
            *errorOut = L"Could not create PowerShell script";
        return false;
    }

    static const unsigned char kBom[] = {0xEF, 0xBB, 0xBF};
    DWORD written = 0;
    if (!WriteFile(file, kBom, 3, &written, nullptr) || written != 3) {
        CloseHandle(file);
        if (errorOut)
            *errorOut = L"Could not write PowerShell script";
        return false;
    }
    written = 0;
    if (!WriteFile(file, utf8Body.data(), static_cast<DWORD>(utf8Body.size()), &written, nullptr)
        || written != utf8Body.size()) {
        CloseHandle(file);
        if (errorOut)
            *errorOut = L"Could not write PowerShell script";
        return false;
    }
    CloseHandle(file);
    return true;
}

std::string wideToUtf8(const std::wstring& value)
{
    if (value.empty())
        return {};
    const int size =
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0,
                            nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

bool runTarExpand(const std::filesystem::path& zipPath, const std::filesystem::path& destinationDir,
                  std::wstring* errorOut)
{
    const std::wstring tar = system32Path(L"tar.exe");
    if (tar.empty() || GetFileAttributesW(tar.c_str()) == INVALID_FILE_ATTRIBUTES)
        return false;

    std::filesystem::create_directories(destinationDir);

    // Windows tar extracts zip; -C must exist. Quote paths for CreateProcess.
    std::wstring command = L"\"" + tar + L"\" -xf \"" + zipPath.wstring() + L"\" -C \""
                           + destinationDir.wstring() + L"\"";
    return runProcess(command, errorOut, L"Archive extraction failed (tar)");
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

    // UTF-8 BOM script: wofstream + default locale breaks Cyrillic LocalAppData paths.
    const std::string body =
        "$ErrorActionPreference = 'Stop'\n"
        "Expand-Archive -LiteralPath " + wideToUtf8(quotePsLiteral(zipPath.wstring()))
        + " -DestinationPath " + wideToUtf8(quotePsLiteral(destinationDir.wstring())) + " -Force\n";

    if (!writeUtf8BomFile(scriptPath, body, errorOut))
        return false;

    const std::wstring powershell = system32Path(L"WindowsPowerShell\\v1.0\\powershell.exe");
    if (powershell.empty()) {
        if (errorOut)
            *errorOut = L"Could not resolve System32 directory";
        return false;
    }

    std::wstring command = L"\"" + powershell
                           + L"\" -NoProfile -ExecutionPolicy Bypass -File \""
                           + scriptPath.wstring() + L"\"";

    const bool ok = runProcess(command, errorOut, L"Archive extraction failed");
    std::error_code ec;
    std::filesystem::remove(scriptPath, ec);
    return ok;
}

bool expandArchive(const std::filesystem::path& zipPath, const std::filesystem::path& destinationDir,
                   std::wstring* errorOut)
{
    // Prefer tar: no script file, Unicode paths via CreateProcessW.
    std::wstring tarError;
    if (runTarExpand(zipPath, destinationDir, &tarError)) {
        if (directoryHasAnyFile(destinationDir))
            return true;
        // tar returned 0 but wrote nothing - try PowerShell next.
    }

    std::wstring psError;
    if (!runPowerShellExpand(zipPath, destinationDir, &psError)) {
        if (errorOut)
            *errorOut = psError.empty() ? (tarError.empty() ? L"Archive extraction failed" : tarError)
                                        : psError;
        return false;
    }

    if (!directoryHasAnyFile(destinationDir)) {
        if (errorOut) {
            *errorOut = L"Archive extraction produced no files.\n"
                        L"If the Windows user name has non-Latin characters, rebuild Arachnel Setup "
                        L"or extract manually.";
        }
        return false;
    }
    return true;
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

    const bool ok = expandArchive(tempZip, destinationDir, errorOut);
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
