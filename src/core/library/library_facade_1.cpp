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
        m_catalogCovers->ensureDiskCover(entry.id, entry.coverUrl);
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
    return m_gameUpdates ? m_gameUpdates->recalculateLibraryUpdates(notify) : 0;
}

void CoreController::onCatalogReady()
{
    const int updates = recalculateLibraryUpdates(false);

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

} // namespace arachnel::core
