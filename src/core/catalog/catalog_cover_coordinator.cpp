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

bool CatalogCoverCoordinator::isHqUrl(const QString& url)
{
    return url.contains(QStringLiteral("library_600x900"));
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
                handleMetadataCover(entryId, coverUrl);
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
        if (remote.isEmpty())
            continue;
        const QString local = m_coverCache->localUrlFor(remote);
        if (!local.isEmpty())
            return local;
    }
    return {};
}

QString CatalogCoverCoordinator::resolveCatalogRemote(const CatalogEntry& entry) const
{
    if (isHttpUrl(entry.remoteCoverUrl))
        return entry.remoteCoverUrl;
    if (entry.steamAppId.isEmpty())
        return {};
    if (const QString mapped = m_remoteBySteamAppId.value(entry.steamAppId); isHttpUrl(mapped))
        return mapped;
    return steamThumbUrl(entry.steamAppId);
}

void CatalogCoverCoordinator::rebuildRemoteCoverIndex()
{
    m_remoteBySteamAppId.clear();
    if (!m_entries)
        return;
    for (const CatalogEntry& entry : std::as_const(m_entries())) {
        if (entry.steamAppId.isEmpty() || !isHttpUrl(entry.remoteCoverUrl))
            continue;
        if (!m_remoteBySteamAppId.contains(entry.steamAppId))
            m_remoteBySteamAppId.insert(entry.steamAppId, entry.remoteCoverUrl);
    }
}

void CatalogCoverCoordinator::clearFailedCoverHints()
{
    m_failedEntries.clear();
    if (m_coverCache)
        m_coverCache->clearAllFailed();
}

void CatalogCoverCoordinator::applyCoverToEntry(const QString& entryId, const QString& coverUrl,
                                                bool pending)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
        return;
    if (!coverUrl.isEmpty() && !coverUrl.startsWith(QStringLiteral("file:")))
        return;
    if (entry->coverUrl == coverUrl && entry->metadataPending == pending)
        return;

    entry->coverUrl = coverUrl;
    entry->metadataPending = pending;
    m_catalog->notifyEntryChanged(entryId);
    if (!coverUrl.isEmpty()) {
        m_plans[entryId].appliedLocal = coverUrl;
        noteApplyLatency(entryId);
    }
    emit coverApplied(entryId, coverUrl);
}

void CatalogCoverCoordinator::applyCover(const QString& entryId, const QString& coverUrl)
{
    applyCoverToEntry(entryId, coverUrl, false);
    CoverPlan& plan = m_plans[entryId];
    plan.phase = coverUrl.isEmpty() ? PlanPhase::Failed : PlanPhase::Done;
}

void CatalogCoverCoordinator::markPending(CatalogEntry* entry)
{
    if (!entry || entry->metadataPending)
        return;
    entry->metadataPending = true;
    m_catalog->notifyEntryChanged(entry->id);
}

