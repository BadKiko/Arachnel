#pragma once

#include "catalog_model.h"
#include "job_model.h"
#include "job_store.h"
#include "library_store.h"
#include "library_model.h"
#include "notification_model.h"
#include "settings_store.h"
#include "source_plugin_model.h"
#include "app_updater.h"
#include "plugin_catalog_service.h"

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

struct GameMetadata;

class CatalogFeedLoader;
class CoverImageCache;
class GameMetadataService;
class HttpDownloadSession;
class JobOrchestrator;
class InstallKindProbeService;
class InstallAnalyzer;
class PluginHost;
class ProtonManager;
class RuntimeDependencyService;
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
    Q_PROPERTY(QString activeCatalogSourceId READ activeCatalogSourceId NOTIFY activeCatalogSourceIdChanged)
    Q_PROPERTY(QStringList activeCatalogSourceIds READ activeCatalogSourceIds NOTIFY activeCatalogSourceIdsChanged)
    Q_PROPERTY(int pluginCount READ pluginCount NOTIFY pluginsChanged)
    Q_PROPERTY(QString pluginsUserDir READ pluginsUserDir CONSTANT)
    Q_PROPERTY(QString pluginsBundleDir READ pluginsBundleDir CONSTANT)
    Q_PROPERTY(QString lastPluginError READ lastPluginError NOTIFY lastPluginErrorChanged)
    Q_PROPERTY(bool gameRunning READ gameRunning NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameId READ runningGameId NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameTitle READ runningGameTitle NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameCoverUrl READ runningGameCoverUrl NOTIFY runningGameChanged)
    Q_PROPERTY(bool runtimeSetupInProgress READ runtimeSetupInProgress NOTIFY runtimeSetupChanged)
    Q_PROPERTY(QString runtimeSetupGameId READ runtimeSetupGameId NOTIFY runtimeSetupChanged)
    Q_PROPERTY(QString runtimeSetupTitle READ runtimeSetupTitle NOTIFY runtimeSetupChanged)
    Q_PROPERTY(QString runtimeSetupCoverUrl READ runtimeSetupCoverUrl NOTIFY runtimeSetupChanged)
    Q_PROPERTY(QString runtimeSetupStatus READ runtimeSetupStatus NOTIFY runtimeSetupChanged)
    Q_PROPERTY(bool protonDownloadInProgress READ protonDownloadInProgress NOTIFY protonDownloadChanged)
    Q_PROPERTY(int protonDownloadProgress READ protonDownloadProgress NOTIFY protonDownloadChanged)
    Q_PROPERTY(QString protonDownloadStatus READ protonDownloadStatus NOTIFY protonDownloadChanged)
    Q_PROPERTY(QString protonLatestRelease READ protonLatestRelease NOTIFY protonLatestReleaseChanged)
    Q_PROPERTY(bool protonReady READ protonReady NOTIFY protonStateChanged)
    Q_PROPERTY(QString protonVersion READ protonVersion NOTIFY protonStateChanged)
    Q_PROPERTY(QVariantList availableProtons READ availableProtons NOTIFY availableProtonsChanged)
    Q_PROPERTY(AppUpdater* appUpdater READ appUpdater CONSTANT)
    Q_PROPERTY(PluginCatalogService* pluginCatalog READ pluginCatalog CONSTANT)

