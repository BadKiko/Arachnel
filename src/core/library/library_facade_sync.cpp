#include "core_controller_impl.h"

namespace arachnel::core {

bool CoreController::isEntryPlayable(const QString& entryId) const
{
    return m_libraryController && m_libraryController->isEntryPlayable(entryId);
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
    if (!entry.steamAppId.isEmpty())
        game.steamAppId = entry.steamAppId;

    QSet<QString> selectedAddons(selectedAddonIds.cbegin(), selectedAddonIds.cend());
    QVector<InstalledComponent> components;
    components.reserve(entry.addons.size());
    for (const auto& addon : entry.addons) {
        if (!selectedAddons.isEmpty() && !selectedAddons.contains(addon.id))
            continue;

        bool installed = false;
        bool enabled = true;
        if (existing) {
            for (const auto& component : existing->components) {
                if (component.id == addon.id) {
                    installed = component.installed;
                    enabled = component.enabled;
                    break;
                }
            }
        }

        InstalledComponent component;
        component.id = addon.id;
        component.title = addon.title;
        component.uploadDate = addon.uploadDate;
        component.installed = installed;
        component.enabled = enabled;
        components.append(component);
    }
    game.components = components;

    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();

    if (!entry.coverUrl.isEmpty())
        m_catalogCovers->ensureDiskCover(entry.id, entry.coverUrl);
}

bool CoreController::restartPluginOwnedDownload(const QString& jobId)
{
    const JobEntry* jobPtr = m_jobStore.jobById(jobId);
    if (!jobPtr || !jobPtr->pluginDownload || !m_pluginHost || !m_jobOrchestrator)
        return false;

    ISourcePlugin* plugin = m_pluginHost->plugin(jobPtr->sourceId);
    if (!plugin) {
        showNotice(QCoreApplication::translate("Core", "Plugin not loaded: %1").arg(jobPtr->sourceId));
        return false;
    }

    const auto entryOpt = resolveCatalogEntry(jobPtr->entryId, jobPtr->sourceId, jobPtr);
    if (!entryOpt) {
        showNotice(QCoreApplication::translate("Core", "Catalog entry not found: %1")
                       .arg(jobPtr->entryId));
        return false;
    }
    const CatalogEntry& entry = *entryOpt;
    const JobEntry job = *jobPtr;
    const bool isUpdate = job.kind == JobKind::Update;
    const QString libId = job.libraryId.isEmpty() ? m_settings.defaultLibraryId() : job.libraryId;

    m_jobOrchestrator->preparePluginJobResume(jobId);

    const LibraryGame* existing = m_libraryStore.gameById(entry.id);
    InstallContext ctx;
    ctx.jobId = jobId;
    ctx.entryId = entry.id;
    ctx.sourceId = entry.sourceId;
    ctx.title = entry.title;
    ctx.targetPath =
        existing && !existing->installPath.isEmpty() && QDir(existing->installPath).exists()
            ? existing->installPath
            : m_settings.gameDirFor(libId, entry.id);
    ctx.downloadsPath = m_settings.resolvedDownloadsRoot(libId);
    ctx.downloadPath = job.savePath.isEmpty()
                           ? (ctx.downloadsPath + QLatin1Char('/')
                              + (isUpdate ? QStringLiteral("update/") : QStringLiteral("install/"))
                              + entry.id)
                           : job.savePath;
    ctx.magnetUri = entry.steamAppId.isEmpty() ? job.magnetUri : entry.steamAppId;
    if (ctx.magnetUri.startsWith(QStringLiteral("steam://app/")))
        ctx.magnetUri = ctx.magnetUri.mid(QStringLiteral("steam://app/").size());
    ctx.uploadDate = entry.uploadDate;
    ctx.version = entry.version;
    ctx.steamAppId = entry.steamAppId;
    ctx.installKind = entry.installKind;
    // Empty = resume/continue; "update" forces verify. Never treat retry as brand-new skip.
    ctx.installMode = isUpdate ? QStringLiteral("update") : QString();
    if (existing) {
        for (const InstalledComponent& c : existing->components) {
            if (c.installed)
                ctx.selectedAddonIds.append(c.id);
        }
    }

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
    return true;
}

void CoreController::resumePluginOwnedDownloads()
{
    if (!m_jobOrchestrator)
        return;
    const QVector<QString> ids = m_jobOrchestrator->pluginJobsNeedingResume();
    for (const QString& jobId : ids)
        restartPluginOwnedDownload(jobId);
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

void CoreController::syncLibraryFromStore()
{
    QVector<LibraryGame> games = m_libraryStore.games();
    for (auto& game : games)
        enrichLibraryGameCover(game);
    m_library.setGamesIncremental(std::move(games));
}

void CoreController::enrichLibraryGameCover(LibraryGame& game) const
{
    // Keep only existing local covers; drop remotes and stale file: paths.
    const QString existing = m_coverCache->localUrlFor(game.coverUrl);
    if (!existing.isEmpty()) {
        game.coverUrl = existing;
        return;
    }
    game.coverUrl.clear();

    if (const JobEntry* job = findLatestJobForEntry(game.id)) {
        const QString jobLocal = m_coverCache->localUrlFor(job->coverUrl);
        if (!jobLocal.isEmpty()) {
            game.coverUrl = jobLocal;
            return;
        }
    }

    const GameMetadata metadata = m_metadataService->metadataForTitle(game.title);
    if (metadata.coverUrl.isEmpty())
        return;

    const QString local = m_coverCache->localUrlFor(metadata.coverUrl);
    if (!local.isEmpty())
        game.coverUrl = local;
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
    if (m_gameUpdates)
        return m_gameUpdates->gameHasUpdate(game, remote);
    return false;
}

int CoreController::recalculateLibraryUpdates(bool notify)
{
    return m_gameUpdates ? m_gameUpdates->recalculateLibraryUpdates(notify) : 0;
}

void CoreController::onCatalogReady()
{
    const int updates = recalculateLibraryUpdates(false);
    if (m_catalogDiscovery)
        m_catalogDiscovery->onCatalogCacheRebuilt();

    // Recalc on every catalog ready (source switch), but notify / auto-install only once per session.
    if (m_startupLibraryUpdatesHandled)
        return;
    m_startupLibraryUpdatesHandled = true;

    if (m_settings.autoCheckUpdates() && updates > 0)
        showNotice(QCoreApplication::translate("Core", "%1 update(s) available").arg(updates));

    runAutoInstallUpdates();
}

void CoreController::runAutoInstallUpdates()
{
    if (m_gameUpdates)
        m_gameUpdates->runAutoInstallUpdates();
}

void CoreController::setGameAutoUpdate(const QString& entryId, bool enabled)
{
    if (m_libraryController)
        m_libraryController->setGameAutoUpdate(entryId, enabled);
}

void CoreController::setGameLaunchArgs(const QString& entryId, const QString& args)
{
    if (m_libraryController)
        m_libraryController->setGameLaunchArgs(entryId, args);
}

void CoreController::setGameExecutableOverride(const QString& entryId, const QString& path)
{
    if (m_libraryController)
        m_libraryController->setGameExecutableOverride(entryId, path);
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

void CoreController::setGameOnlineFixEnabled(const QString& entryId, bool enabled)
{
    if (m_libraryController)
        m_libraryController->setGameOnlineFixEnabled(entryId, enabled);
}

void CoreController::setGameAddonEnabled(const QString& entryId, const QString& addonId,
                                         bool enabled)
{
    if (m_libraryController)
        m_libraryController->setGameAddonEnabled(entryId, addonId, enabled);
}

void CoreController::healInstalledAddons(const QString& entryId)
{
    if (m_libraryController)
        m_libraryController->healInstalledAddons(entryId);
}

} // namespace arachnel::core
