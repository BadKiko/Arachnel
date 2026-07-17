#include "core_controller.h"

#include "../crash_log.h"

#include <QDesktopServices>

#include "catalog_feed_loader.h"
#include "catalog_parser.h"
#include "cover_image_cache.h"
#include "file_utils.h"
#include "i18n.h"
#include "install_analyzer.h"
#include "install_heuristics.h"
#include "install_kind_probe_service.h"
#include "game_metadata_service.h"
#include "http_download_session.h"
#include "job_orchestrator.h"
#include "job_status.h"
#include "job_store.h"
#include "launch_resolver.h"
#include "library_store.h"
#include "plugin_host.h"
#include "plugin_catalog_service.h"
#include "plugin_interface.h"
#include "process_launcher.h"
#include "process_tracker.h"
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
    migratePollutedEntryIds();
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
        reconcileJobInstallState();
        emit pluginsChanged();
    });
    syncSourcesFromPlugins();
    emit pluginsChanged();
    pruneBrokenLibraryEntries();
    pruneAddonLibraryEntries();
    reconcileJobInstallState();
    restoreLibraryPlaceholders();
    pruneCancelledAddonJobs();
    retryPendingInstalls();
    connect(&m_sources, &SourcePluginModel::sourcesChanged, this,
            &CoreController::persistSourcesToSettings);
    syncLibraryFromStore();

    m_runningGameTimer = new QTimer(this);
    m_runningGameTimer->setInterval(1500);
    connect(m_runningGameTimer, &QTimer::timeout, this, &CoreController::pollRunningGame);

    const QString firstSource = m_sources.firstEnabledId();
    if (!firstSource.isEmpty()) {
        m_activeSourceIds = {firstSource};
        emit activeCatalogSourceIdsChanged();
        emit activeCatalogSourceIdChanged();
        requestCatalogLoad(firstSource);
    }

    if (m_settings.autoCheckAppUpdates() && m_appUpdater) {
        QTimer::singleShot(4000, this, [this]() {
            if (m_appUpdater)
                m_appUpdater->checkForUpdates(false);
        });
    }
}

void CoreController::initializeServices()
{
    m_catalogLoader = new CatalogFeedLoader(this);
    m_metadataService = new GameMetadataService(this);
    m_coverCache = new CoverImageCache(this);
    m_torrentSession = new TorrentSession(this);
    m_httpSession = new HttpDownloadSession(this);
    m_jobOrchestrator = new JobOrchestrator(&m_settings, &m_jobStore, m_torrentSession,
                                            m_httpSession, &m_jobs, this);
    m_jobOrchestrator->restoreJobs();
    connect(&m_jobs, &JobModel::jobsChanged, this, &CoreController::syncInstallKindProbeSuspension);
    syncInstallKindProbeSuspension();
    m_protonManager = new ProtonManager(this);
    m_appUpdater = new AppUpdater(this);
    m_pluginCatalog = new PluginCatalogService(this);
    connect(m_pluginCatalog, &PluginCatalogService::installFinished, this,
            [this](const QString& pluginId, bool ok, const QString& detail) {
                if (!ok) {
                    showNotice(detail.isEmpty()
                                   ? QCoreApplication::translate("Core", "Plugin install failed")
                                   : detail);
                    return;
                }
                // detail is temp .arach path on success from catalog service
                if (detail.endsWith(QStringLiteral(".arach"), Qt::CaseInsensitive)
                    || QFileInfo::exists(detail)) {
                    if (installPluginArach(QUrl::fromLocalFile(detail)))
                        QFile::remove(detail);
                } else {
                    showNotice(QCoreApplication::translate("Core", "Plugin installed: %1").arg(pluginId));
                }
            });
    connect(m_appUpdater, &AppUpdater::updateCheckFinished, this,
            [this](bool available, const QString& latestVersion) {
                if (!available)
                    return;
                // History only — AppUpdateSheet in QML owns the prompt UX.
                m_notifications.add(
                    QCoreApplication::translate("Core", "Arachnel %1 is available").arg(latestVersion),
                    QStringLiteral("info"));
            });
    connect(m_appUpdater, &AppUpdater::installerLaunchRequested, this, [this]() {
        prepareShutdown();
        QTimer::singleShot(300, qApp, []() { QCoreApplication::quit(); });
    });
    connect(m_protonManager, &ProtonManager::downloadStateChanged, this,
            &CoreController::protonDownloadChanged);
    connect(m_protonManager, &ProtonManager::downloadFinished, this,
            [this](bool success, const QString& error) {
                emit protonDownloadChanged();
                emit availableProtonsChanged();
                emit protonStateChanged();
                if (success) {
                    syncProtonCatalog();
                    if (m_settings.defaultProtonId().isEmpty()) {
                        const QString latest = m_protonManager->latestGeReleaseName();
                        for (const ProtonEntry& entry :
                             m_protonManager->availableEntries(true)) {
                            if (entry.name == latest) {
                                m_settings.setDefaultProtonId(entry.id);
                                break;
                            }
                        }
                    }
                    showNotice(QCoreApplication::translate("Core", "Proton-GE installed"));
                } else if (!error.isEmpty()) {
                    showNotice(QCoreApplication::translate("Core", "Proton-GE download failed: %1")
                                   .arg(error));
                }
            });
    connect(m_protonManager, &ProtonManager::versionsChanged, this, [this]() {
        syncProtonCatalog();
        emit availableProtonsChanged();
        emit protonStateChanged();
    });
    connect(m_protonManager, &ProtonManager::availableEntriesChanged, this,
            &CoreController::availableProtonsChanged);
    connect(m_protonManager, &ProtonManager::latestGeReleaseChanged, this,
            &CoreController::protonLatestReleaseChanged);
    connect(&m_settings, &SettingsStore::defaultProtonIdChanged, this,
            &CoreController::protonStateChanged);
    connect(&m_settings, &SettingsStore::protonPriorityChanged, this,
            &CoreController::protonStateChanged);

#if defined(Q_OS_LINUX)
    if (m_protonManager) {
        m_protonManager->refreshLatestGeRelease();
        syncProtonCatalog();
    }
#endif

    connect(m_catalogLoader, &CatalogFeedLoader::feedLoaded, this,
            [this](const QString& sourceId, const QVector<CatalogEntry> entries) {
                m_catalogHttpLoadActive = false;
                storeCatalogForSource(sourceId, entries);
                processCatalogLoadQueue();
            });

    connect(m_catalogLoader, &CatalogFeedLoader::feedFailed, this,
            [this](const QString& sourceId, const QString& error) {
                m_catalogHttpLoadActive = false;
                m_loadingSourceIds.remove(sourceId);
                if (m_activeSourceIds.contains(sourceId))
                    showNotice(QCoreApplication::translate("Core", "Catalog error: %1").arg(error));
                rebuildMergedCatalog();
                processCatalogLoadQueue();
            });

    m_catalogProbeLoader = new CatalogFeedLoader(this);
    connect(m_catalogProbeLoader, &CatalogFeedLoader::feedLoaded, this,
            [this](const QString& tag, const QVector<CatalogEntry> entries) {
                if (!tag.startsWith(QStringLiteral("count:")))
                    return;

                const QString sourceId = tag.mid(6);
                m_catalogCounts.insert(sourceId, entries.size());
                emit catalogCountsChanged();
                if (m_activeSourceIds.contains(sourceId) && !m_catalogBySource.contains(sourceId))
                    requestCatalogLoad(sourceId);
                startNextCatalogPrefetch();
            });
    connect(m_catalogProbeLoader, &CatalogFeedLoader::feedFailed, this,
            [this](const QString& tag, const QString& error) {
                if (tag.startsWith(QStringLiteral("count:"))) {
                    const QString sourceId = tag.mid(6);
                    m_catalogCounts.insert(sourceId, -1);
                    emit catalogCountsChanged();
                    startNextCatalogPrefetch();
                }
            });

    m_catalogValidateLoader = new CatalogFeedLoader(this);
    connect(m_catalogValidateLoader, &CatalogFeedLoader::feedLoaded, this,
            [this](const QString& tag, const QVector<CatalogEntry> entries) {
                if (!tag.startsWith(QStringLiteral("validate:")))
                    return;
                const QString requestId = tag.mid(9);
                emit hydraCatalogUrlValidated(requestId, true, entries.size(), {});
            });
    connect(m_catalogValidateLoader, &CatalogFeedLoader::feedFailed, this,
            [this](const QString& tag, const QString& error) {
                if (!tag.startsWith(QStringLiteral("validate:")))
                    return;
                const QString requestId = tag.mid(9);
                emit hydraCatalogUrlValidated(requestId, false, 0, error);
            });

    connect(m_metadataService, &GameMetadataService::coverReady, this,
            [this](const QString& entryId, const QString& coverUrl) {
                if (coverUrl.isEmpty()) {
                    applyCoverToEntry(entryId, QString());
                    return;
                }
                if (coverUrl.startsWith(QStringLiteral("file:"))) {
                    applyCoverToEntry(entryId, coverUrl);
                    return;
                }
                ensureDiskCover(entryId, coverUrl);
            });

    connect(m_metadataService, &GameMetadataService::metadataReady, this,
            [this](const QString& entryId, const GameMetadata& metadata) {
                for (auto& entry : m_catalogCache) {
                    if (entry.id != entryId)
                        continue;
                    applyMetadataToEntry(entry, metadata);
                    syncEntryToCatalogModel(entryId);
                    break;
                }
                if (!metadata.coverUrl.isEmpty())
                    ensureDiskCover(entryId, metadata.coverUrl);
                else
                    applyCoverToEntry(entryId, QString());
                emit entryMetadataChanged(entryId);
            });

    connect(m_coverCache, &CoverImageCache::ready, this,
            [this](const QString& remoteUrl, const QString& localUrl) {
                const QSet<QString> waiters = m_coverWaiters.take(remoteUrl);
                for (const QString& entryId : waiters)
                    applyCoverToEntry(entryId, localUrl);

                for (auto& entry : m_catalogCache) {
                    if (entry.coverUrl == remoteUrl) {
                        entry.coverUrl = localUrl;
                        syncEntryToCatalogModel(entry.id);
                    }
                }
            });

    connect(m_coverCache, &CoverImageCache::failed, this,
            [this](const QString& remoteUrl) {
                const QSet<QString> waiters = m_coverWaiters.take(remoteUrl);
                for (const QString& entryId : waiters)
                    applyCoverToEntry(entryId, QString());
            });

    connect(m_jobOrchestrator, &JobOrchestrator::downloadCompleted, this,
            [this](const QString& jobId, const QString& entryId, const QString& sourceId,
                   const QString& artifactPath, JobKind kind, const QString& libraryId) {
                const JobEntry* job = m_jobStore.jobById(jobId);
                if (job && job->pluginDownload) {
                    const auto entry = resolveCatalogEntry(entryId, sourceId, job);
                    if (!entry) {
                        showNotice(QCoreApplication::translate(
                                       "Core", "Could not find game to install: %1")
                                       .arg(entryId));
                        return;
                    }
                    commitInstalledCatalogGame(*entry, sourceId, job->savePath, libraryId,
                                               artifactPath, entry->installKind);
                    if (GameInstallSession* session = m_installSessions.contains(entryId)
                                                          ? &m_installSessions[entryId]
                                                          : nullptr) {
                        session->gameInstallDone = true;
                        session->installStep = 1;
                        syncInstallSessionPhase(entryId);
                        advanceInstallSession(entryId);
                    } else {
                        m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("completed"),
                                                       QStringLiteral("Installed"));
                    }
                    return;
                }

                if (job && !job->parentEntryId.isEmpty()) {
                    const CatalogEntry* parent = findCatalogEntry(job->parentEntryId);
                    if (!parent) {
                        showNotice(QCoreApplication::translate("Core", "Game not found for add-on"));
                        return;
                    }
                    const CatalogComponent* addon = findCatalogAddon(*parent, entryId);
                    if (!addon) {
                        showNotice(QCoreApplication::translate("Core", "Add-on not found in catalog"));
                        return;
                    }

                    advanceInstallSession(job->parentEntryId);
                    return;
                }

                const JobEntry* jobHint = job;
                const auto entry = resolveCatalogEntry(entryId, sourceId, jobHint);
                if (!entry) {
                    showNotice(QCoreApplication::translate("Core", "Could not find game to install: %1")
                                      .arg(entryId));
                    return;
                }

                InstallContext probeCtx;
                probeCtx.sourceId = sourceId;
                probeCtx.downloadPath = artifactPath;
                if (m_installAnalyzer
                    && m_installAnalyzer->resolveDownload(probeCtx).installerPlugin) {
                    const InstallKind detectedKind =
                        detectInstallKindForEntry(sourceId, artifactPath);
                    syncCatalogInstallKind(entryId, detectedKind);
                    startPluginInstall(*entry, sourceId, artifactPath, kind, libraryId, jobId);
                    return;
                }

                const LibraryGame* existing = m_libraryStore.gameById(entryId);
                const QString installPath =
                    existing && !existing->installPath.isEmpty() ? existing->installPath
                                                                 : QString();
                if (!installPath.isEmpty()) {
                    commitInstalledCatalogGame(*entry, sourceId, artifactPath, libraryId, installPath,
                                               entry->installKind);
                    return;
                }

                ensureLibraryPlaceholder(*entry, libraryId);
                m_jobOrchestrator->setJobPhase(
                    jobId, QStringLiteral("completed"),
                    QCoreApplication::translate("Core", "Download complete — install manually"));
            });

    connect(m_jobOrchestrator, &JobOrchestrator::downloadFailed, this,
            [this](const QString& jobId, const QString& error) {
                Q_UNUSED(jobId)
                showNotice(QCoreApplication::translate("Core", "Download error: %1").arg(error));
            });
}

void CoreController::syncSourcesFromPlugins()
{
    QVector<SourcePluginInfo> merged = m_pluginHost->pluginInfos();
    for (auto& info : merged)
        info.enabled = m_settings.pluginEnabled(info.id, true);

    for (const auto& manual : m_settings.sources()) {
        if (m_pluginHost->hasPlugin(manual.id))
            continue;
        merged.append(manual);
    }

    m_sources.setPlugins(merged);
}

void CoreController::persistSourcesToSettings()
{
    QHash<QString, bool> pluginStates;
    QVector<SourcePluginInfo> manualSources;
    for (const auto& source : m_sources.plugins()) {
        if (source.isPlugin)
            pluginStates.insert(source.id, source.enabled);
        else
            manualSources.append(source);
    }
    m_settings.setPluginEnabledStates(pluginStates);
    m_settings.persistSources(manualSources);
}

void CoreController::applyPluginCatalog(const QString& sourceId,
                                        QVector<CatalogEntry> entries)
{
    storeCatalogForSource(sourceId, std::move(entries));
}