void CatalogCoverCoordinator::clearPendingFlag(CatalogEntry* entry)
{
    if (!entry || !entry->metadataPending)
        return;
    entry->metadataPending = false;
    m_catalog->notifyEntryChanged(entry->id);
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

void CatalogCoverCoordinator::buildPlanUrls(CatalogEntry* entry, CoverPlan& plan) const
{
    plan.urls.clear();
    plan.index = 0;

    const QString catalogRemote = resolveCatalogRemote(*entry);

    auto appendUnique = [&](const QString& url) {
        if (!isHttpUrl(url) || plan.urls.contains(url))
            return;
        if (m_coverCache->hasFailed(url))
            return;
        plan.urls.append(url);
    };

    // Fast paint: capsule before heavy HQ when we have an app id.
    if (!entry->steamAppId.isEmpty()) {
        const bool remoteIsHq = isHqUrl(catalogRemote) || catalogRemote.isEmpty();
        if (remoteIsHq)
            appendUnique(steamThumbUrl(entry->steamAppId));
    }

    appendUnique(catalogRemote);

    const GameMetadata metadata = m_metadataService->metadataForTitle(entry->title);
    appendUnique(metadata.coverUrl);

    if (!entry->steamAppId.isEmpty()) {
        appendUnique(steamHqUrl(entry->steamAppId));
        if (!isHqUrl(catalogRemote))
            appendUnique(steamThumbUrl(entry->steamAppId));
    }
}

void CatalogCoverCoordinator::dropWaiter(const QString& entryId, const QString& remoteUrl)
{
    if (remoteUrl.isEmpty())
        return;
    auto it = m_waiters.find(remoteUrl);
    if (it == m_waiters.end())
        return;
    it.value().remove(entryId);
    if (it.value().isEmpty())
        m_waiters.erase(it);
}

void CatalogCoverCoordinator::releasePlanRemote(const QString& entryId, CoverPlan& plan)
{
    if (plan.activeRemote.isEmpty())
        return;
    const QString remote = plan.activeRemote;
    dropWaiter(entryId, remote);
    plan.activeRemote.clear();
    if (!m_waiters.contains(remote) || m_waiters.value(remote).isEmpty())
        m_coverCache->release(remote);
}

void CatalogCoverCoordinator::ensureCurrentUrl(const QString& entryId, CoverPlan& plan)
{
    while (plan.index < plan.urls.size()) {
        const QString remote = plan.urls.at(plan.index);
        if (const QString local = m_coverCache->localUrlFor(remote); !local.isEmpty()) {
            m_coverCache->noteCacheHit();
            applyCoverToEntry(entryId, local, false);
            plan.phase = isHqUrl(remote) ? PlanPhase::Done : PlanPhase::Showing;
            if (plan.phase == PlanPhase::Showing && plan.interested && !plan.hqQueued) {
                // Continue ladder for HQ while still watching.
                ++plan.index;
                ++m_ladderAdvances;
                continue;
            }
            if (plan.phase == PlanPhase::Done)
                return;
            // Not interested or no further URLs - stop.
            if (!plan.interested || plan.index >= plan.urls.size()) {
                plan.phase = PlanPhase::Done;
                return;
            }
            continue;
        }

        CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
        if (entry)
            markPending(entry);

        plan.phase = PlanPhase::Fetching;
        plan.activeRemote = remote;
        m_waiters[remote].insert(entryId);

        CoverFetchPriority fetchPri = plan.priority;
        if (plan.appliedLocal.startsWith(QStringLiteral("file:")) && isHqUrl(remote)) {
            fetchPri = CoverFetchPriority::Upgrade;
            plan.hqQueued = true;
        }
        m_coverCache->ensure(remote, fetchPri);
        return;
    }

    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (plan.appliedLocal.startsWith(QStringLiteral("file:"))) {
        plan.phase = PlanPhase::Done;
        clearPendingFlag(entry);
        return;
    }

    // Ask metadata once if we still have nothing.
    if (entry && plan.interested) {
        plan.phase = PlanPhase::Fetching;
        markPending(entry);
        m_metadataService->queueFetch(entryId, entry->title, MetadataFetchMode::CoverOnly,
                                      m_settings->uiLanguage(), entry->steamAppId);
        return;
    }

    plan.phase = PlanPhase::Failed;
    m_failedEntries.insert(entryId);
    applyCoverToEntry(entryId, {}, false);
}

void CatalogCoverCoordinator::startOrContinuePlan(const QString& entryId, CoverPlan& plan)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
        return;

    // Instant disk hits before any network.
    const QString catalogRemote = resolveCatalogRemote(*entry);

    QStringList probe;
    if (!entry->steamAppId.isEmpty()) {
        probe << steamHqUrl(entry->steamAppId) << steamThumbUrl(entry->steamAppId)
              << steamHeaderUrl(entry->steamAppId);
    }
    if (!catalogRemote.isEmpty())
        probe.prepend(catalogRemote);
    if (const QString local = firstCached(probe); !local.isEmpty()) {
        m_coverCache->noteCacheHit();
        applyCoverToEntry(entryId, local, false);
        plan.phase = PlanPhase::Showing;
        plan.appliedLocal = local;
        // Build ladder only for optional HQ upgrade when interested.
        buildPlanUrls(entry, plan);
        // Skip URLs already satisfied / failed; jump to first HQ not yet applied.
        for (int i = 0; i < plan.urls.size(); ++i) {
            if (isHqUrl(plan.urls.at(i))
                && m_coverCache->localUrlFor(plan.urls.at(i)).isEmpty()
                && !m_coverCache->hasFailed(plan.urls.at(i))) {
                plan.index = i;
                if (plan.interested)
                    ensureCurrentUrl(entryId, plan);
                else
                    plan.phase = PlanPhase::Done;
                return;
            }
        }
        plan.phase = PlanPhase::Done;
        clearPendingFlag(entry);
        return;
    }

    if (plan.urls.isEmpty())
        buildPlanUrls(entry, plan);
    ensureCurrentUrl(entryId, plan);
}

