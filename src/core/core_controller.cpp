#include "core_controller.h"

#include <QDesktopServices>

#include "catalog_feed_loader.h"
#include "catalog_parser.h"
#include "cover_image_cache.h"
#include "file_utils.h"
#include "i18n.h"
#include "install_kind_probe_service.h"
#include "game_metadata_service.h"
#include "http_download_session.h"
#include "job_orchestrator.h"
#include "job_status.h"
#include "job_store.h"
#include "library_store.h"
#include "plugin_host.h"
#include "plugin_interface.h"
#include "process_launcher.h"
#include "process_tracker.h"
#include "settings_store.h"
#include "storage_library_model.h"
#include "torrent_session.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJSEngine>
#include <QQmlEngine>
#include <QTimer>
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

CoreController::CoreController(QObject* parent)
    : QObject(parent)
{
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
    m_installKindProbe = new InstallKindProbeService(m_pluginHost, this);
    connect(m_installKindProbe, &InstallKindProbeService::installKindResolved, this,
            [this](const QString& entryId, InstallKind kind) {
                syncCatalogInstallKind(entryId, kind);
            });
    initializeServices();
    connect(m_pluginHost, &PluginHost::pluginsChanged, this, [this]() {
        syncSourcesFromPlugins();
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
                QVector<CatalogEntry> normalized = entries;
                normalizeCatalogSourceIds(normalized, sourceId);
                m_catalogCounts.insert(sourceId, normalized.size());
                m_catalogBySource.insert(sourceId, normalized);
                emit catalogCountsChanged();
                if (m_activeSourceIds.contains(sourceId))
                    rebuildMergedCatalog();
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
                    if (!metadata.description.isEmpty())
                        entry.description = metadata.description;
                    if (!metadata.genres.isEmpty())
                        entry.genres = metadata.genres;
                    syncEntryToCatalogModel(entryId);
                    break;
                }
                if (!metadata.coverUrl.isEmpty())
                    ensureDiskCover(entryId, metadata.coverUrl);
                else
                    applyCoverToEntry(entryId, QString());
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

                if (m_pluginHost && m_pluginHost->hasPlugin(sourceId)) {
                    const InstallKind detectedKind =
                        detectInstallKindForEntry(sourceId, artifactPath);
                    syncCatalogInstallKind(entryId, detectedKind);
                    startPluginInstall(*entry, sourceId, artifactPath, kind, libraryId, jobId);
                    return;
                }

                LibraryGame game;
                game.id = entryId;
                game.title = entry->title;
                game.coverUrl = entry->coverUrl;
                game.sourceId = sourceId;
                game.sourceName = m_sources.nameForId(sourceId);
                game.version = entry->version;
                game.installPath = QString();
                game.description = entry->description;
                game.genres = entry->genres;
                game.sizeLabel = entry->sizeLabel;
                game.installKind = entry->installKind;
                game.uploadDate = entry->uploadDate;
                game.magnetUri = entry->magnetUris.value(0);
                game.downloadPath = artifactPath;
                game.libraryId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;
                game.hasUpdate = false;

                for (const auto& addon : entry->addons) {
                    InstalledComponent component;
                    component.id = addon.id;
                    component.title = addon.title;
                    component.uploadDate = addon.uploadDate;
                    component.installed = false;
                    game.components.append(component);
                }

                m_libraryStore.upsertGame(game);
                syncLibraryFromStore();
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

    ISourcePlugin* plugin = m_pluginHost ? m_pluginHost->plugin(sourceId) : nullptr;
    if (!plugin) {
        showNotice(QCoreApplication::translate("Core", "Source plugin not found: %1").arg(sourceId));
        return;
    }

    m_installingEntries.insert(entry.id);

    const QString libId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;
    const InstallKind detectedKind = detectInstallKindForEntry(sourceId, savePath);
    syncCatalogInstallKind(entry.id, detectedKind);

    InstallContext ctx;
    ctx.entryId = entry.id;
    ctx.sourceId = sourceId;
    ctx.title = entry.title;
    ctx.targetPath = m_settings.gameDirFor(libId, entry.id);
    ctx.downloadsPath = m_settings.resolvedDownloadsRoot(libId);
    ctx.downloadPath = savePath;
    ctx.magnetUri = entry.magnetUris.value(0);
    ctx.uploadDate = entry.uploadDate;
    ctx.installKind = detectedKind;

    if (!jobId.isEmpty()) {
        if (m_installSessions.contains(entry.id))
            syncInstallSessionPhase(entry.id);
        else
            m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("installing"),
                                           QStringLiteral("Installing…"));
    }

    m_pluginHost->runInstallAsync(plugin, ctx, [this, entry, sourceId, savePath, kind, libId, jobId,
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
            return;
        }

        LibraryGame game;
        game.id = entry.id;
        game.title = entry.title;
        game.coverUrl = entry.coverUrl;
        game.sourceId = sourceId;
        game.sourceName = m_sources.nameForId(sourceId);
        game.version = entry.version;
        game.installPath = result.installPath;
        game.description = entry.description;
        game.genres = entry.genres;
        game.sizeLabel = entry.sizeLabel;
        game.installKind = detectedKind;
        game.uploadDate = entry.uploadDate;
        game.magnetUri = entry.magnetUris.value(0);
        game.downloadPath = savePath;
        game.libraryId = libId;
        game.hasUpdate = false;

        for (const auto& addon : entry.addons) {
            InstalledComponent component;
            component.id = addon.id;
            component.title = addon.title;
            component.uploadDate = addon.uploadDate;
            component.installed = false;
            game.components.append(component);
        }

        m_libraryStore.upsertGame(game);
        syncLibraryFromStore();

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
        showNotice(QCoreApplication::translate("Core", "Source plugin not found: %1").arg(sourceId));
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

void CoreController::markCatalogAddonInstalled(const QString& parentEntryId,
                                               const QString& addonId, const QString& uploadDate)
{
    const LibraryGame* existing = m_libraryStore.gameById(parentEntryId);
    if (!existing)
        return;

    LibraryGame game = *existing;
    bool found = false;
    for (auto& component : game.components) {
        if (component.id != addonId)
            continue;
        component.installed = true;
        if (!uploadDate.isEmpty())
            component.uploadDate = uploadDate;
        found = true;
        break;
    }

    if (!found) {
        InstalledComponent component;
        component.id = addonId;
        component.installed = true;
        component.uploadDate = uploadDate;
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
    return !isEntryPlayable(entryId);
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

    if (m_pluginHost) {
        if (ISourcePlugin* plugin = m_pluginHost->plugin(game->sourceId)) {
            const LaunchInfo info = plugin->launchInfo(*game);
            if (!info.executable.isEmpty())
                return QFileInfo::exists(info.executable);
        }
    }

    return true;
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
    const QString sourceId = info.value(QStringLiteral("sourceId")).toString();
    if (!downloadPath.isEmpty() && !sourceId.isEmpty()) {
        const InstallKind detected = detectInstallKindForEntry(sourceId, downloadPath);
        info.insert(QStringLiteral("installKind"), static_cast<int>(detected));
        info.insert(QStringLiteral("installKindLabel"), installKindLabel(detected));
    }

    info.insert(QStringLiteral("installed"), isEntryPlayable(entryId));
    return info;
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
        if (!gameNeedsInstall(job.entryId))
            continue;
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
        if (!m_pluginHost->hasPlugin(job.sourceId))
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

bool CoreController::gameHasUpdate(const LibraryGame& game, const CatalogEntry& remote) const
{
    if (remote.id.isEmpty())
        return false;

    if (ISourcePlugin* plugin = m_pluginHost ? m_pluginHost->plugin(game.sourceId) : nullptr) {
        if (plugin->detectUpdate(game, remote))
            return true;
    } else if (isRemoteUploadDateNewer(remote.uploadDate, game.uploadDate)) {
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

const CatalogEntry* CoreController::findCatalogEntry(const QString& entryId) const
{
    for (const auto& entry : m_catalogCache) {
        if (entry.id == entryId)
            return &entry;
    }
    return m_catalog.entryById(entryId);
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
    if (!metadata.description.isEmpty())
        entry.description = metadata.description;
    if (!metadata.genres.isEmpty())
        entry.genres = metadata.genres;
    // Leave metadataPending alone — it tracks an in-flight/queued fetch, not "missing cover".
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
    if (downloadPath.isEmpty() || !m_pluginHost)
        return InstallKind::PortableArchive;

    if (ISourcePlugin* plugin = m_pluginHost->plugin(sourceId))
        return plugin->detectInstallKind(downloadPath);
    return InstallKind::PortableArchive;
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
    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly);
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
    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly);
}

void CoreController::enrichCatalogEntry(const QString& entryId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry)
        return;

    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::Full);

    if (m_installKindProbe && m_pluginHost && m_pluginHost->hasPlugin(entry->sourceId)) {
        QString magnet;
        for (const QString& uri : entry->magnetUris) {
            if (uri.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)) {
                magnet = uri;
                break;
            }
        }
        if (magnet.isEmpty())
            magnet = entry->magnetUris.value(0);
        m_installKindProbe->prioritizeEntry(entry->sourceId, entryId, magnet);
    }
}

void CoreController::normalizeCatalogSourceIds(QVector<CatalogEntry>& entries,
                                               const QString& sourceId)
{
    for (auto& entry : entries)
        entry.sourceId = sourceId;
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

    const QString coverSourceId = m_activeSourceIds.value(0);
    warmCatalogCovers(coverSourceId, query, 80);

    if (m_installKindProbe && m_pluginHost) {
        for (const QString& sourceId : m_activeSourceIds) {
            if (m_pluginHost->hasPlugin(sourceId))
                m_installKindProbe->queueCatalog(sourceId, m_catalogCache, query);
        }
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

    deduplicateCatalogEntries(merged);

    m_catalogCache = std::move(merged);
    for (auto& entry : m_catalogCache)
        applyCachedMetadata(entry);

    if (m_installKindProbe) {
        m_installKindProbe->applyCachedKinds(m_catalogCache);
        for (const QString& sourceId : m_activeSourceIds) {
            if (m_pluginHost && m_pluginHost->hasPlugin(sourceId))
                m_installKindProbe->queueCatalog(sourceId, m_catalogCache, m_activeQuery);
        }
    }

    applyCatalogFilter(m_activeQuery);

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
    if (sourceId.isEmpty() || m_catalogBySource.contains(sourceId))
        return;

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
            m_catalogLoadQueue.removeFirst();
            continue;
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
    if (ISourcePlugin* plugin = m_pluginHost->plugin(sourceId)) {
        auto* watcher = new QFutureWatcher<QVector<CatalogEntry>>(this);
        connect(watcher, &QFutureWatcher<QVector<CatalogEntry>>::finished, this,
                [this, watcher, sourceId]() {
                    const QVector<CatalogEntry> entries = watcher->result();
                    watcher->deleteLater();
                    m_loadingSourceIds.remove(sourceId);
                    if (entries.isEmpty()) {
                        if (m_activeSourceIds.contains(sourceId)) {
                            showNotice(QCoreApplication::translate("Core", "Catalog empty or unavailable: %1")
                                               .arg(m_sources.nameForId(sourceId)));
                        }
                        rebuildMergedCatalog();
                        return;
                    }
                    storeCatalogForSource(sourceId, entries);
                });

        ISourcePlugin* pluginRef = plugin;
        plugin->resetCatalogCache();
        watcher->setFuture(QtConcurrent::run([pluginRef]() { return pluginRef->catalog(); }));
        return;
    }

    const QString url = m_sources.catalogUrlFor(sourceId);
    if (url.isEmpty()) {
        m_loadingSourceIds.remove(sourceId);
        showNotice(QCoreApplication::translate("Core", "No catalog URL configured for source %1").arg(sourceId));
        rebuildMergedCatalog();
        return;
    }

    m_catalogHttpLoadActive = true;
    updateCatalogLoadingState();
    m_catalogLoader->loadFeed(QUrl(url), sourceId);
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

    m_runningGameId.clear();
    m_runningGameTitle.clear();
    m_runningGameCoverUrl.clear();
    m_runningProcessId = 0;
    m_runningGameTimer->stop();
    emit runningGameChanged();
}

void CoreController::pollRunningGame()
{
    if (m_runningGameId.isEmpty())
        return;

    if (m_runningProcessId <= 0)
        return;

    if (!ProcessTracker::isProcessRunning(m_runningProcessId)) {
        clearRunningGame();
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

    if (info.executable.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Executable not found for %1").arg(game->title));
        return;
    }

    QString error;
    qint64 processId = 0;
    if (ProcessLauncher::launch(info, &error, &processId)) {
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
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Catalog entry not found: %1").arg(entryId));
        return;
    }

    if (entry->magnetUris.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "No magnet link for %1").arg(entry->title));
        return;
    }

    const QStringList addonIds = variantListToStringList(addonIdsVariant);
    const QString libId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;

    const QString jobId = m_jobOrchestrator->startCatalogDownload(*entry, JobKind::Download, libId);
    if (jobId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Could not start download for %1").arg(entry->title));
        return;
    }

    ensureLibraryPlaceholder(*entry, libId, addonIds);

    pruneUnselectedAddonJobs(entryId, addonIds);

    if (!addonIds.isEmpty())
        beginInstallSession(entryId, jobId, entry->sourceId, addonIds);

    for (const QString& addonId : addonIds) {
        const CatalogComponent* addon = findCatalogAddon(*entry, addonId);
        if (!addon)
            continue;
        m_jobOrchestrator->startAddonDownload(*entry, *addon);
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
        }
        showNotice(QCoreApplication::translate("Core", "Add-on file not found"));
        return;
    }

    if (job->savePath.isEmpty() || !QDir(job->savePath).exists()) {
        showNotice(QCoreApplication::translate("Core", "Download files not found"));
        return;
    }
    if (!m_pluginHost || !m_pluginHost->hasPlugin(job->sourceId)) {
        showNotice(QCoreApplication::translate("Core", "Source plugin not found"));
        return;
    }

    const auto entry = resolveCatalogEntry(job->entryId, job->sourceId, job);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Could not find game to install"));
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

void CoreController::clearFinishedJobs()
{
    m_jobOrchestrator->clearFinishedJobs();
}

void CoreController::prepareShutdown()
{
    if (m_jobOrchestrator)
        m_jobOrchestrator->flushPersistence();
    if (m_torrentSession)
        m_torrentSession->flushResumeData();
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
        QCoreApplication::translate("Core", "Plugin package (*.arach)"));
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
    qmlRegisterUncreatableType<StorageLibraryModel>("Arachnel.Core", 1, 0, "StorageLibraryModel",
                                                    QStringLiteral("Use Core.settings.storageLibraries"));
}

} // namespace arachnel::core