void CoreController::startPluginInstall(const CatalogEntry& entry, const QString& sourceId,
                                        const QString& savePath, JobKind kind,
                                        const QString& libraryId, const QString& jobId)
{
    if (m_installingEntries.contains(entry.id)) {
        showNotice(QCoreApplication::translate("Core", "Installation of %1 is already in progress").arg(entry.title));
        return;
    }

    const QString libId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;

    InstallContext ctx;
    ctx.jobId = jobId;
    ctx.entryId = entry.id;
    ctx.sourceId = sourceId;
    ctx.title = entry.title;
    ctx.targetPath = m_settings.gameDirFor(libId, entry.id);
    ctx.downloadsPath = m_settings.resolvedDownloadsRoot(libId);
    ctx.downloadPath = savePath;
    ctx.magnetUri = entry.magnetUris.value(0);
    ctx.uploadDate = entry.uploadDate;
    fillProtonInstallFields(ctx.entryId,
                            m_settings.resolvedProtonId(QString(), *m_protonManager),
                            &ctx.protonExecutable, &ctx.compatDataPath, &ctx.steamCompatClientPath);

    const InstallPlan plan =
        m_installAnalyzer ? m_installAnalyzer->resolveDownload(ctx) : InstallPlan{};
    if (!plan.installerPlugin) {
        if (!jobId.isEmpty()) {
            if (const JobEntry* job = m_jobStore.jobById(jobId))
                offerManualInstallForJob(*job);
        } else {
            showNotice(QCoreApplication::translate("Core", "Can't install %1 — install a plugin for this source")
                           .arg(entry.title));
        }
        return;
    }

    m_installingEntries.insert(entry.id);

    const InstallKind detectedKind = plan.analysis.kind;
    syncCatalogInstallKind(entry.id, detectedKind);
    ctx.installKind = detectedKind;

    if (!jobId.isEmpty()) {
        if (m_installSessions.contains(entry.id))
            syncInstallSessionPhase(entry.id);
        else
            m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("installing"),
                                           QStringLiteral("Installing…"));
    }

    m_pluginHost->runInstallAsync(plan.installerPlugin, ctx,
                                  [this, entry, sourceId, savePath, kind, libId, jobId,
                                   detectedKind](const InstallResult& result) {
        m_installingEntries.remove(entry.id);

        if (!result.success) {
            const QString detail =
                result.error.isEmpty()
                    ? QStringLiteral("Install failed")
                    : QStringLiteral("Install failed: %1").arg(result.error);
            if (!jobId.isEmpty())
                m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("completed"), detail);
            m_installSessions.remove(entry.id);
            m_installSelectedAddons.remove(entry.id);
            showNotice(QCoreApplication::translate("Core", "Install failed for %1: %2")
                              .arg(entry.title, result.error));
            if (!jobId.isEmpty()) {
                if (const JobEntry* job = m_jobStore.jobById(jobId))
                    offerManualInstallForJob(*job);
            }
            return;
        }

        commitInstalledCatalogGame(entry, sourceId, savePath, libId, result.installPath,
                                   detectedKind);

        if (GameInstallSession* session = m_installSessions.contains(entry.id)
                                              ? &m_installSessions[entry.id]
                                              : nullptr) {
            session->gameInstallDone = true;
            session->installStep = 1;
            syncInstallSessionPhase(entry.id);
            advanceInstallSession(entry.id);
        } else {
            for (const auto& addon : entry.addons) {
                if (isCatalogAddonInstalled(entry.id, addon.id))
                    continue;
                const QString artifactPath = resolveAddonArtifactPath(entry.id, addon.id);
                if (!artifactPath.isEmpty())
                    startPluginAddonInstall(entry, addon, sourceId, artifactPath, jobId);
            }

            if (!jobId.isEmpty())
                m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("completed"),
                                               QStringLiteral("Installed"));
        }

        reconcileJobInstallState();

        if (kind == JobKind::Update)
            showNotice(QCoreApplication::translate("Core", "Update installed: %1").arg(entry.title));
        else
            showNotice(QCoreApplication::translate("Core", "Installed: %1").arg(entry.title));
    });
}

void CoreController::beginInstallSession(const QString& entryId, const QString& gameJobId,
                                         const QString& sourceId, const QStringList& addonIds)
{
    GameInstallSession session;
    session.gameJobId = gameJobId;
    session.sourceId = sourceId;
    session.selectedAddonIds = addonIds;
    session.installTotal = 1 + addonIds.size();
    session.installStep = 1;
    session.gameInstallDone = false;
    m_installSessions.insert(entryId, session);
    m_installSelectedAddons.insert(entryId, addonIds);
}

void CoreController::syncInstallSessionPhase(const QString& entryId)
{
    const GameInstallSession* session = m_installSessions.contains(entryId)
                                            ? &m_installSessions[entryId]
                                            : nullptr;
    if (!session || session->gameJobId.isEmpty())
        return;

    const QString detail = QStringLiteral("Installing (%1/%2)")
                               .arg(qMax(1, session->installStep))
                               .arg(qMax(1, session->installTotal));
    m_jobOrchestrator->setJobPhase(session->gameJobId, QStringLiteral("installing"), detail);
}

void CoreController::advanceInstallSession(const QString& entryId)
{
    auto it = m_installSessions.find(entryId);
    if (it == m_installSessions.end())
        return;

    GameInstallSession& session = *it;
    if (!session.gameInstallDone)
        return;

    const CatalogEntry* parent = findCatalogEntry(entryId);
    if (!parent || !isEntryPlayable(entryId))
        return;

    int installedCount = 1;
    for (const QString& addonId : session.selectedAddonIds) {
        if (isCatalogAddonInstalled(entryId, addonId))
            ++installedCount;
    }

    for (const QString& addonId : session.selectedAddonIds) {
        if (isCatalogAddonInstalled(entryId, addonId))
            continue;

        const QString artifactPath = resolveAddonArtifactPath(entryId, addonId);
        if (artifactPath.isEmpty())
            return;

        const CatalogComponent* addon = findCatalogAddon(*parent, addonId);
        if (!addon)
            continue;

        session.installStep = installedCount + 1;
        syncInstallSessionPhase(entryId);

        startPluginAddonInstall(
            *parent, *addon, session.sourceId, artifactPath, session.gameJobId,
            [this, entryId](bool success) {
                if (!success) {
                    m_installSessions.remove(entryId);
                    m_installSelectedAddons.remove(entryId);
                    return;
                }
                advanceInstallSession(entryId);
            });
        return;
    }

    m_jobOrchestrator->setJobPhase(session.gameJobId, QStringLiteral("completed"),
                                   QStringLiteral("Installed"));
    m_installSessions.remove(entryId);
    m_installSelectedAddons.remove(entryId);
    reconcileJobInstallState();
}

void CoreController::startPluginAddonInstall(const CatalogEntry& parent,
                                             const CatalogComponent& addon,
                                             const QString& sourceId,
                                             const QString& artifactPath,
                                             const QString& progressJobId,
                                             std::function<void(bool success)> done)
{
    const QString installKey = parent.id + QLatin1Char(':') + addon.id;
    if (m_installingAddons.contains(installKey)) {
        showNotice(QCoreApplication::translate("Core", "Add-on installation is already in progress"));
        return;
    }

    const LibraryGame* game = m_libraryStore.gameById(parent.id);
    if (!game || game->installPath.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Install the game first"));
        return;
    }

    ISourcePlugin* plugin = m_pluginHost ? m_pluginHost->plugin(sourceId) : nullptr;
    if (!plugin) {
        showNotice(QCoreApplication::translate("Core", "Plugin not found for %1 — install it in Settings → Plugins")
                       .arg(sourceId));
        return;
    }

    m_installingAddons.insert(installKey);

    const QVariantMap addonJobMap = m_jobs.jobForAddon(parent.id, addon.id);
    const QString addonJobId = addonJobMap.value(QStringLiteral("jobId")).toString();
    if (!addonJobId.isEmpty()) {
        m_jobOrchestrator->setJobPhase(addonJobId, QStringLiteral("installing"),
                                       QStringLiteral("Installing add-on…"));
    } else if (!progressJobId.isEmpty() && !m_installSessions.contains(parent.id)) {
        m_jobOrchestrator->setJobPhase(progressJobId, QStringLiteral("installing"),
                                       QStringLiteral("Installing add-on…"));
    }
    if (m_installSessions.contains(parent.id))
        syncInstallSessionPhase(parent.id);

    AddonInstallContext ctx;
    ctx.parentEntryId = parent.id;
    ctx.addonId = addon.id;
    ctx.addonTitle = addon.title;
    ctx.gameInstallPath = game->installPath;
    ctx.downloadPath = artifactPath;
    ctx.addonKind = addon.kind;
    fillProtonInstallFields(parent.id,
                            m_settings.resolvedProtonId(QString(), *m_protonManager),
                            &ctx.protonExecutable, &ctx.compatDataPath, &ctx.steamCompatClientPath);

    m_pluginHost->runAddonInstallAsync(plugin, ctx,
                                         [this, parent, addon, progressJobId, installKey, done](
                                             const InstallResult& result) {
                                             m_installingAddons.remove(installKey);

                                             if (!result.success) {
                                                 const QString detail =
                                                     result.error.isEmpty()
                                                         ? QStringLiteral("Install failed")
                                                         : QStringLiteral("Install failed: %1")
                                                               .arg(result.error);

                                                 const QVariantMap addonJobMap =
                                                     m_jobs.jobForAddon(parent.id, addon.id);
                                                 const QString addonJobId =
                                                     addonJobMap.value(QStringLiteral("jobId"))
                                                         .toString();
                                                 if (!addonJobId.isEmpty())
                                                     m_jobOrchestrator->setJobPhase(
                                                         addonJobId,
                                                         QStringLiteral("completed"), detail);
                                                 if (!progressJobId.isEmpty()
                                                     && progressJobId != addonJobId)
                                                     m_jobOrchestrator->setJobPhase(
                                                         progressJobId,
                                                         QStringLiteral("completed"), detail);
                                                 m_installSessions.remove(parent.id);
                                                 m_installSelectedAddons.remove(parent.id);
                                                 showNotice(QCoreApplication::translate("Core", "Add-on install failed for %1: %2")
                                                                   .arg(addon.title, result.error));
                                                 if (done)
                                                     done(false);
                                                 return;
                                             }

                                             markCatalogAddonInstalled(parent.id, addon.id,
                                                                       addon.uploadDate);

                                             const QVariantMap successAddonJobMap =
                                                 m_jobs.jobForAddon(parent.id, addon.id);
                                             const QString successAddonJobId =
                                                 successAddonJobMap.value(QStringLiteral("jobId"))
                                                     .toString();
                                             if (!successAddonJobId.isEmpty())
                                                 m_jobOrchestrator->setJobPhase(
                                                     successAddonJobId,
                                                     QStringLiteral("completed"),
                                                     QStringLiteral("Installed"));

                                             showNotice(QCoreApplication::translate("Core", "Add-on installed: %1")
                                                               .arg(addon.title));
                                             if (done)
                                                 done(true);
                                         });
}

void CoreController::commitInstalledCatalogGame(const CatalogEntry& entryHint,
                                                const QString& sourceId, const QString& savePath,
                                                const QString& libraryId, const QString& installPath,
                                                InstallKind installKind)
{
    const CatalogEntry* fresh = findCatalogEntry(entryHint.id);
    const CatalogEntry& catalog = fresh ? *fresh : entryHint;
    const QString libId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;

    LibraryGame game;
    const LibraryGame* existing = m_libraryStore.gameById(catalog.id);
    if (existing)
        game = *existing;

    game.id = catalog.id;
    game.title = catalog.title;
    game.coverUrl = catalog.coverUrl;
    game.sourceId = sourceId;
    game.sourceName = m_sources.nameForId(sourceId);
    game.version = catalog.version;
    game.description = catalog.description;
    game.genres = catalog.genres;
    game.sizeLabel = catalog.sizeLabel;
    game.installKind = installKind;
    game.uploadDate = catalog.uploadDate;
    game.magnetUri = catalog.magnetUris.value(0);
    game.downloadPath = savePath;
    game.libraryId = libId;
    game.hasUpdate = false;
    if (!installPath.isEmpty())
        game.installPath = installPath;
    if (!installPath.isEmpty() && game.executableOverride.trimmed().isEmpty()) {
        const QString executable = findGameExecutableInTree(installPath);
        if (!executable.isEmpty())
            game.executableOverride = executable;
    }

    QHash<QString, InstalledComponent> previousComponents;
    for (const auto& component : game.components)
        previousComponents.insert(component.id, component);

    QVector<InstalledComponent> components;
    components.reserve(catalog.addons.size());
    for (const auto& addon : catalog.addons) {
        InstalledComponent component;
        component.id = addon.id;
        component.title = addon.title;
        component.uploadDate = addon.uploadDate;
        if (const auto it = previousComponents.constFind(addon.id); it != previousComponents.cend())
            component.installed = it->installed;
        components.append(component);
    }
    game.components = components;

    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
    if (!m_catalogCache.isEmpty())
        recalculateLibraryUpdates(false);
}

void CoreController::markCatalogAddonInstalled(const QString& parentEntryId,
                                               const QString& addonId, const QString& uploadDate)
{
    const LibraryGame* existing = m_libraryStore.gameById(parentEntryId);
    if (!existing)
        return;

    QString resolvedUploadDate = uploadDate;
    if (const CatalogEntry* parent = findCatalogEntry(parentEntryId)) {
        if (const CatalogComponent* remoteAddon = findCatalogAddon(*parent, addonId)) {
            if (!remoteAddon->uploadDate.isEmpty())
                resolvedUploadDate = remoteAddon->uploadDate;
        }
    }

    LibraryGame game = *existing;
    bool found = false;
    for (auto& component : game.components) {
        if (component.id != addonId)
            continue;
        component.installed = true;
        if (!resolvedUploadDate.isEmpty())
            component.uploadDate = resolvedUploadDate;
        found = true;
        break;
    }

    if (!found) {
        InstalledComponent component;
        component.id = addonId;
        component.installed = true;
        component.uploadDate = resolvedUploadDate;
        game.components.append(component);
    }

    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
    if (!m_catalogCache.isEmpty())
        recalculateLibraryUpdates(false);
}

QString CoreController::resolveAddonArtifactPath(const QString& parentEntryId,
                                                 const QString& addonId) const
{
    const JobEntry* latest = nullptr;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId != addonId || job.parentEntryId != parentEntryId)
            continue;
        if (job.status != QStringLiteral("completed"))
            continue;
        if (!latest || job.completedAt > latest->completedAt)
            latest = &job;
    }
    if (!latest)
        return {};

    if (!latest->artifactPath.isEmpty() && QFileInfo::exists(latest->artifactPath))
        return latest->artifactPath;

    if (!latest->savePath.isEmpty() && QFileInfo::exists(latest->savePath))
        return latest->savePath;

    return {};
}

bool CoreController::isCatalogAddonInstalled(const QString& entryId,
                                             const QString& addonId) const
{
    const LibraryGame* game = m_libraryStore.gameById(entryId);
    if (!game)
        return false;
    for (const auto& component : game->components) {
        if (component.id == addonId)
            return component.installed;
    }
    return false;
}

