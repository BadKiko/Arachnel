#include "core_controller.h"

#include "catalog_feed_loader.h"
#include "cover_image_cache.h"
#include "file_utils.h"
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
#include "settings_store.h"
#include "storage_library_model.h"
#include "torrent_session.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJSEngine>
#include <QQmlEngine>
#include <QtConcurrent>
#include <QtQml/qqml.h>

#if defined(Q_OS_WIN)
#include <objbase.h>
#include <shobjidl.h>
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
                if (sourceId != m_activeSourceId)
                    return;

                m_catalogCache = entries;
                for (auto& entry : m_catalogCache)
                    applyCachedMetadata(entry);

                // Model first, then status/loading — otherwise QML can show
                // "Каталог готов · N" with stale "Найдено: 0" / empty state.
                applyCatalogFilter(sourceId, m_activeQuery);
                setCatalogStatus(QStringLiteral("Каталог готов · %1 игр").arg(entries.size()));
                setCatalogLoading(false);
            });

    connect(m_catalogLoader, &CatalogFeedLoader::feedFailed, this,
            [this](const QString& sourceId, const QString& error) {
                if (sourceId != m_activeSourceId)
                    return;
                setCatalogLoading(false);
                setCatalogStatus(error);
                setLastAction(QStringLiteral("Ошибка каталога: %1").arg(error));
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
                        setLastAction(QStringLiteral("Игра не найдена для дополнения"));
                        return;
                    }
                    const CatalogComponent* addon = findCatalogAddon(*parent, entryId);
                    if (!addon) {
                        setLastAction(QStringLiteral("Дополнение не найдено в каталоге"));
                        return;
                    }

                    setLastAction(QStringLiteral("Дополнение загружено: %1").arg(addon->title));
                    advanceInstallSession(job->parentEntryId);
                    return;
                }

                const JobEntry* jobHint = job;
                const auto entry = resolveCatalogEntry(entryId, sourceId, jobHint);
                if (!entry) {
                    setLastAction(QStringLiteral("Не удалось найти игру для установки: %1")
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

                if (kind == JobKind::Update)
                    setLastAction(QStringLiteral("Обновление загружено: %1").arg(entry->title));
                else
                    setLastAction(QStringLiteral("Загрузка завершена: %1").arg(entry->title));
            });

    connect(m_jobOrchestrator, &JobOrchestrator::downloadFailed, this,
            [this](const QString& jobId, const QString& error) {
                Q_UNUSED(jobId)
                setLastAction(QStringLiteral("Ошибка загрузки: %1").arg(error));
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
    if (sourceId != m_activeSourceId)
        return;

    m_catalogCache = std::move(entries);
    for (auto& entry : m_catalogCache)
        applyCachedMetadata(entry);

    if (m_installKindProbe) {
        m_installKindProbe->applyCachedKinds(m_catalogCache);
        m_installKindProbe->queueCatalog(sourceId, m_catalogCache, m_activeQuery);
    }

    applyCatalogFilter(sourceId, m_activeQuery);
    setCatalogStatus(QStringLiteral("Каталог готов · %1 игр").arg(m_catalogCache.size()));
    setCatalogLoading(false);
}

void CoreController::startPluginInstall(const CatalogEntry& entry, const QString& sourceId,
                                        const QString& savePath, JobKind kind,
                                        const QString& libraryId, const QString& jobId)
{
    if (m_installingEntries.contains(entry.id)) {
        setLastAction(QStringLiteral("Установка %1 уже выполняется").arg(entry.title));
        return;
    }

    ISourcePlugin* plugin = m_pluginHost ? m_pluginHost->plugin(sourceId) : nullptr;
    if (!plugin) {
        setLastAction(QStringLiteral("Плагин источника не найден: %1").arg(sourceId));
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
                                           QStringLiteral("Установка…"));
    }
    setLastAction(QStringLiteral("Установка: %1…").arg(entry.title));

    m_pluginHost->runInstallAsync(plugin, ctx, [this, entry, sourceId, savePath, kind, libId, jobId,
                                                detectedKind](const InstallResult& result) {
        m_installingEntries.remove(entry.id);

        if (!result.success) {
            const QString detail =
                result.error.isEmpty()
                    ? QStringLiteral("Ошибка установки")
                    : QStringLiteral("Ошибка установки: %1").arg(result.error);
            if (!jobId.isEmpty())
                m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("completed"), detail);
            m_installSessions.remove(entry.id);
            m_installSelectedAddons.remove(entry.id);
            setLastAction(QStringLiteral("Ошибка установки %1: %2")
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
                                               QStringLiteral("Установлено"));
        }

        if (kind == JobKind::Update)
            setLastAction(QStringLiteral("Обновление установлено: %1").arg(entry.title));
        else
            setLastAction(QStringLiteral("Установлено: %1").arg(entry.title));
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

    const QString detail = QStringLiteral("Установка (%1/%2)")
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
                                   QStringLiteral("Установлено"));
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
        setLastAction(QStringLiteral("Установка дополнения уже выполняется"));
        return;
    }

    const LibraryGame* game = m_libraryStore.gameById(parent.id);
    if (!game || game->installPath.isEmpty()) {
        setLastAction(QStringLiteral("Сначала установите игру"));
        return;
    }

    ISourcePlugin* plugin = m_pluginHost ? m_pluginHost->plugin(sourceId) : nullptr;
    if (!plugin) {
        setLastAction(QStringLiteral("Плагин источника не найден: %1").arg(sourceId));
        return;
    }

    m_installingAddons.insert(installKey);

    if (!progressJobId.isEmpty() && !m_installSessions.contains(parent.id))
        m_jobOrchestrator->setJobPhase(progressJobId, QStringLiteral("installing"),
                                       QStringLiteral("Установка дополнения…"));
    setLastAction(QStringLiteral("Установка дополнения: %1…").arg(addon.title));

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
                                                         ? QStringLiteral("Ошибка установки")
                                                         : QStringLiteral("Ошибка: %1")
                                                               .arg(result.error);
                                                 if (!progressJobId.isEmpty())
                                                     m_jobOrchestrator->setJobPhase(
                                                         progressJobId,
                                                         QStringLiteral("completed"), detail);
                                                 m_installSessions.remove(parent.id);
                                                 m_installSelectedAddons.remove(parent.id);
                                                 setLastAction(QStringLiteral(
                                                     "Ошибка установки дополнения %1: %2")
                                                                   .arg(addon.title, result.error));
                                                 if (done)
                                                     done(false);
                                                 return;
                                             }

                                             markCatalogAddonInstalled(parent.id, addon.id,
                                                                       addon.uploadDate);
                                             setLastAction(QStringLiteral("Дополнение установлено: %1")
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
        setLastAction(QStringLiteral("Игра не найдена"));
        return;
    }

    const CatalogComponent* addon = findCatalogAddon(*parent, addonId);
    if (!addon) {
        setLastAction(QStringLiteral("Дополнение не найдено"));
        return;
    }

    const QString artifactPath = resolveAddonArtifactPath(entryId, addonId);
    if (artifactPath.isEmpty()) {
        setLastAction(QStringLiteral("Сначала скачайте дополнение"));
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
    if (synthetic.title.startsWith(QStringLiteral("Загрузка ")))
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
        if (!job.parentEntryId.isEmpty())
            continue;
        if (job.status != QStringLiteral("completed"))
            continue;
        if (!gameNeedsInstall(job.entryId))
            continue;
        if (job.detail == QStringLiteral("Установлено")) {
            m_jobOrchestrator->setJobPhase(job.id, QStringLiteral("completed"),
                                           QStringLiteral("Требуется установка"));
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
    m_library.setGames(m_libraryStore.games());
}

void CoreController::setLastAction(const QString& action)
{
    if (m_lastAction == action)
        return;
    m_lastAction = action;
    emit lastActionChanged();
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
    const QDateTime remoteDate = QDateTime::fromString(remote, Qt::ISODate);
    const QDateTime localDate = QDateTime::fromString(local, Qt::ISODate);
    if (!remoteDate.isValid() || !localDate.isValid())
        return remote > local;
    return remoteDate > localDate;
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
            applyCatalogFilter(m_activeSourceId, m_activeQuery);
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

void CoreController::applyCatalogFilter(const QString& sourceId, const QString& query)
{
    const QString needle = query.trimmed().toLower();
    QVector<CatalogEntry> filtered;
    filtered.reserve(m_catalogCache.size());

    for (const auto& entry : m_catalogCache) {
        if (entry.sourceId != sourceId)
            continue;
        if (!needle.isEmpty() && !entry.title.toLower().contains(needle))
            continue;
        filtered.append(entry);
    }

    m_catalog.setEntries(std::move(filtered));
    setLastAction(QStringLiteral("Каталог %1: найдено %2").arg(sourceId).arg(m_catalog.rowCount()));

    if (m_installKindProbe && m_pluginHost && m_pluginHost->hasPlugin(sourceId))
        m_installKindProbe->queueCatalog(sourceId, m_catalogCache, query);
}

void CoreController::launchGame(const QString& gameId)
{
    const LibraryGame* game = m_library.gameById(gameId);
    if (!game) {
        setLastAction(QStringLiteral("Игра не найдена: %1").arg(gameId));
        return;
    }
    if (game->installPath.isEmpty()) {
        setLastAction(QStringLiteral("%1 ещё не установлена").arg(game->title));
        return;
    }

    LaunchInfo info;
    if (ISourcePlugin* plugin = m_pluginHost->plugin(game->sourceId))
        info = plugin->launchInfo(*game);

    if (info.executable.isEmpty()) {
        setLastAction(QStringLiteral("Не найден исполняемый файл для %1").arg(game->title));
        return;
    }

    QString error;
    if (ProcessLauncher::launch(info, &error)) {
        setLastAction(QStringLiteral("Запуск: %1").arg(game->title));
        return;
    }
    setLastAction(error.isEmpty() ? QStringLiteral("Не удалось запустить игру") : error);
}

void CoreController::refreshCatalog(const QString& sourceId)
{
    if (ISourcePlugin* plugin = m_pluginHost->plugin(sourceId)) {
        m_activeSourceId = sourceId;
        setCatalogLoading(true);
        setCatalogStatus(QStringLiteral("Загрузка каталога…"));
        m_catalog.clear();
        m_catalogCache.clear();

        auto* watcher = new QFutureWatcher<QVector<CatalogEntry>>(this);
        connect(watcher, &QFutureWatcher<QVector<CatalogEntry>>::finished, this,
                [this, watcher, sourceId]() {
                    const QVector<CatalogEntry> entries = watcher->result();
                    watcher->deleteLater();
                    if (entries.isEmpty()) {
                        setCatalogLoading(false);
                        setCatalogStatus(QStringLiteral("Каталог пуст или недоступен"));
                        return;
                    }
                    applyPluginCatalog(sourceId, entries);
                });

        ISourcePlugin* pluginRef = plugin;
        plugin->resetCatalogCache();
        watcher->setFuture(QtConcurrent::run([pluginRef]() { return pluginRef->catalog(); }));
        return;
    }

    const QString url = m_sources.catalogUrlFor(sourceId);
    if (url.isEmpty()) {
        setCatalogStatus(QStringLiteral("Для источника %1 не задан URL каталога").arg(sourceId));
        return;
    }

    m_activeSourceId = sourceId;
    setCatalogLoading(true);
    setCatalogStatus(QStringLiteral("Загрузка каталога…"));
    m_catalog.clear();
    m_catalogCache.clear();
    m_catalogLoader->loadFeed(QUrl(url), sourceId);
}

void CoreController::searchCatalog(const QString& sourceId, const QString& query)
{
    const SourcePluginInfo* source = m_sources.pluginById(sourceId);
    if (!source) {
        m_catalog.clear();
        setLastAction(QStringLiteral("Неизвестный источник: %1").arg(sourceId));
        return;
    }
    if (!source->enabled) {
        m_catalog.clear();
        setCatalogStatus(QStringLiteral("Источник «%1» выключен в настройках").arg(source->name));
        return;
    }

    m_activeSourceId = sourceId;
    m_activeQuery = query;

    if (m_catalogCache.isEmpty() || m_catalogCache.first().sourceId != sourceId) {
        refreshCatalog(sourceId);
        return;
    }

    applyCatalogFilter(sourceId, query);
}

void CoreController::installCatalogEntry(const QString& entryId, const QString& libraryId,
                                         const QVariantList& addonIdsVariant)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        setLastAction(QStringLiteral("Запись каталога не найдена: %1").arg(entryId));
        return;
    }

    if (entry->magnetUris.isEmpty()) {
        setLastAction(QStringLiteral("Нет magnet-ссылки для %1").arg(entry->title));
        return;
    }

    const QStringList addonIds = variantListToStringList(addonIdsVariant);
    const QString libId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;

    const QString jobId = m_jobOrchestrator->startCatalogDownload(*entry, JobKind::Download, libId);
    if (jobId.isEmpty()) {
        setLastAction(QStringLiteral("Не удалось начать загрузку %1").arg(entry->title));
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

    if (addonIds.isEmpty())
        setLastAction(QStringLiteral("Торрент-загрузка: %1").arg(entry->title));
    else
        setLastAction(QStringLiteral("Загрузка: %1 и %2 доп.")
                          .arg(entry->title)
                          .arg(addonIds.size()));
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
    setLastAction(QStringLiteral("Выбор папки пока доступен только в Windows"));
    return {};
#endif
}

void CoreController::removeGame(const QString& gameId, bool deleteFiles)
{
    const LibraryGame* game = m_libraryStore.gameById(gameId);
    if (!game) {
        setLastAction(QStringLiteral("Игра не найдена в библиотеке"));
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
    setLastAction(QStringLiteral("Игра удалена: %1").arg(game->title));
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
    setLastAction(QStringLiteral("Загрузка удалена"));
}

void CoreController::moveGame(const QString& gameId, const QString& targetLibraryId)
{
    if (targetLibraryId.isEmpty()) {
        setLastAction(QStringLiteral("Не выбран диск назначения"));
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
        setLastAction(QStringLiteral("Игра не найдена"));
        return;
    }

    const QString sourceLibId =
        game.libraryId.isEmpty() ? m_settings.defaultLibraryId() : game.libraryId;
    if (sourceLibId == targetLibraryId) {
        setLastAction(QStringLiteral("Игра уже на этом диске"));
        return;
    }

    const QString srcDir = m_settings.gameDirFor(sourceLibId, gameId);
    const QString dstDir = m_settings.gameDirFor(targetLibraryId, gameId);

    QString error;
    if (QDir(srcDir).exists()) {
        if (!movePathRecursive(srcDir, dstDir, &error)) {
            setLastAction(QStringLiteral("Не удалось перенести: %1").arg(error));
            return;
        }
    } else if (!game.installPath.isEmpty() && QDir(game.installPath).exists()) {
        QDir().mkpath(QFileInfo(dstDir).absolutePath());
        if (!movePathRecursive(game.installPath, dstDir, &error)) {
            setLastAction(QStringLiteral("Не удалось перенести: %1").arg(error));
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
    setLastAction(QStringLiteral("Игра перенесена: %1").arg(game.title));
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
        setLastAction(QStringLiteral("Игра не найдена: %1").arg(entryId));
        return;
    }

    const CatalogComponent* addon = findCatalogAddon(*entry, addonId);
    if (!addon) {
        setLastAction(QStringLiteral("Дополнение не найдено"));
        return;
    }

    const QString jobId = m_jobOrchestrator->startAddonDownload(*entry, *addon);
    if (jobId.isEmpty()) {
        setLastAction(QStringLiteral("Не удалось начать загрузку дополнения"));
        return;
    }

    setLastAction(QStringLiteral("Загрузка дополнения: %1").arg(addon->title));
}

void CoreController::updateCatalogEntry(const QString& entryId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        setLastAction(QStringLiteral("Запись не найдена: %1").arg(entryId));
        return;
    }

    const QString jobId = m_jobOrchestrator->startCatalogDownload(*entry, JobKind::Update);
    if (jobId.isEmpty()) {
        setLastAction(QStringLiteral("Не удалось начать обновление %1").arg(entry->title));
        return;
    }

    setLastAction(QStringLiteral("Обновление: %1").arg(entry->title));
}

void CoreController::checkUpdates()
{
    if (m_catalogCache.isEmpty()) {
        const QString sourceId = m_sources.firstEnabledId();
        if (sourceId.isEmpty()) {
            setLastAction(QStringLiteral("Нет включённых источников каталога"));
            return;
        }
        refreshCatalog(sourceId);
        setLastAction(QStringLiteral("Сначала загружаем каталог для проверки обновлений…"));
        return;
    }

    QHash<QString, CatalogEntry> remoteById;
    for (const auto& entry : m_catalogCache)
        remoteById.insert(entry.id, entry);

    QVector<LibraryGame> games = m_libraryStore.games();
    int updates = 0;
    for (auto& game : games) {
        const CatalogEntry remote = remoteById.value(game.id);
        bool hasUpdate = false;

        if (!remote.id.isEmpty()
            && isRemoteUploadDateNewer(remote.uploadDate, game.uploadDate)) {
            hasUpdate = true;
        }

        for (auto& component : game.components) {
            for (const auto& remoteAddon : remote.addons) {
                if (remoteAddon.id != component.id)
                    continue;
                // Uninstalled addons are available downloads, not "updates".
                if (component.installed
                    && isRemoteUploadDateNewer(remoteAddon.uploadDate, component.uploadDate)) {
                    hasUpdate = true;
                }
            }
        }

        game.hasUpdate = hasUpdate;
        if (hasUpdate)
            ++updates;
    }

    m_libraryStore.setGames(games);
    syncLibraryFromStore();
    setLastAction(QStringLiteral("Проверка обновлений: найдено %1").arg(updates));
}

void CoreController::cancelJob(const QString& jobId)
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (job && !job->parentEntryId.isEmpty()) {
        m_jobOrchestrator->removeJob(jobId);
        setLastAction(QStringLiteral("Дополнение убрано из загрузок"));
        return;
    }

    m_jobOrchestrator->cancelJob(jobId);
    setLastAction(QStringLiteral("Задача отменена"));
}

void CoreController::toggleJobPause(const QString& jobId)
{
    if (jobId.isEmpty())
        return;
    m_jobOrchestrator->toggleJobPause(jobId);
    setLastAction(QStringLiteral("Пауза загрузки"));
}

void CoreController::removeJob(const QString& jobId)
{
    if (jobId.isEmpty())
        return;
    m_jobOrchestrator->removeJob(jobId);
    setLastAction(QStringLiteral("Загрузка удалена из списка"));
}

void CoreController::retryJob(const QString& jobId)
{
    if (jobId.isEmpty())
        return;
    m_jobOrchestrator->retryJob(jobId);
    setLastAction(QStringLiteral("Повтор загрузки"));
}

void CoreController::retryInstall(const QString& jobId)
{
    if (jobId.isEmpty())
        return;

    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job) {
        setLastAction(QStringLiteral("Загрузка не найдена"));
        return;
    }
    if (job->status != QStringLiteral("completed")) {
        setLastAction(QStringLiteral("Установка доступна только для завершённых загрузок"));
        return;
    }
    if (!job->parentEntryId.isEmpty()) {
        setLastAction(QStringLiteral("Дополнения устанавливаются вместе с игрой"));
        return;
    }
    if (!gameNeedsInstall(job->entryId)) {
        setLastAction(QStringLiteral("Игра уже установлена"));
        return;
    }
    if (job->savePath.isEmpty() || !QDir(job->savePath).exists()) {
        setLastAction(QStringLiteral("Файлы загрузки не найдены"));
        return;
    }
    if (!m_pluginHost || !m_pluginHost->hasPlugin(job->sourceId)) {
        setLastAction(QStringLiteral("Плагин источника не найден"));
        return;
    }

    const auto entry = resolveCatalogEntry(job->entryId, job->sourceId, job);
    if (!entry) {
        setLastAction(QStringLiteral("Не удалось найти игру для установки"));
        return;
    }

    startPluginInstall(*entry, job->sourceId, job->savePath, job->kind, job->libraryId, job->id);
    setLastAction(QStringLiteral("Повтор установки: %1…").arg(entry->title));
}

