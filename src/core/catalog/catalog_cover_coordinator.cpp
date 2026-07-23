#include "catalog_cover_coordinator.h"

#include "catalog_model.h"
#include "catalog_types.h"
#include "cover_image_cache.h"
#include "game_metadata_service.h"
#include "settings_store.h"

namespace arachnel::core {

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
                for (const QString& entryId : waiters)
                    applyCoverToEntry(entryId, localUrl);
                for (CatalogEntry& entry : m_entries()) {
                    if (entry.coverUrl == remoteUrl) {
                        entry.coverUrl = localUrl;
                        m_catalog->notifyEntryChanged(entry.id);
                    }
                }
            });
    connect(m_coverCache, &CoverImageCache::failed, this, [this](const QString& remoteUrl) {
        const QSet<QString> waiters = m_coverWaiters.take(remoteUrl);
        for (const QString& entryId : waiters)
            applyCoverToEntry(entryId, {});
    });
}

void CatalogCoverCoordinator::warmCatalogCovers(const QString& sourceId, const QString& query,
                                                const int limit)
{
    const QString needle = query.trimmed().toLower();
    int warmed = 0;
    for (CatalogEntry& entry : m_entries()) {
        if (entry.sourceId != sourceId || (!needle.isEmpty() && !entry.titleLower.contains(needle)))
            continue;
        if (entry.coverUrl.startsWith(QStringLiteral("file:"))) {
            m_catalog->notifyEntryChanged(entry.id);
        } else if (entry.coverUrl.isEmpty()) {
            requestCatalogCover(entry.id);
        } else {
            m_catalog->notifyEntryChanged(entry.id);
            if (isRemoteLibraryCover(entry.coverUrl))
                ensureDiskCover(entry.id, entry.coverUrl);
        }
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

bool CatalogCoverCoordinator::isRemoteLibraryCover(const QString& url)
{
    return url.contains(QStringLiteral("library_capsule"))
        || url.contains(QStringLiteral("library_600x900"));
}

void CatalogCoverCoordinator::applyCoverToEntry(const QString& entryId, const QString& coverUrl)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
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

void CatalogCoverCoordinator::ensureDiskCover(const QString& entryId, const QString& remoteUrl)
{
    if (remoteUrl.isEmpty()) {
        applyCoverToEntry(entryId, {});
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

void CatalogCoverCoordinator::requestCatalogCover(const QString& entryId)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry || entry->coverUrl.startsWith(QStringLiteral("file:")))
        return;

    if (isRemoteLibraryCover(entry->coverUrl)) {
        if (!entry->metadataPending) {
            entry->metadataPending = true;
            m_catalog->notifyEntryChanged(entryId);
        }
        ensureDiskCover(entryId, entry->coverUrl);
        return;
    }

    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (isRemoteLibraryCover(metadata.coverUrl)) {
        if (!entry->metadataPending) {
            entry->metadataPending = true;
            m_catalog->notifyEntryChanged(entryId);
        }
        ensureDiskCover(entryId, metadata.coverUrl);
        return;
    }

    if (!entry->metadataPending) {
        entry->metadataPending = true;
        m_catalog->notifyEntryChanged(entryId);
    }
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

    if (!entry->coverUrl.isEmpty())
        m_coverCache->remove(entry->coverUrl);

    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (!metadata.coverUrl.isEmpty())
        m_coverCache->remove(metadata.coverUrl);

    m_metadataService->clearCachedCover(entry->title);
    entry->coverUrl.clear();
    entry->metadataPending = true;
    m_catalog->notifyEntryChanged(entryId);
    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly,
                                  m_settings->uiLanguage(), entry->steamAppId);
}

} // namespace arachnel::core
