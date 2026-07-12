#pragma once

#include "catalog_model.h"
#include "job_model.h"
#include "job_store.h"
#include "library_store.h"
#include "library_model.h"
#include "notification_model.h"
#include "settings_store.h"
#include "source_plugin_model.h"

#include <QObject>
#include <QHash>
#include <QSet>
#include <QUrl>
#include <QVariant>
#include <QVector>
#include <functional>
#include <optional>

class QTimer;

class QQmlEngine;
class QJSEngine;

namespace arachnel::core {

class CatalogFeedLoader;
class CoverImageCache;
class GameMetadataService;
class HttpDownloadSession;
class JobOrchestrator;
class InstallKindProbeService;
class PluginHost;
class TorrentSession;

class CoreController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(LibraryModel* library READ library CONSTANT)
    Q_PROPERTY(SourcePluginModel* sources READ sources CONSTANT)
    Q_PROPERTY(CatalogModel* catalog READ catalog CONSTANT)
    Q_PROPERTY(JobModel* jobs READ jobs CONSTANT)
    Q_PROPERTY(NotificationModel* notifications READ notifications CONSTANT)
    Q_PROPERTY(SettingsStore* settings READ settings CONSTANT)
    Q_PROPERTY(QString userNotice READ userNotice NOTIFY userNoticeChanged)
    Q_PROPERTY(int userNoticeSerial READ userNoticeSerial NOTIFY userNoticeChanged)
    Q_PROPERTY(bool catalogLoading READ catalogLoading NOTIFY catalogLoadingChanged)
    Q_PROPERTY(QString catalogStatus READ catalogStatus NOTIFY catalogStatusChanged)
    Q_PROPERTY(int pluginCount READ pluginCount NOTIFY pluginsChanged)
    Q_PROPERTY(QString pluginsUserDir READ pluginsUserDir CONSTANT)
    Q_PROPERTY(QString pluginsBundleDir READ pluginsBundleDir CONSTANT)
    Q_PROPERTY(QString lastPluginError READ lastPluginError NOTIFY lastPluginErrorChanged)
    Q_PROPERTY(bool gameRunning READ gameRunning NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameId READ runningGameId NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameTitle READ runningGameTitle NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameCoverUrl READ runningGameCoverUrl NOTIFY runningGameChanged)

public:
    static CoreController* create(QQmlEngine* engine, QJSEngine* scriptEngine);
    static CoreController& instance();

    LibraryModel* library() { return &m_library; }
    SourcePluginModel* sources() { return &m_sources; }
    CatalogModel* catalog() { return &m_catalog; }
    JobModel* jobs() { return &m_jobs; }
    NotificationModel* notifications() { return &m_notifications; }
    SettingsStore* settings() { return &m_settings; }
    QString userNotice() const { return m_userNotice; }
    int userNoticeSerial() const { return m_userNoticeSerial; }
    bool catalogLoading() const { return m_catalogLoading; }
    QString catalogStatus() const { return m_catalogStatus; }
    int pluginCount() const;
    QString pluginsUserDir() const;
    QString pluginsBundleDir() const;
    QString lastPluginError() const { return m_lastPluginError; }
    bool gameRunning() const { return !m_runningGameId.isEmpty(); }
    QString runningGameId() const { return m_runningGameId; }
    QString runningGameTitle() const { return m_runningGameTitle; }
    QString runningGameCoverUrl() const { return m_runningGameCoverUrl; }

    Q_INVOKABLE QVariantList pluginEntries() const;
    Q_INVOKABLE void browsePluginZip();
    Q_INVOKABLE bool installPluginZip(const QUrl& fileUrl);
    Q_INVOKABLE void openPluginsFolder();
    Q_INVOKABLE void rescanPlugins();

    Q_INVOKABLE void launchGame(const QString& gameId);
    Q_INVOKABLE void stopRunningGame();
    Q_INVOKABLE void searchCatalog(const QString& sourceId, const QString& query);
    Q_INVOKABLE void installCatalogEntry(const QString& entryId, const QString& libraryId = {},
                                         const QVariantList& addonIds = {});
    Q_INVOKABLE void installCatalogAddon(const QString& entryId, const QString& addonId);
    Q_INVOKABLE void installDownloadedCatalogAddon(const QString& entryId, const QString& addonId);
    Q_INVOKABLE bool isCatalogAddonInstalled(const QString& entryId, const QString& addonId) const;
    Q_INVOKABLE void updateCatalogEntry(const QString& entryId);
    Q_INVOKABLE bool needsInstallLocationChoice() const;
    Q_INVOKABLE QString browseStorageFolder();
    Q_INVOKABLE void removeGame(const QString& gameId, bool deleteFiles = true);
    Q_INVOKABLE void removeEntry(const QString& entryId, bool deleteFiles = true);
    Q_INVOKABLE void moveGame(const QString& gameId, const QString& targetLibraryId);
    Q_INVOKABLE QVariantList gamesOnLibrary(const QString& libraryId) const;
    Q_INVOKABLE bool isEntryPlayable(const QString& entryId) const;
    Q_INVOKABLE bool isEntryDownloadComplete(const QString& entryId) const;
    Q_INVOKABLE bool entryDownloadFilesExist(const QString& entryId) const;
    Q_INVOKABLE QVariantMap entryDetails(const QString& entryId) const;
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void cancelJob(const QString& jobId);
    Q_INVOKABLE void toggleJobPause(const QString& jobId);
    Q_INVOKABLE void removeJob(const QString& jobId);
    Q_INVOKABLE void retryJob(const QString& jobId);
    Q_INVOKABLE void retryInstall(const QString& jobId);
    Q_INVOKABLE void clearFinishedJobs();
    Q_INVOKABLE void markNotificationsRead();
    Q_INVOKABLE void clearNotifications();
    Q_INVOKABLE void refreshCatalog(const QString& sourceId);
    Q_INVOKABLE void requestCatalogCover(const QString& entryId);
    Q_INVOKABLE void cancelCatalogCover(const QString& entryId);
    Q_INVOKABLE void invalidateCatalogCover(const QString& entryId);
    Q_INVOKABLE void enrichCatalogEntry(const QString& entryId);
    void prepareShutdown();

signals:
    void userNoticeChanged();
    void catalogLoadingChanged();
    void catalogStatusChanged();
    void pluginsChanged();
    void lastPluginErrorChanged();
    void runningGameChanged();

private:
    explicit CoreController(QObject* parent = nullptr);

