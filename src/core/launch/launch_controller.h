#pragma once

#include "library_model.h"

#include <QObject>
#include <QTimer>
#include <functional>

namespace arachnel::core {

class PluginHost;
class SettingsStore;

class LaunchController : public QObject
{
    Q_OBJECT

public:
    struct Hooks {
        std::function<void(const QString&)> notice;
        std::function<bool(const LibraryGame&)> ensureRuntime;
    };

    LaunchController(LibraryModel* library, SettingsStore* settings, PluginHost* plugins,
                     Hooks hooks, QObject* parent = nullptr);
    bool gameRunning() const { return !m_gameId.isEmpty(); }
    QString runningGameId() const { return m_gameId; }
    QString runningGameTitle() const { return m_gameTitle; }
    QString runningGameCoverUrl() const { return m_gameCoverUrl; }
    void launchGame(const QString& gameId);
    void stopRunningGame();

signals:
    void runningGameChanged();

private:
    void markRunning(const LibraryGame& game, qint64 processId);
    void clearRunning();
    void pollRunningGame();
    LibraryModel* m_library;
    SettingsStore* m_settings;
    PluginHost* m_plugins;
    Hooks m_hooks;
    QString m_gameId;
    QString m_gameTitle;
    QString m_gameCoverUrl;
    qint64 m_processId = 0;
    QTimer* m_timer = nullptr;
};

} // namespace arachnel::core
