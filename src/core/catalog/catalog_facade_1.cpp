#include "core_controller_impl.h"

namespace arachnel::core {

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

QVariantMap CoreController::gameRuntimeContainerInfo(const QString& gameId) const
{
#if !defined(Q_OS_LINUX)
    (void)gameId;
    return {};
#else
    if (gameId.trimmed().isEmpty() || !m_runtimeDependencyService)
        return {};

    RuntimeEnsureRequest request;
    request.gameId = gameId;
    if (const LibraryGame* game = m_library.gameById(gameId)) {
        request.steamAppId = game->steamAppId;
        request.title = game->title;
        request.installPath = game->installPath;
    }
    if (request.steamAppId.isEmpty()) {
        if (const CatalogEntry* entry = findCatalogEntry(gameId))
            request.steamAppId = entry->steamAppId;
    }
    return m_runtimeDependencyService->containerInfoForGame(request);
#endif
}

void CoreController::openGameRuntimeContainer(const QString& gameId)
{
#if !defined(Q_OS_LINUX)
    (void)gameId;
#else
    const QVariantMap info = gameRuntimeContainerInfo(gameId);
    const QString path = info.value(QStringLiteral("containerPath")).toString();
    if (path.isEmpty())
        return;
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
}

const CatalogEntry* CoreController::findCatalogEntry(const QString& entryId) const
{
    const QString resolved = repairCatalogEntryId(entryId);
    const auto cacheIt = m_catalogIdToCacheIndex.constFind(resolved);
    if (cacheIt != m_catalogIdToCacheIndex.cend()) {
        const int idx = cacheIt.value();
        if (idx >= 0 && idx < m_catalogCache.size())
            return &m_catalogCache.at(idx);
    }
    if (m_catalogController) {
        const auto& catalogs = m_catalogController->catalogsBySource();
        for (auto it = catalogs.cbegin(); it != catalogs.cend(); ++it) {
            for (const auto& entry : it.value()) {
                if (entry.id == resolved)
                    return &entry;
            }
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
        else if (entry.coverUrl.isEmpty()
                 && (metadata.coverUrl.contains(QStringLiteral("library_capsule"))
                     || metadata.coverUrl.contains(QStringLiteral("library_600x900"))))
            entry.coverUrl = metadata.coverUrl;
    }
    applyMetadataToEntry(entry, metadata);
    // Leave metadataPending alone — it tracks an in-flight/queued fetch, not "missing cover".
}

QString CoreController::sourceWebsiteFor(const QString& sourceId) const
{
    if (sourceId == QStringLiteral("freetp"))
        return QStringLiteral("https://freetp.org/");
    // Do not invent a website from catalogUrl (relay / API hosts are not a public source site).
    Q_UNUSED(sourceId);
    return {};
}

void CoreController::applyMetadataToEntry(CatalogEntry& entry,
                                          const GameMetadata& metadata) const
{
    if (!metadata.description.isEmpty())
        entry.description = metadata.description;
    if (!metadata.genres.isEmpty()) {
        // Keep Ryuu DRM token when Steam store genres replace the catalog string.
        const bool hadDrm = entry.genres.contains(QStringLiteral("DRM"), Qt::CaseInsensitive);
        entry.genres = metadata.genres;
        if (hadDrm && !entry.genres.contains(QStringLiteral("DRM"), Qt::CaseInsensitive)) {
            if (!entry.genres.isEmpty())
                entry.genres += QStringLiteral(", ");
            entry.genres += QStringLiteral("DRM");
        }
    }
    if (!metadata.steamAppId.isEmpty())
        entry.steamAppId = metadata.steamAppId;
    if (!metadata.trailerUrl.isEmpty())
        entry.trailerUrl = metadata.trailerUrl;
    if (!metadata.trailerThumbnailUrl.isEmpty())
        entry.trailerThumbnailUrl = metadata.trailerThumbnailUrl;
    if (!metadata.screenshotUrls.isEmpty())
        entry.screenshotUrls = metadata.screenshotUrls;
    prepareCatalogEntry(entry);
}

void CoreController::syncEntryToCatalogModel(const QString& entryId)
{
    m_catalog.notifyEntryChanged(entryId);
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
    m_installKindProbe->setBackgroundProbesEnabled(m_jobs.activeCount() == 0);
}

void CoreController::syncCatalogInstallKind(const QString& entryId, InstallKind kind)
{
    const auto it = m_catalogIdToCacheIndex.constFind(entryId);
    if (it == m_catalogIdToCacheIndex.cend())
        return;
    const int idx = it.value();
    if (idx < 0 || idx >= m_catalogCache.size())
        return;
    CatalogEntry& entry = m_catalogCache[idx];
    if (entry.installKind == kind)
        return;
    entry.installKind = kind;
    syncEntryToCatalogModel(entryId);
}

void CoreController::requestCatalogCover(const QString& entryId)
{
    if (m_catalogCovers)
        m_catalogCovers->requestCatalogCover(entryId);
}

void CoreController::cancelCatalogCover(const QString& entryId)
{
    if (m_catalogCovers)
        m_catalogCovers->cancelCatalogCover(entryId);
}

void CoreController::invalidateCatalogCover(const QString& entryId)
{
    if (m_catalogCovers)
        m_catalogCovers->invalidateCatalogCover(entryId);
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

int CoreController::catalogTypeFilter() const
{
    return m_catalogFilters ? m_catalogFilters->typeFilter() : -1;
}

void CoreController::setCatalogTypeFilter(int filter)
{
    if (m_catalogFilters)
        m_catalogFilters->setTypeFilter(filter);
}

int CoreController::catalogSizeFilter() const
{
    return m_catalogFilters ? m_catalogFilters->sizeFilter() : 0;
}

void CoreController::setCatalogSizeFilter(int filter)
{
    if (m_catalogFilters)
        m_catalogFilters->setSizeFilter(filter);
}

int CoreController::catalogRecencyFilter() const
{
    return m_catalogFilters ? m_catalogFilters->recencyFilter() : 0;
}

void CoreController::setCatalogRecencyFilter(int filter)
{
    if (m_catalogFilters)
        m_catalogFilters->setRecencyFilter(filter);
}

bool CoreController::catalogHasAddonsFilter() const
{
    return m_catalogFilters && m_catalogFilters->hasAddonsFilter();
}

void CoreController::setCatalogHasAddonsFilter(bool enabled)
{
    if (m_catalogFilters)
        m_catalogFilters->setHasAddonsFilter(enabled);
}

QString CoreController::catalogGenreFilter() const
{
    return m_catalogFilters ? m_catalogFilters->genreFilter() : QString();
}

void CoreController::setCatalogGenreFilter(const QString& genre)
{
    if (m_catalogFilters)
        m_catalogFilters->setGenreFilter(genre);
}

int CoreController::catalogPlayModeFilter() const
{
    return m_catalogFilters ? m_catalogFilters->playModeFilter() : 0;
}

void CoreController::setCatalogPlayModeFilter(int filter)
{
    if (m_catalogFilters)
        m_catalogFilters->setPlayModeFilter(filter);
}

int CoreController::catalogActiveFilterCount() const
{
    return m_catalogFilters ? m_catalogFilters->activeFilterCount() : 0;
}

QStringList CoreController::availableCatalogGenres() const
{
    return m_catalogFilters ? m_catalogFilters->availableGenres() : QStringList();
}

void CoreController::clearCatalogFilters()
{
    if (m_catalogFilters)
        m_catalogFilters->clearFilters();
}

void CoreController::setCatalogFilters(int typeFilter, int sizeFilter, int recencyFilter,
                                       bool hasAddonsFilter, const QString& genreFilter,
                                       int playModeFilter)
{
    if (m_catalogFilters)
        m_catalogFilters->setFilters(typeFilter, sizeFilter, recencyFilter, hasAddonsFilter,
                                     genreFilter, playModeFilter);
}

void CoreController::applyCatalogPresentation(int sortMode, int typeFilter, int sizeFilter,
                                              int recencyFilter, bool hasAddonsFilter,
                                              const QString& genreFilter, int playModeFilter)
{
    if (m_catalogFilters)
        m_catalogFilters->applyPresentation(sortMode, typeFilter, sizeFilter, recencyFilter,
                                            hasAddonsFilter, genreFilter, playModeFilter);
}

void CoreController::applyCatalogFilter(const QString& query)
{
    if (!m_catalogFilters)
        return;
    m_catalogFilters->setActiveQuery(query);
    m_catalogFilters->applyFilter(query);
}

void CoreController::scheduleCatalogRefilter()
{
    if (m_catalogFilters)
        m_catalogFilters->scheduleRefilter();
}

void CoreController::rebuildAvailableCatalogGenres()
{
    if (m_catalogFilters)
        m_catalogFilters->rebuildAvailableGenres();
}

void CoreController::warmActiveCatalogCovers()
{
    if (!m_catalogCovers || !m_catalogController)
        return;
    m_catalogCovers->warmActiveCatalogCovers(m_catalogController->activeCatalogSourceIds(),
                                             m_catalogFilters->activeQuery());
}

void CoreController::rebuildCatalogIdIndex()
{
    m_catalogIdToCacheIndex.clear();
    m_catalogIdToCacheIndex.reserve(m_catalogCache.size());
    for (int i = 0; i < m_catalogCache.size(); ++i)
        m_catalogIdToCacheIndex.insert(m_catalogCache.at(i).id, i);
}

} // namespace arachnel::core
