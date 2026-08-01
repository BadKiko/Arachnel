#include "catalog_cover_coordinator.h"

#include "catalog_model.h"
#include "catalog_types.h"
#include "cover_image_cache.h"
#include "game_metadata_service.h"
#include "settings_store.h"

#include <algorithm>

#include <QDateTime>
#include <QVariantMap>

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
    m_recentApplyMs.resize(kApplyLatencyWindow);
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
    if (!coverUrl.isEmpty())
        noteApplyLatency(entryId);
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

bool CatalogCoverCoordinator::entryHasWaiter(const QString& entryId) const
{
    for (auto it = m_waiters.constBegin(); it != m_waiters.constEnd(); ++it) {
        if (it.value().contains(entryId))
            return true;
    }
    return false;
}

QSet<QString> CatalogCoverCoordinator::takeWaitersForRemote(const QString& remoteUrl)
{
    QSet<QString> waiters = m_waiters.take(remoteUrl);
    for (auto it = m_state.begin(); it != m_state.end(); ++it) {
        if (it->activeRemote != remoteUrl)
            continue;
        if (!waiters.contains(it.key())) {
            waiters.insert(it.key());
            ++m_orphanReclaims;
        }
    }
    return waiters;
}

void CatalogCoverCoordinator::noteApplySample(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    m_applyLatencySumMs += ms;
    if (ms > m_applyLatencyMaxMs)
        m_applyLatencyMaxMs = ms;
    m_recentApplyMs[m_recentApplyWrite % kApplyLatencyWindow] = static_cast<int>(ms);
    ++m_recentApplyWrite;
    refreshApplyPercentiles();
}

void CatalogCoverCoordinator::noteApplyLatency(const QString& entryId)
{
    ++m_applied;
    const qint64 started = m_requestAtMs.take(entryId);
    if (started <= 0)
        return;
    noteApplySample(QDateTime::currentMSecsSinceEpoch() - started);
}

void CatalogCoverCoordinator::refreshApplyPercentiles()
{
    const int n = qMin(m_recentApplyWrite, kApplyLatencyWindow);
    if (n <= 0) {
        m_applyP50Ms = 0;
        m_applyP95Ms = 0;
        return;
    }
    QVector<int> sorted;
    sorted.reserve(n);
    const int start = m_recentApplyWrite >= kApplyLatencyWindow
        ? (m_recentApplyWrite % kApplyLatencyWindow)
        : 0;
    for (int i = 0; i < n; ++i) {
        const int idx =
            m_recentApplyWrite >= kApplyLatencyWindow ? (start + i) % kApplyLatencyWindow : i;
        sorted.append(m_recentApplyMs.at(idx));
    }
    std::sort(sorted.begin(), sorted.end());
    m_applyP50Ms = sorted.at((n - 1) / 2);
    m_applyP95Ms = sorted.at(qMin(n - 1, (n * 95) / 100));
}

