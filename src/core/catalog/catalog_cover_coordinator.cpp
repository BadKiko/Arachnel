#include "catalog_cover_coordinator.h"

#include "catalog_model.h"
#include "catalog_types.h"
#include "cover_image_cache.h"
#include "game_metadata_service.h"
#include "settings_store.h"

namespace arachnel::core {

bool CatalogCoverCoordinator::isHttpUrl(const QString& url)
{
    return url.startsWith(QStringLiteral("http://"))
        || url.startsWith(QStringLiteral("https://"));
}

bool CatalogCoverCoordinator::looksLikeSteamCover(const QString& url)
{
    return url.contains(QStringLiteral("library_capsule"))
        || url.contains(QStringLiteral("library_600x900"))
        || url.contains(QStringLiteral("library_hero"))
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
                EntryCoverState& st = m_state[entryId];
                if (!st.interested && st.phase == Phase::Idle)
                    return;

                if (coverUrl.isEmpty()) {
                    m_failedEntries.insert(entryId);
                    setPhase(entryId, Phase::Failed);
                    applyCoverToEntry(entryId, {}, false);
                    return;
                }
                if (coverUrl.startsWith(QStringLiteral("file:"))) {
                    applyCoverToEntry(entryId, coverUrl, false);
                    setPhase(entryId, Phase::Done);
                    return;
                }
                if (!looksLikeSteamCover(coverUrl) && !isHttpUrl(coverUrl)) {
                    applyCoverToEntry(entryId, {}, false);
                    setPhase(entryId, Phase::Failed);
                    return;
                }
                setPhase(entryId, Phase::DownloadingThumb);
                ensureDiskCover(entryId, coverUrl, st.priority);
            });

    connect(m_coverCache, &CoverImageCache::ready, this,
            [this](const QString& remoteUrl, const QString& localUrl) {
                handleHeroReady(remoteUrl, localUrl);
                handleCacheReady(remoteUrl, localUrl);
            });
    connect(m_coverCache, &CoverImageCache::failed, this, [this](const QString& remoteUrl) {
        handleHeroFailed(remoteUrl);
        handleCacheFailed(remoteUrl);
    });
}

QString CatalogCoverCoordinator::steamThumbUrl(const QString& steamAppId) const
{
    if (steamAppId.isEmpty())
        return {};
    return QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_capsule.jpg")
        .arg(steamAppId);
}

QString CatalogCoverCoordinator::steamHqUrl(const QString& steamAppId) const
{
    if (steamAppId.isEmpty())
        return {};
    return QStringLiteral(
               "https://cdn.akamai.steamstatic.com/steam/apps/%1/library_600x900_2x.jpg")
        .arg(steamAppId);
}

QString CatalogCoverCoordinator::steamHeroUrl(const QString& steamAppId) const
{
    if (steamAppId.isEmpty())
        return {};
    return QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_hero.jpg")
        .arg(steamAppId);
}

QString CatalogCoverCoordinator::steamHeaderUrl(const QString& steamAppId) const
{
    if (steamAppId.isEmpty())
        return {};
    return QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/header.jpg")
        .arg(steamAppId);
}

QString CatalogCoverCoordinator::firstCached(const QStringList& remotes) const
{
    for (const QString& remote : remotes) {
        const QString local = m_coverCache->localUrlFor(remote);
        if (!local.isEmpty())
            return local;
    }
    return {};
}

QString CatalogCoverCoordinator::firstViableRemote(const QStringList& remotes) const
{
    for (const QString& remote : remotes) {
        if (remote.isEmpty() || m_coverCache->hasFailed(remote))
            continue;
        if (!m_coverCache->localUrlFor(remote).isEmpty())
            continue;
        return remote;
    }
    return {};
}

void CatalogCoverCoordinator::setPhase(const QString& entryId, Phase phase)
{
    m_state[entryId].phase = phase;
}

