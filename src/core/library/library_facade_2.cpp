#include "core_controller_impl.h"

namespace arachnel::core {

QString CoreController::browseGameExecutable(const QString& currentPath)
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        const COMDLG_FILTERSPEC filters[] = {
            {L"Executables (*.exe)", L"*.exe"},
            {L"All files (*.*)", L"*.*"},
        };
        dialog->SetFileTypes(2, filters);
        dialog->SetTitle(L"Choose game executable");

        if (!currentPath.isEmpty()) {
            IShellItem* folder = nullptr;
            const QString folderPath = QFileInfo(currentPath).absolutePath();
            if (SUCCEEDED(
                    SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(folderPath.utf16()),
                                                nullptr, IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
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
    return QFileDialog::getOpenFileName(
        nullptr, QCoreApplication::translate("Core", "Choose game executable"),
        currentPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                              : QFileInfo(currentPath).absolutePath(),
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

} // namespace arachnel::core
