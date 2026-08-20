#include "core_controller_impl.h"

#include "desktop_shortcut_service.h"
#include "game_launch_target.h"
#include "steam_shortcut_service.h"

namespace arachnel::core {

namespace {

QString resolveBrowseStartDir(const QString& currentPath, const QString& preferredDir)
{
    const auto existingDir = [](const QString& path) -> QString {
        if (path.isEmpty())
            return {};
        const QFileInfo fi(path);
        if (fi.isDir() && fi.exists())
            return QDir::toNativeSeparators(fi.absoluteFilePath());
        if (fi.isFile() && fi.exists())
            return QDir::toNativeSeparators(fi.absolutePath());
        const QString parent = QDir::toNativeSeparators(fi.absolutePath());
        if (!parent.isEmpty() && QFileInfo(parent).isDir())
            return parent;
        return {};
    };

    if (const QString fromCurrent = existingDir(currentPath); !fromCurrent.isEmpty())
        return fromCurrent;
    if (const QString fromPreferred = existingDir(preferredDir); !fromPreferred.isEmpty())
        return fromPreferred;
    return {};
}

#if defined(Q_OS_WIN)
void setDialogStartFolder(IFileDialog* dialog, const QString& folderPath)
{
    if (!dialog || folderPath.isEmpty())
        return;
    IShellItem* folder = nullptr;
    if (FAILED(SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(folderPath.utf16()),
                                           nullptr, IID_PPV_ARGS(&folder)))) {
        return;
    }
    // Both: SetDefaultFolder alone loses to the last-used folder; SetFolder forces it.
    dialog->SetDefaultFolder(folder);
    dialog->SetFolder(folder);
    folder->Release();
}
#endif

} // namespace

QString CoreController::browseGameExecutable(const QString& currentPath,
                                             const QString& preferredDir)
{
    const QString startDir = resolveBrowseStartDir(currentPath, preferredDir);
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = hr == S_OK;

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        const COMDLG_FILTERSPEC filters[] = {
            {L"Executables (*.exe)", L"*.exe"},
            {L"All files (*.*)", L"*.*"},
        };
        dialog->SetFileTypes(2, filters);
        dialog->SetTitle(L"Choose game executable");
        setDialogStartFolder(dialog, startDir);

        const QFileInfo currentInfo(currentPath);
        if (currentInfo.isFile() && currentInfo.exists())
            dialog->SetFileName(reinterpret_cast<LPCWSTR>(currentInfo.fileName().utf16()));

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    path = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();
    return path;
#else
    const QString start =
        !startDir.isEmpty()
            ? startDir
            : QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return QFileDialog::getOpenFileName(
        nullptr, QCoreApplication::translate("Core", "Choose game executable"), start,
        QCoreApplication::translate("Core", "Executables (*.exe *.sh *.x86_64);;All files (*)"));
#endif
}

QString CoreController::browseStorageFolder()
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)))
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        {
            const QString title =
                QCoreApplication::translate("Core", "Choose library folder");
            dialog->SetTitle(reinterpret_cast<LPCWSTR>(title.utf16()));
        }

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    path = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();
    return path;