void CatalogCoverCoordinator::applyCoverToEntry(const QString& entryId, const QString& coverUrl,
                                                bool pending)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
        return;

    // Hard rule: model never gets remote CDN URLs.
    if (!coverUrl.isEmpty() && !coverUrl.startsWith(QStringLiteral("file:")))
        return;

    if (entry->coverUrl == coverUrl && entry->metadataPending == pending)
        return;

    entry->coverUrl = coverUrl;
    entry->metadataPending = pending;
    m_catalog->notifyEntryChanged(entryId);
    emit coverApplied(entryId, coverUrl);
}

void CatalogCoverCoordinator::applyCover(const QString& entryId, const QString& coverUrl)
{
    applyCoverToEntry(entryId, coverUrl, false);
    setPhase(entryId, coverUrl.isEmpty() ? Phase::Failed : Phase::Done);
}

void CatalogCoverCoordinator::markPending(CatalogEntry* entry)
{
    if (!entry)
        return;
    bool changed = false;
    if (!entry->metadataPending) {
        entry->metadataPending = true;
        changed = true;
    }
    if (!entry->coverUrl.isEmpty() && !entry->coverUrl.startsWith(QStringLiteral("file:"))) {
        entry->coverUrl.clear();
        changed = true;
    }
    if (!changed)
        return;
    m_catalog->notifyEntryChanged(entry->id);
    emit coverApplied(entry->id, entry->coverUrl);
}

void CatalogCoverCoordinator::dropWaitersForEntry(const QString& entryId)
{
    for (auto it = m_waiters.begin(); it != m_waiters.end();) {
        it.value().remove(entryId);
        if (it.value().isEmpty())
            it = m_waiters.erase(it);
        else
            ++it;
    }
    for (auto it = m_heroWaiters.begin(); it != m_heroWaiters.end();) {
        it.value().remove(entryId);
        if (it.value().isEmpty())
            it = m_heroWaiters.erase(it);
        else
            ++it;
    }
}

void CatalogCoverCoordinator::ensureDiskCover(const QString& entryId, const QString& remoteUrl,
                                              CoverFetchPriority priority)
{
    if (remoteUrl.isEmpty()) {
        applyCoverToEntry(entryId, {}, false);
        setPhase(entryId, Phase::Failed);
        return;
    }

    const QString local = m_coverCache->localUrlFor(remoteUrl);
    if (!local.isEmpty()) {
        applyCoverToEntry(entryId, local, false);
        if (remoteUrl.contains(QStringLiteral("library_600x900"))) {
            setPhase(entryId, Phase::Done);
            m_state[entryId].hqQueued = false;
        } else {
            setPhase(entryId, Phase::ShowingThumb);
            queueHqUpgrade(entryId);
        }
        return;
    }

    if (m_coverCache->hasFailed(remoteUrl)) {
        m_waiters[remoteUrl].insert(entryId);
        handleCacheFailed(remoteUrl);
        return;
    }

    m_state[entryId].activeRemote = remoteUrl;
    m_waiters[remoteUrl].insert(entryId);
    m_coverCache->ensure(remoteUrl, priority);
}

void CatalogCoverCoordinator::queueHqUpgrade(const QString& entryId)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry || entry->steamAppId.isEmpty())
        return;

    EntryCoverState& st = m_state[entryId];
    if (st.hqQueued || st.phase == Phase::Done || st.phase == Phase::Failed)
        return;

    const QString hq = steamHqUrl(entry->steamAppId);
    if (hq.isEmpty() || m_coverCache->hasFailed(hq))
        return;

    if (const QString local = m_coverCache->localUrlFor(hq); !local.isEmpty()) {
        applyCoverToEntry(entryId, local, false);
        setPhase(entryId, Phase::Done);
        return;
    }

    st.hqQueued = true;
    setPhase(entryId, Phase::DownloadingHq);
    m_waiters[hq].insert(entryId);
    m_coverCache->ensure(hq, CoverFetchPriority::Upgrade);
}