public:
    static CoreController* create(QQmlEngine* engine, QJSEngine* scriptEngine);
    static CoreController& instance();
    static void setCrashReporterMode(bool enabled);

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
    QString activeCatalogSourceId() const { return m_activeSourceIds.value(0); }
    QStringList activeCatalogSourceIds() const { return m_activeSourceIds; }
    int pluginCount() const;
    QString pluginsUserDir() const;
    QString pluginsBundleDir() const;
    QString lastPluginError() const { return m_lastPluginError; }
    bool gameRunning() const { return !m_runningGameId.isEmpty(); }
    QString runningGameId() const { return m_runningGameId; }
    QString runningGameTitle() const { return m_runningGameTitle; }
    QString runningGameCoverUrl() const { return m_runningGameCoverUrl; }
    bool runtimeSetupInProgress() const { return m_runtimeSetupInProgress; }
    QString runtimeSetupGameId() const { return m_runtimeSetupGameId; }
    QString runtimeSetupTitle() const { return m_runtimeSetupTitle; }
    QString runtimeSetupCoverUrl() const { return m_runtimeSetupCoverUrl; }
    QString runtimeSetupStatus() const { return m_runtimeSetupStatus; }
    bool protonDownloadInProgress() const;
    int protonDownloadProgress() const;
    QString protonDownloadStatus() const;
    QString protonLatestRelease() const;
    bool protonReady() const;
    QString protonVersion() const;
    QVariantList availableProtons() const;
    AppUpdater* appUpdater() { return m_appUpdater; }
    PluginCatalogService* pluginCatalog() { return m_pluginCatalog; }

    Q_INVOKABLE QVariantList pluginEntries() const;
    Q_INVOKABLE void browsePluginArach();
    Q_INVOKABLE bool installPluginArach(const QUrl& fileUrl);
    Q_INVOKABLE bool uninstallPlugin(const QString& pluginId);
    Q_INVOKABLE void refreshOfficialPlugins();
    Q_INVOKABLE void installOfficialPlugin(const QString& pluginId);
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
    Q_INVOKABLE void setGameAutoUpdate(const QString& entryId, bool enabled);
    Q_INVOKABLE void setGameLaunchArgs(const QString& entryId, const QString& args);
    Q_INVOKABLE void setGameExecutableOverride(const QString& entryId, const QString& path);
    Q_INVOKABLE void setGameProtonId(const QString& entryId, const QString& protonId);
    Q_INVOKABLE void refreshAvailableProtons();
    Q_INVOKABLE void moveProtonInPriority(const QString& protonId, int delta);
    Q_INVOKABLE QString protonNameForId(const QString& protonId) const;
    Q_INVOKABLE void downloadProtonGe();
    Q_INVOKABLE void refreshProtonLatestRelease();
    Q_INVOKABLE bool needsProtonOnPlatform() const;
    Q_INVOKABLE bool ensureProtonReady();
    Q_INVOKABLE bool needsInstallLocationChoice() const;
    Q_INVOKABLE QString browseGameExecutable(const QString& currentPath = {});
    Q_INVOKABLE QString browseStorageFolder();
    Q_INVOKABLE QVariantMap gameRuntimeContainerInfo(const QString& gameId) const;
    Q_INVOKABLE void openGameRuntimeContainer(const QString& gameId);
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
    Q_INVOKABLE bool canRetryJobInstall(const QString& jobId) const;
    Q_INVOKABLE bool canManualInstallJob(const QString& jobId) const;
    Q_INVOKABLE void openJobDownloadFolder(const QString& jobId);
    Q_INVOKABLE void confirmManualInstall(const QString& jobId);
    Q_INVOKABLE QString browseInstallFolder(const QString& startPath = {});
    Q_INVOKABLE void clearFinishedJobs();
    Q_INVOKABLE void markNotificationsRead();
    Q_INVOKABLE void clearNotifications();
    Q_INVOKABLE void refreshCatalog(const QString& sourceId);
    Q_INVOKABLE void setActiveCatalogSource(const QString& sourceId);
    Q_INVOKABLE bool isCatalogSourceSelected(const QString& sourceId) const;
    Q_INVOKABLE void toggleCatalogSource(const QString& sourceId);
    Q_INVOKABLE void applyCatalogSearch(const QString& query);
    Q_INVOKABLE void refreshSelectedCatalogs();
    Q_INVOKABLE void pruneDisabledCatalogSources();
    Q_INVOKABLE void selectCatalogSource(const QString& sourceId, const QString& query = {});
    Q_INVOKABLE void clearCatalogView();
    Q_INVOKABLE int catalogEntryCount(const QString& sourceId) const;
    Q_INVOKABLE void prefetchCatalogCounts();
    Q_INVOKABLE void validateHydraCatalogUrl(const QString& requestId, const QString& url);
    Q_INVOKABLE void invalidateSourceCatalog(const QString& sourceId);
    Q_INVOKABLE void openExternalUrl(const QString& url);
    Q_INVOKABLE QString applicationDataPath() const;
    Q_INVOKABLE bool clearApplicationData();
    Q_INVOKABLE bool hasPendingCrashReport() const;
    Q_INVOKABLE QString pendingCrashSummary() const;
    Q_INVOKABLE QString pendingCrashDetails() const;
    Q_INVOKABLE QString pendingCrashReportPath() const;
    Q_INVOKABLE void dismissPendingCrashReport();
    Q_INVOKABLE void openPendingCrashIssue();
    Q_INVOKABLE void revealPendingCrashReport();
    Q_INVOKABLE void copyPendingCrashReport();
    Q_INVOKABLE void requestCatalogCover(const QString& entryId);
    Q_INVOKABLE void cancelCatalogCover(const QString& entryId);
    Q_INVOKABLE void invalidateCatalogCover(const QString& entryId);
    Q_INVOKABLE void enrichCatalogEntry(const QString& entryId);
    void prepareShutdown();

