#include "core_controller_impl.h"

#include <QFutureWatcher>
#include <QtConcurrent>

namespace arachnel::core {

void CoreController::initializeServices()
{
    m_metadataService = new GameMetadataService(this);
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
    connect(m_catalogCovers, &CatalogCoverCoordinator::coverApplied, this,
            [this](const QString& entryId, const QString& coverUrl) {
                const LibraryGame* existing = m_libraryStore.gameById(entryId);
                if (!existing || existing->coverUrl == coverUrl)
                    return;
                LibraryGame game = *existing;
                game.coverUrl = coverUrl;
                m_libraryStore.upsertGame(game);
                if (!m_library.replaceGame(game))
                    syncLibraryFromStore();
            });
    CatalogController::Hooks catalogHooks;
    catalogHooks.prepareEntry = [this](CatalogEntry& entry) { applyCachedMetadata(entry); };
    catalogHooks.mergedEntriesReady = [this](QVector<CatalogEntry>& entries,
                                             const QStringList& sourceIds, const QString& query) {
        if (!m_installKindProbe)
            return;
        m_installKindProbe->applyCachedKinds(entries);
        for (const QString& sourceId : sourceIds)
            m_installKindProbe->queueCatalog(sourceId, entries, query);
    };
    catalogHooks.rebuildIdIndex = [this]() { rebuildCatalogIdIndex(); };
    catalogHooks.applyFilter = [this](const QString& query) { applyCatalogFilter(query); };
    catalogHooks.rebuildGenres = [this]() { rebuildAvailableCatalogGenres(); };
    catalogHooks.warmCovers = [this]() { warmActiveCatalogCovers(); };
    catalogHooks.catalogReady = [this]() { onCatalogReady(); };
    m_catalogController =
        new CatalogController(&m_catalog, &m_sources, m_pluginHost, &m_catalogCache,
                              std::move(catalogHooks), this);
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
    m_jobOrchestrator->restoreJobs();
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
    m_gameUpdates = new GameUpdateService(&m_libraryStore, &m_settings, m_pluginHost,
                                          m_jobOrchestrator, &m_catalogCache, std::move(updateHooks));

    LaunchController::Hooks launchHooks;
    launchHooks.notice = [this](const QString& message) { showNotice(message); };
    launchHooks.ensureRuntime = [this](const LibraryGame& game) {
        return ensureRuntimeDependenciesForGame(game);
    };
    m_launchController =
        new LaunchController(&m_library, &m_settings, m_pluginHost, std::move(launchHooks), this);
    connect(m_launchController, &LaunchController::runningGameChanged, this,
            &CoreController::runningGameChanged);
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

    connect(m_metadataService, &GameMetadataService::metadataReady, this,
            [this](const QString& entryId, const GameMetadata& metadata) {
                for (auto& entry : m_catalogCache) {
                    if (entry.id != entryId)
                        continue;
                    applyMetadataToEntry(entry, metadata);
                    syncEntryToCatalogModel(entryId);
                    if (m_catalogFilters && !m_catalogFilters->genreFilter().isEmpty())
                        scheduleCatalogRefilter();
                    break;
                }
                if (!metadata.coverUrl.isEmpty())
                    m_catalogCovers->ensureDiskCover(entryId, metadata.coverUrl);
                else
                    m_catalogCovers->applyCover(entryId, {});
                emit entryMetadataChanged(entryId);
                if (!metadata.genres.isEmpty())
                    rebuildAvailableCatalogGenres();
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
                    m_installSessionService->completePluginDownload(
                        *entry, sourceId, job->savePath, libraryId, artifactPath, jobId);
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

} // namespace arachnel::core
