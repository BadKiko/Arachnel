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
#include "steam_api_provision.h"
#include "steamless_service.h"
#include "wine_error_probe.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

namespace arachnel::core {

namespace {
constexpr int kPollIntervalMs = 1500;
constexpr int kOnlineFixPollIntervalMs = 400;
constexpr int kOnlineFixWatchMs = 20000;
constexpr int kOnlineFixEarlyExitMs = 12000;
} // namespace

LaunchController::LaunchController(LibraryModel* library, SettingsStore* settings,
                                   PluginHost* plugins, ProtonManager* protons,
                                   SteamlessService* steamless, Hooks hooks, QObject* parent)
    : QObject(parent), m_library(library), m_settings(settings), m_plugins(plugins),
      m_protons(protons), m_steamless(steamless), m_hooks(std::move(hooks)),
      m_timer(new QTimer(this))
{
    m_timer->setInterval(kPollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &LaunchController::pollRunningGame);
}

void LaunchController::markRunning(const LibraryGame& game, qint64 processId,
                                   bool watchingOnlineFix, const WineErrorWatchHints& watchHints)
{
    if (m_hooks.touchLastPlayed)
        m_hooks.touchLastPlayed(game.id);
    m_gameId = game.id;
    m_gameTitle = game.title;
    m_gameCoverUrl = game.coverUrl;
    m_processId = processId;
    m_launchStartedAt = QDateTime::currentDateTime();
    m_watchingOnlineFix = watchingOnlineFix;
    m_watchHints = watchHints;
    m_sawGameExecutable = false;
    m_onlineFixWatchUntil =
        watchingOnlineFix ? m_launchStartedAt.addMSecs(kOnlineFixWatchMs) : QDateTime();
    m_timer->setInterval(watchingOnlineFix ? kOnlineFixPollIntervalMs : kPollIntervalMs);
    logLine(QCoreApplication::translate("Core", "Game process started (PID %1)")
                .arg(processId > 0 ? QString::number(processId)
                                   : QCoreApplication::translate("Core", "n/a")));
    emit runningGameChanged();
    processId > 0 ? m_timer->start() : m_timer->stop();
}

void LaunchController::terminateTrackedLaunch()
{
    if (m_processId > 0) {
        for (qint64 pid : relatedLaunchPids(m_processId, m_watchHints)) {
            if (pid > 0)
                ProcessTracker::terminateProcess(pid);
        }
        ProcessTracker::terminateProcess(m_processId);
    }
}

void LaunchController::clearRunning(bool allowOnlineFixFallback, bool suppressQuickExitLog)
{
    if (m_gameId.isEmpty())
        return;
    logLine(QCoreApplication::translate("Core", "Game process exited"));
    const QString endedId = m_gameId;
    const qint64 elapsedMs =
        m_launchStartedAt.isValid() ? m_launchStartedAt.msecsTo(QDateTime::currentDateTime()) : 0;
    const bool earlyOfExit = allowOnlineFixFallback && m_watchingOnlineFix
        && !m_onlineFixFallbackUsed && elapsedMs >= 0 && elapsedMs < kOnlineFixEarlyExitMs;
    m_gameId.clear();
    m_gameTitle.clear();
    m_gameCoverUrl.clear();
    m_processId = 0;
    m_launchStartedAt = {};
    m_watchingOnlineFix = false;
    m_onlineFixWatchUntil = {};
    m_watchHints = {};
    m_sawGameExecutable = false;
    m_timer->setInterval(kPollIntervalMs);
    m_timer->stop();
    emit runningGameChanged();
    // Suppress log when OF auto-retry will kick in (dialog path or silent early exit).
    emit launchSessionEnded(endedId, elapsedMs, suppressQuickExitLog || earlyOfExit);
    if (earlyOfExit) {
        QTimer::singleShot(0, this, [this, endedId]() {
            handleOnlineFixLaunchFailure(
                endedId,
                QCoreApplication::translate("Core", "Online Fix quit right after launch"));
        });
    }
}

void LaunchController::handleOnlineFixLaunchFailure(const QString& gameId, const QString& reason)
{
    if (gameId.isEmpty() || m_onlineFixFallbackUsed)
        return;
    m_onlineFixFallbackUsed = true;
    logLine(reason);
    logLine(QCoreApplication::translate(
        "Core", "Disabling Online Fix and launching without it"));

    if (m_hooks.setOnlineFixEnabled)
        m_hooks.setOnlineFixEnabled(gameId, false);

    if (m_hooks.notice) {
        m_hooks.notice(QCoreApplication::translate(
            "Core",
            "Online Fix failed to start this game - launched without it. "
            "You can turn Online Fix back on in game settings."));
    }

    terminateTrackedLaunch();
    clearRunning(false, true);
    m_relaunchWithoutOnlineFix = true;
    QTimer::singleShot(350, this, [this, gameId]() { launchGame(gameId); });
}

void LaunchController::logLine(const QString& line)
{
    m_launchLogLines.append(line);
    arachnel::logDiagnostic(
        QStringLiteral("[launch:%1] %2").arg(m_logGameId.isEmpty() ? m_gameId : m_logGameId, line));
}

QString LaunchController::launchLogFilePath() const
{
    return launchLogFilePath(m_logGameId.isEmpty() ? m_gameId : m_logGameId);
}

QString LaunchController::launchLogFilePath(const QString& gameId) const
{
    QString id = gameId.trimmed();
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
    return launchLogText(m_logGameId.isEmpty() ? m_gameId : m_logGameId);
}

QString LaunchController::launchLogText(const QString& gameId) const
{
    QString text;
    if (!gameId.isEmpty() && gameId == m_logGameId)
        text = m_launchLogLines.join(QLatin1Char('\n'));
    else if (gameId.isEmpty())
        text = m_launchLogLines.join(QLatin1Char('\n'));

    const QString capturePath = launchLogFilePath(gameId);
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

bool LaunchController::hasLaunchLog(const QString& gameId) const
{
    if (gameId.isEmpty())
        return false;
    if (gameId == m_logGameId && !m_launchLogLines.isEmpty())
        return true;
    const QString path = launchLogFilePath(gameId);
    return !path.isEmpty() && QFileInfo::exists(path) && QFileInfo(path).size() > 0;
}

void LaunchController::pollRunningGame()
{
    if (m_processId <= 0)
        return;

    if (m_watchingOnlineFix && !m_onlineFixFallbackUsed && m_onlineFixWatchUntil.isValid()
        && QDateTime::currentDateTime() <= m_onlineFixWatchUntil) {
        if (wineErrorDialogVisible(m_processId, m_watchHints)) {
            const QString gameId = m_gameId;
            handleOnlineFixLaunchFailure(
                gameId, QCoreApplication::translate("Core", "Online Fix showed an error dialog"));
            return;
        }

        // Proton often reparents the game off the launch pid. If we saw the exe and it
        // vanishes (silent crash, no MessageBox), fall back the same way.
        if (relatedGameExecutableAlive(m_processId, m_watchHints)) {
            m_sawGameExecutable = true;
        } else if (m_sawGameExecutable) {
            const QString gameId = m_gameId;
            handleOnlineFixLaunchFailure(
                gameId,
                QCoreApplication::translate("Core", "Online Fix quit right after launch"));
            return;
        }
    }

    if (m_watchingOnlineFix && m_onlineFixWatchUntil.isValid()
        && QDateTime::currentDateTime() > m_onlineFixWatchUntil) {
        m_watchingOnlineFix = false;
        m_timer->setInterval(kPollIntervalMs);
    }

    if (!ProcessTracker::isProcessRunning(m_processId))
        QTimer::singleShot(0, this, [this]() { clearRunning(true); });
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

    const bool fromOfFallback = m_relaunchWithoutOnlineFix;
    m_relaunchWithoutOnlineFix = false;
    // Fresh Play click can retry OF fallback; auto relaunch after disable must not.
    if (!fromOfFallback)
        m_onlineFixFallbackUsed = false;

    m_logGameId = gameId;
    if (!fromOfFallback)
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

            const QString protonId = m_settings->resolvedProtonId(gameCopy.protonId, *m_protons);
            const QString protonName = m_protons->activeVersionName(protonId);
            const QString versionRepair =
                m_protons->repairLegacyPrefixVersionForGame(gameCopy.id, protonName);
            if (!versionRepair.isEmpty())
                logLine(versionRepair);
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

        // Default Play pipeline: Steamless (SteamStub) then Online Fix env.
        if (m_steamless) {
            QString steamlessError;
            const int stripped = m_steamless->ensureUnpacked(gameCopy.installPath, &steamlessError);
            if (stripped > 0) {
                logLine(QCoreApplication::translate(
                            "Core", "Steamless removed SteamStub from %1 file(s)")
                            .arg(stripped));
            } else if (stripped < 0) {
                logLine(QCoreApplication::translate("Core", "Steamless: %1")
                            .arg(steamlessError.isEmpty()
                                     ? QCoreApplication::translate("Core", "unknown error")
                                     : steamlessError));
                if (m_hooks.notice) {
                    m_hooks.notice(QCoreApplication::translate(
                                       "Core", "Steamless failed: %1")
                                       .arg(steamlessError.isEmpty()
                                                ? QCoreApplication::translate("Core", "unknown error")
                                                : steamlessError));
                }
                // Still try to launch - games without a live stub may boot anyway.
            }
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

        // Refuse steam://rungameid - that needs a Store license. Depot installs
        // must run the local exe (Proton / Windows), never Steam as the game host.
        const bool steamUriLaunch = std::any_of(
            info.arguments.cbegin(), info.arguments.cend(), [](const QString& arg) {
                return arg.startsWith(QStringLiteral("steam://rungameid/"), Qt::CaseInsensitive);
            });
        if (steamUriLaunch) {
            const QString reason = QCoreApplication::translate(
                "Core",
                "Game files are missing or incomplete - reinstall from the catalog. "
                "Steam Store launch is not used.");
            logLine(reason);
            if (m_hooks.notice)
                m_hooks.notice(reason);
            return;
        }

        const OnlineFixOverlayState overlayBefore = detectOnlineFixOverlay(gameCopy.installPath);
        const bool watchOnlineFix = overlayBefore.enabled && !m_onlineFixFallbackUsed;
        WineErrorWatchHints watchHints;
        if (watchOnlineFix) {
            watchHints.installPath = gameCopy.installPath;
            const QString exePath = !info.executable.isEmpty()
                ? info.executable
                : (!gameCopy.executableOverride.isEmpty() ? gameCopy.executableOverride
                                                          : QString());
            watchHints.executableName = QFileInfo(exePath).fileName();
            watchHints.fakeSteamAppId = QStringLiteral("480");
            const QString ov =
                overlayBefore.overlayDir.isEmpty() ? gameCopy.installPath : overlayBefore.overlayDir;
            const QString onlineFixIni = QDir(ov).filePath(QStringLiteral("OnlineFix.ini"));
            const QString steamFixIni = QDir(ov).filePath(QStringLiteral("SteamFix.ini"));
            auto readFake = [](const QString& path) -> QString {
                QFile f(path);
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                    return {};
                const QString text = QString::fromUtf8(f.readAll());
                for (const QString& line : text.split(QLatin1Char('\n'))) {
                    const QString t = line.trimmed();
                    if (t.startsWith(QLatin1String("FakeAppId="), Qt::CaseInsensitive))
                        return t.section(QLatin1Char('='), 1).trimmed();
                }
                return {};
            };
            if (QFileInfo::exists(onlineFixIni)) {
                const QString id = readFake(onlineFixIni);
                if (!id.isEmpty())
                    watchHints.fakeSteamAppId = id;
            } else if (QFileInfo::exists(steamFixIni)) {
                const QString id = readFake(steamFixIni);
                if (!id.isEmpty())
                    watchHints.fakeSteamAppId = id;
            }
        }
        applyOnlineFixLaunchInfo(gameCopy.installPath, &info);
#if defined(Q_OS_LINUX)
        {
            const OnlineFixOverlayState overlay = detectOnlineFixOverlay(gameCopy.installPath);
            const bool ofEnabled = overlay.enabled
                || info.environmentExtras.value(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"))
                       == QStringLiteral("legacy")
                || info.environmentExtras.value(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"))
                       == QStringLiteral("1");
            if (ofEnabled)
                logLine(QCoreApplication::translate("Core", "Online Fix overlay detected"));

            // FreeTP SteamFix needs the Steam client for SpaceWar/overlay IPC.
            // OF.me (OnlineFix.ini) does not - starting Steam next to it often trips
            // Self-protection (error 4) under Proton.
            const QString ov = overlay.overlayDir.isEmpty() ? gameCopy.installPath : overlay.overlayDir;
            const bool steamFix = QDir(ov).exists(QStringLiteral("SteamFix.ini"))
                                  || QDir(ov).exists(QStringLiteral("SteamFix64.dll"))
                                  || QDir(ov).exists(QStringLiteral("SteamFix32.dll"));
            if (overlay.enabled && steamFix && !isSteamClientRunning()) {
                tryStartSteamClient();
                if (m_hooks.notice) {
                    m_hooks.notice(QCoreApplication::translate(
                        "Core",
                        "Steam is not running - Online Fix needs it for SpaceWar/overlay. Starting Steam…"));
                }
            }
        }
#endif
        {
            QString provisionExe = gameCopy.executableOverride;
            if (provisionExe.isEmpty())
                provisionExe = info.executable;
            if (!provisionExe.isEmpty()) {
                const QString provisionSummary =
                    ensureSteamApiDllForExecutable(gameCopy.installPath, provisionExe);
                if (!provisionSummary.isEmpty())
                    logLine(provisionSummary);
            }
        }
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
        QTimer::singleShot(0, this, [this, gameCopy, processId, watchOnlineFix, watchHints]() {
            markRunning(gameCopy, processId, watchOnlineFix, watchHints);
        });
    });
}

void LaunchController::stopRunningGame()
{
    if (!gameRunning())
        return;
    arachnel::logBreadcrumb(QStringLiteral("stop"), m_gameId);
    m_watchingOnlineFix = false;
    terminateTrackedLaunch();
    QTimer::singleShot(0, this, [this]() { clearRunning(false); });
}

} // namespace arachnel::core
