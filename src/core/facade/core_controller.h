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

class CatalogFilterService;
class CatalogController;
class CatalogCoverCoordinator;
class CatalogFeedLoader;
class CoverImageCache;
class GameMetadataService;
class HttpDownloadSession;
class JobOrchestrator;
class InstallKindProbeService;
class InstallAnalyzer;
class InstallSessionService;
class LibraryController;
class LibraryMaintenanceService;
class GameUpdateService;
class LaunchController;
class PluginHost;
class ProtonManager;
class RuntimeDependencyService;
class TorrentSession;

/** QML singleton façade (`Arachnel.Core`). Bodies live in domain TUs. */
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
    Q_PROPERTY(bool pluginInstallBusy READ pluginInstallBusy NOTIFY pluginInstallBusyChanged)
    Q_PROPERTY(bool pluginAutoUpdating READ pluginAutoUpdating NOTIFY pluginAutoUpdatingChanged)
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
    Q_PROPERTY(int catalogTypeFilter READ catalogTypeFilter WRITE setCatalogTypeFilter NOTIFY catalogFiltersChanged)
    Q_PROPERTY(int catalogSizeFilter READ catalogSizeFilter WRITE setCatalogSizeFilter NOTIFY catalogFiltersChanged)
    Q_PROPERTY(int catalogRecencyFilter READ catalogRecencyFilter WRITE setCatalogRecencyFilter NOTIFY catalogFiltersChanged)
    Q_PROPERTY(bool catalogHasAddonsFilter READ catalogHasAddonsFilter WRITE setCatalogHasAddonsFilter NOTIFY catalogFiltersChanged)
    Q_PROPERTY(QString catalogGenreFilter READ catalogGenreFilter WRITE setCatalogGenreFilter NOTIFY catalogFiltersChanged)
    Q_PROPERTY(int catalogPlayModeFilter READ catalogPlayModeFilter WRITE setCatalogPlayModeFilter NOTIFY catalogFiltersChanged)
    Q_PROPERTY(int catalogActiveFilterCount READ catalogActiveFilterCount NOTIFY catalogFiltersChanged)
    Q_PROPERTY(QStringList availableCatalogGenres READ availableCatalogGenres NOTIFY availableCatalogGenresChanged)

public:
    static CoreController* create(QQmlEngine*, QJSEngine*);
    static CoreController& instance();
    static void setCrashReporterMode(bool);

    LibraryModel* library() { return &m_library; }
    SourcePluginModel* sources() { return &m_sources; }
    CatalogModel* catalog() { return &m_catalog; }
    JobModel* jobs() { return &m_jobs; }
    NotificationModel* notifications() { return &m_notifications; }
    SettingsStore* settings() { return &m_settings; }
    AppUpdater* appUpdater() { return m_appUpdater; }
    PluginCatalogService* pluginCatalog() { return m_pluginCatalog; }

    QString userNotice() const { return m_userNotice; }
    int userNoticeSerial() const { return m_userNoticeSerial; }
    bool catalogLoading() const;
    QString catalogStatus() const;
    QString activeCatalogSourceId() const;
    QStringList activeCatalogSourceIds() const;
    int pluginCount() const;
    QString pluginsUserDir() const;
    QString pluginsBundleDir() const;
    QString lastPluginError() const { return m_lastPluginError; }
    bool pluginInstallBusy() const { return m_pluginInstallBusy; }
    bool pluginAutoUpdating() const { return m_autoUpdatingOfficialPlugins; }
    bool gameRunning() const;
    QString runningGameId() const;
    QString runningGameTitle() const;
    QString runningGameCoverUrl() const;
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

    int catalogTypeFilter() const;
    void setCatalogTypeFilter(int);
    int catalogSizeFilter() const;
    void setCatalogSizeFilter(int);
    int catalogRecencyFilter() const;
    void setCatalogRecencyFilter(int);
    bool catalogHasAddonsFilter() const;
    void setCatalogHasAddonsFilter(bool);
    QString catalogGenreFilter() const;
    void setCatalogGenreFilter(const QString&);
    int catalogPlayModeFilter() const;
    void setCatalogPlayModeFilter(int);
    int catalogActiveFilterCount() const;
    QStringList availableCatalogGenres() const;

#include "core_controller_api.h"

signals:
    void userNoticeChanged();
    void catalogLoadingChanged();
    void catalogStatusChanged();
    void activeCatalogSourceIdChanged();
    void activeCatalogSourceIdsChanged();
    void catalogCountsChanged();
    void hydraCatalogUrlValidated(const QString&, bool, int, const QString&);
    void pluginsChanged();
    void lastPluginErrorChanged();
    void pluginInstallBusyChanged();
    void pluginAutoUpdatingChanged();
    void runningGameChanged();
    void runtimeSetupChanged();
    void protonDownloadChanged();
    void protonLatestReleaseChanged();
    void protonStateChanged();
    void availableProtonsChanged();
    void entryMetadataChanged(const QString&);
    void catalogFiltersChanged();
    void availableCatalogGenresChanged();

private:
#include "core_controller_p.h"
};

void registerCoreTypes();

} // namespace arachnel::core
