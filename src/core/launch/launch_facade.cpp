#include "core_controller_impl.h"

#include "online_fix_overlay.h"

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
    // Do not beginResetModel() here — launch/stop run from QML Button onClicked.
    if (!m_library.replaceGame(game))
        syncLibraryFromStore();
}

void CoreController::markGameRunning(const LibraryGame& game, const qint64 processId)
{
    touchLastPlayed(game.id);
    m_runningGameId = game.id;
    m_runningGameTitle = game.title;
    m_runningGameCoverUrl = game.coverUrl;
    m_runningProcessId = processId;
    arachnel::logDiagnostic(QStringLiteral("Game launched: %1 pid=%2")
                                .arg(game.title)
                                .arg(processId));
    emit runningGameChanged();

    if (processId > 0)
        m_runningGameTimer->start();
    else
        m_runningGameTimer->stop();
}

void CoreController::clearRunningGame()
{
    if (m_runningGameId.isEmpty())
        return;

    const QString endedId = m_runningGameId;
    const QString endedTitle = m_runningGameTitle;
    m_runningGameId.clear();
    m_runningGameTitle.clear();
    m_runningGameCoverUrl.clear();
    m_runningProcessId = 0;
    m_runningGameTimer->stop();
    arachnel::logDiagnostic(QStringLiteral("Game session ended: %1 (%2)").arg(endedTitle, endedId));
    emit runningGameChanged();
}

void CoreController::pollRunningGame()
{
    if (m_runningGameId.isEmpty())
        return;

    if (m_runningProcessId <= 0)
        return;

    if (!ProcessTracker::isProcessRunning(m_runningProcessId)) {
        // Defer UI teardown so we do not re-enter QML/layout from QTimer while the
        // game process is still unwinding (avoids intermittent crashes on Windows).
        const QString endedGameId = m_runningGameId;
        QTimer::singleShot(0, this, [this, endedGameId]() {
            if (m_runningGameId != endedGameId)
                return;
            arachnel::logDiagnostic(
                QStringLiteral("Running game process ended: %1").arg(endedGameId));
            clearRunningGame();
        });
    }
}

void CoreController::launchGame(const QString& gameId)
{
    if (m_launchController)
        m_launchController->launchGame(gameId);
}

void CoreController::launchGameAfterRuntimeSetup(const QString& gameId)
{
    const LibraryGame* game = m_library.gameById(gameId);
    if (!game)
        return;

    LaunchInfo info;
    if (ISourcePlugin* plugin = m_pluginHost->plugin(game->sourceId))
        info = plugin->launchInfo(*game);

    if (info.executable.isEmpty() && game->executableOverride.trimmed().isEmpty()) {
        const QString found = findGameExecutableInTree(game->installPath);
        if (!found.isEmpty())
            info.executable = found;
    }

    applyOnlineFixLaunchInfo(game->installPath, &info);
#if defined(Q_OS_LINUX)
    if (detectOnlineFixOverlay(game->installPath).enabled
        && !info.environmentExtras.contains(QStringLiteral("LD_PRELOAD"))) {
        showNotice(QCoreApplication::translate(
            "Core",
            "Start the Steam client before launching Online Fix games so the Steam overlay can attach."));
    }
#endif

    const ResolvedLaunch resolved = resolveLaunch(info, *game, m_settings);
    if (resolved.program.isEmpty()) {
#if defined(Q_OS_LINUX)
        if (!info.executable.isEmpty()
            && info.executable.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)
            && m_protonManager) {
            const QString protonId =
                m_settings.resolvedProtonId(game->protonId, *m_protonManager);
            if (m_protonManager->executableForId(protonId).isEmpty()) {
                showNotice(QCoreApplication::translate(
                    "Core", "Proton not found. Install Proton-GE in Settings → Launch."));
                return;
            }
        }
#endif
        showNotice(
            QCoreApplication::translate("Core", "Executable not found for %1").arg(game->title));
        return;
    }

    QString error;
    qint64 processId = 0;
    if (ProcessLauncher::launch(resolved, &error, &processId)) {
        // Defer RunningGameBar Loader + library bindings so we do not re-enter QML
        // layout from inside Button onClicked (intermittent AV in Qt6Core on Windows).
        const LibraryGame launched = *game;
        QTimer::singleShot(0, this, [this, launched, processId]() {
            markGameRunning(launched, processId);
        });
        return;
    }
    showNotice(error.isEmpty() ? QCoreApplication::translate("Core", "Failed to launch game") : error);
}

void CoreController::stopRunningGame()
{
    if (m_launchController)
        m_launchController->stopRunningGame();
}

} // namespace arachnel::core
