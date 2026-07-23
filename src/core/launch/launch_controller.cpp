#include "launch_controller.h"

#include "launch_resolver.h"
#include "install_heuristics.h"
#include "online_fix_overlay.h"
#include "plugin_host.h"
#include "plugin_interface.h"
#include "process_launcher.h"
#include "process_tracker.h"
#include "proton_manager.h"
#include "settings_store.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QTimer>

namespace arachnel::core {

LaunchController::LaunchController(LibraryModel* library, SettingsStore* settings,
                                   PluginHost* plugins, Hooks hooks, QObject* parent)
    : QObject(parent), m_library(library), m_settings(settings), m_plugins(plugins),
      m_hooks(std::move(hooks)), m_timer(new QTimer(this))
{
    m_timer->setInterval(1500);
    connect(m_timer, &QTimer::timeout, this, &LaunchController::pollRunningGame);
}

void LaunchController::markRunning(const LibraryGame& game, qint64 processId)
{
    m_gameId = game.id;
    m_gameTitle = game.title;
    m_gameCoverUrl = game.coverUrl;
    m_processId = processId;
    emit runningGameChanged();
    processId > 0 ? m_timer->start() : m_timer->stop();
}

void LaunchController::clearRunning()
{
    if (m_gameId.isEmpty())
        return;
    m_gameId.clear();
    m_gameTitle.clear();
    m_gameCoverUrl.clear();
    m_processId = 0;
    m_timer->stop();
    emit runningGameChanged();
}

void LaunchController::pollRunningGame()
{
    if (m_processId > 0 && !ProcessTracker::isProcessRunning(m_processId))
        QTimer::singleShot(0, this, &LaunchController::clearRunning);
}

void LaunchController::launchGame(const QString& gameId)
{
    const LibraryGame* game = m_library->gameById(gameId);
    if (!game || game->installPath.isEmpty()) {
        if (m_hooks.notice)
            m_hooks.notice(QCoreApplication::translate("Core", "Game is not installed yet"));
        return;
    }
    if (gameRunning() && m_gameId == gameId)
        return;

    // Leave QML Button.onClicked before any blocking runtime work (steamcmd / installers).
    // Nested event loops on the GUI thread abort with QML "object destroyed while handler
    // is in progress" (seen on Linux AppImage when launching via GameDetailsContent).
    const LibraryGame gameCopy = *game;
    QTimer::singleShot(0, this, [this, gameId, gameCopy]() {
        if (m_library->gameById(gameId) == nullptr)
            return;
        if (m_hooks.ensureRuntime && !m_hooks.ensureRuntime(gameCopy))
            return;

        LaunchInfo info;
        if (ISourcePlugin* plugin = m_plugins->plugin(gameCopy.sourceId))
            info = plugin->launchInfo(gameCopy);
        if (info.executable.isEmpty() && gameCopy.executableOverride.isEmpty())
            info.executable = findGameExecutableInTree(gameCopy.installPath);
        applyOnlineFixLaunchInfo(gameCopy.installPath, &info);
#if defined(Q_OS_LINUX)
        if (detectOnlineFixOverlay(gameCopy.installPath).enabled
            || info.environmentExtras.value(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"))
                   == QStringLiteral("1")) {
            if (!isSteamClientRunning()) {
                tryStartSteamClient();
                if (m_hooks.notice) {
                    m_hooks.notice(QCoreApplication::translate(
                        "Core",
                        "Steam must be running for Online Fix. Starting Steam — launch the game again once it is open."));
                }
                return;
            }
            ProtonManager protonMgr;
            if (protonMgr.findSteamLinuxRuntime().isEmpty() && m_hooks.notice) {
                m_hooks.notice(QCoreApplication::translate(
                    "Core",
                    "Steam Linux Runtime (Sniper) not found. Install it from Steam for Online Fix overlay support."));
            }
        }
#endif
        const ResolvedLaunch resolved = resolveLaunch(info, gameCopy, *m_settings);
        if (resolved.program.isEmpty()) {
            if (m_hooks.notice)
                m_hooks.notice(QCoreApplication::translate("Core", "Executable not found for %1")
                                   .arg(gameCopy.title));
            return;
        }
        QString error;
        qint64 processId = 0;
        if (!ProcessLauncher::launch(resolved, &error, &processId)) {
            if (m_hooks.notice) {
                m_hooks.notice(error.isEmpty()
                                   ? QCoreApplication::translate("Core", "Failed to launch game")
                                   : error);
            }
            return;
        }
        QTimer::singleShot(0, this, [this, gameCopy, processId]() {
            markRunning(gameCopy, processId);
        });
    });
}

void LaunchController::stopRunningGame()
{
    if (!gameRunning())
        return;
    if (m_processId <= 0 || ProcessTracker::terminateProcess(m_processId))
        QTimer::singleShot(0, this, &LaunchController::clearRunning);
    else if (m_hooks.notice)
        m_hooks.notice(QCoreApplication::translate("Core", "Failed to stop game"));
}

} // namespace arachnel::core