signals:
    void userNoticeChanged();
    void catalogLoadingChanged();
    void catalogStatusChanged();
    void activeCatalogSourceIdChanged();
    void activeCatalogSourceIdsChanged();
    void catalogCountsChanged();
    void hydraCatalogUrlValidated(const QString& requestId, bool ok, int count,
                                  const QString& error);
    void pluginsChanged();
    void lastPluginErrorChanged();
    void runningGameChanged();
    void runtimeSetupChanged();
    void protonDownloadChanged();
    void protonLatestReleaseChanged();
    void protonStateChanged();
    void availableProtonsChanged();
    void entryMetadataChanged(const QString& entryId);

private:
    explicit CoreController(QObject* parent = nullptr);

    void initializeServices();
    QString sourceWebsiteFor(const QString& sourceId) const;
    void applyMetadataToEntry(CatalogEntry& entry, const GameMetadata& metadata) const;
    void syncSourcesFromPlugins();
    void persistSourcesToSettings();
    void applyPluginCatalog(const QString& sourceId, QVector<CatalogEntry> entries);
    void onCatalogReady();
    void runAutoInstallUpdates();
    void syncLibraryFromStore();
    void syncProtonCatalog();
    void applyCatalogFilter(const QString& query);
    void commitCatalogLoad(const QString& sourceId, QVector<CatalogEntry> entries);
    void storeCatalogForSource(const QString& sourceId, QVector<CatalogEntry> entries);
    void rebuildMergedCatalog();
    void requestCatalogLoad(const QString& sourceId);
    void processCatalogLoadQueue();
    void loadCatalogSourceNow(const QString& sourceId);
    void updateCatalogLoadingState();
    void syncActiveSourceSignals();
    static void normalizeCatalogSourceIds(QVector<CatalogEntry>& entries, const QString& sourceId);
    void startNextCatalogPrefetch();
    void prefetchPluginCatalogCount(const QString& sourceId);
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
    void commitInstalledCatalogGame(const CatalogEntry& entryHint, const QString& sourceId,
                                    const QString& savePath, const QString& libraryId,
                                    const QString& installPath, InstallKind installKind);
    QString resolveAddonArtifactPath(const QString& parentEntryId, const QString& addonId) const;
    std::optional<CatalogEntry> resolveCatalogEntry(const QString& entryId,
                                                    const QString& sourceId,
                                                    const JobEntry* jobHint = nullptr) const;
    bool gameNeedsInstall(const QString& entryId) const;
    void retryPendingInstalls();
    void pruneBrokenLibraryEntries();
    void migratePollutedEntryIds();
    void pruneAddonLibraryEntries();
    void restoreLibraryPlaceholders();
    void ensureLibraryPlaceholder(const CatalogEntry& entry, const QString& libraryId,
                                  const QStringList& selectedAddonIds = {});
    void reconcileJobInstallState();
    void removeJobsForEntry(const QString& entryId);
    void pruneUnselectedAddonJobs(const QString& parentEntryId, const QStringList& selectedAddonIds);
    void pruneCancelledAddonJobs();
    void markGameRunning(const LibraryGame& game, qint64 processId);
    void touchLastPlayed(const QString& gameId);
    void clearRunningGame();
    void pollRunningGame();
    const JobEntry* findLatestJobForEntry(const QString& entryId) const;
    bool entryHasActiveJob(const QString& entryId) const;
    void showNotice(const QString& message, bool addToHistory = true);
    void setCatalogLoading(bool loading);
    void setCatalogStatus(const QString& status);
    bool isRemoteUploadDateNewer(const QString& remote, const QString& local) const;
    bool gameHasUpdate(const LibraryGame& game, const CatalogEntry& remote) const;
    int recalculateLibraryUpdates(bool notify);
    const CatalogEntry* findCatalogEntry(const QString& entryId) const;
    std::optional<CatalogEntry> resolveCatalogEntry(const QString& entryId) const;
    const CatalogComponent* findCatalogAddon(const CatalogEntry& entry,
                                             const QString& addonId) const;
    void syncEntryToCatalogModel(const QString& entryId);
    InstallKind detectInstallKindForEntry(const QString& sourceId,
                                          const QString& downloadPath) const;
    bool hasInstallHandlerForPath(const QString& sourceId, const QString& downloadPath) const;
    void offerManualInstallForJob(const JobEntry& job);
    void syncCatalogInstallKind(const QString& entryId, InstallKind kind);
    void syncInstallKindProbeSuspension();
    void applyCachedMetadata(CatalogEntry& entry) const;
    void enrichLibraryGameCover(LibraryGame& game) const;
    bool ensureRuntimeDependenciesForGame(const LibraryGame& game);
    void setRuntimeSetupActive(const LibraryGame& game, const QString& status);
    void clearRuntimeSetup();
    void launchGameAfterRuntimeSetup(const QString& gameId);
    void warmCatalogCovers(const QString& sourceId, const QString& query, int limit);
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
    CatalogFeedLoader* m_catalogProbeLoader = nullptr;
    CatalogFeedLoader* m_catalogValidateLoader = nullptr;
    GameMetadataService* m_metadataService = nullptr;
    CoverImageCache* m_coverCache = nullptr;
    TorrentSession* m_torrentSession = nullptr;
    HttpDownloadSession* m_httpSession = nullptr;
    JobOrchestrator* m_jobOrchestrator = nullptr;
    PluginHost* m_pluginHost = nullptr;
    InstallAnalyzer* m_installAnalyzer = nullptr;
    InstallKindProbeService* m_installKindProbe = nullptr;
    RuntimeDependencyService* m_runtimeDependencyService = nullptr;
    ProtonManager* m_protonManager = nullptr;
    AppUpdater* m_appUpdater = nullptr;
    PluginCatalogService* m_pluginCatalog = nullptr;

    QVector<CatalogEntry> m_catalogCache;
    QHash<QString, QVector<CatalogEntry>> m_catalogBySource;
    QHash<QString, int> m_catalogCounts;
    QStringList m_catalogPrefetchQueue;
    QStringList m_activeSourceIds;
    QStringList m_catalogLoadQueue;
    QSet<QString> m_loadingSourceIds;
    bool m_catalogHttpLoadActive = false;
    QHash<QString, QSet<QString>> m_coverWaiters;
    QString m_activeQuery;
    QString m_userNotice;
    int m_userNoticeSerial = 0;
    QString m_catalogStatus;
    QString m_lastPluginError;
    QString m_runningGameId;
    QString m_runningGameTitle;
    QString m_runningGameCoverUrl;
    qint64 m_runningProcessId = 0;
    bool m_runtimeSetupInProgress = false;
    QString m_runtimeSetupGameId;
    QString m_runtimeSetupTitle;
    QString m_runtimeSetupCoverUrl;
    QString m_runtimeSetupStatus;
    QTimer* m_runningGameTimer = nullptr;
    bool m_catalogLoading = false;
    bool m_applicationDataCleared = false;
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