void CoreController::installDownloadedCatalogAddon(const QString& entryId, const QString& addonId)
{
    const CatalogEntry* parent = findCatalogEntry(entryId);
    if (!parent) {
        showNotice(QCoreApplication::translate("Core", "Game not found"));
        return;
    }

    const CatalogComponent* addon = findCatalogAddon(*parent, addonId);
    if (!addon) {
        showNotice(QCoreApplication::translate("Core", "Add-on not found"));
        return;
    }

    const QString artifactPath = resolveAddonArtifactPath(entryId, addonId);
    if (artifactPath.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Download the add-on first"));
        return;
    }

    const QVariantMap jobMap = m_jobs.jobForAddon(entryId, addonId);
    const QString jobId = jobMap.value(QStringLiteral("jobId")).toString();
    startPluginAddonInstall(*parent, *addon, parent->sourceId, artifactPath, jobId);
}

std::optional<CatalogEntry> CoreController::resolveCatalogEntry(const QString& entryId,
                                                                const QString& sourceId,
                                                                const JobEntry* jobHint) const
{
    if (const CatalogEntry* cached = findCatalogEntry(entryId))
        return *cached;

    if (ISourcePlugin* plugin = m_pluginHost->plugin(sourceId)) {
        if (const auto remote = plugin->entryById(entryId))
            return *remote;
    }

    if (!jobHint)
        return std::nullopt;

    CatalogEntry synthetic;
    synthetic.id = entryId;
    synthetic.sourceId = sourceId;
    synthetic.title = jobHint->title;
    if (synthetic.title.startsWith(QStringLiteral("Downloading ")))
        synthetic.title = synthetic.title.mid(12);
    else if (synthetic.title.startsWith(QStringLiteral("Загрузка ")))
        synthetic.title = synthetic.title.mid(9);
    else if (synthetic.title.startsWith(QStringLiteral("Updating ")))
        synthetic.title = synthetic.title.mid(9);
    else if (synthetic.title.startsWith(QStringLiteral("Обновление ")))
        synthetic.title = synthetic.title.mid(11);
    if (!jobHint->magnetUri.isEmpty())
        synthetic.magnetUris.append(jobHint->magnetUri);
    synthetic.coverUrl = jobHint->coverUrl;
    return synthetic;
}

bool CoreController::gameNeedsInstall(const QString& entryId) const
{
    const LibraryGame* game = m_libraryStore.gameById(entryId);
    if (!game || game->installPath.isEmpty())
        return true;
    return !QFileInfo::exists(game->installPath);
}

const JobEntry* CoreController::findLatestJobForEntry(const QString& entryId) const
{
    const JobEntry* latest = nullptr;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId != entryId)
            continue;
        if (!latest || job.completedAt > latest->completedAt || job.createdAt > latest->createdAt)
            latest = &job;
    }
    return latest;
}

bool CoreController::isEntryPlayable(const QString& entryId) const
{
    const LibraryGame* game = m_libraryStore.gameById(entryId);
    if (!game || game->installPath.isEmpty())
        return false;
    if (!QFileInfo::exists(game->installPath))
        return false;

    LaunchInfo info;
    if (m_pluginHost) {
        if (ISourcePlugin* plugin = m_pluginHost->plugin(game->sourceId))
            info = plugin->launchInfo(*game);
    }

    if (info.executable.isEmpty() && game->executableOverride.trimmed().isEmpty()) {
        const QString found = findGameExecutableInTree(game->installPath);
        if (!found.isEmpty())
            info.executable = found;
    }

    const ResolvedLaunch resolved = resolveLaunch(info, *game, m_settings);
    return !resolved.program.isEmpty() && QFileInfo::exists(resolved.program);
}

bool CoreController::isEntryDownloadComplete(const QString& entryId) const
{
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId == entryId && job.status == QStringLiteral("completed"))
            return true;
    }
    return false;
}

bool CoreController::entryDownloadFilesExist(const QString& entryId) const
{
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId != entryId || job.savePath.isEmpty())
            continue;
        if (QDir(job.savePath).exists())
            return true;
    }

    if (const LibraryGame* game = m_libraryStore.gameById(entryId)) {
        if (!game->downloadPath.isEmpty() && QDir(game->downloadPath).exists())
            return true;
    }

    return false;
}

QVariantMap CoreController::entryDetails(const QString& entryId) const
{
    QVariantMap info = m_library.gameInfo(entryId);
    const QVariantMap catalogInfo = m_catalog.entryInfo(entryId);

    if (info.isEmpty() && catalogInfo.isEmpty())
        return {};

    if (info.isEmpty())
        info = catalogInfo;
    else if (!catalogInfo.isEmpty()) {
        const auto fillIfEmpty = [&info, &catalogInfo](const QString& key) {
            if (!info.value(key).toString().isEmpty())
                return;
            const QVariant value = catalogInfo.value(key);
            if (value.isValid() && !value.toString().isEmpty())
                info.insert(key, value);
        };

        fillIfEmpty(QStringLiteral("description"));
        fillIfEmpty(QStringLiteral("genres"));
        fillIfEmpty(QStringLiteral("sizeLabel"));
        fillIfEmpty(QStringLiteral("coverUrl"));
        fillIfEmpty(QStringLiteral("version"));
        fillIfEmpty(QStringLiteral("uploadDate"));
        fillIfEmpty(QStringLiteral("installKind"));
        fillIfEmpty(QStringLiteral("installKindLabel"));
        fillIfEmpty(QStringLiteral("sourcePageUrl"));
        fillIfEmpty(QStringLiteral("steamAppId"));
        fillIfEmpty(QStringLiteral("trailerUrl"));
        fillIfEmpty(QStringLiteral("trailerThumbnailUrl"));
        if (!info.contains(QStringLiteral("screenshotUrls"))
            || info.value(QStringLiteral("screenshotUrls")).toList().isEmpty()) {
            const QVariantList shots = catalogInfo.value(QStringLiteral("screenshotUrls")).toList();
            if (!shots.isEmpty())
                info.insert(QStringLiteral("screenshotUrls"), shots);
        }
    }

    const QString title = info.value(QStringLiteral("title")).toString();
    if (!title.isEmpty()) {
        const GameMetadata cached = m_metadataService->metadataForTitle(title);
        const QString uiLanguage = m_settings.uiLanguage();
        const bool languageMatches =
            cached.descriptionLanguage.isEmpty()
            || cached.descriptionLanguage.compare(uiLanguage, Qt::CaseInsensitive) == 0;
        if (info.value(QStringLiteral("description")).toString().isEmpty()
            && !cached.description.isEmpty() && languageMatches)
            info.insert(QStringLiteral("description"), cached.description);
        if (info.value(QStringLiteral("genres")).toString().isEmpty() && !cached.genres.isEmpty())
            info.insert(QStringLiteral("genres"), cached.genres);
        if (info.value(QStringLiteral("steamAppId")).toString().isEmpty()
            && !cached.steamAppId.isEmpty())
            info.insert(QStringLiteral("steamAppId"), cached.steamAppId);
        if (info.value(QStringLiteral("trailerUrl")).toString().isEmpty()
            && !cached.trailerUrl.isEmpty())
            info.insert(QStringLiteral("trailerUrl"), cached.trailerUrl);
        if (info.value(QStringLiteral("trailerThumbnailUrl")).toString().isEmpty()
            && !cached.trailerThumbnailUrl.isEmpty())
            info.insert(QStringLiteral("trailerThumbnailUrl"), cached.trailerThumbnailUrl);
        if (!info.contains(QStringLiteral("screenshotUrls"))
            || info.value(QStringLiteral("screenshotUrls")).toList().isEmpty()) {
            if (!cached.screenshotUrls.isEmpty())
                info.insert(QStringLiteral("screenshotUrls"), QVariant::fromValue(cached.screenshotUrls));
        }
    }

    const QString sourceId = info.value(QStringLiteral("sourceId")).toString();
    info.insert(QStringLiteral("sourceWebsiteUrl"), sourceWebsiteFor(sourceId));
    const QString steamAppId = info.value(QStringLiteral("steamAppId")).toString();
    if (!steamAppId.isEmpty()) {
        info.insert(QStringLiteral("steamStoreUrl"),
                    QStringLiteral("https://store.steampowered.com/app/%1/").arg(steamAppId));
    }

    if (const CatalogEntry* catalogEntry = findCatalogEntry(entryId)) {
        info.insert(QStringLiteral("addonCount"), catalogEntry->addons.size());
        info.insert(QStringLiteral("hasAddons"), !catalogEntry->addons.isEmpty());
    } else {
        const int componentCount = info.value(QStringLiteral("componentCount")).toInt();
        info.insert(QStringLiteral("addonCount"), componentCount);
        info.insert(QStringLiteral("hasAddons"), componentCount > 0);
    }

    if (info.value(QStringLiteral("downloadPath")).toString().isEmpty()) {
        if (const JobEntry* job = findLatestJobForEntry(entryId))
            info.insert(QStringLiteral("downloadPath"), job->savePath);
    }

    const QString downloadPath = info.value(QStringLiteral("downloadPath")).toString();
    if (!downloadPath.isEmpty() && !sourceId.isEmpty()) {
        const InstallKind detected = detectInstallKindForEntry(sourceId, downloadPath);
        info.insert(QStringLiteral("installKind"), static_cast<int>(detected));
        info.insert(QStringLiteral("installKindLabel"), installKindLabel(detected));
    }

    info.insert(QStringLiteral("installed"), isEntryPlayable(entryId));
    return info;
}

void CoreController::migratePollutedEntryIds()
{
    bool libraryDirty = false;
    QVector<LibraryGame> games = m_libraryStore.games();
    QSet<QString> seen;
    QVector<LibraryGame> unique;
    unique.reserve(games.size());

    for (auto& game : games) {
        const QString beforeId = game.id;
        game.id = repairCatalogEntryId(game.id);
        const QString beforeDownload = game.downloadPath;
        const QString beforeInstall = game.installPath;
        game.downloadPath.replace(QStringLiteral("count:"), QString());
        game.installPath.replace(QStringLiteral("count:"), QString());

        if (game.id != beforeId || game.downloadPath != beforeDownload
            || game.installPath != beforeInstall)
            libraryDirty = true;

        if (seen.contains(game.id))
            continue;
        seen.insert(game.id);
        unique.append(game);
    }

    if (libraryDirty) {
        m_libraryStore.setGames(unique);
        m_libraryStore.save();
    }

    bool jobsDirty = false;
    QVector<JobEntry> jobs = m_jobStore.jobs();
    for (auto& job : jobs) {
        const QString entryBefore = job.entryId;
        const QString parentBefore = job.parentEntryId;
        const QString pathBefore = job.savePath;
        job.entryId = repairCatalogEntryId(job.entryId);
        job.parentEntryId = repairCatalogEntryId(job.parentEntryId);
        job.savePath.replace(QStringLiteral("count:"), QString());

        if (job.entryId != entryBefore || job.parentEntryId != parentBefore
            || job.savePath != pathBefore)
            jobsDirty = true;
    }

    if (jobsDirty) {
        m_jobStore.setJobs(jobs);
        m_jobStore.save();
    }
}

void CoreController::pruneBrokenLibraryEntries()
{
    QVector<QString> brokenIds;
    for (const LibraryGame& game : m_libraryStore.games()) {
        if (game.installPath.isEmpty())
            continue;
        if (!QFileInfo::exists(game.installPath))
            brokenIds.append(game.id);
    }

    if (brokenIds.isEmpty())
        return;

    for (const QString& id : brokenIds)
        m_libraryStore.removeGame(id);
    m_libraryStore.save();
    syncLibraryFromStore();
}

void CoreController::ensureLibraryPlaceholder(const CatalogEntry& entry, const QString& libraryId,
                                              const QStringList& selectedAddonIds)
{
    const LibraryGame* existing = m_libraryStore.gameById(entry.id);
    if (existing && !existing->installPath.isEmpty()
        && QFileInfo::exists(existing->installPath))
        return;

    const QString libId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;

    LibraryGame game;
    if (existing)
        game = *existing;

    game.id = entry.id;
    game.title = entry.title;
    game.coverUrl = entry.coverUrl;
    game.sourceId = entry.sourceId;
    game.sourceName = m_sources.nameForId(entry.sourceId);
    game.version = entry.version;
    game.description = entry.description;
    game.genres = entry.genres;
    game.sizeLabel = entry.sizeLabel;
    game.installKind = entry.installKind;
    game.uploadDate = entry.uploadDate;
    game.magnetUri = entry.magnetUris.value(0);
    game.libraryId = libId;
    game.hasUpdate = false;
    game.installPath = QString();

    QSet<QString> selectedAddons(selectedAddonIds.cbegin(), selectedAddonIds.cend());
    QVector<InstalledComponent> components;
    components.reserve(entry.addons.size());
    for (const auto& addon : entry.addons) {
        if (!selectedAddons.isEmpty() && !selectedAddons.contains(addon.id))
            continue;

        bool installed = false;
        if (existing) {
            for (const auto& component : existing->components) {
                if (component.id == addon.id) {
                    installed = component.installed;
                    break;
                }
            }
        }

        InstalledComponent component;
        component.id = addon.id;
        component.title = addon.title;
        component.uploadDate = addon.uploadDate;
        component.installed = installed;
        components.append(component);
    }
    game.components = components;

    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();

    if (!entry.coverUrl.isEmpty())
        ensureDiskCover(entry.id, entry.coverUrl);
}

void CoreController::restoreLibraryPlaceholders()
{
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (!job.parentEntryId.isEmpty())
            continue;
        if (!isJobInProgress(job.status))
            continue;

        const auto entry = resolveCatalogEntry(job.entryId, job.sourceId, &job);
        if (!entry)
            continue;

        ensureLibraryPlaceholder(*entry, job.libraryId);
    }
}

void CoreController::pruneAddonLibraryEntries()
{
    QSet<QString> addonIds;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (!job.parentEntryId.isEmpty())
            addonIds.insert(job.entryId);
    }
    for (const auto& entry : m_catalogCache) {
        for (const auto& addon : entry.addons)
            addonIds.insert(addon.id);
    }

    bool changed = false;
    for (const QString& id : addonIds) {
        if (!m_libraryStore.gameById(id))
            continue;
        m_libraryStore.removeGame(id);
        changed = true;
    }

    if (changed) {
        m_libraryStore.save();
        syncLibraryFromStore();
    }
}

void CoreController::reconcileJobInstallState()
{
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (!job.parentEntryId.isEmpty()) {
            if (job.status != QStringLiteral("installing"))
                continue;
            if (isCatalogAddonInstalled(job.parentEntryId, job.entryId)) {
                m_jobOrchestrator->setJobPhase(job.id, QStringLiteral("completed"),
                                               QStringLiteral("Installed"));
                continue;
            }
            if (!resolveAddonArtifactPath(job.parentEntryId, job.entryId).isEmpty()) {
                m_jobOrchestrator->setJobPhase(job.id, QStringLiteral("completed"),
                                               QStringLiteral("Download complete"));
            }
            continue;
        }
        if (job.status != QStringLiteral("completed"))
            continue;
        if (!gameNeedsInstall(job.entryId)) {
            if (job.detail == QStringLiteral("Installation required")
                || job.detail == QStringLiteral("Требуется установка")) {
                m_jobOrchestrator->setJobPhase(job.id, QStringLiteral("completed"),
                                               QStringLiteral("Installed"));
            }
            continue;
        }
        if (job.detail == QStringLiteral("Installed")
            || job.detail == QStringLiteral("Установлено")) {
            m_jobOrchestrator->setJobPhase(job.id, QStringLiteral("completed"),
                                           QStringLiteral("Installation required"));
        }
    }
}

