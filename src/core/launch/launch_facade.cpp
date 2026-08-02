#include "core_controller_impl.h"

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

} // namespace arachnel::core
