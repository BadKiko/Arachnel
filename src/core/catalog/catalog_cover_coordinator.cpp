#include "catalog_cover_coordinator.h"

#include "catalog_model.h"
#include "catalog_types.h"
#include "cover_image_cache.h"
#include "game_metadata_service.h"
#include "settings_store.h"

namespace arachnel::core {

bool CatalogCoverCoordinator::isAllowedSteamCoverUrl(const QString& url)
{
    if (url.startsWith(QStringLiteral("file:")))
        return true;
    return url.contains(QStringLiteral("library_capsule"))
        || url.contains(QStringLiteral("library_600x900"))
        || url.contains(QStringLiteral("/header."));
}

bool CatalogCoverCoordinator::isRemoteLibraryCover(const QString& url)
{
    return url.contains(QStringLiteral("library_capsule"))
        || url.contains(QStringLiteral("library_600x900"))
        || url.contains(QStringLiteral("/header."));
}

CatalogCoverCoordinator::CatalogCoverCoordinator(CoverImageCache* coverCache,
                                                 GameMetadataService* metadataService,
                                                 SettingsStore* settings, CatalogModel* catalog,
                                                 EntryLookup findEntry, EntryList entries,
                                                 QObject* parent)
    : QObject(parent)
    , m_coverCache(coverCache)
    , m_metadataService(metadataService)
    , m_settings(settings)
    , m_catalog(catalog)
    , m_findEntry(std::move(findEntry))
    , m_entries(std::move(entries))
{
    connect(m_metadataService, &GameMetadataService::coverReady, this,
            [this](const QString& entryId, const QString& coverUrl) {
                if (coverUrl.isEmpty() || coverUrl.startsWith(QStringLiteral("file:"))) {
                    applyCoverToEntry(entryId, coverUrl);
                    return;
                }
                ensureDiskCover(entryId, coverUrl);
            });
    connect(m_coverCache, &CoverImageCache::ready, this,
            [this](const QString& remoteUrl, const QString& localUrl) {
                const QSet<QString> waiters = m_coverWaiters.take(remoteUrl);
                for (const QString& entryId : waiters) {
                    m_coverAttempt.remove(entryId);
                    m_coverGiveUp.remove(entryId);
                    applyCoverToEntry(entryId, localUrl);
                }
            });
    connect(m_coverCache, &CoverImageCache::failed, this,
            [this](const QString& remoteUrl) { handleCoverDownloadFailed(remoteUrl); });
}

QStringList CatalogCoverCoordinator::steamCoverCandidates(const QString& steamAppId) const
{
    if (steamAppId.isEmpty())
        return {};
    const QString base =
        QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/").arg(steamAppId);
    // Portrait first; header is landscape fallback for apps without library art.
    return {
        base + QStringLiteral("library_600x900_2x.jpg"),
        base + QStringLiteral("library_600x900.jpg"),
        base + QStringLiteral("library_capsule_2x.jpg"),
        base + QStringLiteral("library_capsule.jpg"),
        base + QStringLiteral("header.jpg"),
    };
}

QString CatalogCoverCoordinator::nextSteamCoverFallback(const QString& entryId,
                                                        const QString& failedUrl) const
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry || entry->steamAppId.isEmpty())
        return {};

    const QStringList candidates = steamCoverCandidates(entry->steamAppId);
    const int idx = candidates.indexOf(failedUrl);
    if (idx < 0 || idx + 1 >= candidates.size())
        return {};
    return candidates.at(idx + 1);
}

void CatalogCoverCoordinator::handleCoverDownloadFailed(const QString& remoteUrl)
{
    const QSet<QString> waiters = m_coverWaiters.take(remoteUrl);
    for (const QString& entryId : waiters) {
        const QString next = nextSteamCoverFallback(entryId, remoteUrl);
        if (!next.isEmpty()) {
            ensureDiskCover(entryId, next);
            continue;
        }
        m_coverAttempt.remove(entryId);
        m_coverGiveUp.insert(entryId);
        applyCoverToEntry(entryId, {});
    }
}

void CatalogCoverCoordinator::warmCatalogCovers(const QString& sourceId, const QString& query,
                                                const int limit)
{
    const QString needle = query.trimmed().toLower();
    int warmed = 0;
    for (CatalogEntry& entry : m_entries()) {
        if (entry.sourceId != sourceId || (!needle.isEmpty() && !entry.titleLower.contains(needle)))
            continue;
        requestCatalogCover(entry.id);
        if (++warmed >= limit)
            break;
    }
}

void CatalogCoverCoordinator::warmActiveCatalogCovers(const QStringList& sourceIds,
                                                      const QString& query, const int limit)
{
    if (sourceIds.isEmpty())
        return;
    const int perSourceLimit = qMax(1, limit / sourceIds.size());
    for (const QString& sourceId : sourceIds)
        warmCatalogCovers(sourceId, query, perSourceLimit);
}