void CoreController::pruneUnselectedAddonJobs(const QString& parentEntryId,
                                              const QStringList& selectedAddonIds)
{
    if (parentEntryId.isEmpty())
        return;

    const QSet<QString> keep(selectedAddonIds.cbegin(), selectedAddonIds.cend());
    QVector<QString> removeIds;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.parentEntryId != parentEntryId)
            continue;
        if (keep.contains(job.entryId))
            continue;
        removeIds.append(job.id);
    }

    for (const QString& jobId : removeIds)
        m_jobOrchestrator->removeJob(jobId);
}

void CoreController::pruneCancelledAddonJobs()
{
    QVector<QString> removeIds;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.parentEntryId.isEmpty())
            continue;
        if (job.status == QStringLiteral("cancelled"))
            removeIds.append(job.id);
    }

    for (const QString& jobId : removeIds)
        m_jobOrchestrator->removeJob(jobId);
}

void CoreController::removeJobsForEntry(const QString& entryId)
{
    QVector<QString> jobIds;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId == entryId)
            jobIds.append(job.id);
    }
    for (const QString& jobId : jobIds)
        m_jobOrchestrator->removeJob(jobId);
}

void CoreController::retryPendingInstalls()
{
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (!job.parentEntryId.isEmpty())
            continue;
        if (job.status != QStringLiteral("completed"))
            continue;
        if (job.kind != JobKind::Download && job.kind != JobKind::Update)
            continue;
        if (!gameNeedsInstall(job.entryId))
            continue;
        if (job.savePath.isEmpty() || !QDir(job.savePath).exists())
            continue;
        if (!hasInstallHandlerForPath(job.sourceId, job.savePath))
            continue;

        const auto entry = resolveCatalogEntry(job.entryId, job.sourceId, &job);
        if (!entry)
            continue;

        startPluginInstall(*entry, job.sourceId, job.savePath, job.kind, job.libraryId, job.id);
    }
}

void CoreController::syncLibraryFromStore()
{
    QVector<LibraryGame> games = m_libraryStore.games();
    for (auto& game : games)
        enrichLibraryGameCover(game);
    m_library.setGames(games);
}

void CoreController::enrichLibraryGameCover(LibraryGame& game) const
{
    if (!game.coverUrl.isEmpty())
        return;

    if (const JobEntry* job = findLatestJobForEntry(game.id)) {
        if (!job->coverUrl.isEmpty()) {
            game.coverUrl = job->coverUrl;
            return;
        }
    }

    const GameMetadata metadata = m_metadataService->metadataForTitle(game.title);
    if (metadata.coverUrl.isEmpty())
        return;

    const QString local = m_coverCache->localUrlFor(metadata.coverUrl);
    game.coverUrl = !local.isEmpty() ? local : metadata.coverUrl;
}

void CoreController::warmCatalogCovers(const QString& sourceId, const QString& query, int limit)
{
    const QString needle = query.trimmed().toLower();
    int warmed = 0;
    for (auto& entry : m_catalogCache) {
        if (entry.sourceId != sourceId)
            continue;
        if (!needle.isEmpty() && !entry.title.toLower().contains(needle))
            continue;

        applyCachedMetadata(entry);

        if (entry.coverUrl.startsWith(QStringLiteral("file:"))) {
            syncEntryToCatalogModel(entry.id);
            ++warmed;
            continue;
        }

        if (entry.coverUrl.isEmpty()) {
            requestCatalogCover(entry.id);
        } else {
            syncEntryToCatalogModel(entry.id);
            if (isRemoteLibraryCover(entry.coverUrl))
                ensureDiskCover(entry.id, entry.coverUrl);
        }

        if (++warmed >= limit)
            break;
    }
}

void CoreController::showNotice(const QString& message, bool addToHistory)
{
    if (message.isEmpty())
        return;
    m_userNotice = message;
    ++m_userNoticeSerial;
    if (addToHistory)
        m_notifications.add(message, notificationKindForMessage(message));
    emit userNoticeChanged();
}

void CoreController::markNotificationsRead()
{
    m_notifications.markAllRead();
}

void CoreController::clearNotifications()
{
    m_notifications.clear();
}

void CoreController::setCatalogLoading(bool loading)
{
    if (m_catalogLoading == loading)
        return;
    m_catalogLoading = loading;
    emit catalogLoadingChanged();
}

void CoreController::setCatalogStatus(const QString& status)
{
    if (m_catalogStatus == status)
        return;
    m_catalogStatus = status;
    emit catalogStatusChanged();
}

bool CoreController::isRemoteUploadDateNewer(const QString& remote, const QString& local) const
{
    if (remote.isEmpty() || local.isEmpty())
        return false;
    if (remote == local)
        return false;

    const QDateTime remoteDate = QDateTime::fromString(remote, Qt::ISODate);
    const QDateTime localDate = QDateTime::fromString(local, Qt::ISODate);
    if (remoteDate.isValid() && localDate.isValid())
        return remoteDate > localDate;

    const QDate remoteDay = QDate::fromString(remote.left(10), Qt::ISODate);
    const QDate localDay = QDate::fromString(local.left(10), Qt::ISODate);
    if (remoteDay.isValid() && localDay.isValid())
        return remoteDay > localDay;

    return remote > local;
}

namespace {

bool installedGameMatchesCatalogTorrent(const LibraryGame& game, const CatalogEntry& remote)
{
    if (game.installPath.isEmpty() || !QFileInfo::exists(game.installPath))
        return false;

    const QString localHash = catalogMagnetInfoHash(
        game.magnetUri.isEmpty() ? QStringList{} : QStringList{game.magnetUri});
    const QString remoteHash = catalogMagnetInfoHash(remote.magnetUris);
    return !localHash.isEmpty() && localHash == remoteHash;
}

} // namespace

bool CoreController::gameHasUpdate(const LibraryGame& game, const CatalogEntry& remote) const
{
    if (remote.id.isEmpty())
        return false;

    if (!installedGameMatchesCatalogTorrent(game, remote)) {
        if (ISourcePlugin* plugin = m_pluginHost ? m_pluginHost->plugin(game.sourceId) : nullptr) {
            if (plugin->detectUpdate(game, remote))
                return true;
        }
        // Fallback for plugins that omit detectUpdate: compare uploadDate.
        if (isRemoteUploadDateNewer(remote.uploadDate, game.uploadDate))
            return true;
    }

    for (const auto& component : game.components) {
        if (!component.installed)
            continue;
        for (const auto& remoteAddon : remote.addons) {
            if (remoteAddon.id != component.id)
                continue;
            if (isRemoteUploadDateNewer(remoteAddon.uploadDate, component.uploadDate))
                return true;
        }
    }

    return false;
}

int CoreController::recalculateLibraryUpdates(bool notify)
{
    if (m_catalogCache.isEmpty())
        return 0;

    QHash<QString, CatalogEntry> remoteById;
    for (const auto& entry : m_catalogCache)
        remoteById.insert(entry.id, entry);

    QVector<LibraryGame> games = m_libraryStore.games();
    int updates = 0;
    for (auto& game : games) {
        const CatalogEntry remote = remoteById.value(game.id);
        if (game.magnetUri.isEmpty() && !remote.magnetUris.isEmpty()
            && !game.installPath.isEmpty() && QFileInfo::exists(game.installPath))
            game.magnetUri = remote.magnetUris.value(0);

        if (installedGameMatchesCatalogTorrent(game, remote)) {
            game.uploadDate = remote.uploadDate;
            game.version = remote.version;
        }

        const bool hasUpdate = gameHasUpdate(game, remote);
        game.hasUpdate = hasUpdate;
        if (hasUpdate)
            ++updates;
    }

    m_libraryStore.setGames(games);
    syncLibraryFromStore();

    if (notify) {
        if (updates > 0)
            showNotice(QCoreApplication::translate("Core", "%1 update(s) available").arg(updates));
        else
            showNotice(QCoreApplication::translate("Core", "No updates"), false);
    }

    return updates;
}

void CoreController::onCatalogReady()
{
    const int updates = recalculateLibraryUpdates(false);

    if (m_settings.autoCheckUpdates() && updates > 0)
        showNotice(QCoreApplication::translate("Core", "%1 update(s) available").arg(updates));

    runAutoInstallUpdates();
}

void CoreController::runAutoInstallUpdates()
{
    if (!m_settings.autoInstallUpdates())
        return;

    int started = 0;
    for (const LibraryGame& game : m_libraryStore.games()) {
        if (!game.hasUpdate || !game.autoUpdate)
            continue;
        if (!isEntryPlayable(game.id))
            continue;
        if (entryHasActiveJob(game.id))
            continue;

        const CatalogEntry* entry = findCatalogEntry(game.id);
        if (!entry)
            continue;

        const QString jobId = m_jobOrchestrator->startCatalogDownload(*entry, JobKind::Update);
        if (!jobId.isEmpty())
            ++started;
    }

    if (started > 0) {
        showNotice(QCoreApplication::translate("Core", "Started %1 update(s)").arg(started));
    }
}

bool CoreController::entryHasActiveJob(const QString& entryId) const
{
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId != entryId && job.parentEntryId != entryId)
            continue;
        if (isJobInProgress(job.status))
            return true;
    }
    return false;
}

void CoreController::setGameAutoUpdate(const QString& entryId, bool enabled)
{
    const LibraryGame* existing = m_libraryStore.gameById(entryId);
    if (!existing)
        return;

    LibraryGame game = *existing;
    if (game.autoUpdate == enabled)
        return;

    game.autoUpdate = enabled;
    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
}

void CoreController::setGameLaunchArgs(const QString& entryId, const QString& args)
{
    const LibraryGame* existing = m_libraryStore.gameById(entryId);
    if (!existing)
        return;

    LibraryGame game = *existing;
    if (game.launchArgs == args)
        return;

    game.launchArgs = args;
    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
}

void CoreController::setGameExecutableOverride(const QString& entryId, const QString& path)
{
    const LibraryGame* existing = m_libraryStore.gameById(entryId);
    if (!existing)
        return;

    LibraryGame game = *existing;
    if (game.executableOverride == path)
        return;

    game.executableOverride = path;
    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
}

void CoreController::setGameProtonId(const QString& entryId, const QString& protonId)
{
    const QString normalized = protonId.trimmed();

    const LibraryGame* existing = m_libraryStore.gameById(entryId);
    if (existing) {
        LibraryGame game = *existing;
        if (game.protonId == normalized)
            return;
        game.protonId = normalized;
        m_libraryStore.upsertGame(game);
        syncLibraryFromStore();
        return;
    }

    const CatalogEntry* catalogEntry = findCatalogEntry(entryId);
    if (!catalogEntry)
        return;

    LibraryGame game;
    game.id = catalogEntry->id;
    game.title = catalogEntry->title;
    game.coverUrl = catalogEntry->coverUrl;
    game.sourceId = catalogEntry->sourceId;
    game.sourceName = m_sources.nameForId(catalogEntry->sourceId);
    game.version = catalogEntry->version;
    game.description = catalogEntry->description;
    game.genres = catalogEntry->genres;
    game.sizeLabel = catalogEntry->sizeLabel;
    game.installKind = catalogEntry->installKind;
    game.uploadDate = catalogEntry->uploadDate;
    game.magnetUri = catalogEntry->magnetUris.value(0);
    game.libraryId = m_settings.defaultLibraryId();
    game.protonId = normalized;
    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
}

