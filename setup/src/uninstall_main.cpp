#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shlobj.h>

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
    MessageBoxW(nullptr, text, L"Arachnel", MB_ICONERROR | MB_OK);
}

bool removeShortcutIfExists(const std::filesystem::path& path)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return true;
    return std::filesystem::remove(path, ec);
}

std::filesystem::path desktopDir()
{
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &path)))
        return {};
    const std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result;
}

std::filesystem::path startMenuProgramsDir()
{
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &path)))
        return {};
    const std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result;
}

bool deleteUninstallRegistry()
{
    constexpr wchar_t kKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Arachnel";
    for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        const LONG result = RegDeleteTreeW(root, kKey);
        if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
            return true;
    }
    return false;
}

bool scheduleFolderRemoval(const std::filesystem::path& installDir)
{
    const std::wstring quoted = L"\"" + installDir.wstring() + L"\"";
    const std::wstring command =
        L"cmd.exe /c timeout /t 2 /nobreak > nul && rd /s /q " + quoted;

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    if (!CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startupInfo, &processInfo)) {
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const std::filesystem::path installDir = modulePath().parent_path();
    if (installDir.empty()) {
        showError(L"Could not resolve install folder.");
        return 1;
    }

    const int answer = MessageBoxW(
        nullptr,
        L"Remove Arachnel from this computer?\n\nThis deletes the install folder and shortcuts.",
        L"Uninstall Arachnel",
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
    if (answer != IDYES)
        return 0;

    const auto desktop = desktopDir();
    if (!desktop.empty())
        removeShortcutIfExists(desktop / L"Arachnel.lnk");

    const auto startMenu = startMenuProgramsDir();
    if (!startMenu.empty()) {
        removeShortcutIfExists(startMenu / L"Arachnel" / L"Arachnel.lnk");
        std::error_code ec;
        std::filesystem::remove_all(startMenu / L"Arachnel", ec);
    }

    deleteUninstallRegistry();

    if (!scheduleFolderRemoval(installDir)) {
        showError(L"Could not schedule folder removal. Delete the install folder manually.");
        return 1;
    }

    MessageBoxW(nullptr, L"Arachnel was uninstalled.", L"Arachnel", MB_ICONINFORMATION | MB_OK);
    return 0;
}