void CatalogCoverCoordinator::beginSteamPipeline(CatalogEntry* entry, CoverFetchPriority priority)
{
    const QString appId = entry->steamAppId;
    const QString hq = steamHqUrl(appId);
    const QString thumb = steamThumbUrl(appId);

    if (const QString localHq = m_coverCache->localUrlFor(hq); !localHq.isEmpty()) {
        applyCoverToEntry(entry->id, localHq, false);
        setPhase(entry->id, Phase::Done);
        return;
    }

    // Prefer canonical metadata cover (correct CDN path) when already resolved.
    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (!metadata.coverUrl.isEmpty()) {
        if (const QString localMeta = m_coverCache->localUrlFor(metadata.coverUrl);
            !localMeta.isEmpty()) {
            applyCoverToEntry(entry->id, localMeta, false);
            setPhase(entry->id, Phase::ShowingThumb);
            queueHqUpgrade(entry->id);
            return;
        }
        if (!m_coverCache->hasFailed(metadata.coverUrl)) {
            markPending(entry);
            setPhase(entry->id, Phase::DownloadingThumb);
            ensureDiskCover(entry->id, metadata.coverUrl, priority);
            return;
        }
    }

    if (const QString localThumb = m_coverCache->localUrlFor(thumb); !localThumb.isEmpty()) {
        applyCoverToEntry(entry->id, localThumb, false);
        setPhase(entry->id, Phase::ShowingThumb);
        queueHqUpgrade(entry->id);
        return;
    }

    // One fast guess only — no multi-URL ladder spam.
    if (!thumb.isEmpty() && !m_coverCache->hasFailed(thumb)) {
        markPending(entry);
        setPhase(entry->id, Phase::DownloadingThumb);
        ensureDiskCover(entry->id, thumb, priority);
        // Also resolve canonical asset via metadata for next time / HQ path.
        m_metadataService->queueFetch(entry->id, entry->title, MetadataFetchMode::CoverOnly,
                                      m_settings->uiLanguage(), appId);
        return;
    }

    markPending(entry);
    setPhase(entry->id, Phase::Resolving);
    m_metadataService->queueFetch(entry->id, entry->title, MetadataFetchMode::CoverOnly,
                                  m_settings->uiLanguage(), appId);
}

void CatalogCoverCoordinator::handleCacheReady(const QString& remoteUrl, const QString& localUrl)
{
    const QSet<QString> waiters = m_waiters.take(remoteUrl);
    if (waiters.isEmpty())
        return;

    const bool isHq = remoteUrl.contains(QStringLiteral("library_600x900"));
    for (const QString& entryId : waiters) {
        EntryCoverState& st = m_state[entryId];
        if (!st.interested && st.phase == Phase::Idle)
            continue;

        applyCoverToEntry(entryId, localUrl, false);
        st.activeRemote.clear();
        if (isHq) {
            st.hqQueued = false;
            setPhase(entryId, Phase::Done);
        } else {
            setPhase(entryId, Phase::ShowingThumb);
            queueHqUpgrade(entryId);
        }
        m_failedEntries.remove(entryId);
    }
}