QString CoreController::browseGameExecutable(const QString& currentPath)
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        const COMDLG_FILTERSPEC filters[] = {
            {L"Executables (*.exe)", L"*.exe"},
            {L"All files (*.*)", L"*.*"},
        };
        dialog->SetFileTypes(2, filters);
        dialog->SetTitle(L"Choose game executable");

        if (!currentPath.isEmpty()) {
            IShellItem* folder = nullptr;
            const QString folderPath = QFileInfo(currentPath).absolutePath();
            if (SUCCEEDED(
                    SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(folderPath.utf16()),
                                                nullptr, IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    path = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();
    return path;
#else
    return QFileDialog::getOpenFileName(
        nullptr, QCoreApplication::translate("Core", "Choose game executable"),
        currentPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                              : QFileInfo(currentPath).absolutePath(),
        QCoreApplication::translate("Core", "Executables (*.exe *.sh *.x86_64);;All files (*)"));
#endif
}

void CoreController::refreshAvailableProtons()
{
    if (!m_protonManager)
        return;
    m_protonManager->availableEntries(true);
    syncProtonCatalog();
    emit availableProtonsChanged();
}

void CoreController::moveProtonInPriority(const QString& protonId, int delta)
{
    const QString id = protonId.trimmed();
    if (id.isEmpty() || delta == 0)
        return;

    QStringList priority = m_settings.protonPriority();
    const int index = priority.indexOf(id);
    if (index < 0)
        return;

    const int target = index + delta;
    if (target < 0 || target >= priority.size())
        return;

    priority.move(index, target);
    m_settings.setProtonPriority(priority);
}

QString CoreController::protonNameForId(const QString& protonId) const
{
    return m_protonManager ? m_protonManager->nameForId(protonId) : QString();
}

QVariantList CoreController::availableProtons() const
{
    QVariantList result;
    if (!m_protonManager)
        return result;

    const QString defaultId = m_settings.defaultProtonId();
    const QStringList priority = m_settings.protonPriority();
    QSet<QString> emitted;

    const auto appendEntry = [&](const ProtonEntry& entry) {
        if (emitted.contains(entry.id))
            return;
        emitted.insert(entry.id);
        result.append(QVariantMap{
            {QStringLiteral("id"), entry.id},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("source"), entry.source},
            {QStringLiteral("sourceLabel"), entry.sourceLabel},
            {QStringLiteral("installDir"), entry.installDir},
            {QStringLiteral("isDefault"), entry.id == defaultId},
            {QStringLiteral("priorityIndex"), priority.indexOf(entry.id)},
        });
    };

    for (const QString& id : priority) {
        for (const ProtonEntry& entry : m_protonManager->availableEntries()) {
            if (entry.id == id)
                appendEntry(entry);
        }
    }

    for (const ProtonEntry& entry : m_protonManager->availableEntries())
        appendEntry(entry);

    return result;
}

void CoreController::syncProtonCatalog()
{
    if (!m_protonManager)
        return;

    const QVector<ProtonEntry> entries = m_protonManager->availableEntries(true);
    QStringList priority = m_settings.protonPriority();
    bool priorityChanged = false;

    for (const ProtonEntry& entry : entries) {
        if (!priority.contains(entry.id)) {
            priority.append(entry.id);
            priorityChanged = true;
        }
    }

    if (priorityChanged)
        m_settings.setProtonPriority(priority);

    if (!m_settings.legacyProtonPath().isEmpty()) {
        const QString legacyId = m_protonManager->idForInstallDir(m_settings.legacyProtonPath());
        if (!legacyId.isEmpty() && m_settings.defaultProtonId().isEmpty())
            m_settings.setDefaultProtonId(legacyId);
        m_settings.clearLegacyProtonPath();
    }

    if (m_settings.defaultProtonId().isEmpty() && !entries.isEmpty())
        m_settings.setDefaultProtonId(entries.first().id);
}

void CoreController::downloadProtonGe()
{
    if (!m_protonManager)
        return;
    m_protonManager->downloadLatestGe();
}

void CoreController::refreshProtonLatestRelease()
{
    if (m_protonManager)
        m_protonManager->refreshLatestGeRelease();
}

bool CoreController::needsProtonOnPlatform() const
{
#if defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

bool CoreController::protonReady() const
{
    if (!needsProtonOnPlatform())
        return true;
    if (!m_protonManager)
        return false;
    const QString protonId = m_settings.resolvedProtonId(QString(), *m_protonManager);
    return !m_protonManager->executableForId(protonId).isEmpty();
}

QString CoreController::protonVersion() const
{
    if (!m_protonManager)
        return {};
    const QString protonId = m_settings.resolvedProtonId(QString(), *m_protonManager);
    return m_protonManager->nameForId(protonId);
}

QString CoreController::protonLatestRelease() const
{
    return m_protonManager ? m_protonManager->latestGeReleaseName() : QString();
}

bool CoreController::ensureProtonReady()
{
    if (protonReady())
        return true;

    refreshProtonLatestRelease();

    const QString latest = protonLatestRelease();
    if (latest.isEmpty()) {
        showNotice(QCoreApplication::translate(
            "Core", "Install Proton-GE in Settings → Launch before downloading games"));
    } else {
        showNotice(QCoreApplication::translate(
            "Core", "Install %1 (Proton-GE) in Settings → Launch before downloading games")
                       .arg(latest));
    }
    return false;
}

bool CoreController::protonDownloadInProgress() const
{
    return m_protonManager && m_protonManager->isDownloading();
}

int CoreController::protonDownloadProgress() const
{
    return m_protonManager ? m_protonManager->downloadProgress() : 0;
}

QString CoreController::protonDownloadStatus() const
{
    return m_protonManager ? m_protonManager->downloadStatus() : QString();
}

const CatalogEntry* CoreController::findCatalogEntry(const QString& entryId) const
{
    const QString resolved = repairCatalogEntryId(entryId);
    for (const auto& entry : m_catalogCache) {
        if (entry.id == resolved)
            return &entry;
    }
    for (auto it = m_catalogBySource.cbegin(); it != m_catalogBySource.cend(); ++it) {
        for (const auto& entry : it.value()) {
            if (entry.id == resolved)
                return &entry;
        }
    }
    if (const CatalogEntry* found = m_catalog.entryById(resolved))
        return found;
    if (resolved != entryId)
        return m_catalog.entryById(entryId);
    return nullptr;
}

std::optional<CatalogEntry> CoreController::resolveCatalogEntry(const QString& entryId) const
{
    if (const CatalogEntry* found = findCatalogEntry(entryId))
        return *found;

    const LibraryGame* game = m_libraryStore.gameById(entryId);
    if (!game || game->magnetUri.isEmpty())
        return std::nullopt;

    CatalogEntry entry;
    entry.id = game->id;
    entry.title = game->title;
    entry.coverUrl = game->coverUrl;
    entry.sourceId = game->sourceId;
    entry.version = game->version;
    entry.sizeLabel = game->sizeLabel;
    entry.uploadDate = game->uploadDate;
    entry.installKind = game->installKind;
    entry.magnetUris.append(game->magnetUri);
    return entry;
}

const CatalogComponent* CoreController::findCatalogAddon(const CatalogEntry& entry,
                                                         const QString& addonId) const
{
    for (const auto& addon : entry.addons) {
        if (addon.id == addonId)
            return &addon;
    }
    return nullptr;
}

void CoreController::applyCachedMetadata(CatalogEntry& entry) const
{
    const GameMetadata metadata = m_metadataService->metadataForTitle(entry.title);
    // Prefer on-disk cover so Image never hits Steam CDN after a warm cache.
    if (!metadata.coverUrl.isEmpty()) {
        const QString local = m_coverCache->localUrlFor(metadata.coverUrl);
        if (!local.isEmpty())
            entry.coverUrl = local;
        else if (entry.coverUrl.isEmpty() && isRemoteLibraryCover(metadata.coverUrl))
            entry.coverUrl = metadata.coverUrl;
    }
    applyMetadataToEntry(entry, metadata);
    // Leave metadataPending alone — it tracks an in-flight/queued fetch, not "missing cover".
}

QString CoreController::sourceWebsiteFor(const QString& sourceId) const
{
    if (sourceId == QStringLiteral("freetp"))
        return QStringLiteral("https://freetp.org/");

    if (const SourcePluginInfo* plugin = m_sources.pluginById(sourceId)) {
        const QUrl catalogUrl(plugin->catalogUrl);
        if (catalogUrl.isValid() && !catalogUrl.host().isEmpty())
            return QStringLiteral("%1://%2").arg(catalogUrl.scheme(), catalogUrl.host());
    }
    return {};
}

void CoreController::applyMetadataToEntry(CatalogEntry& entry,
                                          const GameMetadata& metadata) const
{
    if (!metadata.description.isEmpty())
        entry.description = metadata.description;
    if (!metadata.genres.isEmpty())
        entry.genres = metadata.genres;
    if (!metadata.steamAppId.isEmpty())
        entry.steamAppId = metadata.steamAppId;
    if (!metadata.trailerUrl.isEmpty())
        entry.trailerUrl = metadata.trailerUrl;
    if (!metadata.trailerThumbnailUrl.isEmpty())
        entry.trailerThumbnailUrl = metadata.trailerThumbnailUrl;
    if (!metadata.screenshotUrls.isEmpty())
        entry.screenshotUrls = metadata.screenshotUrls;
}

bool CoreController::isRemoteLibraryCover(const QString& url)
{
    return url.contains(QStringLiteral("library_capsule"))
        || url.contains(QStringLiteral("library_600x900"));
}

void CoreController::applyCoverToEntry(const QString& entryId, const QString& coverUrl)
{
    for (auto& entry : m_catalogCache) {
        if (entry.id != entryId)
            continue;
        entry.coverUrl = coverUrl;
        entry.metadataPending = false;
        syncEntryToCatalogModel(entryId);
        break;
    }

    const LibraryGame* existing = m_libraryStore.gameById(entryId);
    if (!existing || existing->coverUrl == coverUrl)
        return;

    LibraryGame game = *existing;
    game.coverUrl = coverUrl;
    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
}

void CoreController::ensureDiskCover(const QString& entryId, const QString& remoteUrl)
{
    if (remoteUrl.isEmpty()) {
        applyCoverToEntry(entryId, QString());
        return;
    }

    const QString local = m_coverCache->localUrlFor(remoteUrl);
    if (!local.isEmpty()) {
        applyCoverToEntry(entryId, local);
        return;
    }

    m_coverWaiters[remoteUrl].insert(entryId);
    m_coverCache->ensure(remoteUrl);
}

void CoreController::syncEntryToCatalogModel(const QString& entryId)
{
    for (const auto& entry : m_catalogCache) {
        if (entry.id != entryId)
            continue;
        if (!m_catalog.updateEntry(entry))
            applyCatalogFilter(m_activeQuery);
        return;
    }
}

InstallKind CoreController::detectInstallKindForEntry(const QString& sourceId,
                                                      const QString& downloadPath) const
{
    if (downloadPath.isEmpty() || !m_installAnalyzer)
        return InstallKind::PortableArchive;

    InstallContext ctx;
    ctx.sourceId = sourceId;
    ctx.downloadPath = downloadPath;
    return m_installAnalyzer->resolveDownload(ctx).analysis.kind;
}

bool CoreController::hasInstallHandlerForPath(const QString& sourceId,
                                              const QString& downloadPath) const
{
    if (downloadPath.isEmpty() || !m_installAnalyzer)
        return false;

    InstallContext ctx;
    ctx.sourceId = sourceId;
    ctx.downloadPath = downloadPath;
    return m_installAnalyzer->resolveDownload(ctx).installerPlugin != nullptr;
}

void CoreController::syncInstallKindProbeSuspension()
{
    if (!m_installKindProbe)
        return;

    bool hasActiveTorrent = false;
    for (int i = 0; i < m_jobs.rowCount(); ++i) {
        const QModelIndex idx = m_jobs.index(i, 0);
        if (m_jobs.data(idx, JobModel::HttpDownloadRole).toBool())
            continue;
        const QString status = m_jobs.data(idx, JobModel::StatusRole).toString();
        if (isJobActive(status) || isJobQueued(status) || isJobPaused(status)) {
            hasActiveTorrent = true;
            break;
        }
    }

    m_installKindProbe->setBackgroundProbesEnabled(!hasActiveTorrent);
}

void CoreController::syncCatalogInstallKind(const QString& entryId, InstallKind kind)
{
    for (auto& entry : m_catalogCache) {
        if (entry.id != entryId)
            continue;
        if (entry.installKind == kind)
            return;
        entry.installKind = kind;
        syncEntryToCatalogModel(entryId);
        return;
    }
}

void CoreController::requestCatalogCover(const QString& entryId)
{
    CatalogEntry* entry = nullptr;
    for (auto& candidate : m_catalogCache) {
        if (candidate.id == entryId) {
            entry = &candidate;
            break;
        }
    }
    if (!entry)
        return;

    if (entry->coverUrl.startsWith(QStringLiteral("file:")))
        return;

    if (isRemoteLibraryCover(entry->coverUrl)) {
        if (!entry->metadataPending) {
            entry->metadataPending = true;
            syncEntryToCatalogModel(entryId);
        }
        ensureDiskCover(entryId, entry->coverUrl);
        return;
    }

    // Metadata may already know the Steam URL — disk-cache it without CDN in Image.
    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (isRemoteLibraryCover(metadata.coverUrl)) {
        if (!entry->metadataPending) {
            entry->metadataPending = true;
            syncEntryToCatalogModel(entryId);
        }
        ensureDiskCover(entryId, metadata.coverUrl);
        return;
    }

    if (!entry->metadataPending) {
        entry->metadataPending = true;
        syncEntryToCatalogModel(entryId);
    }
    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly,
                                  m_settings.uiLanguage());
}

void CoreController::cancelCatalogCover(const QString& entryId)
{
    if (!m_metadataService->cancelPending(entryId))
        return;

    for (auto& entry : m_catalogCache) {
        if (entry.id != entryId)
            continue;
        if (!entry.metadataPending)
            break;
        entry.metadataPending = false;
        syncEntryToCatalogModel(entryId);
        break;
    }
}

void CoreController::invalidateCatalogCover(const QString& entryId)
{
    CatalogEntry* entry = nullptr;
    for (auto& candidate : m_catalogCache) {
        if (candidate.id == entryId) {
            entry = &candidate;
            break;
        }
    }
    if (!entry)
        return;

    if (!entry->coverUrl.isEmpty())
        m_coverCache->remove(entry->coverUrl);

    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (!metadata.coverUrl.isEmpty())
        m_coverCache->remove(metadata.coverUrl);

    m_metadataService->clearCachedCover(entry->title);
    entry->coverUrl.clear();
    entry->metadataPending = true;
    syncEntryToCatalogModel(entryId);
    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly,
                                  m_settings.uiLanguage());
}

void CoreController::enrichCatalogEntry(const QString& entryId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry)
        return;

    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::Full,
                                  m_settings.uiLanguage());

    if (m_installKindProbe) {
        QString magnet;
        for (const QString& uri : entry->magnetUris) {
            if (uri.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)) {
                magnet = uri;
                break;
            }
        }
        if (magnet.isEmpty())
            magnet = entry->magnetUris.value(0);
        if (!magnet.isEmpty())
            m_installKindProbe->prioritizeEntry(entry->sourceId, entryId, magnet);
    }
}

void CoreController::normalizeCatalogSourceIds(QVector<CatalogEntry>& entries,
                                               const QString& sourceId)
{
    for (auto& entry : entries) {
        entry.id = repairCatalogEntryId(entry.id);
        entry.sourceId = sourceId;
    }
}

void CoreController::applyCatalogFilter(const QString& query)
{
    const QString needle = query.trimmed().toLower();
    QVector<CatalogEntry> filtered;
    filtered.reserve(m_catalogCache.size());

    for (const auto& entry : m_catalogCache) {
        if (!needle.isEmpty() && !entry.title.toLower().contains(needle))
            continue;
        filtered.append(entry);
    }

    m_catalog.setEntries(std::move(filtered));

    const int warmLimit =
        qMax(32, 80 / qMax(1, static_cast<int>(m_activeSourceIds.size())));
    for (const QString& sourceId : m_activeSourceIds)
        warmCatalogCovers(sourceId, query, warmLimit);

    if (m_installKindProbe) {
        for (const QString& sourceId : m_activeSourceIds)
            m_installKindProbe->queueCatalog(sourceId, m_catalogCache, query);
    }
}

void CoreController::storeCatalogForSource(const QString& sourceId,
                                           QVector<CatalogEntry> entries)
{
    normalizeCatalogSourceIds(entries, sourceId);
    m_catalogBySource.insert(sourceId, entries);
    m_catalogCounts.insert(sourceId, entries.size());
    emit catalogCountsChanged();
    m_loadingSourceIds.remove(sourceId);

    if (m_activeSourceIds.contains(sourceId))
        rebuildMergedCatalog();
    else
        updateCatalogLoadingState();
}

void CoreController::rebuildMergedCatalog()
{
    if (m_activeSourceIds.isEmpty()) {
        m_catalogCache.clear();
        m_catalog.clear();
        setCatalogStatus({});
        updateCatalogLoadingState();
        return;
    }

    QVector<CatalogEntry> merged;
    QStringList pendingLoads;
    for (const QString& sourceId : m_activeSourceIds) {
        const SourcePluginInfo* source = m_sources.pluginById(sourceId);
        if (!source || !source->enabled)
            continue;

        if (m_catalogBySource.contains(sourceId)) {
            const QVector<CatalogEntry>& sourceEntries = m_catalogBySource.value(sourceId);
            merged.reserve(merged.size() + sourceEntries.size());
            merged += sourceEntries;
        } else if (!m_loadingSourceIds.contains(sourceId)) {
            pendingLoads.append(sourceId);
        }
    }

    for (const QString& sourceId : pendingLoads)
        requestCatalogLoad(sourceId);

    for (auto& entry : merged)
        entry.id = repairCatalogEntryId(entry.id);

    deduplicateCatalogEntries(merged);

    logDiagnostic(QStringLiteral("rebuildMergedCatalog: %1 entries after dedupe").arg(merged.size()));

    m_catalogCache = std::move(merged);
    for (auto& entry : m_catalogCache)
        applyCachedMetadata(entry);

    logDiagnostic(QStringLiteral("rebuildMergedCatalog: metadata applied"));

    if (m_installKindProbe) {
        m_installKindProbe->applyCachedKinds(m_catalogCache);
        for (const QString& sourceId : m_activeSourceIds)
            m_installKindProbe->queueCatalog(sourceId, m_catalogCache, m_activeQuery);
    }

    logDiagnostic(QStringLiteral("rebuildMergedCatalog: install kinds queued"));

    applyCatalogFilter(m_activeQuery);

    logDiagnostic(QStringLiteral("rebuildMergedCatalog: filter applied, visible=%1").arg(m_catalog.count()));

    if (m_activeSourceIds.size() == 1) {
        const SourcePluginInfo* source = m_sources.pluginById(m_activeSourceIds.first());
        const QString name = source ? source->name : m_activeSourceIds.first();
        setCatalogStatus(QCoreApplication::translate("Core", "%1 · %2 games").arg(name).arg(m_catalog.count()));
    } else {
        setCatalogStatus(QCoreApplication::translate("Core", "%1 sources · %2 games")
                             .arg(m_activeSourceIds.size())
                             .arg(m_catalog.count()));
    }

    updateCatalogLoadingState();
    onCatalogReady();
}

