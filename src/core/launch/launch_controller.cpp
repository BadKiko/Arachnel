#include "launch_controller.h"

#include "crash_log.h"
#include "file_utils.h"
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
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
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
    logLine(QCoreApplication::translate("Core", "Game process started (PID %1)")
                .arg(processId > 0 ? QString::number(processId)
                                   : QCoreApplication::translate("Core", "n/a")));
    emit runningGameChanged();
    processId > 0 ? m_timer->start() : m_timer->stop();
}

void LaunchController::clearRunning()
{
    if (m_gameId.isEmpty())
        return;
    logLine(QCoreApplication::translate("Core", "Game process exited"));
    m_gameId.clear();
    m_gameTitle.clear();
    m_gameCoverUrl.clear();
    m_processId = 0;
    m_timer->stop();
    emit runningGameChanged();
}

void LaunchController::logLine(const QString& line)
{
    m_launchLogLines.append(line);
    arachnel::logDiagnostic(
        QStringLiteral("[launch:%1] %2").arg(m_logGameId.isEmpty() ? m_gameId : m_logGameId, line));
}

QString LaunchController::launchLogFilePath() const
{
    QString id = m_logGameId.isEmpty() ? m_gameId : m_logGameId;
    if (id.isEmpty())
        return {};
    for (QChar& ch : id) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('-') && ch != QLatin1Char('_'))
            ch = QLatin1Char('_');
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/launch-%1.log").arg(id);
}

QString LaunchController::launchLogText() const
{
    QString text = m_launchLogLines.join(QLatin1Char('\n'));
    const QString capturePath = launchLogFilePath();
    if (!capturePath.isEmpty() && QFileInfo::exists(capturePath)) {
        QFile file(capturePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString output = QString::fromUtf8(file.readAll()).trimmed();
            if (!output.isEmpty()) {
                if (!text.isEmpty())
                    text += QLatin1String("\n\n");
                text += QCoreApplication::translate("Core", "--- Game output (%1) ---")
                            .arg(QFileInfo(capturePath).fileName());
                text += QLatin1Char('\n');
                text += output;
            }
        }
    }
    if (text.isEmpty())
        text = QCoreApplication::translate("Core", "No launch has been attempted for this game yet.");
    return text;
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

    m_logGameId = gameId;
    m_launchLogLines.clear();
    logLine(QCoreApplication::translate("Core", "Launching %1 (%2)").arg(game->title, gameId));
    logLine(QCoreApplication::translate("Core", "Install path: %1").arg(game->installPath));
    logLine(QCoreApplication::translate("Core", "Source: %1").arg(game->sourceId));

    const LibraryGame gameCopyBase = *game;
    QTimer::singleShot(0, this, [this, gameId, gameCopyBase]() {
        if (m_library->gameById(gameId) == nullptr)
            return;
        LibraryGame gameCopy = gameCopyBase;
        if (!gameCopy.installPath.isEmpty())
            healWindowsInstallLayout(gameCopy.installPath);
        if (m_protons) {
            const int repairedPrefixes = m_protons->repairCorruptPrefixForGame(gameCopy.id);
            if (repairedPrefixes > 0)
                logLine(QCoreApplication::translate(
                            "Core",
                            "Repaired %1 corrupted Proton prefix director%2 before launch")
                            .arg(repairedPrefixes)
                            .arg(repairedPrefixes == 1 ? QStringLiteral("y") : QStringLiteral("ies")));
        }
        if (gameCopy.executableOverride.contains(QLatin1Char('\\'))) {
            QString override = gameCopy.executableOverride;
            override.replace(QLatin1Char('\\'), QLatin1Char('/'));
            gameCopy.executableOverride = QFileInfo::exists(override) ? override : QString();
        }
        if (!gameCopy.executableOverride.isEmpty())
            logLine(QCoreApplication::translate("Core", "Executable override: %1")
                        .arg(gameCopy.executableOverride));
        if (m_hooks.ensureRuntime && !m_hooks.ensureRuntime(gameCopy)) {
            logLine(QCoreApplication::translate("Core", "Failed to prepare runtime (Proton/Wine)"));
            return;
        }

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
            logLine(QCoreApplication::translate("Core", "Online Fix overlay detected"));
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
            const QString reason = QCoreApplication::translate(
                "Core",
                "Could not resolve a launch command (missing Proton or game executable).");
            logLine(reason);
            if (m_hooks.notice)
                m_hooks.notice(QCoreApplication::translate("Core", "Executable not found for %1")
                                   .arg(gameCopy.title));
            return;
        }
        logLine(QCoreApplication::translate("Core", "Program: %1").arg(resolved.program));
        logLine(QCoreApplication::translate("Core", "Working dir: %1").arg(resolved.workingDirectory));
        logLine(QCoreApplication::translate("Core", "Args: %1")
                    .arg(resolved.arguments.join(QLatin1Char(' '))));

        QString error;
        qint64 processId = 0;
        if (!ProcessLauncher::launch(resolved, &error, &processId, launchLogFilePath())) {
            logLine(QCoreApplication::translate("Core", "Failed to start process: %1")
                        .arg(error.isEmpty()
                                 ? QCoreApplication::translate("Core", "unknown error")
                                 : error));
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
