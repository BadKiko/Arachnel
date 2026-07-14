#include "win_install_registry.h"

#include <QDir>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace arachnel::setup {

namespace {

constexpr wchar_t kUninstallKeyName[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Arachnel";

bool writeStringValue(HKEY key, const wchar_t* name, const QString& value)
{
    const std::wstring wide = value.toStdWString();
    const DWORD byteSize = static_cast<DWORD>((wide.size() + 1) * sizeof(wchar_t));
    return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(wide.c_str()),
                          byteSize)
           == ERROR_SUCCESS;
}

bool writeDwordValue(HKEY key, const wchar_t* name, DWORD value)
{
    return RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value),
                          sizeof(value))
           == ERROR_SUCCESS;
}

bool createUninstallKey(HKEY root, HKEY* keyOut)
{
    HKEY key = nullptr;
    const LONG result =
        RegCreateKeyExW(root, kUninstallKeyName, 0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS)
        return false;
    *keyOut = key;
    return true;
}

} // namespace

bool registerWindowsUninstall(const QString& installPath, const QString& uninstallExe,
                              const QString& displayVersion, QString* errorOut)
{
#if !defined(Q_OS_WIN)
    Q_UNUSED(installPath);
    Q_UNUSED(uninstallExe);
    Q_UNUSED(displayVersion);
    Q_UNUSED(errorOut);
    return true;
#else
    HKEY key = nullptr;
    HKEY root = HKEY_LOCAL_MACHINE;
    if (!createUninstallKey(root, &key)) {
        root = HKEY_CURRENT_USER;
        if (!createUninstallKey(root, &key)) {
            if (errorOut)
                *errorOut = QStringLiteral("Could not create uninstall registry key");
            return false;
        }
    }

    const QString quotedUninstall = QStringLiteral("\"%1\"").arg(uninstallExe);
    const QString displayIcon =
        QDir(installPath).absoluteFilePath(QStringLiteral("arachnel_app.exe"))
        + QStringLiteral(",0");

    const bool ok = writeStringValue(key, L"DisplayName", QStringLiteral("Arachnel"))
                    && writeStringValue(key, L"DisplayVersion", displayVersion)
                    && writeStringValue(key, L"Publisher", QStringLiteral("PetWork"))
                    && writeStringValue(key, L"InstallLocation", installPath)
                    && writeStringValue(key, L"UninstallString", quotedUninstall)
                    && writeStringValue(key, L"QuietUninstallString", quotedUninstall)
                    && writeStringValue(key, L"DisplayIcon", displayIcon)
                    && writeDwordValue(key, L"NoModify", 1)
                    && writeDwordValue(key, L"NoRepair", 1);

    RegCloseKey(key);

    if (!ok && errorOut)
        *errorOut = QStringLiteral("Could not write uninstall registry values");
    return ok;
#endif
}

bool unregisterWindowsUninstall(QString* errorOut)
{
#if !defined(Q_OS_WIN)
    Q_UNUSED(errorOut);
    return true;
#else
    for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        const LONG result = RegDeleteTreeW(root, kUninstallKeyName);
        if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
            return true;
    }
    if (errorOut)
        *errorOut = QStringLiteral("Could not remove uninstall registry key");
    return false;
#endif
}

} // namespace arachnel::setup
