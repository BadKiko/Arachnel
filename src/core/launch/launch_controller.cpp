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
#include "vr_service.h"
#include "wine_error_probe.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QStringView>
#include <QSysInfo>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>

namespace arachnel::core {

namespace {
constexpr int kPollIntervalMs = 1500;
constexpr int kOnlineFixPollIntervalMs = 400;
constexpr int kOnlineFixWatchMs = 20000;
constexpr int kOnlineFixEarlyExitMs = 12000;

QString formatProcessExitCode(int exitCode)
{
    if (exitCode < 0)
        return QCoreApplication::translate("Core", "n/a");
    const auto u = static_cast<quint32>(exitCode);
    if (u > 255)
        return QLatin1String("0x") + QString::number(u, 16).toUpper();
    return QString::number(exitCode);
}

bool exitCodeLooksLikeCrash(int exitCode)
{
    if (exitCode < 0)
        return false;
    const auto u = static_cast<quint32>(exitCode);
    if (u == 0xC000013A) // STATUS_CONTROL_C_EXIT - console close / Ctrl+C
        return false;
    if ((u & 0xF0000000u) == 0xC0000000u)
        return true;
    if (u == 0x40000015u) // STATUS_FATAL_APP_EXIT
        return true;
    const int sig = exitCode - 128;
    if (sig >= 1 && sig <= 31) {
        switch (sig) {
        case 1: // SIGHUP
        case 2: // SIGINT
        case 9: // SIGKILL
        case 15: // SIGTERM
            return false;
        default:
            return true;
        }
    }
    return false;
}

bool exitCodeLooksLikeCleanQuit(int exitCode)
{
    if (exitCode == 0)
        return true;
    if (static_cast<quint32>(exitCode) == 0xC000013Au)
        return true;
    const int sig = exitCode - 128;
    return sig == 1 || sig == 2 || sig == 15;
}

bool textLooksLikeGameCrash(const QString& text)
{
    const QString lower = text.toLower();
    static const QStringView keys[] = {
        u"assertion failed",
        u"fatal error",
        u"crash!!!",
        u"unhandled exception",
        u"access violation",
        u"segmentation fault",
        u"wine: unhandled",
    };
    for (const QStringView key : keys) {
        if (lower.contains(key))
            return true;
    }
    return false;
}

bool installUsesSteamFix(const QString& installPath)
{
    if (installPath.isEmpty())
        return false;
    const OnlineFixOverlayState overlay = detectOnlineFixOverlay(installPath);
    const QString ov = overlay.overlayDir.isEmpty() ? installPath : overlay.overlayDir;
    const QDir dir(ov);
    auto has = [&](const QString& name) {
        return dir.exists(name) || dir.exists(name + QStringLiteral(".arachnel-off"));
    };
    return has(QStringLiteral("SteamFix.ini")) || has(QStringLiteral("SteamFix64.dll"))
        || has(QStringLiteral("SteamFix32.dll"));
}

constexpr int kPlayerLogMaxBytes = 16384;
constexpr int kPlayerLogMaxLines = 100;

QString readSteamAppId(const QString& installPath)
{
    if (installPath.isEmpty())
        return {};
    const QString path = QDir(installPath).filePath(QStringLiteral("steam_appid.txt"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString findNewestNamedFile(const QString& root, const QStringList& names)
{
    if (root.isEmpty() || !QFileInfo::exists(root))
        return {};
    QString bestPath;
    QDateTime bestTime;
    QDirIterator it(root, names, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (!bestTime.isValid() || info.lastModified() > bestTime) {
            bestTime = info.lastModified();
            bestPath = info.absoluteFilePath();
        }
    }
    return bestPath;
}

QString smartLogTail(const QString& text)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    if (lines.isEmpty())
        return {};

    int crashIdx = -1;
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString& line = lines.at(i);
        if (line.contains(QLatin1String("Crash!!!"), Qt::CaseInsensitive)
            || line.contains(QLatin1String("Fatal error"), Qt::CaseInsensitive)
            || line.contains(QLatin1String("EXCEPTION_"), Qt::CaseInsensitive)
            || line.contains(QLatin1String("Unable to find type"), Qt::CaseInsensitive)) {
            crashIdx = i;
            break;
        }
    }
    if (crashIdx >= 0) {
        const int start = qMax(0, crashIdx - 45);
        return lines.mid(start).join(QLatin1Char('\n'));
    }
    if (lines.size() <= kPlayerLogMaxLines)
        return lines.join(QLatin1Char('\n'));
    return lines.mid(lines.size() - kPlayerLogMaxLines).join(QLatin1Char('\n'));
}

QString readFileTailUtf8(const QString& path, int maxBytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const qint64 size = file.size();
    if (size > maxBytes)
        file.seek(size - maxBytes);
    return QString::fromUtf8(file.readAll());
}

QString onlineFixStatusLabel(const OnlineFixOverlayState& state)
{
    if (!state.present)
        return QCoreApplication::translate("Core", "not installed");
    return state.enabled ? QCoreApplication::translate("Core", "enabled")
                         : QCoreApplication::translate("Core", "disabled");
}
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
    if (m_processId > 0 && m_processId != processId)
        ProcessTracker::release(m_processId);
    m_gameId = game.id;
    m_gameTitle = game.title;
    m_gameCoverUrl = game.coverUrl;
    m_processId = processId;
    m_launchStartedAt = QDateTime::currentDateTime();
    m_watchingOnlineFix = watchingOnlineFix;
    m_watchHints = watchHints;
    m_sawGameExecutable = false;
    m_userStopped = false;
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

void LaunchController::clearRunning(bool allowOnlineFixFallback, bool suppressQuickExitLog,
                                    int exitCode)
{
    if (m_gameId.isEmpty())
        return;

    QString combinedLog = m_launchLogLines.join(QLatin1Char('\n'));
    const QString capturePath = launchLogFilePath();
    if (!capturePath.isEmpty() && QFileInfo::exists(capturePath)) {
        QFile file(capturePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            combinedLog += QString::fromUtf8(file.readAll());
    }

    const bool userStopped = m_userStopped;
    const bool crashed = exitCodeLooksLikeCrash(exitCode) || textLooksLikeGameCrash(combinedLog);
    // Exit 0 only counts as a user close after the game exe actually came up.
    // SteamAPI_Init failure is also 0, with no window - that's a failed start.
    const bool cleanQuit = !crashed
        && (userStopped || (exitCodeLooksLikeCleanQuit(exitCode) && m_sawGameExecutable));

    logLine(QCoreApplication::translate("Core", "Game process exited (code %1)")
                .arg(formatProcessExitCode(exitCode)));
    if (cleanQuit)
        logLine(QCoreApplication::translate("Core", "Stopped by the user"));

    const QString endedId = m_gameId;
    const qint64 elapsedMs =
        m_launchStartedAt.isValid() ? m_launchStartedAt.msecsTo(QDateTime::currentDateTime()) : 0;
    m_lastSessionGameId = endedId;
    m_lastSessionElapsedMs = elapsedMs;
    if (elapsedMs > 0) {
        logLine(QCoreApplication::translate("Core", "Session lasted %1 s")
                    .arg(QString::number(elapsedMs / 1000.0, 'f', 1)));
    }
    if (!cleanQuit && elapsedMs >= 0 && elapsedMs < 20000) {
        logLine(QCoreApplication::translate(
            "Core",
            "Exited quickly. If game output is empty, check Player.log in the diagnostics below."));
    }
    const bool earlyOfExit = allowOnlineFixFallback && m_watchingOnlineFix
        && !m_onlineFixFallbackUsed && elapsedMs >= 0 && elapsedMs < kOnlineFixEarlyExitMs
        && !cleanQuit;
    const qint64 endedPid = m_processId;
    m_gameId.clear();
    m_gameTitle.clear();
    m_gameCoverUrl.clear();
    m_processId = 0;
    m_launchStartedAt = {};
    m_watchingOnlineFix = false;
    m_onlineFixWatchUntil = {};
    m_watchHints = {};
    m_sawGameExecutable = false;
    m_userStopped = false;
    m_timer->setInterval(kPollIntervalMs);
    m_timer->stop();
    ProcessTracker::release(endedPid);
    emit runningGameChanged();
    emit launchSessionEnded(endedId, elapsedMs,
                            suppressQuickExitLog || earlyOfExit || cleanQuit);
    if (earlyOfExit) {
        QTimer::singleShot(0, this, [this, endedId]() {
            handleOnlineFixLaunchFailure(
                endedId,
                QCoreApplication::translate("Core", "Online Fix quit right after launch"));
        });
    } else if (!cleanQuit && elapsedMs >= 0 && elapsedMs < kOnlineFixEarlyExitMs && m_library) {
        const LibraryGame* game = m_library->gameById(endedId);
        if (game && !game->installPath.isEmpty()) {
            const OnlineFixOverlayState overlay = detectOnlineFixOverlay(game->installPath);
            if (overlay.present && !overlay.enabled && m_hooks.notice) {
                m_hooks.notice(QCoreApplication::translate(
                    "Core",
                    "Online Fix is off, and the game quit right after start. "
                    "Turn it back on in game settings."));
            }
        }
    }
}

void LaunchController::handleOnlineFixLaunchFailure(const QString& gameId, const QString& reason)
{
    if (gameId.isEmpty() || m_onlineFixFallbackUsed)
        return;
    m_onlineFixFallbackUsed = true;
    logLine(reason);

    if (m_library) {
        const LibraryGame* game = m_library->gameById(gameId);
        if (game && installUsesSteamFix(game->installPath)) {
            logLine(QCoreApplication::translate(
                "Core", "This game needs Online Fix - not launching without it"));
            if (m_hooks.notice) {
                m_hooks.notice(QCoreApplication::translate(
                    "Core", "This game needs Online Fix. It quit right after start."));
            }
            return;
        }
    }

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

QString LaunchController::launchLogHeadline(const QString& gameId) const
{
    if (gameId.isEmpty())
        return {};

    QStringList parts;
    if (!m_gameId.isEmpty() && m_gameId == gameId) {
        parts.append(QCoreApplication::translate("Core", "Running"));
    } else if (m_lastSessionGameId == gameId && m_lastSessionElapsedMs >= 0) {
        const QString secs = QString::number(m_lastSessionElapsedMs / 1000.0, 'f', 1);
        if (m_lastSessionElapsedMs < 20000) {
            parts.append(QCoreApplication::translate("Core", "Exited after %1 s").arg(secs));
        } else {
            parts.append(QCoreApplication::translate("Core", "Last session %1 s").arg(secs));
        }
    }

    const LibraryGame* game = m_library ? m_library->gameById(gameId) : nullptr;
    if (game && !game->installPath.isEmpty()) {
        const OnlineFixOverlayState of = detectOnlineFixOverlay(game->installPath);
        parts.append(QCoreApplication::translate("Core", "Online Fix: %1")
                         .arg(onlineFixStatusLabel(of)));
        if (m_steamless) {
            const QVariantMap steamless = SteamlessService::installInfo(game->installPath);
            parts.append(QCoreApplication::translate("Core", "Steamless: %1")
                             .arg(steamless.value(QStringLiteral("steamlessLabel")).toString()));
        }
    }

#if defined(Q_OS_LINUX)
    if (m_protons && m_settings) {
        const QString protonId =
            game ? m_settings->resolvedProtonId(game->protonId, *m_protons) : QString();
        const QString protonName = m_protons->activeVersionName(protonId);
        if (!protonName.isEmpty())
            parts.append(protonName);
        const QString compat = m_protons->compatDataPathForGame(gameId);
        const QString playerLog = findNewestNamedFile(
            compat, {QStringLiteral("Player.log"), QStringLiteral("output_log.txt")});
        if (!playerLog.isEmpty()) {
            const QString sample = readFileTailUtf8(playerLog, kPlayerLogMaxBytes);
            if (sample.contains(QLatin1String("Crash!!!"), Qt::CaseInsensitive)
                || sample.contains(QLatin1String("Fatal error"), Qt::CaseInsensitive)) {
                parts.append(QCoreApplication::translate("Core", "Player.log has a crash"));
            } else {
                parts.append(QCoreApplication::translate("Core", "Player.log found"));
            }
        }
    }
#endif

    return parts.join(QStringLiteral(" · "));
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

    const LibraryGame* game = (!gameId.isEmpty() && m_library) ? m_library->gameById(gameId) : nullptr;
    QStringList extras;
    extras.append(QCoreApplication::translate("Core", "--- Diagnostics ---"));
    extras.append(QStringLiteral("Arachnel %1").arg(QCoreApplication::applicationVersion()));
    extras.append(QStringLiteral("Qt %1").arg(QString::fromLatin1(qVersion())));
    extras.append(QStringLiteral("OS: %1 (%2)")
                      .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture()));
    extras.append(QStringLiteral("Time: %1")
                      .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
    if (game) {
        extras.append(QCoreApplication::translate("Core", "Game: %1 (%2)")
                          .arg(game->title, game->id));
        extras.append(QCoreApplication::translate("Core", "Source: %1").arg(game->sourceId));
        extras.append(QCoreApplication::translate("Core", "Install path: %1")
                          .arg(game->installPath));
        if (!game->executableOverride.isEmpty()) {
            extras.append(QCoreApplication::translate("Core", "Executable override: %1")
                              .arg(game->executableOverride));
        }
        const OnlineFixOverlayState of = detectOnlineFixOverlay(game->installPath);
        extras.append(QCoreApplication::translate("Core", "Online Fix: %1")
                          .arg(onlineFixStatusLabel(of)));
        if (of.present && !of.overlayDir.isEmpty()) {
            extras.append(QCoreApplication::translate("Core", "Online Fix dir: %1")
                              .arg(of.overlayDir));
        }
        if (m_steamless) {
            const QVariantMap steamless = SteamlessService::installInfo(game->installPath);
            extras.append(QCoreApplication::translate("Core", "Steamless: %1")
                              .arg(steamless.value(QStringLiteral("steamlessLabel")).toString()));
        }
        const QString appId = readSteamAppId(game->installPath);
        if (!appId.isEmpty())
            extras.append(QStringLiteral("steam_appid.txt: %1").arg(appId));
    }

#if defined(Q_OS_LINUX)
    if (m_protons) {
        const QString protonId =
            (game && m_settings) ? m_settings->resolvedProtonId(game->protonId, *m_protons)
                                 : QString();
        const QString protonName = m_protons->activeVersionName(protonId);
        if (!protonName.isEmpty())
            extras.append(QCoreApplication::translate("Core", "Proton: %1").arg(protonName));
        const QString compat = m_protons->compatDataPathForGame(gameId);
        if (!compat.isEmpty()) {
            extras.append(QStringLiteral("STEAM_COMPAT_DATA_PATH: %1").arg(compat));
            const QString playerLog = findNewestNamedFile(
                compat, {QStringLiteral("Player.log"), QStringLiteral("output_log.txt")});
            if (!playerLog.isEmpty()) {
                const QString raw = readFileTailUtf8(playerLog, kPlayerLogMaxBytes);
                const QString snippet = smartLogTail(raw.trimmed());
                extras.append(QCoreApplication::translate("Core", "--- Player.log (%1) ---")
                                  .arg(playerLog));
                if (!snippet.isEmpty())
                    extras.append(snippet);
            } else {
                extras.append(QCoreApplication::translate(
                    "Core", "Player.log: not found under Proton prefix yet"));
            }
        }
    }
#else
    if (game && !game->installPath.isEmpty()) {
        const QString playerLog = findNewestNamedFile(
            game->installPath, {QStringLiteral("Player.log"), QStringLiteral("output_log.txt")});
        if (!playerLog.isEmpty()) {
            const QString raw = readFileTailUtf8(playerLog, kPlayerLogMaxBytes);
            const QString snippet = smartLogTail(raw.trimmed());
            extras.append(QCoreApplication::translate("Core", "--- Player.log (%1) ---")
                              .arg(playerLog));
            if (!snippet.isEmpty())
                extras.append(snippet);
        }
    }
#endif

    if (!text.isEmpty())
        text += QLatin1String("\n\n");
    text += extras.join(QLatin1Char('\n'));

    if (text.trimmed().isEmpty())
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

    if (relatedGameExecutableAlive(m_processId, m_watchHints))
        m_sawGameExecutable = true;

    if (m_watchingOnlineFix && !m_onlineFixFallbackUsed && m_onlineFixWatchUntil.isValid()
        && QDateTime::currentDateTime() <= m_onlineFixWatchUntil) {
        if (wineErrorDialogVisible(m_processId, m_watchHints)) {
            int dialogExit = -1;
            const bool parentAlive = ProcessTracker::isProcessRunning(m_processId, &dialogExit);
            const bool gameAlive = relatedGameExecutableAlive(m_processId, m_watchHints);
            if (!parentAlive && !gameAlive) {
                const QString gameId = m_gameId;
                handleOnlineFixLaunchFailure(
                    gameId,
                    QCoreApplication::translate("Core", "Online Fix showed an error dialog"));
                return;
            }
        }

        // Proton often reparents the game off the launch pid. If the exe vanishes
        // with a crash in the log, fall back. A clean close waits for the wrapper.
        if (m_sawGameExecutable && !relatedGameExecutableAlive(m_processId, m_watchHints)) {
            int exitCode = -1;
            const bool wrapperAlive = ProcessTracker::isProcessRunning(m_processId, &exitCode);
            QString combinedLog = m_launchLogLines.join(QLatin1Char('\n'));
            const QString capturePath = launchLogFilePath();
            if (!capturePath.isEmpty() && QFileInfo::exists(capturePath)) {
                QFile file(capturePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text))
                    combinedLog += QString::fromUtf8(file.readAll());
            }
            const bool crashed = exitCodeLooksLikeCrash(exitCode)
                || textLooksLikeGameCrash(combinedLog);
            if (!wrapperAlive) {
                QTimer::singleShot(0, this, [this, exitCode]() { clearRunning(true, false, exitCode); });
                return;
            }
            if (crashed) {
                const QString gameId = m_gameId;
                handleOnlineFixLaunchFailure(
                    gameId,
                    QCoreApplication::translate("Core", "Online Fix quit right after launch"));
                return;
            }
        }
    }

    if (m_watchingOnlineFix && m_onlineFixWatchUntil.isValid()
        && QDateTime::currentDateTime() > m_onlineFixWatchUntil) {
        m_watchingOnlineFix = false;
        m_timer->setInterval(kPollIntervalMs);
    }

    int exitCode = -1;
    if (!ProcessTracker::isProcessRunning(m_processId, &exitCode)) {
        if (relatedGameExecutableAlive(m_processId, m_watchHints))
            return;
        QTimer::singleShot(0, this, [this, exitCode]() { clearRunning(true, false, exitCode); });
    }
}

QVector<GameLaunchOption> LaunchController::availableLaunchOptions(const QString& gameId) const
{
    const LibraryGame* game = m_library ? m_library->gameById(gameId) : nullptr;
    if (!game || game->installPath.isEmpty())
        return {};

    if (m_plugins) {
        if (ISourcePlugin* plugin = m_plugins->plugin(game->sourceId)) {
            const auto opts = plugin->launchOptions(*game);
            if (!opts.isEmpty())
                return opts;
        }
    }

    if (!game->launchOptions.isEmpty())
        return game->launchOptions;

    const QString markerPath = game->installPath + QStringLiteral("/.arachnel-steamidra");
    if (QFileInfo::exists(markerPath)) {
        QFile f(markerPath);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject rootObj = QJsonDocument::fromJson(f.readAll()).object();
            const QJsonArray arr = rootObj.value(QStringLiteral("launchOptions")).toArray();
            QVector<GameLaunchOption> markerOpts;
            for (const auto& v : arr) {
                if (!v.isObject())
                    continue;
                const QJsonObject o = v.toObject();
                GameLaunchOption opt;
                opt.id = o.value(QStringLiteral("id")).toString();
                opt.title = o.value(QStringLiteral("title")).toString();
                opt.executable = o.value(QStringLiteral("path")).toString();
                if (opt.executable.isEmpty())
                    opt.executable = o.value(QStringLiteral("executable")).toString();
                opt.workingDirectory = o.value(QStringLiteral("workingDirectory")).toString();
                for (const auto& a : o.value(QStringLiteral("arguments")).toArray())
                    opt.arguments.append(a.toString());
                opt.type = o.value(QStringLiteral("type")).toString();
                opt.isDefault = o.value(QStringLiteral("isDefault")).toBool(false);
                if (!opt.executable.isEmpty() && QFileInfo::exists(opt.executable))
                    markerOpts.append(opt);
            }
            if (!markerOpts.isEmpty())
                return markerOpts;
        }
    }

    return {};
}

void LaunchController::setGameSelectedLaunchOption(const QString& gameId, const QString& optionId)
{
    if (m_hooks.setSelectedLaunchOption)
        m_hooks.setSelectedLaunchOption(gameId, optionId);
}

void LaunchController::launchGame(const QString& gameId, const QString& optionId)
{
    const LibraryGame* game = m_library->gameById(gameId);
    if (!game || game->installPath.isEmpty()) {
        if (m_hooks.notice)
            m_hooks.notice(QCoreApplication::translate("Core", "Game is not installed yet"));
        return;
    }
    if (gameRunning() && m_gameId == gameId)
        return;

    // When no optionId is passed and no preferred launch option is saved, ask user if multiple options exist
    if (optionId.isEmpty() && game->selectedLaunchOptionId.isEmpty()) {
        const QVector<GameLaunchOption> options = availableLaunchOptions(gameId);
        if (options.size() > 1) {
            emit launchOptionSelectionRequested(gameId, options);
            return;
        }
    }

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

    LibraryGame gameCopyBase = *game;
    if (!optionId.isEmpty())
        gameCopyBase.selectedLaunchOptionId = optionId;
    QTimer::singleShot(0, this, [this, gameId, gameCopyBase]() {
        if (m_library->gameById(gameId) == nullptr)
            return;
        LibraryGame gameCopy = gameCopyBase;
        if (!gameCopy.installPath.isEmpty()) {
            healWindowsInstallLayout(gameCopy.installPath);
            const int unityHealed = healUnityScriptingAssemblies(gameCopy.installPath);
            if (unityHealed > 0) {
                const QString repaired = QCoreApplication::translate(
                    "Core",
                    "Repaired mixed Unity data files (Windows/Linux/Mac depots overlapped)");
                logLine(repaired);
                if (m_hooks.notice)
                    m_hooks.notice(QCoreApplication::translate(
                        "Core",
                        "Repaired mixed Unity files. Turn Online Fix back on in game settings."));
            }
        }
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
        if (!gameCopy.selectedLaunchOptionId.isEmpty() && (info.executable.isEmpty() || !QFileInfo::exists(info.executable))) {
            const auto opts = availableLaunchOptions(gameCopy.id);
            for (const auto& opt : opts) {
                if (opt.id == gameCopy.selectedLaunchOptionId && !opt.executable.isEmpty()
                    && QFileInfo::exists(opt.executable)) {
                    info.executable = opt.executable;
                    info.workingDirectory = opt.workingDirectory.isEmpty()
                                                ? QFileInfo(opt.executable).absolutePath()
                                                : opt.workingDirectory;
                    info.arguments = opt.arguments;
                    break;
                }
            }
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
        watchHints.installPath = gameCopy.installPath;
        {
            const QString exePath = !info.executable.isEmpty()
                ? info.executable
                : (!gameCopy.executableOverride.isEmpty() ? gameCopy.executableOverride
                                                          : QString());
            watchHints.executableName = QFileInfo(exePath).fileName();
        }
        watchHints.fakeSteamAppId = QStringLiteral("480");
        {
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
        {
            const OnlineFixOverlayState overlay = detectOnlineFixOverlay(gameCopy.installPath);
#if defined(Q_OS_LINUX)
            const bool ofEnabled = overlay.enabled
                || info.environmentExtras.value(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"))
                       == QStringLiteral("legacy")
                || info.environmentExtras.value(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"))
                       == QStringLiteral("1");
            if (ofEnabled)
                logLine(QCoreApplication::translate("Core", "Online Fix overlay detected"));
#endif

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
        {
            const QVector<GameLaunchOption> launchOpts = availableLaunchOptions(gameCopy.id);
            const VrGameDetection vrDetection =
                detectVrGame(gameCopy.installPath, info.executable, gameCopy.genres, gameCopy.title,
                             launchOpts, gameCopy.selectedLaunchOptionId,
                             info.arguments + splitLaunchArguments(gameCopy.launchArgs));
            if (vrDetection.isCurrentLaunchVr) {
                logLine(QCoreApplication::translate("Core", "VR launch mode active (runtime: %1, engine: %2)")
                            .arg(vrRuntimeName(vrDetection.runtime),
                                 vrDetection.isUnreal ? QStringLiteral("Unreal Engine")
                                                      : (vrDetection.isUnity ? QStringLiteral("Unity")
                                                                             : QStringLiteral("Standard"))));
                if (!isSteamVrRunning()) {
                    logLine(QCoreApplication::translate("Core", "SteamVR is not running. Starting SteamVR…"));
                    if (tryStartSteamVr()) {
                        logLine(QCoreApplication::translate("Core", "SteamVR start requested"));
                    } else {
                        logLine(QCoreApplication::translate(
                            "Core",
                            "Could not start SteamVR automatically. Make sure SteamVR is installed and running."));
                    }
                }

                const bool hasVrArg =
                    gameCopy.launchArgs.contains(QStringLiteral("-vr"), Qt::CaseInsensitive)
                    || gameCopy.launchArgs.contains(QStringLiteral("-vrmode"), Qt::CaseInsensitive)
                    || info.arguments.contains(QStringLiteral("-vr"), Qt::CaseInsensitive)
                    || info.arguments.contains(QStringLiteral("-vrmode"), Qt::CaseInsensitive);

                if (!hasVrArg && !vrDetection.suggestedArgs.isEmpty()) {
                    info.arguments += vrDetection.suggestedArgs;
                    logLine(QCoreApplication::translate("Core", "Applied VR launch arguments: %1")
                                .arg(vrDetection.suggestedArgs.join(QLatin1Char(' '))));
                }
            } else if (vrDetection.vrMode == VrMode::VrOptional) {
                logLine(QCoreApplication::translate("Core", "Hybrid VR/Desktop game detected - launching in 2D Desktop mode"));
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
        logLine(QStringLiteral("Arachnel %1 · %2 (%3)")
                    .arg(QCoreApplication::applicationVersion(), QSysInfo::prettyProductName(),
                         QSysInfo::currentCpuArchitecture()));
        if (m_protons) {
            const QString protonId = m_settings->resolvedProtonId(gameCopy.protonId, *m_protons);
            const QString protonName = m_protons->activeVersionName(protonId);
            if (!protonName.isEmpty())
                logLine(QCoreApplication::translate("Core", "Proton: %1").arg(protonName));
            const QString compat = m_protons->compatDataPathForGame(gameCopy.id);
            if (!compat.isEmpty())
                logLine(QStringLiteral("STEAM_COMPAT_DATA_PATH: %1").arg(compat));
        }
        {
            const OnlineFixOverlayState of = detectOnlineFixOverlay(gameCopy.installPath);
            logLine(QCoreApplication::translate("Core", "Online Fix: %1")
                        .arg(onlineFixStatusLabel(of)));
            if (of.present && !of.overlayDir.isEmpty())
                logLine(QCoreApplication::translate("Core", "Online Fix dir: %1")
                            .arg(of.overlayDir));
        }
        if (m_steamless) {
            const QVariantMap steamless = SteamlessService::installInfo(gameCopy.installPath);
            logLine(QCoreApplication::translate("Core", "Steamless: %1")
                        .arg(steamless.value(QStringLiteral("steamlessLabel")).toString()));
        }
        {
            const QString appId = readSteamAppId(gameCopy.installPath);
            if (!appId.isEmpty())
                logLine(QStringLiteral("steam_appid.txt: %1").arg(appId));
        }
        logLine(QCoreApplication::translate("Core", "Program: %1").arg(resolved.program));
        logLine(QCoreApplication::translate("Core", "Working dir: %1").arg(resolved.workingDirectory));
        logLine(QCoreApplication::translate("Core", "Args: %1")
                    .arg(resolved.arguments.join(QLatin1Char(' '))));
        {
            const QString wineDll =
                resolved.environment.value(QStringLiteral("WINEDLLOVERRIDES"));
            if (!wineDll.isEmpty())
                logLine(QStringLiteral("WINEDLLOVERRIDES: %1").arg(wineDll));
            const QString preload = resolved.environment.value(QStringLiteral("LD_PRELOAD"));
            if (!preload.isEmpty())
                logLine(QStringLiteral("LD_PRELOAD: %1").arg(preload));
            const QString steamAppId = resolved.environment.value(QStringLiteral("SteamAppId"));
            const QString steamGameId = resolved.environment.value(QStringLiteral("SteamGameId"));
            if (!steamAppId.isEmpty() || !steamGameId.isEmpty()) {
                logLine(QStringLiteral("SteamAppId=%1 SteamGameId=%2")
                            .arg(steamAppId.isEmpty() ? QStringLiteral("-") : steamAppId,
                                 steamGameId.isEmpty() ? QStringLiteral("-") : steamGameId));
            }
        }

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
    m_userStopped = true;
    m_watchingOnlineFix = false;
    terminateTrackedLaunch();
    int exitCode = -1;
    ProcessTracker::isProcessRunning(m_processId, &exitCode);
    clearRunning(false, true, exitCode);
}

} // namespace arachnel::core