void CatalogCoverCoordinator::handleCacheFailed(const QString& remoteUrl)
{
    const QSet<QString> waiters = m_waiters.take(remoteUrl);
    if (waiters.isEmpty())
        return;

    const bool isHq = remoteUrl.contains(QStringLiteral("library_600x900"));
    for (const QString& entryId : waiters) {
        CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
        EntryCoverState& st = m_state[entryId];
        st.activeRemote.clear();

        if (isHq) {
            // Keep whatever thumb we already showed.
            st.hqQueued = false;
            if (entry && entry->coverUrl.startsWith(QStringLiteral("file:"))) {
                setPhase(entryId, Phase::ShowingThumb);
                if (entry->metadataPending) {
                    entry->metadataPending = false;
                    m_catalog->notifyEntryChanged(entryId);
                }
            } else {
                setPhase(entryId, Phase::Failed);
                applyCoverToEntry(entryId, {}, false);
            }
            continue;
        }

        // Thumb failed — fall back to metadata resolve, then optional HQ.
        if (entry && !entry->steamAppId.isEmpty()) {
            const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
            if (!metadata.coverUrl.isEmpty() && metadata.coverUrl != remoteUrl
                && !m_coverCache->hasFailed(metadata.coverUrl)) {
                setPhase(entryId, Phase::DownloadingThumb);
                ensureDiskCover(entryId, metadata.coverUrl, st.priority);
                continue;
            }

            const QString hq = steamHqUrl(entry->steamAppId);
            if (!hq.isEmpty() && hq != remoteUrl && !m_coverCache->hasFailed(hq)) {
                setPhase(entryId, Phase::DownloadingHq);
                ensureDiskCover(entryId, hq, st.priority);
                continue;
            }

            // Ask metadata service if we haven't resolved assets yet.
            setPhase(entryId, Phase::Resolving);
            markPending(entry);
            m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly,
                                          m_settings->uiLanguage(), entry->steamAppId);
            continue;
        }

        m_failedEntries.insert(entryId);
        setPhase(entryId, Phase::Failed);
        applyCoverToEntry(entryId, {}, false);
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
        requestCatalogCover(entry.id, CoverFetchPriority::Warm);
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

void CatalogCoverCoordinator::requestCatalogCover(const QString& entryId,
                                                  CoverFetchPriority priority)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
        return;

    if (m_failedEntries.contains(entryId)
        && !entry->coverUrl.startsWith(QStringLiteral("file:")))
        return;

    EntryCoverState& st = m_state[entryId];
    st.interested = true;
    if (static_cast<int>(priority) > static_cast<int>(st.priority))
        st.priority = priority;

    // Strip plugin / remote leftovers, and stale file: paths whose cache file is gone.
    if (!entry->coverUrl.isEmpty()) {
        const bool okLocal = entry->coverUrl.startsWith(QStringLiteral("file:"))
            && !m_coverCache->localUrlFor(entry->coverUrl).isEmpty();
        if (!okLocal) {
            entry->coverUrl.clear();
            m_catalog->notifyEntryChanged(entryId);
        }
    }

    if (entry->coverUrl.startsWith(QStringLiteral("file:"))) {
        // Already showing something — still try HQ upgrade if possible.
        setPhase(entryId, Phase::ShowingThumb);
        if (!entry->steamAppId.isEmpty())
            queueHqUpgrade(entryId);
        else
            setPhase(entryId, Phase::Done);
        return;
    }

    if (st.phase == Phase::DownloadingThumb || st.phase == Phase::DownloadingHq
        || st.phase == Phase::Resolving)
        return;

    if (!entry->steamAppId.isEmpty()) {
        beginSteamPipeline(entry, st.priority);
        return;
    }

    // No steam id — resolve via metadata title search.
    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (!metadata.coverUrl.isEmpty()) {
        if (const QString local = m_coverCache->localUrlFor(metadata.coverUrl); !local.isEmpty()) {
            applyCoverToEntry(entryId, local, false);
            setPhase(entryId, Phase::Done);
            return;
        }
        if (!m_coverCache->hasFailed(metadata.coverUrl)) {
            markPending(entry);
            setPhase(entryId, Phase::DownloadingThumb);
            ensureDiskCover(entryId, metadata.coverUrl, st.priority);
            return;
        }
    }

    markPending(entry);
    setPhase(entryId, Phase::Resolving);
    m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly,
                                  m_settings->uiLanguage(), entry->steamAppId);
}

void CatalogCoverCoordinator::cancelCatalogCover(const QString& entryId)
{
    EntryCoverState& st = m_state[entryId];
    st.interested = false;
    dropWaitersForEntry(entryId);
    m_metadataService->cancelPending(entryId);

    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry || !entry->metadataPending)
        return;
    // Keep pending if we already have a local cover (upgrade in background is fine).
    if (entry->coverUrl.startsWith(QStringLiteral("file:"))) {
        entry->metadataPending = false;
        m_catalog->notifyEntryChanged(entryId);
        return;
    }
    entry->metadataPending = false;
    m_catalog->notifyEntryChanged(entryId);
}

