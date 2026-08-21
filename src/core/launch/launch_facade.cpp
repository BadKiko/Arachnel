#include "core_controller_impl.h"

#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#if !defined(Q_OS_WIN)
#include <QFileDialog>
#endif

namespace arachnel::core {

void CoreController::touchLastPlayed(const QString& gameId)
{
    if (gameId.isEmpty())
        return;

    const LibraryGame* existing = m_libraryStore.gameById(gameId);
    if (!existing)
        return;

    LibraryGame game = *existing;
    game.lastPlayedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_libraryStore.upsertGame(game);
    enrichLibraryGameCover(game);
    // Do not beginResetModel() here - launch/stop run from QML Button onClicked.
    if (!m_library.replaceGame(game))
        syncLibraryFromStore();
}

void CoreController::launchGame(const QString& gameId)
{
    if (m_launchController)
        m_launchController->launchGame(gameId);
}

void CoreController::stopRunningGame()
{
    if (m_launchController)
        m_launchController->stopRunningGame();
}

QString CoreController::gameLaunchLog() const
{
    return m_launchController ? m_launchController->launchLogText() : QString();
}

QString CoreController::gameLaunchLog(const QString& gameId) const
{
    return m_launchController ? m_launchController->launchLogText(gameId) : QString();
}

bool CoreController::hasGameLaunchLog(const QString& gameId) const
{
    return m_launchController && m_launchController->hasLaunchLog(gameId);
}

void CoreController::copyGameLaunchLog()
{
    copyGameLaunchLog(QString());
}

void CoreController::copyGameLaunchLog(const QString& gameId)
{
    const QString text = gameId.isEmpty() ? gameLaunchLog() : gameLaunchLog(gameId);
    if (QGuiApplication* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        if (QClipboard* clipboard = gui->clipboard())
            clipboard->setText(text);
    }
    showNotice(QCoreApplication::translate("Core", "Launch log copied"));
}

void CoreController::saveGameLaunchLog()
{
    saveGameLaunchLog(QString());
}

void CoreController::saveGameLaunchLog(const QString& gameId)
{
    const QString text = gameId.isEmpty() ? gameLaunchLog() : gameLaunchLog(gameId);
    if (text.isEmpty()
        || text == QCoreApplication::translate("Core", "No launch has been attempted for this game yet."))
        return;
    const QString defaultPath =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/log.txt");

    QString target;
#if defined(Q_OS_WIN)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = hr == S_OK;

    IFileSaveDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        const COMDLG_FILTERSPEC filters[] = {
            {L"Text files (*.txt)", L"*.txt"},
            {L"All files (*.*)", L"*.*"},
        };
        dialog->SetFileTypes(2, filters);
        dialog->SetDefaultExtension(L"txt");
        const QString title = QCoreApplication::translate("Core", "Save launch log");
        dialog->SetTitle(reinterpret_cast<LPCWSTR>(title.utf16()));

        const QFileInfo info(defaultPath);
        const QString folderPath = info.absolutePath();
        const QString fileName = info.fileName();
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(folderPath.utf16()),
                                                  nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetDefaultFolder(folder);
            dialog->SetFolder(folder);
            folder->Release();
        }
        dialog->SetFileName(reinterpret_cast<LPCWSTR>(fileName.utf16()));

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    target = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();
#else
    target = QFileDialog::getSaveFileName(
        nullptr, QCoreApplication::translate("Core", "Save launch log"), defaultPath,
        QCoreApplication::translate("Core", "Text files (*.txt);;All files (*)"));
#endif
    if (target.isEmpty())
        return;

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        showNotice(QCoreApplication::translate("Core", "Could not save log: %1")
                       .arg(file.errorString()));
        return;
    }
    file.write(text.toUtf8());
    file.close();
    showNotice(QCoreApplication::translate("Core", "Launch log saved to %1").arg(target));
}

} // namespace arachnel::core