    void initializeServices();
    void syncSourcesFromPlugins();
    void persistSourcesToSettings();
    void applyPluginCatalog(const QString& sourceId, QVector<CatalogEntry> entries);
    void syncLibraryFromStore();
    void applyCatalogFilter(const QString& sourceId, const QString& query);
    void startPluginInstall(const CatalogEntry& entry, const QString& sourceId,
                            const QString& savePath, JobKind kind,
                            const QString& libraryId = {}, const QString& jobId = {});
    void startPluginAddonInstall(const CatalogEntry& parent, const CatalogComponent& addon,
                                 const QString& sourceId, const QString& artifactPath,
                                 const QString& progressJobId = {},
                                 std::function<void(bool success)> done = {});
    void beginInstallSession(const QString& entryId, const QString& gameJobId,
                             const QString& sourceId, const QStringList& addonIds);
    void advanceInstallSession(const QString& entryId);
    void syncInstallSessionPhase(const QString& entryId);
    void markCatalogAddonInstalled(const QString& parentEntryId, const QString& addonId,
                                   const QString& uploadDate);
    QString resolveAddonArtifactPath(const QString& parentEntryId, const QString& addonId) const;
    std::optional<CatalogEntry> resolveCatalogEntry(const QString& entryId,
                                                    const QString& sourceId,
                                                    const JobEntry* jobHint = nullptr) const;
    bool gameNeedsInstall(const QString& entryId) const;
    void retryPendingInstalls();
    void pruneBrokenLibraryEntries();
    void pruneAddonLibraryEntries();
    void restoreLibraryPlaceholders();
    void ensureLibraryPlaceholder(const CatalogEntry& entry, const QString& libraryId,
                                  const QStringList& selectedAddonIds = {});
    void reconcileJobInstallState();
    void removeJobsForEntry(const QString& entryId);
    void pruneUnselectedAddonJobs(const QString& parentEntryId, const QStringList& selectedAddonIds);
    void pruneCancelledAddonJobs();
    void markGameRunning(const LibraryGame& game, qint64 processId);
    void clearRunningGame();
    void pollRunningGame();
    const JobEntry* findLatestJobForEntry(const QString& entryId) const;
    void showNotice(const QString& message);
    void setCatalogLoading(bool loading);
    void setCatalogStatus(const QString& status);
    bool isRemoteUploadDateNewer(const QString& remote, const QString& local) const;
    const CatalogEntry* findCatalogEntry(const QString& entryId) const;
    const CatalogComponent* findCatalogAddon(const CatalogEntry& entry,
                                             const QString& addonId) const;
    void syncEntryToCatalogModel(const QString& entryId);
    InstallKind detectInstallKindForEntry(const QString& sourceId,
                                          const QString& downloadPath) const;
    void syncCatalogInstallKind(const QString& entryId, InstallKind kind);
    void applyCachedMetadata(CatalogEntry& entry) const;
    void applyCoverToEntry(const QString& entryId, const QString& coverUrl);
    void ensureDiskCover(const QString& entryId, const QString& remoteUrl);
    static bool isRemoteLibraryCover(const QString& url);

    LibraryModel m_library;
    SourcePluginModel m_sources;
    CatalogModel m_catalog;
    JobModel m_jobs;
    NotificationModel m_notifications;
    SettingsStore m_settings;
    LibraryStore m_libraryStore;
    JobStore m_jobStore;
    CatalogFeedLoader* m_catalogLoader = nullptr;
    GameMetadataService* m_metadataService = nullptr;
    CoverImageCache* m_coverCache = nullptr;
    TorrentSession* m_torrentSession = nullptr;
    HttpDownloadSession* m_httpSession = nullptr;
    JobOrchestrator* m_jobOrchestrator = nullptr;
    PluginHost* m_pluginHost = nullptr;
    InstallKindProbeService* m_installKindProbe = nullptr;

    QVector<CatalogEntry> m_catalogCache;
    QHash<QString, QSet<QString>> m_coverWaiters;
    QString m_activeSourceId;
    QString m_activeQuery;
    QString m_userNotice;
    int m_userNoticeSerial = 0;
    QString m_catalogStatus;
    QString m_lastPluginError;
    QString m_runningGameId;
    QString m_runningGameTitle;
    QString m_runningGameCoverUrl;
    qint64 m_runningProcessId = 0;
    QTimer* m_runningGameTimer = nullptr;
    bool m_catalogLoading = false;
    QSet<QString> m_installingEntries;
    QSet<QString> m_installingAddons;
    QHash<QString, QStringList> m_installSelectedAddons;

    struct GameInstallSession {
        QString gameJobId;
        QString sourceId;
        QStringList selectedAddonIds;
        int installStep = 0;
        int installTotal = 1;
        bool gameInstallDone = false;
    };
    QHash<QString, GameInstallSession> m_installSessions;
};

void registerCoreTypes();

} // namespace arachnel::core
