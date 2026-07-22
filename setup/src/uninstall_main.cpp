#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shlobj.h>

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::wstring toLower(std::wstring s)
{
    for (wchar_t& ch : s)
        ch = static_cast<wchar_t>(towlower(ch));
    return s;
}

fs::path modulePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return {};
        if (length < buffer.size()) {
            buffer.resize(length);
            return fs::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
}

void showError(const wchar_t* text)
{
    MessageBoxW(nullptr, text, L"Arachnel", MB_ICONERROR | MB_OK);
}

bool removeShortcutIfExists(const fs::path& path)
{
    std::error_code ec;
    if (!fs::exists(path, ec))
        return true;
    return fs::remove(path, ec);
}

fs::path knownFolder(REFKNOWNFOLDERID id)
{
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &path)))
        return {};
    const fs::path result(path);
    CoTaskMemFree(path);
    return result;
}

bool deleteUninstallRegistry()
{
    constexpr wchar_t kKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Arachnel";
    bool ok = false;
    for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        const LONG result = RegDeleteTreeW(root, kKey);
        if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
            ok = true;
    }
    return ok;
}

void deleteSettingsRegistry()
{
    const wchar_t* keys[] = {
        L"Software\\Arachnel",
        L"Software\\PetWork\\Arachnel",
        L"Software\\Unknown Organization\\Arachnel",
    };
    for (const wchar_t* key : keys)
        RegDeleteTreeW(HKEY_CURRENT_USER, key);
}

bool pathLooksSafeToWipe(const fs::path& path)
{
    if (path.empty() || !path.has_root_path())
        return false;
    std::error_code ec;
    fs::path use = fs::weakly_canonical(path, ec);
    if (ec)
        use = path.lexically_normal();
    if (use.empty() || use == use.root_path())
        return false;
    const std::wstring lower = toLower(use.wstring());
    if (lower.size() < 8)
        return false;
    return lower.find(L"arachnel") != std::wstring::npos;
}

void collectIfExists(std::vector<fs::path>& out, const fs::path& path)
{
    std::error_code ec;
    if (!path.empty() && fs::exists(path, ec) && pathLooksSafeToWipe(path))
        out.push_back(path);
}

std::string readFileUtf8(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void collectLibraryPathsFromSettings(const fs::path& settingsFile, std::vector<fs::path>& out)
{
    const std::string text = readFileUtf8(settingsFile);
    if (text.empty())
        return;
    const std::string key = "\"path\"";
    size_t pos = 0;
    while ((pos = text.find(key, pos)) != std::string::npos) {
        pos += key.size();
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == ':'))
            ++pos;
        if (pos >= text.size() || text[pos] != '"')
            continue;
        ++pos;
        std::string value;
        while (pos < text.size() && text[pos] != '"') {
            if (text[pos] == '\\' && pos + 1 < text.size()) {
                value.push_back(text[pos + 1]);
                pos += 2;
                continue;
            }
            value.push_back(text[pos++]);
        }
        if (value.empty())
            continue;
        const int needed =
            MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (needed <= 0)
            continue;
        std::wstring wide(static_cast<size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(),
                            needed);
        collectIfExists(out, fs::path(wide));
    }
}

void removePathNow(const fs::path& path)
{
    if (!pathLooksSafeToWipe(path))
        return;
    std::error_code ec;
    fs::remove_all(path, ec);
}

bool scheduleFolderRemovals(const std::vector<fs::path>& folders)
{
    if (folders.empty())
        return true;

    std::wstring command = L"cmd.exe /c timeout /t 2 /nobreak > nul";
    for (const fs::path& folder : folders) {
        if (!pathLooksSafeToWipe(folder))
            continue;
        command += L" & rd /s /q \"";
        command += folder.wstring();
        command += L"\"";
    }

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

void terminateArachnelProcesses()
{
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    std::wstring command =
        L"cmd.exe /c taskkill /F /IM arachnel_app.exe >nul 2>&1 & taskkill /F /IM Arachnel.exe >nul 2>&1";
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(L'\0');
    if (CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                       nullptr, &startupInfo, &processInfo)) {
        WaitForSingleObject(processInfo.hProcess, 5000);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
    }
    Sleep(500);
}

void appendUnique(std::vector<fs::path>& unique, const fs::path& path)
{
    if (!pathLooksSafeToWipe(path))
        return;
    const std::wstring key = toLower(path.lexically_normal().wstring());
    for (const fs::path& u : unique) {
        if (toLower(u.lexically_normal().wstring()) == key)
            return;
    }
    unique.push_back(path);
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const fs::path installDir = modulePath().parent_path();
    if (installDir.empty()) {
        showError(L"Could not resolve install folder.");
        return 1;
    }

    const int answer = MessageBoxW(
        nullptr,
        L"Remove Arachnel from this computer?\n\n"
        L"This deletes:\n"
        L"• Install folder and shortcuts\n"
        L"• AppData / LocalAppData (settings, plugins, caches)\n"
        L"• Game libraries configured in Arachnel\n\n"
        L"This cannot be undone.",
        L"Uninstall Arachnel",
        MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
    if (answer != IDYES)
        return 0;

    terminateArachnelProcesses();

    const fs::path desktop = knownFolder(FOLDERID_Desktop);
    if (!desktop.empty())
        removeShortcutIfExists(desktop / L"Arachnel.lnk");

    const fs::path startMenu = knownFolder(FOLDERID_Programs);
    if (!startMenu.empty()) {
        removeShortcutIfExists(startMenu / L"Arachnel" / L"Arachnel.lnk");
        std::error_code ec;
        fs::remove_all(startMenu / L"Arachnel", ec);
    }

    deleteUninstallRegistry();
    deleteSettingsRegistry();

    const fs::path roaming = knownFolder(FOLDERID_RoamingAppData);
    const fs::path local = knownFolder(FOLDERID_LocalAppData);

    std::vector<fs::path> delayed;
    if (!roaming.empty()) {
        collectLibraryPathsFromSettings(roaming / L"Arachnel" / L"Arachnel" / L"settings.json",
                                        delayed);
        collectLibraryPathsFromSettings(roaming / L"Arachnel" / L"settings.json", delayed);
        collectLibraryPathsFromSettings(roaming / L"PetWork" / L"Arachnel" / L"settings.json",
                                        delayed);
    }
    collectIfExists(delayed, fs::path(L"C:/Games/Arachnel"));

    if (!roaming.empty()) {
        removePathNow(roaming / L"Arachnel");
        removePathNow(roaming / L"PetWork" / L"Arachnel");
    }
    if (!local.empty()) {
        removePathNow(local / L"Arachnel");
        removePathNow(local / L"Arachnel Setup");
        removePathNow(local / L"PetWork" / L"Arachnel");
    }

    std::vector<fs::path> unique;
    for (const fs::path& p : delayed)
        appendUnique(unique, p);
    appendUnique(unique, installDir);

    if (!scheduleFolderRemovals(unique)) {
        showError(L"Could not schedule folder removal. Delete leftover folders manually.");
        return 1;
    }

    MessageBoxW(nullptr,
                L"Arachnel was uninstalled.\n\nApp data and game libraries will finish deleting "
                L"in a few seconds.",
                L"Arachnel", MB_ICONINFORMATION | MB_OK);
    return 0;
}