void CatalogCoverCoordinator::invalidateCatalogCover(const QString& entryId)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
        return;

    m_failedEntries.remove(entryId);
    dropWaitersForEntry(entryId);
    m_state.remove(entryId);
    m_heroLocal.remove(entryId);

    if (!entry->coverUrl.isEmpty())
        m_coverCache->remove(entry->coverUrl);

    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (!metadata.coverUrl.isEmpty())
        m_coverCache->remove(metadata.coverUrl);
    if (!entry->steamAppId.isEmpty()) {
        m_coverCache->clearFailed(steamThumbUrl(entry->steamAppId));
        m_coverCache->clearFailed(steamHqUrl(entry->steamAppId));
        m_coverCache->remove(steamThumbUrl(entry->steamAppId));
        m_coverCache->remove(steamHqUrl(entry->steamAppId));
        m_coverCache->clearFailed(steamHeroUrl(entry->steamAppId));
        m_coverCache->clearFailed(steamHeaderUrl(entry->steamAppId));
    }

    m_metadataService->clearCachedCover(entry->title);
    entry->coverUrl.clear();
    entry->metadataPending = true;
    m_catalog->notifyEntryChanged(entryId);

    requestCatalogCover(entryId, CoverFetchPriority::Visible);
}

QString CatalogCoverCoordinator::heroCoverUrl(const QString& entryId) const
{
    return m_heroLocal.value(entryId);
}

void CatalogCoverCoordinator::queueHeroDownload(const QString& entryId, const QString& remoteUrl)
{
    if (remoteUrl.isEmpty())
        return;
    if (const QString local = m_coverCache->localUrlFor(remoteUrl); !local.isEmpty()) {
        m_heroLocal.insert(entryId, local);
        emit heroCoverApplied(entryId, local);
        return;
    }
    if (m_coverCache->hasFailed(remoteUrl))
        return;
    m_heroWaiters[remoteUrl].insert(entryId);
    m_coverCache->ensure(remoteUrl, CoverFetchPriority::Visible);
}

void CatalogCoverCoordinator::requestCatalogHeroCover(const QString& entryId)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry || entry->steamAppId.isEmpty())
        return;

    if (m_heroLocal.contains(entryId)) {
        emit heroCoverApplied(entryId, m_heroLocal.value(entryId));
        return;
    }

    const QString hero = steamHeroUrl(entry->steamAppId);
    const QString header = steamHeaderUrl(entry->steamAppId);
    if (const QString local = firstCached({hero, header}); !local.isEmpty()) {
        m_heroLocal.insert(entryId, local);
        emit heroCoverApplied(entryId, local);
        return;
    }

    const QString next = firstViableRemote({hero, header});
    if (!next.isEmpty())
        queueHeroDownload(entryId, next);
}

void CatalogCoverCoordinator::handleHeroReady(const QString& remoteUrl, const QString& localUrl)
{
    const QSet<QString> waiters = m_heroWaiters.take(remoteUrl);
    for (const QString& entryId : waiters) {
        m_heroLocal.insert(entryId, localUrl);
        emit heroCoverApplied(entryId, localUrl);
    }
}

void CatalogCoverCoordinator::handleHeroFailed(const QString& remoteUrl)
{
    const QSet<QString> waiters = m_heroWaiters.take(remoteUrl);
    for (const QString& entryId : waiters) {
        CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
        if (!entry || entry->steamAppId.isEmpty())
            continue;
        const QString hero = steamHeroUrl(entry->steamAppId);
        const QString header = steamHeaderUrl(entry->steamAppId);
        if (remoteUrl == hero && !m_coverCache->hasFailed(header)) {
            queueHeroDownload(entryId, header);
            continue;
        }
        // Fall back to card cover if we have one.
        if (entry->coverUrl.startsWith(QStringLiteral("file:"))) {
            m_heroLocal.insert(entryId, entry->coverUrl);
            emit heroCoverApplied(entryId, entry->coverUrl);
        }
    }
}

} // namespace arachnel::core