void CatalogCoverCoordinator::advancePlan(const QString& entryId, CoverPlan& plan)
{
    ++plan.index;
    ++m_ladderAdvances;
    plan.activeRemote.clear();
    if (!plan.interested) {
        plan.phase = plan.appliedLocal.startsWith(QStringLiteral("file:")) ? PlanPhase::Done
                                                                           : PlanPhase::Idle;
        return;
    }
    ensureCurrentUrl(entryId, plan);
}

void CatalogCoverCoordinator::handleCacheReady(const QString& remoteUrl, const QString& localUrl)
{
    const QSet<QString> waiters = m_waiters.take(remoteUrl);
    if (waiters.isEmpty())
        return;

    const bool hq = isHqUrl(remoteUrl);
    for (const QString& entryId : waiters) {
        CoverPlan& plan = m_plans[entryId];
        if (plan.activeRemote == remoteUrl)
            plan.activeRemote.clear();

        applyCoverToEntry(entryId, localUrl, false);
        plan.phase = hq ? PlanPhase::Done : PlanPhase::Showing;

        if (!plan.interested) {
            if (hq)
                plan.phase = PlanPhase::Done;
            continue;
        }

        // Still watching: continue ladder for HQ only (one URL at a time).
        if (!hq) {
            advancePlan(entryId, plan);
        } else {
            plan.hqQueued = false;
            plan.phase = PlanPhase::Done;
            CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
            clearPendingFlag(entry);
        }
    }
}

void CatalogCoverCoordinator::handleCacheFailed(const QString& remoteUrl)
{
    const QSet<QString> waiters = m_waiters.take(remoteUrl);
    if (waiters.isEmpty())
        return;

    for (const QString& entryId : waiters) {
        CoverPlan& plan = m_plans[entryId];
        if (plan.activeRemote == remoteUrl)
            plan.activeRemote.clear();
        if (!plan.interested) {
            plan.phase = plan.appliedLocal.startsWith(QStringLiteral("file:")) ? PlanPhase::Done
                                                                               : PlanPhase::Idle;
            continue;
        }
        advancePlan(entryId, plan);
    }
}

void CatalogCoverCoordinator::handleMetadataCover(const QString& entryId, const QString& coverUrl)
{
    CoverPlan& plan = m_plans[entryId];
    if (!plan.interested && plan.phase != PlanPhase::Fetching)
        return;

    if (coverUrl.isEmpty()) {
        if (!plan.appliedLocal.startsWith(QStringLiteral("file:"))) {
            m_failedEntries.insert(entryId);
            plan.phase = PlanPhase::Failed;
            applyCoverToEntry(entryId, {}, false);
        }
        return;
    }
    if (coverUrl.startsWith(QStringLiteral("file:"))) {
        applyCoverToEntry(entryId, coverUrl, false);
        plan.phase = PlanPhase::Done;
        return;
    }
    if (!isHttpUrl(coverUrl)) {
        if (!plan.appliedLocal.startsWith(QStringLiteral("file:"))) {
            plan.phase = PlanPhase::Failed;
            applyCoverToEntry(entryId, {}, false);
        }
        return;
    }

    if (!plan.urls.contains(coverUrl))
        plan.urls.insert(plan.index, coverUrl);
    else
        plan.index = plan.urls.indexOf(coverUrl);
    ensureCurrentUrl(entryId, plan);
}