void CoreController::requestCatalogLoad(const QString& sourceId)
{
    if (sourceId.isEmpty())
        return;

    if (m_catalogBySource.contains(sourceId)) {
        const QVector<CatalogEntry>& cached = m_catalogBySource.value(sourceId);
        if (!catalogCacheHasPollutedIds(cached))
            return;
        m_catalogBySource.remove(sourceId);
    }

    if (m_loadingSourceIds.contains(sourceId)) {
        if (!m_catalogLoadQueue.contains(sourceId))
            m_catalogLoadQueue.append(sourceId);
        return;
    }

    m_loadingSourceIds.insert(sourceId);
    updateCatalogLoadingState();

    if (m_pluginHost && m_pluginHost->hasPlugin(sourceId)) {
        loadCatalogSourceNow(sourceId);
        return;
    }

    if (!m_catalogLoadQueue.contains(sourceId))
        m_catalogLoadQueue.append(sourceId);
    processCatalogLoadQueue();
}

void CoreController::processCatalogLoadQueue()
{
    if (m_catalogHttpLoadActive)
        return;

    while (!m_catalogLoadQueue.isEmpty()) {
        const QString sourceId = m_catalogLoadQueue.first();
        if (m_catalogBySource.contains(sourceId)) {
            const QVector<CatalogEntry>& cached = m_catalogBySource.value(sourceId);
            if (!catalogCacheHasPollutedIds(cached)) {
                m_catalogLoadQueue.removeFirst();
                continue;
            }
            m_catalogBySource.remove(sourceId);
        }
        if (m_loadingSourceIds.contains(sourceId) && m_pluginHost
            && m_pluginHost->hasPlugin(sourceId))
            return;

        m_catalogLoadQueue.removeFirst();
        loadCatalogSourceNow(sourceId);
        return;
    }

    updateCatalogLoadingState();
}

void CoreController::loadCatalogSourceNow(const QString& sourceId)
{
    // DLL plugins own their catalog (catalogUrl is for the plugin's own sync, not Hydra feeds).
    if (m_pluginHost) {
        if (ISourcePlugin* plugin = m_pluginHost->plugin(sourceId)) {
            logDiagnostic(QStringLiteral("Loading catalog via plugin DLL: %1").arg(sourceId));
            auto* watcher = new QFutureWatcher<QVector<CatalogEntry>>(this);
            connect(watcher, &QFutureWatcher<QVector<CatalogEntry>>::finished, this,
                    [this, watcher, sourceId]() {
                        const QVector<CatalogEntry> entries = watcher->result();
                        watcher->deleteLater();
                        logDiagnostic(QStringLiteral("Plugin catalog ready: %1 entries=%2")
                                          .arg(sourceId)
                                          .arg(entries.size()));
                        if (entries.isEmpty()) {
                            // DLL may ship without a bundled JSON; use catalogUrl when set.
                            const QString catalogUrl = m_sources.catalogUrlFor(sourceId);
                            if (!catalogUrl.isEmpty()) {
                                logDiagnostic(
                                    QStringLiteral(
                                        "Plugin catalog empty; falling back to feed: %1 url=%2")
                                        .arg(sourceId, catalogUrl));
                                m_catalogHttpLoadActive = true;
                                updateCatalogLoadingState();
                                m_catalogLoader->loadFeed(QUrl(catalogUrl), sourceId);
                                return;
                            }
                            m_loadingSourceIds.remove(sourceId);
                            if (m_activeSourceIds.contains(sourceId)) {
                                showNotice(QCoreApplication::translate(
                                               "Core", "Catalog empty or unavailable: %1")
                                               .arg(m_sources.nameForId(sourceId)));
                            }
                            rebuildMergedCatalog();
                            return;
                        }
                        storeCatalogForSource(sourceId, entries);
                    });

            ISourcePlugin* pluginRef = plugin;
            // Only force a cold re-seed on explicit refreshCatalog(); normal loads reuse
            // the plugin's in-memory/local cache (55k-entry parse is expensive).
            watcher->setFuture(QtConcurrent::run([pluginRef]() { return pluginRef->catalog(); }));
            return;
        }
    }

    const QString catalogUrl = m_sources.catalogUrlFor(sourceId);
    if (!catalogUrl.isEmpty()) {
        logDiagnostic(QStringLiteral("Loading catalog via feed: %1 url=%2").arg(sourceId, catalogUrl));
        m_catalogHttpLoadActive = true;
        updateCatalogLoadingState();
        m_catalogLoader->loadFeed(QUrl(catalogUrl), sourceId);
        return;
    }

    m_loadingSourceIds.remove(sourceId);
    showNotice(QCoreApplication::translate("Core", "No catalog URL configured for source %1").arg(sourceId));
    rebuildMergedCatalog();
}

void CoreController::updateCatalogLoadingState()
{
    setCatalogLoading(!m_loadingSourceIds.isEmpty() || m_catalogHttpLoadActive);
}

void CoreController::syncActiveSourceSignals()
{
    emit activeCatalogSourceIdsChanged();
    emit activeCatalogSourceIdChanged();
}

void CoreController::commitCatalogLoad(const QString& sourceId,
                                         QVector<CatalogEntry> entries)
{
    storeCatalogForSource(sourceId, std::move(entries));
}

void CoreController::touchLastPlayed(const QString& gameId)
{
    if (gameId.isEmpty())
        return;

    const LibraryGame* existing = m_libraryStore.gameById(gameId);
    if (!existing)
        return;

    LibraryGame game = *existing;
    game.lastPlayedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
}

void CoreController::markGameRunning(const LibraryGame& game, const qint64 processId)
{
    touchLastPlayed(game.id);
    m_runningGameId = game.id;
    m_runningGameTitle = game.title;
    m_runningGameCoverUrl = game.coverUrl;
    m_runningProcessId = processId;
    arachnel::logDiagnostic(QStringLiteral("Game launched: %1 pid=%2")
                                .arg(game.title)
                                .arg(processId));
    emit runningGameChanged();

    if (processId > 0)
        m_runningGameTimer->start();
    else
        m_runningGameTimer->stop();
}

void CoreController::clearRunningGame()
{
    if (m_runningGameId.isEmpty())
        return;

    const QString endedId = m_runningGameId;
    const QString endedTitle = m_runningGameTitle;
    m_runningGameId.clear();
    m_runningGameTitle.clear();
    m_runningGameCoverUrl.clear();
    m_runningProcessId = 0;
    m_runningGameTimer->stop();
    arachnel::logDiagnostic(QStringLiteral("Game session ended: %1 (%2)").arg(endedTitle, endedId));
    emit runningGameChanged();
}

void CoreController::pollRunningGame()
{
    if (m_runningGameId.isEmpty())
        return;

    if (m_runningProcessId <= 0)
        return;

    if (!ProcessTracker::isProcessRunning(m_runningProcessId)) {
        // Defer UI teardown so we do not re-enter QML/layout from QTimer while the
        // game process is still unwinding (avoids intermittent crashes on Windows).
        const QString endedGameId = m_runningGameId;
        QTimer::singleShot(0, this, [this, endedGameId]() {
            if (m_runningGameId != endedGameId)
                return;
            arachnel::logDiagnostic(
                QStringLiteral("Running game process ended: %1").arg(endedGameId));
            clearRunningGame();
        });
    }
}

void CoreController::launchGame(const QString& gameId)
{
    const LibraryGame* game = m_library.gameById(gameId);
    if (!game) {
        showNotice(QCoreApplication::translate("Core", "Game not found: %1").arg(gameId));
        return;
    }
    if (game->installPath.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "%1 is not installed yet").arg(game->title));
        return;
    }

    if (gameRunning() && m_runningGameId == gameId) {
        showNotice(QCoreApplication::translate("Core", "%1 is already running").arg(game->title));
        return;
    }

    LaunchInfo info;
    if (ISourcePlugin* plugin = m_pluginHost->plugin(game->sourceId))
        info = plugin->launchInfo(*game);

    if (info.executable.isEmpty() && game->executableOverride.trimmed().isEmpty()) {
        const QString found = findGameExecutableInTree(game->installPath);
        if (!found.isEmpty())
            info.executable = found;
    }

    const ResolvedLaunch resolved = resolveLaunch(info, *game, m_settings);
    if (resolved.program.isEmpty()) {
#if defined(Q_OS_LINUX)
        if (!info.executable.isEmpty()
            && info.executable.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)
            && m_protonManager) {
            const QString protonId =
                m_settings.resolvedProtonId(game->protonId, *m_protonManager);
            if (m_protonManager->executableForId(protonId).isEmpty()) {
                showNotice(QCoreApplication::translate(
                    "Core", "Proton not found. Install Proton-GE in Settings → Launch."));
                return;
            }
        }
#endif
        showNotice(
            QCoreApplication::translate("Core", "Executable not found for %1").arg(game->title));
        return;
    }

    QString error;
    qint64 processId = 0;
    if (ProcessLauncher::launch(resolved, &error, &processId)) {
        markGameRunning(*game, processId);
        return;
    }
    showNotice(error.isEmpty() ? QCoreApplication::translate("Core", "Failed to launch game") : error);
}

void CoreController::stopRunningGame()
{
    if (!gameRunning())
        return;

    if (m_runningProcessId <= 0) {
        clearRunningGame();
        return;
    }

    if (ProcessTracker::terminateProcess(m_runningProcessId))
        clearRunningGame();
    else
        showNotice(QCoreApplication::translate("Core", "Failed to stop game"));
}

void CoreController::refreshCatalog(const QString& sourceId)
{
    m_catalogBySource.remove(sourceId);
    m_catalogCounts.remove(sourceId);
    emit catalogCountsChanged();
    m_loadingSourceIds.remove(sourceId);
    m_catalogLoadQueue.removeAll(sourceId);

    if (m_pluginHost) {
        if (ISourcePlugin* plugin = m_pluginHost->plugin(sourceId))
            plugin->resetCatalogCache();
    }

    if (m_activeSourceIds.contains(sourceId))
        requestCatalogLoad(sourceId);
}

void CoreController::refreshSelectedCatalogs()
{
    for (const QString& sourceId : m_activeSourceIds)
        refreshCatalog(sourceId);
}

void CoreController::searchCatalog(const QString& sourceId, const QString& query)
{
    const SourcePluginInfo* source = m_sources.pluginById(sourceId);
    if (!source) {
        showNotice(QCoreApplication::translate("Core", "Unknown source: %1").arg(sourceId));
        return;
    }
    if (!source->enabled) {
        showNotice(QCoreApplication::translate("Core", "Source \"%1\" is disabled in settings").arg(source->name));
        return;
    }

    if (!m_activeSourceIds.contains(sourceId)) {
        m_activeSourceIds = {sourceId};
        syncActiveSourceSignals();
    }
    m_activeQuery = query;

    if (!m_catalogBySource.contains(sourceId))
        requestCatalogLoad(sourceId);

    rebuildMergedCatalog();
}

void CoreController::setActiveCatalogSource(const QString& sourceId)
{
    const QString trimmed = sourceId.trimmed();
    if (trimmed.isEmpty()) {
        clearCatalogView();
        return;
    }

    if (m_activeSourceIds.size() == 1 && m_activeSourceIds.first() == trimmed) {
        rebuildMergedCatalog();
        return;
    }

    m_activeSourceIds = {trimmed};
    syncActiveSourceSignals();

    if (!m_catalogBySource.contains(trimmed))
        requestCatalogLoad(trimmed);
    else
        rebuildMergedCatalog();
}

bool CoreController::isCatalogSourceSelected(const QString& sourceId) const
{
    return m_activeSourceIds.contains(sourceId);
}

void CoreController::toggleCatalogSource(const QString& sourceId)
{
    if (sourceId.isEmpty())
        return;

    if (m_activeSourceIds.contains(sourceId)) {
        m_activeSourceIds.removeAll(sourceId);
        syncActiveSourceSignals();
        rebuildMergedCatalog();
        return;
    }

    const SourcePluginInfo* source = m_sources.pluginById(sourceId);
    if (!source || !source->enabled)
        return;

    m_activeSourceIds.append(sourceId);
    syncActiveSourceSignals();

    if (!m_catalogBySource.contains(sourceId))
        requestCatalogLoad(sourceId);
    else
        rebuildMergedCatalog();
}

void CoreController::applyCatalogSearch(const QString& query)
{
    m_activeQuery = query;
    rebuildMergedCatalog();
}

void CoreController::pruneDisabledCatalogSources()
{
    QStringList valid;
    valid.reserve(m_activeSourceIds.size());
    for (const QString& sourceId : m_activeSourceIds) {
        if (m_sources.isSourceEnabled(sourceId))
            valid.append(sourceId);
    }

    if (valid == m_activeSourceIds)
        return;

    m_activeSourceIds = valid;
    syncActiveSourceSignals();
    rebuildMergedCatalog();
}

void CoreController::selectCatalogSource(const QString& sourceId, const QString& query)
{
    if (sourceId.isEmpty())
        return;

    m_activeQuery = query;
    setActiveCatalogSource(sourceId);
    if (!query.isEmpty())
        applyCatalogSearch(query);
}

void CoreController::clearCatalogView()
{
    m_activeSourceIds.clear();
    syncActiveSourceSignals();
    m_activeQuery.clear();
    m_catalogCache.clear();
    m_catalog.clear();
    setCatalogLoading(false);
    setCatalogStatus({});
}

int CoreController::catalogEntryCount(const QString& sourceId) const
{
    if (sourceId.isEmpty())
        return -1;

    if (m_catalogBySource.contains(sourceId))
        return m_catalogBySource.value(sourceId).size();

    if (m_catalogCounts.contains(sourceId))
        return m_catalogCounts.value(sourceId);

    return -1;
}

void CoreController::invalidateSourceCatalog(const QString& sourceId)
{
    m_catalogBySource.remove(sourceId);
    m_catalogCounts.remove(sourceId);
    emit catalogCountsChanged();

    if (m_activeSourceIds.contains(sourceId))
        rebuildMergedCatalog();
}

void CoreController::openExternalUrl(const QString& url)
{
    const QUrl parsed(url.trimmed());
    if (parsed.isValid())
        QDesktopServices::openUrl(parsed);
}

QString CoreController::applicationDataPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

