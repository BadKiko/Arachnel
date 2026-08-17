#include "core_controller_impl.h"

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>
#include <QWriteLocker>
#include <QtConcurrent>

namespace arachnel::core {

void CoreController::initializeServices()
{
    m_metadataService = new GameMetadataService(this);
    {
        const QUrl catalogUrl(m_settings.catalogUrlForSource(QStringLiteral("steamidra")));
        if (catalogUrl.isValid() && !catalogUrl.host().isEmpty()) {
            const QString base = QStringLiteral("%1://%2")
                                     .arg(catalogUrl.scheme().isEmpty() ? QStringLiteral("https")
                                                                        : catalogUrl.scheme(),
                                          catalogUrl.authority());
            m_metadataService->setSizeApiBaseUrl(base);
        } else {
            m_metadataService->setSizeApiBaseUrl(QStringLiteral("https://ryuu.badkiko.ru"));
        }
    }
    m_coverCache = new CoverImageCache(this);
    m_catalogCovers = new CatalogCoverCoordinator(
        m_coverCache, m_metadataService, &m_settings, &m_catalog,
        [this](const QString& entryId) -> CatalogEntry* {
            const auto index = m_catalogIdToCacheIndex.constFind(repairCatalogEntryId(entryId));
            if (index != m_catalogIdToCacheIndex.cend() && index.value() >= 0
                && index.value() < m_catalogCache.size()) {
                return &m_catalogCache[index.value()];
            }
            for (CatalogEntry& entry : m_catalogCache) {
                if (entry.id == entryId)
                    return &entry;
            }
            return nullptr;
        },
        [this]() -> QVector<CatalogEntry>& { return m_catalogCache; }, this);
    m_catalogCovers->setCacheLock(&m_catalogCacheLock);

    if (qEnvironmentVariableIntValue("ARACHNEL_COVER_METRICS") > 0) {
        auto* logTimer = new QTimer(this);
        logTimer->setInterval(2000);
        QObject::connect(logTimer, &QTimer::timeout, this, [this]() {
            if (m_catalogCovers)
                qInfo().noquote() << "[cover-metrics]" << m_catalogCovers->metricsText();
        });
        logTimer->start();
    }
    connect(m_catalogCovers, &CatalogCoverCoordinator::coverApplied, this,
            [this](const QString& entryId, const QString& coverUrl) {
                // Discovery shelves use CatalogShelfModel, not CatalogModel - refresh them too.
                if (m_catalogDiscovery)
                    m_catalogDiscovery->onEntryMetadataChanged(entryId);

                // Persist only local cached covers into the library store.
                if (!coverUrl.startsWith(QStringLiteral("file:")))
                    return;

                const LibraryGame* existing = m_libraryStore.gameById(entryId);
                if (!existing || existing->coverUrl == coverUrl)
                    return;
                LibraryGame game = *existing;
                game.coverUrl = coverUrl;
                m_libraryStore.upsertGame(game);
                if (!m_library.replaceGame(game))
                    syncLibraryFromStore();
            });
    connect(m_catalogCovers, &CatalogCoverCoordinator::heroCoverApplied, this,
            [this](const QString& entryId, const QString&) {
                emit catalogHeroCoverChanged(entryId);
            });
    CatalogController::Hooks catalogHooks;
    catalogHooks.prepareEntry = [this](CatalogEntry& entry) { applyCachedMetadata(entry); };
    catalogHooks.mergedEntriesReady = [this](QVector<CatalogEntry>& entries,
                                             const QStringList& sourceIds, const QString& query) {
        if (!m_installKindProbe)
            return;
        // applyCachedKinds is non-const and would detach a shared QVector.
        bool anyMagnet = false;
        for (const CatalogEntry& entry : std::as_const(entries)) {
            if (!entry.magnetUris.isEmpty()) {
                anyMagnet = true;
                break;
            }
        }
        if (!anyMagnet)
            return;
        m_installKindProbe->applyCachedKinds(entries);
        for (const QString& sourceId : sourceIds)
            m_installKindProbe->queueCatalog(sourceId, entries, query);
    };
    catalogHooks.rebuildIdIndex = [this]() {
        rebuildCatalogIdIndex();
        if (m_catalogCovers) {
            m_catalogCovers->rebuildRemoteCoverIndex();
            m_catalogCovers->clearFailedCoverHints();
        }
    };
    catalogHooks.applyFilter = [this](const QString& query) { applyCatalogFilter(query); };
    catalogHooks.rebuildGenres = [this]() { rebuildAvailableCatalogGenres(); };
    catalogHooks.warmCovers = [this]() {
        QTimer::singleShot(750, this, [this]() { warmActiveCatalogCovers(); });
    };
    catalogHooks.catalogReady = [this]() { onCatalogReady(); };
    m_catalogController =
        new CatalogController(&m_catalog, &m_sources, m_pluginHost, &m_catalogCache,
                              std::move(catalogHooks), this);
    m_catalogController->setMergedCacheLock(&m_catalogCacheLock);
    m_catalog.setExtraEntryLookup([this](const QString& id) -> const CatalogEntry* {
        return m_catalogController ? m_catalogController->entryByIdDeep(id) : nullptr;
    });
    if (m_catalogFilters)
        m_catalogFilters->setCacheLock(&m_catalogCacheLock);
    if (m_pluginHost) {
        m_pluginHost->setBeforeUnloadHook([this]() {
            m_pluginCallsBlocked = true;
            waitForCatalogAddonEnrich();
            if (m_catalogController)
                m_catalogController->waitForInFlightPluginCatalogLoads();
            // Drain anything queued while catalog loads finished.
            waitForCatalogAddonEnrich();
            // Install / owned-download workers still hold ISourcePlugin* into the DSO.
            m_pluginHost->waitForInFlightPluginWorkers();
        });
    }
    connect(m_catalogController, &CatalogController::catalogLoadingChanged, this,
            [this](bool) { emit catalogLoadingChanged(); });
    connect(m_catalogController, &CatalogController::catalogStatusChanged, this,
            [this](const QString&) { emit catalogStatusChanged(); });
    connect(m_catalogController, &CatalogController::activeCatalogSourcesChanged, this,
            [this]() {
                emit activeCatalogSourceIdsChanged();
                emit activeCatalogSourceIdChanged();
            });
    connect(m_catalogController, &CatalogController::catalogCountsChanged, this,
            &CoreController::catalogCountsChanged);
    connect(m_catalogController, &CatalogController::noticeRequested, this,
            [this](const QString& message) { showNotice(message); });
    m_torrentSession = new TorrentSession(this);
    m_httpSession = new HttpDownloadSession(this);
    m_jobOrchestrator = new JobOrchestrator(&m_settings, &m_jobStore, m_torrentSession,
                                            m_httpSession, &m_jobs, this);
    connect(m_jobOrchestrator, &JobOrchestrator::pluginDownloadResumeRequested, this,
            [this](const QString& jobId) { restartPluginOwnedDownload(jobId); });
    m_jobOrchestrator->restoreJobs();
    resumePluginOwnedDownloads();
    connect(&m_jobs, &JobModel::jobsChanged, this, &CoreController::syncInstallKindProbeSuspension);
    syncInstallKindProbeSuspension();
    m_protonManager = new ProtonManager(this);
    m_runtimeDependencyService = new RuntimeDependencyService(this);
    InstallSessionService::Hooks installHooks;
    installHooks.showNotice = [this](const QString& message, bool addToHistory) {
        showNotice(message, addToHistory);
    };
    installHooks.findCatalogEntry = [this](const QString& entryId) {
        return findCatalogEntry(entryId);
    };
    installHooks.findCatalogAddon = [this](const CatalogEntry& entry, const QString& addonId) {
        return findCatalogAddon(entry, addonId);
    };
    installHooks.isEntryPlayable = [this](const QString& entryId) {
        return isEntryPlayable(entryId);
    };
    installHooks.isAddonInstalled = [this](const QString& entryId, const QString& addonId) {
        return isCatalogAddonInstalled(entryId, addonId);
    };
    installHooks.addonArtifactPath = [this](const QString& entryId, const QString& addonId) {
        return resolveAddonArtifactPath(entryId, addonId);
    };
    installHooks.markAddonInstalled = [this](const QString& entryId, const QString& addonId,
                                             const QString& uploadDate) {
        markCatalogAddonInstalled(entryId, addonId, uploadDate);
    };
    installHooks.syncCatalogInstallKind = [this](const QString& entryId, InstallKind kind) {
        syncCatalogInstallKind(entryId, kind);
    };
    installHooks.offerManualInstall = [this](const JobEntry& job) { offerManualInstallForJob(job); };
    installHooks.reconcileJobInstallState = [this]() { reconcileJobInstallState(); };
    installHooks.syncLibrary = [this]() { syncLibraryFromStore(); };
    installHooks.recalculateLibraryUpdates = [this]() {
        if (!m_catalogCache.isEmpty())
            recalculateLibraryUpdates(false);
    };
    installHooks.sourceNameForId = [this](const QString& sourceId) {
        return m_sources.nameForId(sourceId);
    };
    installHooks.metadataSteamAppIdForTitle = [this](const QString& title) {
        return m_metadataService ? m_metadataService->metadataForTitle(title).steamAppId : QString();
    };
    installHooks.findGameExecutable = [](const QString& path) {
        return findGameExecutableInTree(path);
    };
    installHooks.fillProtonInstallFields = [](const QString& entryId, const QString& protonId,
                                              QString* executable, QString* compatData,
                                              QString* compatClient) {
        fillProtonInstallFields(entryId, protonId, executable, compatData, compatClient);
    };
    installHooks.gameCommitted = [this](const LibraryGame& game) {
#if defined(Q_OS_LINUX)
        setRuntimeSetupActive(
            game, QCoreApplication::translate("Core", "Preparing runtime environment…"));
        QTimer::singleShot(0, this, [this, game]() {
            ensureRuntimeDependenciesForGame(game);
            clearRuntimeSetup();
        });
#else
        Q_UNUSED(game)
#endif
    };
    m_installSessionService =
        new InstallSessionService(&m_settings, &m_libraryStore, &m_jobStore, &m_jobs,
                                  m_jobOrchestrator, m_pluginHost, m_installAnalyzer,
                                  m_protonManager, std::move(installHooks), this);
    LibraryController::Hooks libraryHooks;
    libraryHooks.syncLibrary = [this]() { syncLibraryFromStore(); };
    libraryHooks.removeJobs = [this](const QString& entryId) { removeJobsForEntry(entryId); };
    libraryHooks.notice = [this](const QString& message) { showNotice(message); };
    libraryHooks.deleteGameFilesAsync = [this](const QStringList& paths, const QString& title) {
        // Jobs were already cancelled on the UI thread; give torrent/HTTP a beat to drop
        // file handles before the wipe worker hits the same folders (Windows locks).
        QTimer::singleShot(400, this, [this, paths, title]() {
            auto* watcher = new QFutureWatcher<QString>(this);
            QObject::connect(watcher, &QFutureWatcher<QString>::finished, this,
                             [this, watcher, title]() {
                                 const QString error = watcher->result();
                                 watcher->deleteLater();
                                 if (!error.isEmpty()) {
                                     showNotice(error);
                                     return;
                                 }
                                 showNotice(QCoreApplication::translate("Core", "Game removed: %1")
                                                .arg(title));
                             });
            watcher->setFuture(QtConcurrent::run([paths]() -> QString {
                QString error;
                for (const QString& path : paths) {
                    if (!removePathRecursive(path, &error))
                        return error;
                }
                return {};
            }));
        });
    };
    libraryHooks.moveGameAsync = [this](const LibraryController::MoveGameWork& work) {
        JobEntry job;
        job.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        job.title = QCoreApplication::translate("Core", "Moving %1").arg(work.title);
        job.kind = JobKind::Move;
        job.status = QStringLiteral("moving");
        job.progress = 0;
        job.detail = QCoreApplication::translate("Core", "Preparing…");
        job.entryId = work.gameId;
        job.sourceId = work.sourceId;
        job.coverUrl = work.coverUrl;
        job.libraryId = work.targetLibraryId;
        job.savePath = work.toPath;
        job.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        m_jobs.addJob(job);
        m_jobStore.upsertJob(job);

        struct MoveResult {
            QString error;
            qint64 totalBytes = 0;
        };

        auto* watcher = new QFutureWatcher<MoveResult>(this);
        const QString jobId = job.id;
        QObject::connect(watcher, &QFutureWatcher<MoveResult>::finished, this,
                         [this, watcher, work, jobId]() {
                             const MoveResult result = watcher->result();
                             watcher->deleteLater();

                             JobEntry done = m_jobStore.jobById(jobId) ? *m_jobStore.jobById(jobId)
                                                                       : JobEntry{};
                             if (done.id.isEmpty()) {
                                 done.id = jobId;
                                 done.kind = JobKind::Move;
                                 done.entryId = work.gameId;
                                 done.title =
                                     QCoreApplication::translate("Core", "Moving %1").arg(work.title);
                             }
                             done.completedAt =
                                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

                             if (!result.error.isEmpty()) {
                                 done.status = QStringLiteral("failed");
                                 done.detail = result.error;
                                 done.progress = 0;
                                 m_jobs.updateJob(done);
                                 m_jobStore.upsertJob(done);
                                 showNotice(QCoreApplication::translate("Core", "Could not move: %1")
                                                .arg(result.error));
                                 return;
                             }

                             if (m_libraryController)
                                 m_libraryController->finalizeMovedGame(
                                     work.gameId, work.targetLibraryId, work.fromPath, work.toPath);

                             done.status = QStringLiteral("completed");
                             done.progress = 100;
                             done.detail.clear();
                             done.bytesDownloaded = result.totalBytes;
                             done.totalBytes = result.totalBytes;
                             m_jobs.updateJob(done);
                             m_jobStore.upsertJob(done);
                             showNotice(QCoreApplication::translate("Core", "Game moved: %1")
                                            .arg(work.title));
                         });

        static QThreadPool movePool;
        static bool movePoolReady = false;
        if (!movePoolReady) {
            movePool.setMaxThreadCount(1);
            movePoolReady = true;
        }

        QPointer<CoreController> self(this);
        watcher->setFuture(QtConcurrent::run(&movePool, [self, work, jobId]() -> MoveResult {
            MoveResult out;
            out.totalBytes = pathByteSize(work.fromPath);
            QDir().mkpath(QFileInfo(work.toPath).absolutePath());

            QElapsedTimer throttle;
            throttle.start();
            auto publish = [&](qint64 done, qint64 total) {
                if (!self)
                    return;
                if (throttle.elapsed() < 200 && done < total)
                    return;
                throttle.restart();
                const int pct =
                    total > 0 ? static_cast<int>(qMin<qint64>(100, (done * 100) / total)) : 0;
                QMetaObject::invokeMethod(
                    self.data(),
                    [self, jobId, done, total, pct]() {
                        if (!self)
                            return;
                        const JobEntry* cur = self->m_jobStore.jobById(jobId);
                        JobEntry job = cur ? *cur : JobEntry{};
                        if (job.id.isEmpty())
                            return;
                        job.status = QStringLiteral("moving");
                        job.progress = pct;
                        job.bytesDownloaded = done;
                        job.totalBytes = total;
                        job.detail = QCoreApplication::translate("Core", "Copying files…");
                        self->m_jobs.updateJob(job);
                        self->m_jobStore.upsertJob(job);
                    },
                    Qt::QueuedConnection);
            };

            QString error;
            if (!movePathRecursive(work.fromPath, work.toPath, &error, publish)) {
                out.error = error.isEmpty()
                                ? QCoreApplication::translate("Core", "Move failed")
                                : error;
                // Best-effort cleanup of a partial destination.
                removePathRecursive(work.toPath);
                return out;
            }
            return out;
        }));
    };
    libraryHooks.findCatalogEntry = [this](const QString& entryId) {
        return findCatalogEntry(entryId);
    };
    libraryHooks.findLatestJob = [this](const QString& entryId) {
        return findLatestJobForEntry(entryId);
    };
    libraryHooks.sourceWebsiteFor = [this](const QString& sourceId) {
        return sourceWebsiteFor(sourceId);
    };
    libraryHooks.detectInstallKind = [this](const QString& sourceId, const QString& path) {
        return detectInstallKindForEntry(sourceId, path);
    };
    m_libraryController = new LibraryController(
        &m_library, &m_catalog, &m_libraryStore, &m_jobStore, &m_settings, m_pluginHost,
        m_metadataService, std::move(libraryHooks));

    LibraryMaintenanceService::Hooks maintenanceHooks;
    maintenanceHooks.syncLibrary = [this]() { syncLibraryFromStore(); };
    maintenanceHooks.restorePlaceholders = [this]() { restoreLibraryPlaceholders(); };
    maintenanceHooks.reconcileInstallState = [this]() { reconcileJobInstallState(); };
    maintenanceHooks.retryPendingInstalls = [this]() { retryPendingInstalls(); };
    m_libraryMaintenance =
        new LibraryMaintenanceService(&m_libraryStore, &m_jobStore, std::move(maintenanceHooks));

    GameUpdateService::Hooks updateHooks;
    updateHooks.syncLibrary = [this]() { syncLibraryFromStore(); };
    updateHooks.notice = [this](const QString& message) { showNotice(message); };
    updateHooks.refreshCatalog = [this]() {
        const QString sourceId = m_sources.firstEnabledId();
        if (sourceId.isEmpty())
            showNotice(QCoreApplication::translate("Core", "No catalog sources enabled"));
        else
            refreshCatalog(sourceId);
    };
    updateHooks.entryPlayable = [this](const QString& entryId) { return isEntryPlayable(entryId); };
    updateHooks.entryHasActiveJob = [this](const QString& entryId) {
        return entryHasActiveJob(entryId);
    };
    updateHooks.updateCatalogEntry = [this](const QString& entryId) {
        updateCatalogEntry(entryId);
    };
    updateHooks.catalogUpdateHasDlcRisk = [this](const QString& entryId) {
        return catalogUpdateHasDlcRisk(entryId);
    };
    m_gameUpdates = new GameUpdateService(&m_libraryStore, &m_settings, m_pluginHost,
                                          m_jobOrchestrator, &m_catalogCache, std::move(updateHooks));

    LaunchController::Hooks launchHooks;
    launchHooks.notice = [this](const QString& message) { showNotice(message); };
    launchHooks.ensureRuntime = [this](const LibraryGame& game) {
        return ensureRuntimeDependenciesForGame(game);
    };
    launchHooks.touchLastPlayed = [this](const QString& gameId) { touchLastPlayed(gameId); };
    m_launchController =
        new LaunchController(&m_library, &m_settings, m_pluginHost, m_protonManager,
                             std::move(launchHooks), this);
    connect(m_launchController, &LaunchController::runningGameChanged, this,
            &CoreController::runningGameChanged);
    m_appUpdater = new AppUpdater(this);
    m_appUpdater->setIncludePreReleases(m_settings.includeAppPreReleases());
    connect(&m_settings, &SettingsStore::includeAppPreReleasesChanged, this, [this]() {
        if (m_appUpdater)
            m_appUpdater->setIncludePreReleases(m_settings.includeAppPreReleases());
    });
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
                    if (installPluginArachInternal(QUrl::fromLocalFile(detail),
                                                   m_autoUpdatingOfficialPlugins)) {
                        QFile::remove(detail);
                        if (m_autoUpdatingOfficialPlugins)
                            ++m_autoUpdatePluginSuccessCount;
                    }
                } else if (!m_autoUpdatingOfficialPlugins) {
                    showNotice(QCoreApplication::translate("Core", "Plugin installed: %1").arg(pluginId));
                } else {
                    ++m_autoUpdatePluginSuccessCount;
                }
            });
    connect(m_pluginCatalog, &PluginCatalogService::installQueueDrained, this, [this]() {
        if (m_autoUpdatingOfficialPlugins)
            finishOfficialPluginAutoUpdate();
    });
    connect(m_appUpdater, &AppUpdater::updateCheckFinished, this,
            [this](bool available, const QString& latestVersion) {
                if (!available)
                    return;
                // History only - AppUpdateSheet in QML owns the prompt UX.
                m_notifications.add(
                    QCoreApplication::translate("Core", "Arachnel %1 is available").arg(latestVersion),
                    QStringLiteral("info"));
            });
    connect(m_appUpdater, &AppUpdater::installerLaunchRequested, this, [this]() {
        prepareShutdown();
        // Give the Setup process time to create its window before we exit.
        QTimer::singleShot(1200, qApp, []() { QCoreApplication::quit(); });
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

    m_catalogValidateLoader = new CatalogFeedLoader(this);
    connect(m_catalogValidateLoader, &CatalogFeedLoader::feedLoaded, this,
            [this](const QString& tag, const QVector<CatalogEntry>& entries, const QByteArray&) {
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

    connect(m_metadataService, &GameMetadataService::metadataReady, this,
            [this](const QString& entryId, const GameMetadata& metadata) {
                bool applied = false;
                {
                    QWriteLocker locker(&m_catalogCacheLock);
                    for (auto& entry : m_catalogCache) {
                        if (entry.id != entryId)
                            continue;
                        applyMetadataToEntry(entry, metadata);
                        applied = true;
                        break;
                    }
                }
                if (applied) {
                    syncEntryToCatalogModel(entryId);
                    if (m_catalogFilters
                        && (!m_catalogFilters->genreFilter().isEmpty()
                            || m_catalogFilters->sizeFilter() > 0
                            || m_catalogFilters->playModeFilter() > 0))
                        scheduleCatalogRefilter();
                }
                if (!metadata.coverUrl.isEmpty())
                    m_catalogCovers->ensureDiskCover(entryId, metadata.coverUrl);
                else
                    m_catalogCovers->applyCover(entryId, {});
                emit entryMetadataChanged(entryId);
                if (m_catalogDiscovery)
                    m_catalogDiscovery->onEntryMetadataChanged(entryId);
                if (!metadata.genres.isEmpty())
                    rebuildAvailableCatalogGenres();
            });

    connect(m_jobOrchestrator, &JobOrchestrator::downloadCompleted, this,
            [this](const QString& jobId, const QString& entryId, const QString& sourceId,
                   const QString& artifactPath, JobKind kind, const QString& libraryId) {
                const JobEntry* job = m_jobStore.jobById(jobId);
                if (job && job->pluginDownload) {
                    auto entry = resolveCatalogEntry(entryId, sourceId, job);
                    if (!entry) {
                        showNotice(QCoreApplication::translate(
                                       "Core", "Could not find game to install: %1")
                                       .arg(entryId));
                        return;
                    }

                    CatalogEntry resolved = *entry;
                    if (kind == JobKind::Update) {
                        // Prefer job expected markers / merged cache over plugin->entryById
                        // (CatalogEntry still crosses the DLL on API 4).
                        if (!job->expectedVersion.isEmpty()
                            && (resolved.version.isEmpty()
                                || job->expectedVersion > resolved.version))
                            resolved.version = job->expectedVersion;
                        if (!job->expectedUploadDate.isEmpty()
                            && (resolved.uploadDate.isEmpty()
                                || job->expectedUploadDate > resolved.uploadDate))
                            resolved.uploadDate = job->expectedUploadDate;
                        if (resolved.steamAppId.isEmpty() && !job->expectedSteamAppId.isEmpty())
                            resolved.steamAppId = job->expectedSteamAppId;

                        const bool weakMarkers =
                            resolved.uploadDate.isEmpty() && resolved.version.isEmpty();
                        if (weakMarkers) {
                            qWarning().noquote()
                                << "[update-commit] weak remote markers for" << entryId
                                << "source" << sourceId
                                << "- committing without advancing version/uploadDate";
                            showNotice(QCoreApplication::translate(
                                "Core",
                                "Update finished, but version info is incomplete. "
                                "Refresh the catalog and update again if the chip stays."));
                            if (m_catalogController && !sourceId.isEmpty())
                                m_catalogController->requestCatalogLoad(sourceId);
                        } else {
                            qInfo().noquote()
                                << "[update-commit]" << entryId
                                << "version" << resolved.version
                                << "uploadDate" << resolved.uploadDate;
                        }
                    }

                    m_installSessionService->completePluginDownload(
                        resolved, sourceId, job->savePath, libraryId, artifactPath, jobId);
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
                    QCoreApplication::translate("Core", "Download complete - install manually"));
            });

    connect(m_jobOrchestrator, &JobOrchestrator::downloadFailed, this,
            [this](const QString& jobId, const QString& error) {
                Q_UNUSED(jobId)
                showNotice(QCoreApplication::translate("Core", "Download error: %1").arg(error));
            });
}

} // namespace arachnel::core