void CatalogCoverCoordinator::ensureDiskCover(const QString& entryId, const QString& remoteUrl,
                                              CoverFetchPriority priority)
{
    if (remoteUrl.isEmpty()) {
        applyCoverToEntry(entryId, {}, false);
        return;
    }
    if (remoteUrl.startsWith(QStringLiteral("file:"))) {
        if (!m_coverCache->localUrlFor(remoteUrl).isEmpty())
            applyCoverToEntry(entryId, remoteUrl, false);
        return;
    }
    if (!isHttpUrl(remoteUrl))
        return;

    CoverPlan& plan = m_plans[entryId];
    plan.interested = true;
    if (static_cast<int>(priority) > static_cast<int>(plan.priority))
        plan.priority = priority;

    if (const QString local = m_coverCache->localUrlFor(remoteUrl); !local.isEmpty()) {
        m_coverCache->noteCacheHit();
        applyCoverToEntry(entryId, local, false);
        plan.phase = PlanPhase::Done;
        return;
    }
    if (m_coverCache->hasFailed(remoteUrl)) {
        applyCoverToEntry(entryId, {}, false);
        return;
    }

    plan.phase = PlanPhase::Fetching;
    plan.activeRemote = remoteUrl;
    if (!plan.urls.contains(remoteUrl))
        plan.urls.prepend(remoteUrl);
    plan.index = plan.urls.indexOf(remoteUrl);
    m_waiters[remoteUrl].insert(entryId);
    m_coverCache->ensure(remoteUrl, priority);
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

    if (priority == CoverFetchPriority::Visible)
        m_failedEntries.remove(entryId);

    if (m_failedEntries.contains(entryId)
        && !entry->coverUrl.startsWith(QStringLiteral("file:")))
        return;

    CoverPlan& plan = m_plans[entryId];
    const bool wasInterested = plan.interested;
    plan.interested = true;
    if (static_cast<int>(priority) > static_cast<int>(plan.priority))
        plan.priority = priority;

    // Strip stale non-file display URLs without spamming QML (http was never shown).
    if (!entry->coverUrl.isEmpty()) {
        const bool okLocal = entry->coverUrl.startsWith(QStringLiteral("file:"))
            && !m_coverCache->localUrlFor(entry->coverUrl).isEmpty();
        if (!okLocal) {
            if (isHttpUrl(entry->coverUrl) && entry->remoteCoverUrl.isEmpty())
                entry->remoteCoverUrl = entry->coverUrl;
            const bool wasFile = entry->coverUrl.startsWith(QStringLiteral("file:"));
            entry->coverUrl.clear();
            if (wasFile)
                m_catalog->notifyEntryChanged(entryId);
        }
    }

    if (entry->coverUrl.startsWith(QStringLiteral("file:"))) {
        plan.appliedLocal = entry->coverUrl;
        plan.phase = PlanPhase::Showing;
        // Optional HQ only when Visible and still interested.
        if (priority == CoverFetchPriority::Visible && !entry->steamAppId.isEmpty()) {
            buildPlanUrls(entry, plan);
            for (int i = 0; i < plan.urls.size(); ++i) {
                if (isHqUrl(plan.urls.at(i))
                    && m_coverCache->localUrlFor(plan.urls.at(i)).isEmpty()
                    && !m_coverCache->hasFailed(plan.urls.at(i))) {
                    plan.index = i;
                    ensureCurrentUrl(entryId, plan);
                    return;
                }
            }
        }
        plan.phase = PlanPhase::Done;
        return;
    }

    // Already fetching this generation - just bump priority / re-front queue.
    if (plan.phase == PlanPhase::Fetching && !plan.activeRemote.isEmpty()) {
        ++m_dupSuppressed;
        m_coverCache->ensure(plan.activeRemote, plan.priority);
        return;
    }

    if (!wasInterested || plan.urls.isEmpty() || plan.phase == PlanPhase::Idle
        || plan.phase == PlanPhase::Failed) {
        ++plan.generation;
        plan.hqQueued = false;
        releasePlanRemote(entryId, plan);
        buildPlanUrls(entry, plan);
    }

    startOrContinuePlan(entryId, plan);
}

void CatalogCoverCoordinator::cancelCatalogCover(const QString& entryId)
{
    ++m_cancels;
    auto it = m_plans.find(entryId);
    if (it == m_plans.end()) {
        m_metadataService->cancelPending(entryId);
        CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
        clearPendingFlag(entry);
        return;
    }
    CoverPlan& plan = it.value();
    const bool abandoned = plan.phase == PlanPhase::Fetching;
    plan.interested = false;
    if (abandoned)
        ++m_abandoned;

    releasePlanRemote(entryId, plan);
    m_metadataService->cancelPending(entryId);

    if (abandoned) {
        plan.phase = PlanPhase::Idle;
        plan.hqQueued = false;
    }

    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    clearPendingFlag(entry);

    if (!abandoned && plan.phase != PlanPhase::Fetching)
        m_plans.erase(it);
}

