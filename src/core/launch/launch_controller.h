#pragma once

#include "library_model.h"
#include "wine_error_probe.h"

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <functional>

namespace arachnel::core {

class PluginHost;
class ProtonManager;
class SettingsStore;
class SteamlessService;

class LaunchController : public QObject
{
    Q_OBJECT

public:
    struct Hooks {
        std::function<void(const QString&)> notice;
        std::function<bool(const LibraryGame&)> ensureRuntime;
        std::function<void(const QString&)> touchLastPlayed;
        std::function<void(const QString& gameId, bool enabled)> setOnlineFixEnabled;
    };

    LaunchController(LibraryModel* library, SettingsStore* settings, PluginHost* plugins,
                     ProtonManager* protons, SteamlessService* steamless, Hooks hooks,
                     QObject* parent = nullptr);
    bool gameRunning() const { return !m_gameId.isEmpty(); }
    QString runningGameId() const { return m_gameId; }
    QString runningGameTitle() const { return m_gameTitle; }
    QString runningGameCoverUrl() const { return m_gameCoverUrl; }

    QString launchLogText() const;
    QString launchLogText(const QString& gameId) const;
    QString launchLogFilePath() const;
    QString launchLogFilePath(const QString& gameId) const;
    bool hasLaunchLog(const QString& gameId) const;

    void launchGame(const QString& gameId);
    void stopRunningGame();

signals:
    void runningGameChanged();
    /** Emitted when a tracked session ends. elapsedMs is wall time since markRunning.
     *  suppressQuickExitLog: OF auto-retry or the user closed the game - don't pop the log. */
    void launchSessionEnded(const QString& gameId, qint64 elapsedMs, bool suppressQuickExitLog);

private:
    void markRunning(const LibraryGame& game, qint64 processId, bool watchingOnlineFix,
                     const WineErrorWatchHints& watchHints);
    void clearRunning(bool allowOnlineFixFallback, bool suppressQuickExitLog = false,
                     int exitCode = -1);
    void pollRunningGame();
    void handleOnlineFixLaunchFailure(const QString& gameId, const QString& reason);
    void terminateTrackedLaunch();
    void logLine(const QString& line);
    LibraryModel* m_library;
    SettingsStore* m_settings;
    PluginHost* m_plugins;
    ProtonManager* m_protons = nullptr;
    SteamlessService* m_steamless = nullptr;
    Hooks m_hooks;
    QString m_gameId;
    QString m_gameTitle;
    QString m_gameCoverUrl;
    qint64 m_processId = 0;
    QTimer* m_timer = nullptr;
    QString m_logGameId;
    QStringList m_launchLogLines;
    QDateTime m_launchStartedAt;
    bool m_watchingOnlineFix = false;
    bool m_onlineFixFallbackUsed = false;
    bool m_relaunchWithoutOnlineFix = false;
    bool m_sawGameExecutable = false;
    bool m_userStopped = false;
    QDateTime m_onlineFixWatchUntil;
    WineErrorWatchHints m_watchHints;
};

} // namespace arachnel::core