#else
    return QFileDialog::getExistingDirectory(
        nullptr,
        QCoreApplication::translate("Core", "Choose library folder"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
#endif
}

void CoreController::removeGame(const QString& gameId, bool deleteFiles)
{
    if (m_libraryController)
        m_libraryController->removeGame(gameId, deleteFiles);
}

void CoreController::removeEntry(const QString& entryId, bool deleteFiles)
{
    if (m_libraryController)
        m_libraryController->removeEntry(entryId, deleteFiles);
}

void CoreController::moveGame(const QString& gameId, const QString& targetLibraryId)
{
    if (m_libraryController)
        m_libraryController->moveGame(gameId, targetLibraryId);
}

QVariantList CoreController::gamesOnLibrary(const QString& libraryId) const
{
    return m_libraryController ? m_libraryController->gamesOnLibrary(libraryId) : QVariantList();
}

bool CoreController::removeStorageLibrary(const QString& libraryId, bool force)
{
    return m_libraryController && m_libraryController->removeStorageLibrary(libraryId, force);
}

int CoreController::scanInstalledGames()
{
    if (!m_libraryController)
        return 0;
    const int added = m_libraryController->scanInstalledGames();
    if (added > 0) {
        showNotice(QCoreApplication::translate("Core", "Found %1 game(s) on disk").arg(added));
        if (m_gameUpdates)
            m_gameUpdates->recalculateLibraryUpdates(false);
    } else {
        showNotice(QCoreApplication::translate("Core", "No new games found on disk"));
    }
    return added;
}

void CoreController::checkUpdates()
{
    if (m_gameUpdates)
        m_gameUpdates->checkUpdates(!m_catalogCache.isEmpty());
}

GameLaunchTarget CoreController::resolveShortcutTarget(const LibraryGame& game) const
{
    LaunchInfo info;
    if (m_pluginHost) {
        if (ISourcePlugin* plugin = m_pluginHost->plugin(game.sourceId))
            info = plugin->launchInfo(game);
    }
    return resolveGameLaunchTarget(game, info, m_settings);
}

void CoreController::createGameDesktopShortcut(const QString& gameId)
{
#if !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)
    showNotice(QCoreApplication::translate("Core", "Shortcuts are not supported on this platform"));
    return;
#else
    const LibraryGame* game = m_libraryStore.gameById(gameId);
    if (!game) {
        showNotice(QCoreApplication::translate("Core", "Game not found"));
        return;
    }
    const GameLaunchTarget target = resolveShortcutTarget(*game);
    if (target.executable.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Executable not found for %1").arg(game->title));
        return;
    }
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Desktop folder not found"));
        return;
    }
#if defined(Q_OS_WIN)
    const QString ext = QStringLiteral(".lnk");
#else
    const QString ext = QStringLiteral(".desktop");
#endif
    const QString linkPath =
        desktop + QLatin1Char('/') + sanitizeShortcutFileName(target.title) + ext;
    QString error;
    if (!createOsShortcut(linkPath, target, &error)) {
        showNotice(error.isEmpty()
                       ? QCoreApplication::translate("Core", "Failed to create desktop shortcut")
                       : error);
        return;
    }
    showNotice(QCoreApplication::translate("Core", "Desktop shortcut created"));
#endif
}

void CoreController::createGameStartMenuShortcut(const QString& gameId)
{
#if !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)
    showNotice(QCoreApplication::translate("Core", "Shortcuts are not supported on this platform"));
    return;
#else
    const LibraryGame* game = m_libraryStore.gameById(gameId);
    if (!game) {
        showNotice(QCoreApplication::translate("Core", "Game not found"));
        return;
    }
    const GameLaunchTarget target = resolveShortcutTarget(*game);
    if (target.executable.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Executable not found for %1").arg(game->title));
        return;
    }
    const QString startMenu =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)
        + QStringLiteral("/Arachnel Games");
#if defined(Q_OS_WIN)
    const QString ext = QStringLiteral(".lnk");
#else
    const QString ext = QStringLiteral(".desktop");
#endif
    const QString linkPath =
        startMenu + QLatin1Char('/') + sanitizeShortcutFileName(target.title) + ext;
    QString error;
    if (!createOsShortcut(linkPath, target, &error)) {
        showNotice(error.isEmpty()
                       ? QCoreApplication::translate("Core", "Failed to create Start menu shortcut")
                       : error);
        return;
    }
    showNotice(QCoreApplication::translate("Core", "Start menu shortcut created"));
#endif
}

void CoreController::addGameToSteam(const QString& gameId)
{
#if !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)
    showNotice(QCoreApplication::translate("Core", "Adding to Steam is not supported on this platform"));
    return;
#else
    const LibraryGame* game = m_libraryStore.gameById(gameId);
    if (!game) {
        showNotice(QCoreApplication::translate("Core", "Game not found"));
        return;
    }
    const GameLaunchTarget target = resolveShortcutTarget(*game);
    if (target.executable.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Executable not found for %1").arg(game->title));
        return;
    }

    SteamShortcutRequest req;
    req.target = target;
    req.steamAppId = game->steamAppId;
    if (game->coverUrl.startsWith(QStringLiteral("file:")))
        req.coverFileUrl = game->coverUrl;

    const SteamShortcutResult result = addOrUpdateSteamShortcut(req);
    if (!result.ok) {
        QString message = result.error;
        if (message == QLatin1String("Steam userdata not found"))
            message = QCoreApplication::translate("Core", "Steam userdata not found");
        else if (message == QLatin1String("Executable not found"))
            message = QCoreApplication::translate("Core", "Executable not found for %1").arg(game->title);
        else if (message == QLatin1String("Could not parse shortcuts.vdf"))
            message = QCoreApplication::translate("Core", "Could not parse Steam shortcuts.vdf");
        else if (message == QLatin1String("Could not read shortcuts.vdf"))
            message = QCoreApplication::translate("Core", "Could not read Steam shortcuts.vdf");
        else if (message == QLatin1String("Could not write shortcuts.vdf"))
            message = QCoreApplication::translate("Core", "Could not write Steam shortcuts.vdf");
        showNotice(message);
        return;
    }
    showNotice(QCoreApplication::translate(
        "Core", "Added to Steam. Restart Steam to see the game and artwork."));
#endif
}

} // namespace arachnel::core