void CatalogCoverCoordinator::invalidateCatalogCover(const QString& entryId)
{
    CatalogEntry* entry = m_findEntry ? m_findEntry(entryId) : nullptr;
    if (!entry)
        return;

    m_failedEntries.remove(entryId);
    CoverPlan& plan = m_plans[entryId];
    releasePlanRemote(entryId, plan);

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
        for (const QString& u :
             {steamThumbUrl(entry->steamAppId), steamHqUrl(entry->steamAppId),
              steamHeroUrl(entry->steamAppId), steamHeaderUrl(entry->steamAppId)}) {
            m_coverCache->clearFailed(u);
            m_coverCache->remove(u);
        }
    }

    m_metadataService->clearCachedCover(entry->title);
    m_heroLocal.remove(entryId);
    plan = CoverPlan{};
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
    if (!entry)
        return;
    if (m_heroLocal.contains(entryId)) {
        emit heroCoverApplied(entryId, m_heroLocal.value(entryId));
        return;
    }
    if (entry->coverUrl.startsWith(QStringLiteral("file:"))) {
        m_heroLocal.insert(entryId, entry->coverUrl);
        emit heroCoverApplied(entryId, entry->coverUrl);
    }
    if (entry->steamAppId.isEmpty())
        return;
    if (const QString local = firstCached(
            {steamHeroUrl(entry->steamAppId), steamHeaderUrl(entry->steamAppId)});
        !local.isEmpty()) {
        m_heroLocal.insert(entryId, local);
        emit heroCoverApplied(entryId, local);
        return;
    }
    queueHeroDownload(entryId, steamHeroUrl(entry->steamAppId));
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
    s.abandoned = m_abandoned;
    s.dupSuppressed = m_dupSuppressed;
    s.ladderAdvances = m_ladderAdvances;
    s.applyLatencySumMs = m_applyLatencySumMs;
    s.applyLatencyMaxMs = m_applyLatencyMaxMs;
    s.applyP50Ms = m_applyP50Ms;
    s.applyP95Ms = m_applyP95Ms;
    for (auto it = m_plans.constBegin(); it != m_plans.constEnd(); ++it) {
        if (it->interested)
            ++s.interested;
        if (it->phase == PlanPhase::Fetching || it->phase == PlanPhase::Showing)
            ++s.plansActive;
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
        {QStringLiteral("abandoned"), s.abandoned},
        {QStringLiteral("dupSuppressed"), s.dupSuppressed},
        {QStringLiteral("ladderAdvances"), s.ladderAdvances},
        {QStringLiteral("applyAvgMs"),
         s.applied > 0 ? static_cast<qint64>(s.applyLatencySumMs / s.applied) : 0},
        {QStringLiteral("applyMaxMs"), s.applyLatencyMaxMs},
        {QStringLiteral("applyP50Ms"), s.applyP50Ms},
        {QStringLiteral("applyP95Ms"), s.applyP95Ms},
        {QStringLiteral("interested"), s.interested},
        {QStringLiteral("plansActive"), s.plansActive},
    };
}

void CatalogCoverCoordinator::resetStats()
{
    m_requests = 0;
    m_cancels = 0;
    m_applied = 0;
    m_abandoned = 0;
    m_dupSuppressed = 0;
    m_ladderAdvances = 0;
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
               "cover req=%1 apply=%2 int=%3 plans=%4 dup=%5 abandon=%6 | "
               "net ok=%7 fail=%8 hit=%9 preempt=%10 | "
               "q=%11/%12 apply p50=%13 p95=%14 | dl p50=%15 p95=%16")
        .arg(c.requests)
        .arg(c.applied)
        .arg(c.interested)
        .arg(c.plansActive)
        .arg(c.dupSuppressed)
        .arg(c.abandoned)
        .arg(net.downloadsOk)
        .arg(net.downloadsFail)
        .arg(net.cacheHits)
        .arg(net.preempts)
        .arg(net.active)
        .arg(net.pending)
        .arg(c.applyP50Ms)
        .arg(c.applyP95Ms)
        .arg(net.p50Ms)
        .arg(net.p95Ms);
}

} // namespace arachnel::core