bool CoreController::clearApplicationData()
{
    if (m_applicationDataCleared)
        return true;

    const QString dataDir = applicationDataPath();
    if (dataDir.isEmpty()
        || !dataDir.contains(QStringLiteral("Arachnel"), Qt::CaseInsensitive)) {
        showNotice(QCoreApplication::translate("Core", "Could not resolve application data folder"));
        return false;
    }

    // Stop I/O without rewriting jobs/settings into AppData.
    if (m_runningGameTimer)
        m_runningGameTimer->stop();
    clearRunningGame();

    if (m_catalogLoader)
        m_catalogLoader->cancelActive();
    if (m_catalogProbeLoader)
        m_catalogProbeLoader->cancelActive();
    if (m_catalogValidateLoader)
        m_catalogValidateLoader->cancelActive();
    if (m_httpSession)
        m_httpSession->shutdown();
    if (m_torrentSession)
        m_torrentSession->shutdown();
    if (m_pluginHost)
        m_pluginHost->shutdownPlugins();

    QSettings appearanceSettings;
    appearanceSettings.clear();
    appearanceSettings.sync();

    if (QDir(dataDir).exists() && !QDir(dataDir).removeRecursively()) {
        showNotice(QCoreApplication::translate("Core", "Failed to delete application data"));
        return false;
    }

    // Seed a minimal settings file so the next launch shows first-run onboarding.
    if (!QDir().mkpath(dataDir)) {
        showNotice(QCoreApplication::translate("Core", "Failed to reset application data"));
        return false;
    }
    QFile settingsFile(dataDir + QStringLiteral("/settings.json"));
    if (settingsFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonObject obj;
        obj.insert(QStringLiteral("onboardingCompleted"), false);
        settingsFile.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        settingsFile.close();
    }

    m_applicationDataCleared = true;
    showNotice(QCoreApplication::translate(
        "Core", "Application data deleted. Arachnel will quit now."));
    QTimer::singleShot(400, qApp, []() { QCoreApplication::quit(); });
    return true;
}

void CoreController::prefetchCatalogCounts()
{
    m_catalogPrefetchQueue.clear();

    for (const auto& source : m_sources.plugins()) {
        if (!source.enabled)
            continue;
        if (m_catalogBySource.contains(source.id))
            continue;

        if (m_pluginHost && m_pluginHost->hasPlugin(source.id))
            m_catalogPrefetchQueue.append(source.id);
        else if (!source.catalogUrl.trimmed().isEmpty())
            m_catalogPrefetchQueue.append(QStringLiteral("url:%1").arg(source.id));
    }

    startNextCatalogPrefetch();
}

void CoreController::prefetchPluginCatalogCount(const QString& sourceId)
{
    ISourcePlugin* plugin = m_pluginHost ? m_pluginHost->plugin(sourceId) : nullptr;
    if (!plugin) {
        startNextCatalogPrefetch();
        return;
    }

    auto* watcher = new QFutureWatcher<QVector<CatalogEntry>>(this);
    connect(watcher, &QFutureWatcher<QVector<CatalogEntry>>::finished, this,
            [this, watcher, sourceId]() {
                const QVector<CatalogEntry> entries = watcher->result();
                watcher->deleteLater();

                if (!entries.isEmpty()) {
                    m_catalogBySource.insert(sourceId, entries);
                    m_catalogCounts.insert(sourceId, entries.size());
                    emit catalogCountsChanged();
                }

                startNextCatalogPrefetch();
            });

    ISourcePlugin* pluginRef = plugin;
    watcher->setFuture(QtConcurrent::run([pluginRef]() { return pluginRef->catalog(); }));
}

void CoreController::startNextCatalogPrefetch()
{
    if (m_catalogPrefetchQueue.isEmpty())
        return;

    const QString item = m_catalogPrefetchQueue.takeFirst();

    if (item.startsWith(QStringLiteral("url:"))) {
        const QString sourceId = item.mid(4);
        const QString url = m_sources.catalogUrlFor(sourceId);
        if (url.isEmpty()) {
            startNextCatalogPrefetch();
            return;
        }

        m_catalogCounts.insert(sourceId, -1);
        emit catalogCountsChanged();
        m_catalogProbeLoader->loadFeed(QUrl(url), QStringLiteral("count:%1").arg(sourceId));
        return;
    }

    prefetchPluginCatalogCount(item);
}

void CoreController::validateHydraCatalogUrl(const QString& requestId, const QString& url)
{
    const QString trimmed = url.trimmed();
    if (requestId.isEmpty() || trimmed.isEmpty()) {
        emit hydraCatalogUrlValidated(requestId, false, 0,
                                      QCoreApplication::translate("Core", "Enter a catalog URL"));
        return;
    }

    const QUrl parsed(trimmed);
    if (!parsed.isValid() || !parsed.scheme().startsWith(QStringLiteral("http"),
                                                         Qt::CaseInsensitive)) {
        emit hydraCatalogUrlValidated(requestId, false, 0,
                                      QCoreApplication::translate("Core", "Invalid URL — http or https required"));
        return;
    }

    m_catalogValidateLoader->loadFeed(parsed, QStringLiteral("validate:%1").arg(requestId));
}

void CoreController::installCatalogEntry(const QString& entryId, const QString& libraryId,
                                         const QVariantList& addonIdsVariant)
{
    if (!ensureProtonReady())
        return;

    const std::optional<CatalogEntry> entryOpt = resolveCatalogEntry(entryId);
    if (!entryOpt) {
        if (const LibraryGame* game = m_libraryStore.gameById(entryId)) {
            if (!game->sourceId.isEmpty())
                requestCatalogLoad(game->sourceId);
        }
        showNotice(QCoreApplication::translate("Core", "Catalog entry not found: %1").arg(entryId));
        return;
    }

    const CatalogEntry& entry = *entryOpt;

    const bool ownsDownload =
        m_pluginHost && m_pluginHost->pluginOwnsDownload(entry.sourceId);

    if (!ownsDownload && entry.magnetUris.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "No download link for %1").arg(entry.title));
        return;
    }

    if (ownsDownload && entry.steamAppId.isEmpty() && entry.magnetUris.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "No Steam App ID for %1").arg(entry.title));
        return;
    }

    const QStringList addonIds = variantListToStringList(addonIdsVariant);
    const QString libId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;

    if (ownsDownload) {
        ISourcePlugin* plugin = m_pluginHost->plugin(entry.sourceId);
        if (!plugin) {
            showNotice(QCoreApplication::translate("Core", "Plugin not loaded: %1").arg(entry.sourceId));
            return;
        }

        const QString jobId =
            m_jobOrchestrator->startPluginOwnedDownload(entry, JobKind::Download, libId);
        if (jobId.isEmpty()) {
            showNotice(
                QCoreApplication::translate("Core", "Could not start download for %1").arg(entry.title));
            return;
        }

        ensureLibraryPlaceholder(entry, libId, addonIds);

        pruneUnselectedAddonJobs(entryId, addonIds);
        if (!addonIds.isEmpty())
            beginInstallSession(entryId, jobId, entry.sourceId, addonIds);
        for (const QString& addonId : addonIds) {
            const CatalogComponent* addon = findCatalogAddon(entry, addonId);
            if (!addon)
                continue;
            m_jobOrchestrator->startAddonDownload(entry, *addon);
        }

        InstallContext ctx;
        ctx.jobId = jobId;
        ctx.entryId = entry.id;
        ctx.sourceId = entry.sourceId;
        ctx.title = entry.title;
        ctx.targetPath = m_settings.resolvedLibraryRoot(libId) + QLatin1Char('/') + entry.id;
        ctx.downloadsPath = m_settings.resolvedDownloadsRoot(libId);
        ctx.downloadPath = ctx.downloadsPath + QLatin1Char('/') + QStringLiteral("install/") + entry.id;
        ctx.magnetUri = entry.steamAppId;
        ctx.uploadDate = entry.uploadDate;
        ctx.installKind = entry.installKind;

        m_pluginHost->runOwnedDownloadAsync(
            plugin, ctx,
            [this, jobId](const OwnedDownloadProgress& progress) {
                m_jobOrchestrator->reportPluginProgress(jobId, progress);
            },
            [this, jobId](const InstallResult& result) {
                if (result.success)
                    m_jobOrchestrator->completePluginDownload(jobId, result.installPath);
                else
                    m_jobOrchestrator->failPluginDownload(
                        jobId, result.error.isEmpty()
                                   ? QCoreApplication::translate("Core", "Install failed")
                                   : result.error);
            });
        return;
    }

    const QString jobId = m_jobOrchestrator->startCatalogDownload(entry, JobKind::Download, libId);
    if (jobId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Could not start download for %1").arg(entry.title));
        return;
    }

    ensureLibraryPlaceholder(entry, libId, addonIds);

    pruneUnselectedAddonJobs(entryId, addonIds);

    if (!addonIds.isEmpty())
        beginInstallSession(entryId, jobId, entry.sourceId, addonIds);

    for (const QString& addonId : addonIds) {
        const CatalogComponent* addon = findCatalogAddon(entry, addonId);
        if (!addon)
            continue;
        m_jobOrchestrator->startAddonDownload(entry, *addon);
    }
}

bool CoreController::needsInstallLocationChoice() const
{
    return m_settings.storageLibraries()->count() > 1;
}

QString CoreController::browseStorageFolder()
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)))
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dialog->SetTitle(L"Выберите папку библиотеки");

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    path = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();
    return path;
#else
    return QFileDialog::getExistingDirectory(
        nullptr,
        QCoreApplication::translate("Core", "Choose library folder"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
#endif
}

void CoreController::removeGame(const QString& gameId, bool deleteFiles)
{
    const LibraryGame* game = m_libraryStore.gameById(gameId);
    if (!game) {
        showNotice(QCoreApplication::translate("Core", "Game not found in library"));
        return;
    }

    if (deleteFiles) {
        QString error;
        const QString libId =
            game->libraryId.isEmpty() ? m_settings.defaultLibraryId() : game->libraryId;
        const QString gameDir = m_settings.gameDirFor(libId, gameId);

        if (!gameDir.isEmpty())
            removePathRecursive(gameDir, &error);

        if (!game->installPath.isEmpty()
            && game->installPath.compare(gameDir, Qt::CaseInsensitive) != 0)
            removePathRecursive(game->installPath, &error);

        if (!game->downloadPath.isEmpty())
            removePathRecursive(game->downloadPath, &error);
    }

    m_libraryStore.removeGame(gameId);
    m_libraryStore.save();
    syncLibraryFromStore();
    removeJobsForEntry(gameId);
    showNotice(QCoreApplication::translate("Core", "Game removed: %1").arg(game->title));
}

void CoreController::removeEntry(const QString& entryId, bool deleteFiles)
{
    if (m_libraryStore.gameById(entryId)) {
        removeGame(entryId, deleteFiles);
        return;
    }

    if (deleteFiles) {
        QString error;
        for (const JobEntry& job : m_jobStore.jobs()) {
            if (job.entryId != entryId || job.savePath.isEmpty())
                continue;
            removePathRecursive(job.savePath, &error);

            const QString libId =
                job.libraryId.isEmpty() ? m_settings.defaultLibraryId() : job.libraryId;
            const QString gameDir = m_settings.gameDirFor(libId, entryId);
            if (!gameDir.isEmpty())
                removePathRecursive(gameDir, &error);
        }
    }

    removeJobsForEntry(entryId);
}

void CoreController::moveGame(const QString& gameId, const QString& targetLibraryId)
{
    if (targetLibraryId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "No destination library selected"));
        return;
    }

    LibraryGame game;
    bool found = false;
    for (const auto& candidate : m_libraryStore.games()) {
        if (candidate.id == gameId) {
            game = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        showNotice(QCoreApplication::translate("Core", "Game not found"));
        return;
    }

    const QString sourceLibId =
        game.libraryId.isEmpty() ? m_settings.defaultLibraryId() : game.libraryId;
    if (sourceLibId == targetLibraryId) {
        showNotice(QCoreApplication::translate("Core", "Game is already on this library"));
        return;
    }

    const QString srcDir = m_settings.gameDirFor(sourceLibId, gameId);
    const QString dstDir = m_settings.gameDirFor(targetLibraryId, gameId);

    QString error;
    if (QDir(srcDir).exists()) {
        if (!movePathRecursive(srcDir, dstDir, &error)) {
            showNotice(QCoreApplication::translate("Core", "Could not move: %1").arg(error));
            return;
        }
    } else if (!game.installPath.isEmpty() && QDir(game.installPath).exists()) {
        QDir().mkpath(QFileInfo(dstDir).absolutePath());
        if (!movePathRecursive(game.installPath, dstDir, &error)) {
            showNotice(QCoreApplication::translate("Core", "Could not move: %1").arg(error));
            return;
        }
    }

    game.libraryId = targetLibraryId;
    if (!game.installPath.isEmpty())
        game.installPath = relocatePathPrefix(game.installPath, srcDir, dstDir);
    if (!game.downloadPath.isEmpty()) {
        const QString oldDownloads = m_settings.resolvedDownloadsRoot(sourceLibId);
        const QString newDownloads = m_settings.resolvedDownloadsRoot(targetLibraryId);
        game.downloadPath = relocatePathPrefix(game.downloadPath, oldDownloads, newDownloads);
    }

    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
    showNotice(QCoreApplication::translate("Core", "Game moved: %1").arg(game.title));
}

QVariantList CoreController::gamesOnLibrary(const QString& libraryId) const
{
    const QString libId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;
    QVariantList rows;
    for (const auto& game : m_libraryStore.games()) {
        const QString gameLib =
            game.libraryId.isEmpty() ? m_settings.defaultLibraryId() : game.libraryId;
        if (gameLib != libId)
            continue;

        QVariantMap row;
        row.insert(QStringLiteral("gameId"), game.id);
        row.insert(QStringLiteral("title"), game.title);
        row.insert(QStringLiteral("coverUrl"), game.coverUrl);
        row.insert(QStringLiteral("sizeLabel"), game.sizeLabel);
        row.insert(QStringLiteral("installPath"), game.installPath);
        row.insert(QStringLiteral("version"), game.version);
        rows.append(row);
    }
    return rows;
}


void CoreController::installCatalogAddon(const QString& entryId, const QString& addonId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Game not found: %1").arg(entryId));
        return;
    }

    const CatalogComponent* addon = findCatalogAddon(*entry, addonId);
    if (!addon) {
        showNotice(QCoreApplication::translate("Core", "Add-on not found"));
        return;
    }

    const QString jobId = m_jobOrchestrator->startAddonDownload(*entry, *addon);
    if (jobId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Could not start add-on download"));
        return;
    }
}

void CoreController::updateCatalogEntry(const QString& entryId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Entry not found: %1").arg(entryId));
        return;
    }

    if (m_pluginHost && m_pluginHost->pluginOwnsDownload(entry->sourceId)) {
        const LibraryGame* game = m_libraryStore.gameById(entryId);
        const QString libId = game && !game->libraryId.isEmpty() ? game->libraryId
                                                                : m_settings.defaultLibraryId();
        installCatalogEntry(entryId, libId, {});
        return;
    }

    const QString jobId = m_jobOrchestrator->startCatalogDownload(*entry, JobKind::Update);
    if (jobId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Could not start update for %1").arg(entry->title));
        return;
    }
}

void CoreController::checkUpdates()
{
    if (m_catalogCache.isEmpty()) {
        const QString sourceId = m_sources.firstEnabledId();
        if (sourceId.isEmpty()) {
            showNotice(QCoreApplication::translate("Core", "No catalog sources enabled"));
            return;
        }
        refreshCatalog(sourceId);
        return;
    }

    recalculateLibraryUpdates(true);
}