bool CatalogCoverCoordinator::tryResumeKnownCover(CatalogEntry* entry, CoverFetchPriority priority)
{
    if (!entry)
        return false;

    EntryCoverState& st = m_state[entry->id];

    if (const QString localSteam = firstCachedSteamCover(entry->steamAppId); !localSteam.isEmpty()) {
        applyCoverToEntry(entry->id, localSteam, false);
        setPhase(entry->id, Phase::ShowingThumb);
        queueHqUpgrade(entry->id);
        return true;
    }

    if (!st.catalogRemote.isEmpty()) {
        if (const QString local = m_coverCache->localUrlFor(st.catalogRemote); !local.isEmpty()) {
            applyCoverToEntry(entry->id, local, false);
            if (st.catalogRemote.contains(QStringLiteral("library_600x900"))) {
                setPhase(entry->id, Phase::Done);
            } else {
                setPhase(entry->id, Phase::ShowingThumb);
                queueHqUpgrade(entry->id);
            }
            return true;
        }

        if (st.catalogRemote.contains(QStringLiteral("library_600x900"))
            && !entry->steamAppId.isEmpty()) {
            const QString thumb = steamThumbUrl(entry->steamAppId);
            if (!thumb.isEmpty() && !m_coverCache->hasFailed(thumb)) {
                if (const QString localThumb = m_coverCache->localUrlFor(thumb);
                    !localThumb.isEmpty()) {
                    applyCoverToEntry(entry->id, localThumb, false);
                    setPhase(entry->id, Phase::ShowingThumb);
                    queueHqUpgrade(entry->id);
                    return true;
                }
                markPending(entry);
                setPhase(entry->id, Phase::DownloadingThumb);
                ensureDiskCover(entry->id, thumb, priority);
                return true;
            }
        }

        if (!m_coverCache->hasFailed(st.catalogRemote)) {
            markPending(entry);
            setPhase(entry->id, Phase::DownloadingThumb);
            ensureDiskCover(entry->id, st.catalogRemote, priority);
            return true;
        }
    }

    return false;
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
    if (!entry)
        return;

    EntryCoverState& st = m_state[entryId];
    if (st.hqQueued || st.phase == Phase::Done || st.phase == Phase::Failed)
        return;

    // Catalog/relay HQ after we showed a fast capsule thumb first.
    if (!st.catalogRemote.isEmpty() && st.catalogRemoteTried
        && st.catalogRemote.contains(QStringLiteral("library_600x900"))) {
        if (const QString local = m_coverCache->localUrlFor(st.catalogRemote); !local.isEmpty()) {
            applyCoverToEntry(entryId, local, false);
            setPhase(entryId, Phase::Done);
            return;
        }
        if (!m_coverCache->hasFailed(st.catalogRemote)) {
            st.hqQueued = true;
            setPhase(entryId, Phase::DownloadingHq);
            m_waiters[st.catalogRemote].insert(entryId);
            m_coverCache->ensure(st.catalogRemote, CoverFetchPriority::Upgrade);
            return;
        }
    }

    if (entry->steamAppId.isEmpty()) {
        setPhase(entryId, Phase::Done);
        return;
    }

    const QString hq = steamHqUrl(entry->steamAppId);
    if (hq.isEmpty() || m_coverCache->hasFailed(hq)) {
        setPhase(entryId, Phase::Done);
        return;
    }
    if (!st.catalogRemote.isEmpty() && hq == st.catalogRemote) {
        setPhase(entryId, Phase::Done);
        return;
    }

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

QString CatalogCoverCoordinator::resolveCatalogRemote(const CatalogEntry& entry) const
{
    if (isHttpUrl(entry.remoteCoverUrl))
        return entry.remoteCoverUrl;

    if (entry.steamAppId.isEmpty())
        return {};
    return m_remoteBySteamAppId.value(entry.steamAppId);
}

QString CatalogCoverCoordinator::firstCachedSteamCover(const QString& steamAppId) const
{
    if (steamAppId.isEmpty())
        return {};
    return firstCached({steamHqUrl(steamAppId), steamThumbUrl(steamAppId),
                        steamHeaderUrl(steamAppId)});
}

void CatalogCoverCoordinator::rebuildRemoteCoverIndex()
{
    m_remoteBySteamAppId.clear();
    if (!m_entries)
        return;
    for (const CatalogEntry& entry : m_entries()) {
        if (entry.steamAppId.isEmpty() || !isHttpUrl(entry.remoteCoverUrl))
            continue;
        // Prefer vertical library art over header.jpg when multiple sources collide.
        const auto it = m_remoteBySteamAppId.constFind(entry.steamAppId);
        if (it == m_remoteBySteamAppId.cend()) {
            m_remoteBySteamAppId.insert(entry.steamAppId, entry.remoteCoverUrl);
            continue;
        }
        const bool incomingBetter =
            entry.remoteCoverUrl.contains(QStringLiteral("library_600x900"))
            || entry.remoteCoverUrl.contains(QStringLiteral("library_capsule"));
        const bool existingHeader = it.value().contains(QStringLiteral("/header."));
        if (incomingBetter && existingHeader)
            m_remoteBySteamAppId.insert(entry.steamAppId, entry.remoteCoverUrl);
    }
}

void CatalogCoverCoordinator::clearFailedCoverHints()
{
    m_failedEntries.clear();
    if (!m_coverCache)
        return;
    m_coverCache->clearAllFailed();
}

bool CatalogCoverCoordinator::beginCatalogRemote(CatalogEntry* entry, CoverFetchPriority priority)
{
    if (!entry)
        return false;

    EntryCoverState& st = m_state[entry->id];
    if (st.catalogRemoteTried)
        return false;

    const QString remote = resolveCatalogRemote(*entry);
    st.catalogRemoteTried = true;
    st.catalogRemote = remote;

    // Reuse previously downloaded Steam CDN files for this app even if the relay URL is new.
    if (const QString localSteam = firstCachedSteamCover(entry->steamAppId); !localSteam.isEmpty()) {
        applyCoverToEntry(entry->id, localSteam, false);
        setPhase(entry->id, Phase::ShowingThumb);
        if (!entry->steamAppId.isEmpty())
            queueHqUpgrade(entry->id);
        else
            setPhase(entry->id, Phase::Done);
        // Still try the relay URL in the background if present and not yet cached.
        if (!remote.isEmpty() && m_coverCache->localUrlFor(remote).isEmpty()
            && !m_coverCache->hasFailed(remote)) {
            m_coverCache->ensure(remote, CoverFetchPriority::Upgrade);
            m_waiters[remote].insert(entry->id);
        }
        return true;
    }

    if (remote.isEmpty())
        return false;

    // Persist cross-source relay hit onto this entry for later retries / library.
    if (entry->remoteCoverUrl.isEmpty())
        entry->remoteCoverUrl = remote;

    if (const QString local = m_coverCache->localUrlFor(remote); !local.isEmpty()) {
        applyCoverToEntry(entry->id, local, false);
        if (remote.contains(QStringLiteral("library_600x900"))
            || remote.contains(QStringLiteral("library_capsule"))) {
            setPhase(entry->id, Phase::Done);
        } else {
            setPhase(entry->id, Phase::ShowingThumb);
            queueHqUpgrade(entry->id);
        }
        return true;
    }

    if (m_coverCache->hasFailed(remote))
        return false;

    // Fast first paint: capsule is much smaller than library_600x900.
    // Visible/Warm get the thumb now; HQ upgrades in the background.
    if (remote.contains(QStringLiteral("library_600x900")) && !entry->steamAppId.isEmpty()) {
        const QString thumb = steamThumbUrl(entry->steamAppId);
        if (!thumb.isEmpty() && !m_coverCache->hasFailed(thumb)) {
            if (const QString localThumb = m_coverCache->localUrlFor(thumb); !localThumb.isEmpty()) {
                applyCoverToEntry(entry->id, localThumb, false);
                setPhase(entry->id, Phase::ShowingThumb);
                queueHqUpgrade(entry->id);
                return true;
            }
            markPending(entry);
            setPhase(entry->id, Phase::DownloadingThumb);
            ensureDiskCover(entry->id, thumb, priority);
            // Start catalog HQ in parallel - many apps 404 on library_capsule.
            if (!m_coverCache->hasFailed(remote)
                && m_coverCache->localUrlFor(remote).isEmpty()) {
                m_waiters[remote].insert(entry->id);
                m_coverCache->ensure(remote, priority == CoverFetchPriority::Visible
                                                 ? CoverFetchPriority::Warm
                                                 : CoverFetchPriority::Upgrade);
            }
            return true;
        }
    }

    markPending(entry);
    setPhase(entry->id, Phase::DownloadingThumb);
    ensureDiskCover(entry->id, remote, priority);
    return true;
}

void CatalogCoverCoordinator::continueAfterCatalogMiss(CatalogEntry* entry,
                                                       CoverFetchPriority priority)
{
    if (!entry)
        return;
    if (!entry->steamAppId.isEmpty()) {
        beginSteamPipeline(entry, priority);
        return;
    }
    beginMetadataPipeline(entry, priority);
}

void CatalogCoverCoordinator::beginMetadataPipeline(CatalogEntry* entry,
                                                    CoverFetchPriority priority)
{
    if (!entry)
        return;

    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    if (!metadata.coverUrl.isEmpty()) {
        if (const QString local = m_coverCache->localUrlFor(metadata.coverUrl); !local.isEmpty()) {
            applyCoverToEntry(entry->id, local, false);
            setPhase(entry->id, Phase::Done);
            return;
        }
        if (!m_coverCache->hasFailed(metadata.coverUrl)) {
            markPending(entry);
            setPhase(entry->id, Phase::DownloadingThumb);
            ensureDiskCover(entry->id, metadata.coverUrl, priority);
            return;
        }
    }

    markPending(entry);
    setPhase(entry->id, Phase::Resolving);
    m_metadataService->queueFetch(entry->id, entry->title, MetadataFetchMode::CoverOnly,
                                  m_settings->uiLanguage(), entry->steamAppId);
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
    const QSet<QString> waiters = takeWaitersForRemote(remoteUrl);
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
    const QSet<QString> waiters = takeWaitersForRemote(remoteUrl);
    if (waiters.isEmpty())
        return;

    const bool isHq = remoteUrl.contains(QStringLiteral("library_600x900"));
    for (const QString& entryId : waiters) {
        CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
        EntryCoverState& st = m_state[entryId];
        st.activeRemote.clear();

        if (isHq && remoteUrl != st.catalogRemote) {
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

        // Catalog/relay cover missed - fall back to Steam / metadata.
        if (entry && !st.catalogRemote.isEmpty() && remoteUrl == st.catalogRemote) {
            continueAfterCatalogMiss(entry, st.priority);
            continue;
        }

        // Capsule / non-catalog URL missed - prefer known catalog HQ before metadata queue.
        if (entry && !st.catalogRemote.isEmpty() && remoteUrl != st.catalogRemote
            && !m_coverCache->hasFailed(st.catalogRemote)) {
            setPhase(entryId, Phase::DownloadingThumb);
            ensureDiskCover(entryId, st.catalogRemote, st.priority);
            continue;
        }

        // Thumb failed - fall back to metadata resolve, then optional HQ.
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

    ++m_requests;
    if (!m_requestAtMs.contains(entryId))
        m_requestAtMs.insert(entryId, QDateTime::currentMSecsSinceEpoch());

    // Visible cards must be able to retry after a previous miss (hover / recycle).
    if (priority == CoverFetchPriority::Visible)
        m_failedEntries.remove(entryId);

    if (m_failedEntries.contains(entryId)
        && !entry->coverUrl.startsWith(QStringLiteral("file:")))
        return;

    EntryCoverState& st = m_state[entryId];
    st.interested = true;
    if (static_cast<int>(priority) > static_cast<int>(st.priority))
        st.priority = priority;

    // Strip plugin / remote leftovers from the display field, and stale file: paths.
    if (!entry->coverUrl.isEmpty()) {
        const bool okLocal = entry->coverUrl.startsWith(QStringLiteral("file:"))
            && !m_coverCache->localUrlFor(entry->coverUrl).isEmpty();
        if (!okLocal) {
            // Migrate leftover https on coverUrl into remoteCoverUrl once.
            if (isHttpUrl(entry->coverUrl) && entry->remoteCoverUrl.isEmpty())
                entry->remoteCoverUrl = entry->coverUrl;
            entry->coverUrl.clear();
            m_catalog->notifyEntryChanged(entryId);
        }
    }

    if (entry->coverUrl.startsWith(QStringLiteral("file:"))) {
        // Already showing something - still try HQ upgrade if possible.
        setPhase(entryId, Phase::ShowingThumb);
        if (!entry->steamAppId.isEmpty())
            queueHqUpgrade(entryId);
        else
            setPhase(entryId, Phase::Done);
        return;
    }

    // Stuck after recycle: phase still Downloading* but waiter was dropped.
    if (st.phase == Phase::DownloadingThumb || st.phase == Phase::DownloadingHq
        || st.phase == Phase::Resolving) {
        if (entryHasWaiter(entryId))
            return;
        ++m_stuckRecoveries;
        st.phase = Phase::Idle;
        st.hqQueued = false;
        st.activeRemote.clear();
    }

    if (st.catalogRemoteTried) {
        if (tryResumeKnownCover(entry, st.priority))
            return;
        continueAfterCatalogMiss(entry, st.priority);
        return;
    }

    // 1) Catalog / relay cover (including cross-source by steamAppId).
    if (beginCatalogRemote(entry, st.priority))
        return;

    // 2) Steam CDN / metadata fallback.
    continueAfterCatalogMiss(entry, st.priority);
}

void CatalogCoverCoordinator::cancelCatalogCover(const QString& entryId)
{
    ++m_cancels;
    EntryCoverState& st = m_state[entryId];
    const bool abandoned = st.phase == Phase::DownloadingThumb
        || st.phase == Phase::DownloadingHq || st.phase == Phase::Resolving;
    st.interested = false;
    dropWaitersForEntry(entryId);
    m_metadataService->cancelPending(entryId);
    if (abandoned) {
        ++m_abandoned;
        // Allow a later request to restart; keep catalogRemote for fast resume.
        st.phase = Phase::Idle;
        st.hqQueued = false;
        st.activeRemote.clear();
    }

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
    if (!entry->remoteCoverUrl.isEmpty()) {
        m_coverCache->remove(entry->remoteCoverUrl);
        m_coverCache->clearFailed(entry->remoteCoverUrl);
    }

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

CoverCoordinatorStats CatalogCoverCoordinator::stats() const
{
    CoverCoordinatorStats s;
    s.requests = m_requests;
    s.cancels = m_cancels;
    s.applied = m_applied;
    s.stuckRecoveries = m_stuckRecoveries;
    s.abandoned = m_abandoned;
    s.orphanReclaims = m_orphanReclaims;
    s.applyLatencySumMs = m_applyLatencySumMs;
    s.applyLatencyMaxMs = m_applyLatencyMaxMs;
    s.applyP50Ms = m_applyP50Ms;
    s.applyP95Ms = m_applyP95Ms;
    for (auto it = m_state.constBegin(); it != m_state.constEnd(); ++it) {
        if (it->interested)
            ++s.interested;
        if (it->phase == Phase::DownloadingThumb || it->phase == Phase::DownloadingHq
            || it->phase == Phase::Resolving)
            ++s.downloading;
    }
    return s;
}

QVariantMap CatalogCoverCoordinator::statsMap() const
{
    const CoverCoordinatorStats s = stats();
    return QVariantMap{
        {QStringLiteral("requests"), s.requests},
        {QStringLiteral("cancels"), s.cancels},
        {QStringLiteral("applied"), s.applied},
        {QStringLiteral("stuckRecoveries"), s.stuckRecoveries},
        {QStringLiteral("abandoned"), s.abandoned},
        {QStringLiteral("orphanReclaims"), s.orphanReclaims},
        {QStringLiteral("applyAvgMs"),
         s.applied > 0 ? static_cast<qint64>(s.applyLatencySumMs / s.applied) : 0},
        {QStringLiteral("applyMaxMs"), s.applyLatencyMaxMs},
        {QStringLiteral("applyP50Ms"), s.applyP50Ms},
        {QStringLiteral("applyP95Ms"), s.applyP95Ms},
        {QStringLiteral("interested"), s.interested},
        {QStringLiteral("downloading"), s.downloading},
    };
}

void CatalogCoverCoordinator::resetStats()
{
    m_requests = 0;
    m_cancels = 0;
    m_applied = 0;
    m_stuckRecoveries = 0;
    m_abandoned = 0;
    m_orphanReclaims = 0;
    m_applyLatencySumMs = 0;
    m_applyLatencyMaxMs = 0;
    m_applyP50Ms = 0;
    m_applyP95Ms = 0;
    m_recentApplyWrite = 0;
    m_recentApplyMs.fill(0);
    m_requestAtMs.clear();
}

QString CatalogCoverCoordinator::metricsText() const
{
    const CoverCoordinatorStats c = stats();
    const CoverCacheStats net = m_coverCache ? m_coverCache->stats() : CoverCacheStats{};
    return QStringLiteral(
               "cover req=%1 apply=%2 stuck=%3 abandon=%4 | "
               "net ok=%5 fail=%6 hit=%7 neg=%8 preempt=%9 | "
               "q=%10/%11 apply p50=%12 p95=%13 max=%14 | dl p50=%15 p95=%16")
        .arg(c.requests)
        .arg(c.applied)
        .arg(c.stuckRecoveries)
        .arg(c.abandoned)
        .arg(net.downloadsOk)
        .arg(net.downloadsFail)
        .arg(net.cacheHits)
        .arg(net.negativeHits)
        .arg(net.preempts)
        .arg(net.active)
        .arg(net.pending)
        .arg(c.applyP50Ms)
        .arg(c.applyP95Ms)
        .arg(c.applyLatencyMaxMs)
        .arg(net.p50Ms)
        .arg(net.p95Ms);
}

} // namespace arachnel::core