void CoreController::clearFinishedJobs()
{
    m_jobOrchestrator->clearFinishedJobs();
    setLastAction(QStringLiteral("История загрузок очищена"));
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

bool CoreController::installPluginZip(const QUrl& fileUrl)
{
    if (!m_pluginHost)
        return false;

    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    const bool ok = m_pluginHost->installFromZip(path);
    m_lastPluginError = ok ? QString() : m_pluginHost->lastError();
    emit lastPluginErrorChanged();
    if (ok) {
        setLastAction(QStringLiteral("Плагин установлен"));
        emit pluginsChanged();
    } else {
        setLastAction(QStringLiteral("Ошибка установки плагина: %1").arg(m_lastPluginError));
    }
    return ok;
}

void CoreController::browsePluginZip()
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        const COMDLG_FILTERSPEC filters[] = {
            {L"Пакет плагина (*.zip)", L"*.zip"},
            {L"Все файлы", L"*.*"},
        };
        dialog->SetFileTypes(2, filters);
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
        installPluginZip(QUrl::fromLocalFile(path));
#else
    setLastAction(QStringLiteral("Выбор файла пока доступен только в Windows"));
#endif
}

void CoreController::openPluginsFolder()
{
    if (!m_pluginHost)
        return;
    if (!PluginHost::openWritablePluginsDir())
        setLastAction(QStringLiteral("Не удалось открыть папку плагинов"));
}

void CoreController::rescanPlugins()
{
    if (!m_pluginHost)
        return;
    m_pluginHost->scan();
    setLastAction(QStringLiteral("Плагины: %1").arg(m_pluginHost->count()));
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
    qmlRegisterUncreatableType<SettingsStore>("Arachnel.Core", 1, 0, "SettingsStore",
                                              QStringLiteral("Use Core.settings"));
    qmlRegisterUncreatableType<StorageLibraryModel>("Arachnel.Core", 1, 0, "StorageLibraryModel",
                                                    QStringLiteral("Use Core.settings.storageLibraries"));
}

} // namespace arachnel::core
