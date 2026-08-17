#include "launch_controller.h"

#include "crash_log.h"
#include "launch_resolver.h"
#include "file_utils.h"
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
                                   PluginHost* plugins, ProtonManager* protons, Hooks hooks,
                                   QObject* parent)
    : QObject(parent), m_library(library), m_settings(settings), m_plugins(plugins),
      m_protons(protons), m_hooks(std::move(hooks)), m_timer(new QTimer(this))
{
    m_timer->setInterval(1500);
    connect(m_timer, &QTimer::timeout, this, &LaunchController::pollRunningGame);
}

void LaunchController::markRunning(const LibraryGame& game, qint64 processId)
{
    if (m_hooks.touchLastPlayed)
        m_hooks.touchLastPlayed(game.id);
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

    arachnel::logBreadcrumb(QStringLiteral("launch"), gameId);

    const LibraryGame gameCopyBase = *game;
    QTimer::singleShot(0, this, [this, gameId, gameCopyBase]() {
        if (m_library->gameById(gameId) == nullptr)
            return;
        LibraryGame gameCopy = gameCopyBase;
        if (!gameCopy.installPath.isEmpty())
            healWindowsInstallLayout(gameCopy.installPath);
        if (gameCopy.executableOverride.contains(QLatin1Char('\\'))) {
            QString override = gameCopy.executableOverride;
            override.replace(QLatin1Char('\\'), QLatin1Char('/'));
            gameCopy.executableOverride = QFileInfo::exists(override) ? override : QString();
        }
        if (m_hooks.ensureRuntime && !m_hooks.ensureRuntime(gameCopy))
            return;

        LaunchInfo info;
        if (ISourcePlugin* plugin = m_plugins->plugin(gameCopy.sourceId)) {
            // Keep SteamFix.ini / stplug lua in sync with library toggles (toggle alone can race).
            QStringList enabledDlc;
            enabledDlc.reserve(gameCopy.components.size());
            for (const InstalledComponent& component : gameCopy.components) {
                if (component.installed && component.enabled)
                    enabledDlc.append(component.id);
            }
            if (!plugin->applySelectedDlc(gameCopy, enabledDlc) && m_hooks.notice) {
                m_hooks.notice(QCoreApplication::translate("Core", "Couldn't update DLC unlocks."));
            }
            info = plugin->launchInfo(gameCopy);
        }
        if (info.executable.isEmpty() && gameCopy.executableOverride.isEmpty())
            info.executable = findGameExecutableInTree(gameCopy.installPath);
        applyOnlineFixLaunchInfo(gameCopy.installPath, &info);
#if defined(Q_OS_LINUX)
        if (detectOnlineFixOverlay(gameCopy.installPath).enabled
            || info.environmentExtras.value(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"))
                   == QStringLiteral("legacy")
            || info.environmentExtras.value(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"))
                   == QStringLiteral("1")) {
            if (!isSteamClientRunning()) {
                tryStartSteamClient();
                if (m_hooks.notice) {
                    m_hooks.notice(QCoreApplication::translate(
                        "Core",
                        "Steam is not running - Online Fix needs it for SpaceWar/overlay. Starting Steam…"));
                }
            }
        }
#endif
        const ResolvedLaunch resolved = resolveLaunch(info, gameCopy, *m_settings, m_protons);
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
    arachnel::logBreadcrumb(QStringLiteral("stop"), m_gameId);
    if (m_processId <= 0 || ProcessTracker::terminateProcess(m_processId))
        QTimer::singleShot(0, this, &LaunchController::clearRunning);
    else if (m_hooks.notice)
        m_hooks.notice(QCoreApplication::translate("Core", "Failed to stop game"));
}

} // namespace arachnel::core
