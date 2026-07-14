#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "payload_footer_io.h"
#include "stub_extractor.h"

#include <shellapi.h>

#include <filesystem>
#include <string>
#include <vector>

namespace {

std::filesystem::path modulePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return {};
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
}

void showError(const wchar_t* text)
{
    MessageBoxW(nullptr, text, L"Arachnel Setup", MB_ICONERROR | MB_OK);
}

std::wstring buildEnvironmentBlock(const std::wstring& containerPath)
{
    wchar_t* environment = GetEnvironmentStringsW();
    if (!environment)
        return {};

    std::wstring block;
    for (const wchar_t* cursor = environment; *cursor != L'\0';
         cursor += std::wcslen(cursor) + 1) {
        const std::wstring entry(cursor);
        if (entry.rfind(L"ARACHNEL_SETUP_RUNTIME=", 0) == 0
            || entry.rfind(L"ARACHNEL_SETUP_CONTAINER=", 0) == 0) {
            continue;
        }
        block.append(entry);
        block.push_back(L'\0');
    }
    FreeEnvironmentStringsW(environment);

    block.append(L"ARACHNEL_SETUP_RUNTIME=1");
    block.push_back(L'\0');
    block.append(L"ARACHNEL_SETUP_CONTAINER=");
    block.append(containerPath);
    block.push_back(L'\0');
    block.push_back(L'\0');
    return block;
}

bool launchSetupUi(const std::filesystem::path& runtimeDir,
                   const std::filesystem::path& containerPath, std::wstring* errorOut)
{
    const std::filesystem::path targetExe = runtimeDir / L"arachnel_setup.exe";
    if (!std::filesystem::exists(targetExe)) {
        if (errorOut)
            *errorOut = L"Installer runtime is incomplete";
        return false;
    }

    const std::wstring environment = buildEnvironmentBlock(containerPath.wstring());
    if (environment.empty()) {
        if (errorOut)
            *errorOut = L"Could not prepare process environment";
        return false;
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    std::wstring command = L"\"" + targetExe.wstring() + L"\"";

    if (!CreateProcessW(targetExe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT,
                        const_cast<wchar_t*>(environment.c_str()), runtimeDir.c_str(), &startupInfo,
                        &processInfo)) {
        if (errorOut)
            *errorOut = L"Could not start installer UI";
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const std::filesystem::path containerPath = modulePath();
    if (containerPath.empty()) {
        showError(L"Could not resolve installer path.");
        return 1;
    }

    const auto footer = arachnel::setup::readPayloadFooterFile(containerPath);
    if (!footer.valid || footer.runtimeSize == 0) {
        showError(L"Installer payload is missing or invalid.\nRebuild with .\\run.ps1 --installer");
        return 1;
    }

    const std::filesystem::path runtimeDir = arachnel::setup::runtimeCacheDirNative();
    if (runtimeDir.empty()) {
        showError(L"Could not resolve local cache directory.");
        return 1;
    }

    if (!arachnel::setup::runtimeIsCurrentNative(runtimeDir, footer.runtimeOffset,
                                                 footer.runtimeSize)) {
        std::error_code ec;
        std::filesystem::remove_all(runtimeDir, ec);
        std::filesystem::create_directories(runtimeDir, ec);

        std::wstring extractError;
        if (!arachnel::setup::extractZipSliceNative(containerPath, footer.runtimeOffset,
                                                    footer.runtimeSize, runtimeDir, &extractError)) {
            showError(extractError.empty() ? L"Could not extract installer runtime." : extractError.c_str());
            return 1;
        }

        if (!arachnel::setup::isRuntimeReadyNative(runtimeDir)) {
            showError(L"Installer runtime extraction is incomplete.");
            return 1;
        }

        if (!arachnel::setup::writeRuntimeManifestNative(runtimeDir, footer.runtimeOffset,
                                                         footer.runtimeSize)) {
            showError(L"Could not update installer runtime cache.");
            return 1;
        }
    }

    std::wstring launchError;
    if (!launchSetupUi(runtimeDir, containerPath, &launchError)) {
        showError(launchError.empty() ? L"Could not launch installer." : launchError.c_str());
        return 1;
    }

    return 0;
}