void CatalogCoverCoordinator::applyCoverToEntry(const QString& entryId, const QString& coverUrl)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
        return;

    // Never expose remote CDN URLs to QML — Image would hammer 404s.
    if (!coverUrl.isEmpty() && !coverUrl.startsWith(QStringLiteral("file:")))
        return;

    entry->coverUrl = coverUrl;
    entry->metadataPending = false;
    m_catalog->notifyEntryChanged(entryId);
    emit coverApplied(entryId, coverUrl);
}

void CatalogCoverCoordinator::applyCover(const QString& entryId, const QString& coverUrl)
{
    applyCoverToEntry(entryId, coverUrl);
}

void CatalogCoverCoordinator::markCoverPending(CatalogEntry* entry)
{
    if (!entry || entry->metadataPending)
        return;
    entry->metadataPending = true;
    // Clear non-local covers so QML does not load https.
    if (!entry->coverUrl.startsWith(QStringLiteral("file:")))
        entry->coverUrl.clear();
    m_catalog->notifyEntryChanged(entry->id);
    emit coverApplied(entry->id, entry->coverUrl);
}

void CatalogCoverCoordinator::ensureDiskCover(const QString& entryId, const QString& remoteUrl)
{
    if (remoteUrl.isEmpty()) {
        applyCoverToEntry(entryId, {});
        return;
    }

    const QString local = m_coverCache->localUrlFor(remoteUrl);
    if (!local.isEmpty()) {
        m_coverAttempt.remove(entryId);
        m_coverGiveUp.remove(entryId);
        applyCoverToEntry(entryId, local);
        return;
    }

    m_coverAttempt.insert(entryId, remoteUrl);
    m_coverWaiters[remoteUrl].insert(entryId);
    m_coverCache->ensure(remoteUrl);
}

void CatalogCoverCoordinator::requestCatalogCover(const QString& entryId)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
        return;

    if (m_coverGiveUp.contains(entryId))
        return;

    // Already have a local cover.
    if (entry->coverUrl.startsWith(QStringLiteral("file:")))
        return;

    // Hard policy: plugin covers are disabled; keep only Steam/library covers.
    if (!entry->coverUrl.isEmpty() && !isAllowedSteamCoverUrl(entry->coverUrl)) {
        entry->coverUrl.clear();
        m_catalog->notifyEntryChanged(entryId);
    }

    // Prefer Steam library capsule whenever we know the app id.
    if (!entry->steamAppId.isEmpty()) {
        const QStringList candidates = steamCoverCandidates(entry->steamAppId);
        for (const QString& candidate : candidates) {
            const QString local = m_coverCache->localUrlFor(candidate);
            if (!local.isEmpty()) {
                applyCoverToEntry(entryId, local);
                return;
            }
        }
        markCoverPending(entry);
        ensureDiskCover(entryId, candidates.first());
        return;
    }

    if (isRemoteLibraryCover(entry->coverUrl)) {
        const QString remote = entry->coverUrl;
        markCoverPending(entry);
        ensureDiskCover(entryId, remote);
        return;
    }

    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (isRemoteLibraryCover(metadata.coverUrl)) {
        markCoverPending(entry);
        ensureDiskCover(entryId, metadata.coverUrl);
        return;
    }

    markCoverPending(entry);
    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly,
                                  m_settings->uiLanguage(), entry->steamAppId);
}

void CatalogCoverCoordinator::cancelCatalogCover(const QString& entryId)
{
    if (!m_metadataService->cancelPending(entryId))
        return;

    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry || !entry->metadataPending)
        return;

    entry->metadataPending = false;
    m_catalog->notifyEntryChanged(entryId);
}

void CatalogCoverCoordinator::invalidateCatalogCover(const QString& entryId)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
        return;

    m_coverGiveUp.remove(entryId);
    m_coverAttempt.remove(entryId);

    if (!entry->coverUrl.isEmpty())
        m_coverCache->remove(entry->coverUrl);

    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (!metadata.coverUrl.isEmpty())
        m_coverCache->remove(metadata.coverUrl);

    m_metadataService->clearCachedCover(entry->title);
    entry->coverUrl.clear();
    entry->metadataPending = true;
    m_catalog->notifyEntryChanged(entryId);

    // Prefer Steam CDN fallbacks over metadata re-scrape when we know the app id.
    if (!entry->steamAppId.isEmpty()) {
        const QStringList candidates = steamCoverCandidates(entry->steamAppId);
        if (!candidates.isEmpty()) {
            ensureDiskCover(entryId, candidates.first());
            return;
        }
    }

    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly,
                                  m_settings->uiLanguage(), entry->steamAppId);
}

} // namespace arachnel::core