void CoreController::cancelJob(const QString& jobId)
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (job && !job->parentEntryId.isEmpty()) {
        m_jobOrchestrator->removeJob(jobId);
        return;
    }

    if (job && job->pluginDownload && m_pluginHost)
        m_pluginHost->cancelOwnedDownload(job->sourceId, jobId);

    const QString entryId = job ? job->entryId : QString();
    m_jobOrchestrator->cancelJob(jobId);

    if (entryId.isEmpty())
        return;

    m_installingEntries.remove(entryId);
    m_installSessions.remove(entryId);
    m_installSelectedAddons.remove(entryId);

    QVector<QString> staleJobIds;
    for (const JobEntry& stored : m_jobStore.jobs()) {
        if (stored.entryId != entryId || !stored.parentEntryId.isEmpty())
            continue;
        if (stored.status == QStringLiteral("cancelled")
            || stored.status == QStringLiteral("failed"))
            staleJobIds.append(stored.id);
    }
    for (const QString& staleId : staleJobIds)
        m_jobOrchestrator->removeJob(staleId);
}

void CoreController::toggleJobPause(const QString& jobId)
{
    if (jobId.isEmpty())
        return;
    m_jobOrchestrator->toggleJobPause(jobId);
}

void CoreController::removeJob(const QString& jobId)
{
    if (jobId.isEmpty())
        return;
    m_jobOrchestrator->removeJob(jobId);
}

void CoreController::retryJob(const QString& jobId)
{
    if (jobId.isEmpty())
        return;
    m_jobOrchestrator->retryJob(jobId);
}

void CoreController::retryInstall(const QString& jobId)
{
    if (jobId.isEmpty())
        return;

    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job) {
        showNotice(QCoreApplication::translate("Core", "Download not found"));
        return;
    }
    if (job->status != QStringLiteral("completed")) {
        showNotice(QCoreApplication::translate("Core", "Installation is only available for completed downloads"));
        return;
    }

    if (!job->parentEntryId.isEmpty()) {
        installDownloadedCatalogAddon(job->parentEntryId, job->entryId);
        return;
    }

    if (!gameNeedsInstall(job->entryId)) {
        for (const JobEntry& addonJob : m_jobStore.jobs()) {
            if (addonJob.parentEntryId != job->entryId)
                continue;
            if (isCatalogAddonInstalled(job->entryId, addonJob.entryId))
                continue;
            if (resolveAddonArtifactPath(job->entryId, addonJob.entryId).isEmpty())
                continue;
            installDownloadedCatalogAddon(job->entryId, addonJob.entryId);
            return;
        }

        const CatalogEntry* parent = findCatalogEntry(job->entryId);
        if (parent) {
            for (const auto& addon : parent->addons) {
                if (isCatalogAddonInstalled(job->entryId, addon.id))
                    continue;
                if (!resolveAddonArtifactPath(job->entryId, addon.id).isEmpty()) {
                    installDownloadedCatalogAddon(job->entryId, addon.id);
                    return;
                }
            }

            bool pendingAddon = false;
            for (const auto& addon : parent->addons) {
                if (!isCatalogAddonInstalled(job->entryId, addon.id)) {
                    pendingAddon = true;
                    break;
                }
            }
            if (pendingAddon) {
                showNotice(QCoreApplication::translate("Core", "Add-on file not found"));
                return;
            }
        }

        if (isEntryPlayable(job->entryId))
            return;

        // Install folder exists but launch is not ready — allow base game reinstall.
    }

    if (job->savePath.isEmpty() || !QDir(job->savePath).exists()) {
        showNotice(QCoreApplication::translate("Core", "Download files not found"));
        return;
    }

    const auto entry = resolveCatalogEntry(job->entryId, job->sourceId, job);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Could not find game to install"));
        return;
    }

    if (!hasInstallHandlerForPath(job->sourceId, job->savePath)) {
        offerManualInstallForJob(*job);
        return;
    }

    startPluginInstall(*entry, job->sourceId, job->savePath, job->kind, job->libraryId, job->id);
}

bool CoreController::canRetryJobInstall(const QString& jobId) const
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job || job->status != QStringLiteral("completed"))
        return false;

    if (isJobInstallFailed(job->detail)) {
        if (!job->parentEntryId.isEmpty()) {
            if (isCatalogAddonInstalled(job->parentEntryId, job->entryId))
                return false;
            return !resolveAddonArtifactPath(job->parentEntryId, job->entryId).isEmpty();
        }
        if (gameNeedsInstall(job->entryId))
            return !job->savePath.isEmpty() && QDir(job->savePath).exists();
        for (const JobEntry& addonJob : m_jobStore.jobs()) {
            if (addonJob.parentEntryId != job->entryId)
                continue;
            if (isCatalogAddonInstalled(job->entryId, addonJob.entryId))
                continue;
            if (!resolveAddonArtifactPath(job->entryId, addonJob.entryId).isEmpty())
                return true;
        }
        return false;
    }

    if (!job->parentEntryId.isEmpty()) {
        if (isCatalogAddonInstalled(job->parentEntryId, job->entryId))
            return false;
        return !resolveAddonArtifactPath(job->parentEntryId, job->entryId).isEmpty();
    }

    if (gameNeedsInstall(job->entryId))
        return !job->savePath.isEmpty() && QDir(job->savePath).exists();

    const CatalogEntry* parent = findCatalogEntry(job->entryId);
    if (!parent)
        return false;
    for (const auto& addon : parent->addons) {
        if (isCatalogAddonInstalled(job->entryId, addon.id))
            continue;
        if (!resolveAddonArtifactPath(job->entryId, addon.id).isEmpty())
            return true;
    }
    return false;
}

bool CoreController::canManualInstallJob(const QString& jobId) const
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job || job->status != QStringLiteral("completed"))
        return false;
    if (!job->parentEntryId.isEmpty())
        return false;
    if (!gameNeedsInstall(job->entryId))
        return false;
    return !job->savePath.isEmpty() && QDir(job->savePath).exists();
}

void CoreController::openJobDownloadFolder(const QString& jobId)
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job || job->savePath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(job->savePath));
}

QString CoreController::browseInstallFolder(const QString& startPath)
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)))
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dialog->SetTitle(L"Choose game install folder");

        if (!startPath.isEmpty()) {
            IShellItem* folder = nullptr;
            QString folderPath = QFileInfo(startPath).isDir() ? startPath
                                                              : QFileInfo(startPath).absolutePath();
            folderPath = QDir::toNativeSeparators(folderPath);
            if (QDir(folderPath).exists()
                && SUCCEEDED(SHCreateItemFromParsingName(
                    reinterpret_cast<LPCWSTR>(folderPath.utf16()), nullptr,
                    IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    path = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();
    return path;
#else
    return QFileDialog::getExistingDirectory(
        nullptr, QCoreApplication::translate("Core", "Choose game install folder"), startPath);
#endif
}

void CoreController::offerManualInstallForJob(const JobEntry& job)
{
    openJobDownloadFolder(job.id);
    showNotice(QCoreApplication::translate(
        "Core",
        "Automatic install is unavailable. Run setup.exe from the download folder, then use the folder button to point to the game."));
}

void CoreController::confirmManualInstall(const QString& jobId)
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job)
        return;

    if (job->savePath.isEmpty() || !QDir(job->savePath).exists()) {
        showNotice(QCoreApplication::translate("Core", "Download files not found"));
        return;
    }

    const QString installFolder = browseInstallFolder(job->savePath);
    if (installFolder.isEmpty())
        return;

    const QString executable = findGameExecutableInTree(installFolder);
    if (executable.isEmpty()) {
        showNotice(QCoreApplication::translate(
            "Core", "No game executable found in %1").arg(installFolder));
        return;
    }

    const auto entry = resolveCatalogEntry(job->entryId, job->sourceId, job);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Could not find game to install"));
        return;
    }

    const InstallKind kind = detectInstallKindForEntry(job->sourceId, job->savePath);
    commitInstalledCatalogGame(*entry, job->sourceId, job->savePath, job->libraryId, installFolder,
                               kind);
    setGameExecutableOverride(job->entryId, executable);
    m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("completed"),
                                   QCoreApplication::translate("Core", "Installed"));
    showNotice(QCoreApplication::translate("Core", "Manual install complete for %1").arg(entry->title));
}

void CoreController::clearFinishedJobs()
{
    m_jobOrchestrator->clearFinishedJobs();
}

void CoreController::prepareShutdown()
{
    if (m_applicationDataCleared)
        return;

    if (m_runningGameTimer)
        m_runningGameTimer->stop();
    clearRunningGame();

    if (m_catalogLoader)
        m_catalogLoader->cancelActive();
    if (m_catalogProbeLoader)
        m_catalogProbeLoader->cancelActive();
    if (m_catalogValidateLoader)
        m_catalogValidateLoader->cancelActive();

    if (m_jobOrchestrator)
        m_jobOrchestrator->flushPersistence();
    if (m_httpSession)
        m_httpSession->shutdown();
    if (m_torrentSession)
        m_torrentSession->shutdown();
    if (m_pluginHost)
        m_pluginHost->shutdownPlugins();
}

int CoreController::pluginCount() const
{
    return m_pluginHost ? m_pluginHost->count() : 0;
}

QString CoreController::pluginsUserDir() const
{
    return PluginHost::writablePluginsDir();
}

QString CoreController::pluginsBundleDir() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");
}

QVariantList CoreController::pluginEntries() const
{
    QVariantList entries;
    for (const auto& source : m_sources.plugins()) {
        if (!source.isPlugin)
            continue;
        QVariantMap row;
        row.insert(QStringLiteral("pluginId"), source.id);
        row.insert(QStringLiteral("name"), source.name);
        row.insert(QStringLiteral("description"), source.description);
        row.insert(QStringLiteral("pluginVersion"), source.pluginVersion);
        row.insert(QStringLiteral("pluginRootPath"), source.pluginRootPath);
        row.insert(QStringLiteral("sourceEnabled"), source.enabled);
        entries.append(row);
    }
    return entries;
}

bool CoreController::installPluginArach(const QUrl& fileUrl)
{
    if (!m_pluginHost)
        return false;

    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    const bool ok = m_pluginHost->installFromArach(path);
    m_lastPluginError = ok ? QString() : m_pluginHost->lastError();
    emit lastPluginErrorChanged();
    if (ok) {
        showNotice(QCoreApplication::translate("Core", "Plugin installed"));
        emit pluginsChanged();
    } else {
        showNotice(QCoreApplication::translate("Core", "Plugin install failed: %1").arg(m_lastPluginError));
    }
    return ok;
}

bool CoreController::uninstallPlugin(const QString& pluginId)
{
    if (!m_pluginHost)
        return false;

    const bool ok = m_pluginHost->uninstallPlugin(pluginId);
    m_lastPluginError = ok ? QString() : m_pluginHost->lastError();
    emit lastPluginErrorChanged();
    if (ok) {
        showNotice(QCoreApplication::translate("Core", "Plugin removed"));
        // PluginHost::scan() already emits pluginsChanged → syncSourcesFromPlugins.
    } else {
        showNotice(QCoreApplication::translate("Core", "Could not remove plugin: %1")
                       .arg(m_lastPluginError));
    }
    return ok;
}

void CoreController::refreshOfficialPlugins()
{
    if (m_pluginCatalog)
        m_pluginCatalog->refresh();
}

void CoreController::installOfficialPlugin(const QString& pluginId)
{
    if (m_pluginCatalog)
        m_pluginCatalog->installPlugin(pluginId);
}

void CoreController::browsePluginArach()
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        const COMDLG_FILTERSPEC filters[] = {
            {L"Пакет плагина (*.arach)", L"*.arach"},
        };
        dialog->SetFileTypes(1, filters);
        dialog->SetTitle(L"Установить плагин");
        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    path = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();

    if (!path.isEmpty())
        installPluginArach(QUrl::fromLocalFile(path));
#else
    const QString path = QFileDialog::getOpenFileName(
        nullptr,
        QCoreApplication::translate("Core", "Install plugin"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        QCoreApplication::translate("Core", "Plugin files (*.arach)"));
    if (!path.isEmpty())
        installPluginArach(QUrl::fromLocalFile(path));
#endif
}

void CoreController::openPluginsFolder()
{
    if (!m_pluginHost)
        return;
    if (!PluginHost::openWritablePluginsDir())
        showNotice(QCoreApplication::translate("Core", "Could not open plugins folder"));
}

void CoreController::rescanPlugins()
{
    if (!m_pluginHost)
        return;
    m_pluginHost->scan();
}

bool CoreController::hasPendingCrashReport() const
{
    return arachnel::hasPendingCrashReport();
}

QString CoreController::pendingCrashSummary() const
{
    return arachnel::pendingCrashSummary();
}

QString CoreController::pendingCrashDetails() const
{
    return arachnel::pendingCrashDetails();
}

QString CoreController::pendingCrashReportPath() const
{
    return arachnel::pendingCrashReportPath();
}

void CoreController::dismissPendingCrashReport()
{
    arachnel::dismissPendingCrashReport();
}

void CoreController::openPendingCrashIssue()
{
    arachnel::openPendingCrashIssue();
}

void CoreController::revealPendingCrashReport()
{
    arachnel::revealPendingCrashReport();
}

void CoreController::copyPendingCrashReport()
{
    if (QGuiApplication* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        if (QClipboard* clipboard = gui->clipboard())
            clipboard->setText(pendingCrashDetails());
    }
}

void registerCoreTypes()
{
    qmlRegisterSingletonType<CoreController>("Arachnel.Core", 1, 0, "Core", &CoreController::create);
    qmlRegisterUncreatableType<LibraryModel>("Arachnel.Core", 1, 0, "LibraryModel",
                                             QStringLiteral("Use Core.library"));
    qmlRegisterUncreatableType<SourcePluginModel>("Arachnel.Core", 1, 0, "SourcePluginModel",
                                                  QStringLiteral("Use Core.sources"));
    qmlRegisterUncreatableType<CatalogModel>("Arachnel.Core", 1, 0, "CatalogModel",
                                             QStringLiteral("Use Core.catalog"));
    qmlRegisterUncreatableType<JobModel>("Arachnel.Core", 1, 0, "JobModel",
                                         QStringLiteral("Use Core.jobs"));
    qmlRegisterUncreatableType<NotificationModel>("Arachnel.Core", 1, 0, "NotificationModel",
                                                  QStringLiteral("Use Core.notifications"));
    qmlRegisterUncreatableType<SettingsStore>("Arachnel.Core", 1, 0, "SettingsStore",
                                              QStringLiteral("Use Core.settings"));
    qmlRegisterUncreatableType<AppUpdater>("Arachnel.Core", 1, 0, "AppUpdater",
                                           QStringLiteral("Use Core.appUpdater"));
    qmlRegisterUncreatableType<PluginCatalogService>("Arachnel.Core", 1, 0, "PluginCatalogService",
                                                     QStringLiteral("Use Core.pluginCatalog"));
    qmlRegisterUncreatableType<StorageLibraryModel>("Arachnel.Core", 1, 0, "StorageLibraryModel",
                                                    QStringLiteral("Use Core.settings.storageLibraries"));
}

} // namespace arachnel::core
