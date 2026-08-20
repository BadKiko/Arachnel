#include "core_controller_impl.h"

#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QStandardPaths>

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

void CoreController::copyGameLaunchLog()
{
    const QString text = gameLaunchLog();
    if (QGuiApplication* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        if (QClipboard* clipboard = gui->clipboard())
            clipboard->setText(text);
    }
    showNotice(QCoreApplication::translate("Core", "Launch log copied"));
}

void CoreController::saveGameLaunchLog()
{
    const QString text = gameLaunchLog();
    if (text.isEmpty())
        return;
    const QString defaultPath =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/log.txt");
    const QString target = QFileDialog::getSaveFileName(
        nullptr, QCoreApplication::translate("Core", "Save launch log"), defaultPath,
        QCoreApplication::translate("Core", "Text files (*.txt);;All files (*)"));
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
