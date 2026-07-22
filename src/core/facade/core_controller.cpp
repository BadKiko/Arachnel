#include "core_controller_impl.h"

#include "crash_log.h"

#include <QDesktopServices>

#include "catalog_feed_loader.h"
#include "catalog_controller.h"
#include "catalog_cover_coordinator.h"
#include "catalog_filter_service.h"
#include "catalog_parser.h"
#include "cover_image_cache.h"
#include "file_utils.h"
#include "i18n.h"
#include "install_analyzer.h"
#include "install_heuristics.h"
#include "install_kind_probe_service.h"
#include "install_session_service.h"
#include "game_metadata_service.h"
#include "http_download_session.h"
#include "job_orchestrator.h"
#include "job_status.h"
#include "job_store.h"
#include "launch_resolver.h"
#include "library_store.h"
#include "library_controller.h"
#include "library_maintenance_service.h"
#include "game_update_service.h"
#include "launch_controller.h"
#include "plugin_host.h"
#include "plugin_catalog_service.h"
#include "plugin_interface.h"
#include "process_launcher.h"
#include "process_tracker.h"
#include "runtime_dependency_service.h"
#include "proton_manager.h"
#include "windows_runner.h"
#include "app_updater.h"
#include "settings_store.h"
#include "storage_library_model.h"
#include "torrent_session.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJSEngine>
#include <QQmlEngine>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>
#include <QtQml/qqml.h>

#include <QStandardPaths>

#if defined(Q_OS_WIN)
#include <objbase.h>
#include <shobjidl.h>
#else
#include <QFileDialog>
#endif

namespace arachnel::core {

namespace {

bool catalogCacheHasPollutedIds(const QVector<CatalogEntry>& entries)
{
    for (const auto& entry : entries) {
        if (entry.id.startsWith(QStringLiteral("count:")))
            return true;
    }
    return false;
}

QStringList variantListToStringList(const QVariantList& values)
{
    QStringList result;
    result.reserve(values.size());
    for (const QVariant& value : values) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty())
            result.append(text);
    }
    return result;
}

bool g_crashReporterMode = false;

} // namespace

CoreController* CoreController::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    return &instance();
}

CoreController& CoreController::instance()
{
    static CoreController controller;
    return controller;
}

bool CoreController::catalogLoading() const
{
    return m_catalogController && m_catalogController->catalogLoading();
}

QString CoreController::catalogStatus() const
{
    return m_catalogController ? m_catalogController->catalogStatus() : QString();
}

QString CoreController::activeCatalogSourceId() const
{
    return m_catalogController ? m_catalogController->activeCatalogSourceId() : QString();
}

QStringList CoreController::activeCatalogSourceIds() const
{
    return m_catalogController ? m_catalogController->activeCatalogSourceIds() : QStringList();
}

bool CoreController::gameRunning() const
{
    return m_launchController && m_launchController->gameRunning();
}

QString CoreController::runningGameId() const
{
    return m_launchController ? m_launchController->runningGameId() : QString();
}

QString CoreController::runningGameTitle() const
{
    return m_launchController ? m_launchController->runningGameTitle() : QString();
}

QString CoreController::runningGameCoverUrl() const
{
    return m_launchController ? m_launchController->runningGameCoverUrl() : QString();
}

void CoreController::setCrashReporterMode(bool enabled)
{
    g_crashReporterMode = enabled;
}

CoreController::CoreController(QObject* parent)
    : QObject(parent)
{
    if (g_crashReporterMode)
        return;

    m_settings.load();
    m_jobStore.load();
    m_libraryStore.load();
    {
        QVector<LibraryGame> games = m_libraryStore.games();
        bool cleared = false;
        for (auto& game : games) {
            if (!game.hasUpdate)
                continue;
            game.hasUpdate = false;
            cleared = true;
        }
        if (cleared)
            m_libraryStore.setGames(games);
    }
    m_pluginHost = new PluginHost(this);
    m_pluginHost->scan();
    logDiagnostic(QStringLiteral("Core CatalogEntry=%1 bytes").arg(sizeof(CatalogEntry)));
    m_installAnalyzer = new InstallAnalyzer(m_pluginHost);
    m_installKindProbe = new InstallKindProbeService(m_installAnalyzer, this);
    connect(m_installKindProbe, &InstallKindProbeService::installKindResolved, this,
            [this](const QString& entryId, InstallKind kind) {
                syncCatalogInstallKind(entryId, kind);
            });
    initializeServices();
    connect(m_pluginHost, &PluginHost::pluginsChanged, this, [this]() {
        syncSourcesFromPlugins();
        pruneDisabledCatalogSources();
        reconcileJobInstallState();
        emit pluginsChanged();
    });
    syncSourcesFromPlugins();
    emit pluginsChanged();
    m_libraryMaintenance->migratePollutedEntryIds();
    m_libraryMaintenance->pruneBrokenLibraryEntries();
    pruneAddonLibraryEntries();
    m_libraryMaintenance->runStartupMaintenance();
    connect(&m_sources, &SourcePluginModel::sourcesChanged, this,
            &CoreController::persistSourcesToSettings);
    syncLibraryFromStore();
    if (m_libraryController)
        m_libraryController->scanInstalledGames();

    m_catalog.bindSource(&m_catalogCache);
    if (!m_catalogFilters) {
        m_catalogFilters = new CatalogFilterService(&m_catalog, this);
        m_catalogFilters->setCache(&m_catalogCache);
        connect(m_catalogFilters, &CatalogFilterService::filtersChanged, this,
                &CoreController::catalogFiltersChanged);
        connect(m_catalogFilters, &CatalogFilterService::availableGenresChanged, this,
                &CoreController::availableCatalogGenresChanged);
    }

    const QString firstSource = m_sources.firstEnabledId();
    if (!firstSource.isEmpty() && m_catalogController)
        m_catalogController->setActiveCatalogSource(firstSource);

    if (m_settings.autoCheckAppUpdates() && m_appUpdater) {
        QTimer::singleShot(4000, this, [this]() {
            if (m_appUpdater)
                m_appUpdater->checkForUpdates(false);
        });
    }

    QTimer::singleShot(5000, this, [this]() { scheduleOfficialPluginAutoUpdate(); });
}


} // namespace arachnel::core
