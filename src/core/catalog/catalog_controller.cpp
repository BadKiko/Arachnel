#include "catalog_controller.h"

#include "catalog_disk_cache.h"
#include "catalog_feed_loader.h"
#include "catalog_model.h"
#include "catalog_parser.h"
#include "crash_log.h"
#include "plugin_catalog_json.h"
#include "plugin_host.h"
#include "source_plugin_model.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QReadLocker>
#include <QTimer>
#include <QUrl>
#include <QWriteLocker>
#include <QtConcurrent>

#include <optional>

namespace arachnel::core {
namespace {
bool catalogCacheHasPollutedIds(const QVector<CatalogEntry>& entries)
{
    for (const CatalogEntry& entry : entries) {
        if (entry.id.startsWith(QStringLiteral("count:")))
            return true;
    }
    return false;
}

} // namespace

CatalogController::CatalogController(CatalogModel* catalog, SourcePluginModel* sources,
                                     PluginHost* pluginHost, QVector<CatalogEntry>* mergedCache,
                                     Hooks hooks, QObject* parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_sources(sources)
    , m_pluginHost(pluginHost)
    , m_mergedCache(mergedCache)
    , m_hooks(std::move(hooks))
    , m_loader(new CatalogFeedLoader(this))
    , m_probeLoader(new CatalogFeedLoader(this))
    , m_cacheTtlTimer(new QTimer(this))
{
    m_cacheTtlTimer->setInterval(60 * 1000);
    connect(m_cacheTtlTimer, &QTimer::timeout, this, &CatalogController::refreshStaleSources);
    m_cacheTtlTimer->start();

    connect(m_loader, &CatalogFeedLoader::feedLoaded, this,
            [this](const QString& sourceId, QVector<CatalogEntry> entries,
                   const QByteArray& payloadSha) {
                m_catalogHttpLoadActive = false;
                if (!payloadSha.isEmpty() && m_sourcePayloadSha.value(sourceId) == payloadSha
                    && m_catalogBySource.contains(sourceId)) {
                    m_sourceLoadedAtMs.insert(sourceId, QDateTime::currentMSecsSinceEpoch());
                    m_loadingSourceIds.remove(sourceId);
                    updateCatalogLoadingState();
                    processCatalogLoadQueue();
                    return;
                }
                if (!payloadSha.isEmpty())
                    m_sourcePayloadSha.insert(sourceId, payloadSha);

                // Prepare off the UI thread (same as disk/plugin paths).
                auto* watcher = new QFutureWatcher<QVector<CatalogEntry>>(this);
                m_inFlightPluginCatalogWatchers.append(watcher);
                const auto prepare = m_hooks.prepareEntry;
                connect(watcher, &QFutureWatcher<QVector<CatalogEntry>>::finished, this,
                        [this, watcher, sourceId]() {
                            m_inFlightPluginCatalogWatchers.removeAll(watcher);
                            QVector<CatalogEntry> prepared = watcher->result();
                            watcher->deleteLater();
                            storeCatalogForSource(sourceId, std::move(prepared),
                                                  /*prepareEntries=*/false);
                            processCatalogLoadQueue();
                        });
                watcher->setFuture(QtConcurrent::run(
                    [entries = std::move(entries), sourceId, prepare]() mutable {
                        for (CatalogEntry& entry : entries) {
                            entry.sourceId = sourceId;
                            entry.id = repairCatalogEntryId(entry.id);
                            if (prepare)
                                prepare(entry);
                            else
                                prepareCatalogEntry(entry);
                        }
                        return entries;
                    }));
            });
    connect(m_loader, &CatalogFeedLoader::feedNotModified, this,
            [this](const QString& sourceId) {
                m_catalogHttpLoadActive = false;
                m_sourceLoadedAtMs.insert(sourceId, QDateTime::currentMSecsSinceEpoch());
                m_loadingSourceIds.remove(sourceId);
                updateCatalogLoadingState();
                processCatalogLoadQueue();
            });
    connect(m_loader, &CatalogFeedLoader::feedFailed, this,
            [this](const QString& sourceId, const QString& error) {
                m_catalogHttpLoadActive = false;
                m_loadingSourceIds.remove(sourceId);
                if (m_activeSourceIds.contains(sourceId) && !m_catalogBySource.contains(sourceId)) {
                    emit noticeRequested(
                        QCoreApplication::translate("Core", "Catalog error: %1").arg(error));
                }
                if (m_activeSourceIds.contains(sourceId))
                    rebuildMergedCatalog();
                else
                    updateCatalogLoadingState();
                processCatalogLoadQueue();
            });
    connect(m_probeLoader, &CatalogFeedLoader::feedCountLoaded, this,
            [this](const QString& tag, int count) {
                if (!tag.startsWith(QStringLiteral("count:")))
                    return;
                const QString sourceId = tag.mid(6);
                m_catalogCounts.insert(sourceId, count);
                emit catalogCountsChanged();
                if (m_activeSourceIds.contains(sourceId) && !m_catalogBySource.contains(sourceId))
                    requestCatalogLoad(sourceId);
                startNextCatalogPrefetch();
            });
    connect(m_probeLoader, &CatalogFeedLoader::feedFailed, this,
            [this](const QString& tag, const QString&) {
                if (!tag.startsWith(QStringLiteral("count:")))
                    return;
                m_catalogCounts.insert(tag.mid(6), -1);
                emit catalogCountsChanged();
                startNextCatalogPrefetch();
            });
}

bool CatalogController::catalogLoading() const
{
    // Prefetch count watchers share m_inFlightPluginCatalogWatchers but must not
    // keep the catalog chrome stuck on "Loading…" after games are already shown.
    return !m_loadingSourceIds.isEmpty() || m_catalogHttpLoadActive;
}
QString CatalogController::catalogStatus() const { return m_catalogStatus; }
QString CatalogController::activeCatalogSourceId() const { return m_activeSourceIds.value(0); }
QStringList CatalogController::activeCatalogSourceIds() const { return m_activeSourceIds; }
int CatalogController::catalogEntryCount(const QString& id) const
{
    return id.isEmpty()
               ? -1
               : m_catalogBySource.contains(id) ? m_catalogBySource.value(id).size()
                                                : m_catalogCounts.value(id, -1);
}
bool CatalogController::isCatalogSourceSelected(const QString& id) const
{
    return m_activeSourceIds.contains(id);
}
const QHash<QString, QVector<CatalogEntry>>& CatalogController::catalogsBySource() const
{
    return m_catalogBySource;
}

QString CatalogController::normalizeTitleKey(const QString& title)
{
    QString key = title.trimmed().toLower();
    key.remove(QLatin1Char(' '));
    key.remove(QLatin1Char('-'));
    key.remove(QLatin1Char('_'));
    key.remove(QLatin1Char(':'));
    key.remove(QLatin1Char('\''));
    key.remove(QLatin1Char('"'));
    return key;
}

QString CatalogController::offerGroupKey(const CatalogEntry& entry)
{
    if (!entry.steamAppId.trimmed().isEmpty())
        return QStringLiteral("steam:%1").arg(entry.steamAppId.trimmed());
    const QString titleKey = normalizeTitleKey(entry.title);
    if (!titleKey.isEmpty())
        return QStringLiteral("title:%1").arg(titleKey);
    return QStringLiteral("id:%1").arg(entry.id);
}

int CatalogController::showcaseScore(const CatalogEntry& entry)
{
    int score = 0;
    if (!entry.coverUrl.isEmpty() || !entry.remoteCoverUrl.isEmpty())
        score += 100;
    if (!entry.steamAppId.isEmpty())
        score += 50;
    if (!entry.description.isEmpty())
        score += 20;
    if (entry.sourceId.contains(QStringLiteral("steam"), Qt::CaseInsensitive))
        score += 10;
    if (entry.releaseDay > 0)
        score += qMin(30, int(entry.releaseDay / 30));
    if (entry.sizeBytes > 0)
        score += 5;
    return score;
}

bool CatalogController::isSourceCacheFresh(const QString& sourceId) const
{
    if (!m_catalogBySource.contains(sourceId))
        return false;
    if (catalogCacheHasPollutedIds(m_catalogBySource.value(sourceId)))
        return false;
    const qint64 loadedAt = m_sourceLoadedAtMs.value(sourceId, 0);
    if (loadedAt <= 0)
        return false;
    return (QDateTime::currentMSecsSinceEpoch() - loadedAt) < kCatalogCacheTtlMs;
}

void CatalogController::refreshStaleSources()
{
    for (const QString& sourceId : m_activeSourceIds) {
        if (!m_catalogBySource.contains(sourceId))
            continue;
        if (isSourceCacheFresh(sourceId))
            continue;
        if (m_loadingSourceIds.contains(sourceId))
            continue;
        // Soft revalidate — hard refresh freezes the UI on huge catalogs.
        revalidateCatalogSource(sourceId);
    }
}

void CatalogController::normalizeCatalogSourceIds(QVector<CatalogEntry>& entries,
                                                  const QString& sourceId)
{
    for (CatalogEntry& entry : entries) {
        entry.sourceId = sourceId;
        entry.id = repairCatalogEntryId(entry.id);
    }
}

void CatalogController::storeCatalogForSource(const QString& sourceId, QVector<CatalogEntry> entries,
                                              bool prepareEntries)
{
    arachnel::logBreadcrumb(QStringLiteral("catalog.store"),
                            QStringLiteral("%1 n=%2").arg(sourceId).arg(entries.size()));
    normalizeCatalogSourceIds(entries, sourceId);
    if (prepareEntries) {
        for (CatalogEntry& entry : entries) {
            // prepareEntry (metadata) calls prepareCatalogEntry; otherwise prepare once here.
            if (m_hooks.prepareEntry)
                m_hooks.prepareEntry(entry);
            else
                prepareCatalogEntry(entry);
        }
    }
    m_catalogBySource.insert(sourceId, std::move(entries));
    m_sourceLoadedAtMs.insert(sourceId, QDateTime::currentMSecsSinceEpoch());
    m_catalogCounts.insert(sourceId, m_catalogBySource.value(sourceId).size());
    emit catalogCountsChanged();
    m_loadingSourceIds.remove(sourceId);
    if (m_activeSourceIds.contains(sourceId))
        rebuildMergedCatalog();
    else
        updateCatalogLoadingState();
}

void CatalogController::commitCatalogLoad(const QString& sourceId, QVector<CatalogEntry> entries)
{
    storeCatalogForSource(sourceId, std::move(entries));
}

void CatalogController::rebuildMergedCatalog()
{
    if (m_activeSourceIds.isEmpty()) {
        ++m_mergeGeneration;
        m_installOffers.clear();
        m_entryIdToOfferGroup.clear();
        if (m_mergedCacheLock) {
            QWriteLocker locker(m_mergedCacheLock);
            m_mergedCache->clear();
        } else {
            m_mergedCache->clear();
        }
        m_catalog->clear();
        if (m_hooks.rebuildIdIndex)
            m_hooks.rebuildIdIndex();
        if (m_hooks.rebuildGenres)
            m_hooks.rebuildGenres();
        setCatalogStatus({});
        updateCatalogLoadingState();
        return;
    }

    QStringList enabledActiveIds;
    enabledActiveIds.reserve(m_activeSourceIds.size());
    for (const QString& sourceId : m_activeSourceIds) {
        const SourcePluginInfo* source = m_sources->pluginById(sourceId);
        if (source && source->enabled)
            enabledActiveIds.append(sourceId);
    }
    const bool multiSource = enabledActiveIds.size() > 1;

    // Kick loads for missing sources on the UI thread; heavy merge runs off-thread.
    // Empty bySource rows after a single-source evacuate must reload when multi-source.
    for (const QString& sourceId : enabledActiveIds) {
        const bool missing = !m_catalogBySource.contains(sourceId);
        const bool evacuated =
            multiSource && m_catalogBySource.contains(sourceId)
            && m_catalogBySource.value(sourceId).isEmpty();
        if (evacuated) {
            m_catalogBySource.remove(sourceId);
            m_sourceLoadedAtMs.remove(sourceId);
        }
        if ((missing || evacuated) && !m_loadingSourceIds.contains(sourceId))
            requestCatalogLoad(sourceId);
    }

    QStringList loadedSourceIds;
    loadedSourceIds.reserve(enabledActiveIds.size());
    for (const QString& sourceId : enabledActiveIds) {
        if (!m_catalogBySource.contains(sourceId))
            continue;
        if (m_catalogBySource.value(sourceId).isEmpty())
            continue;
        loadedSourceIds.append(sourceId);
    }

    // Evacuate only when a single enabled source is configured. If another plugin is
    // still loading, moving the first catalog out of bySource drops it from later merges
    // and install offers stay single-source (Steam only, FreeTP gone).
    if (!multiSource && loadedSourceIds.size() == 1) {
        const QString sourceId = loadedSourceIds.first();
        QVector<CatalogEntry> local;
        {
            const auto it = m_catalogBySource.find(sourceId);
            if (it != m_catalogBySource.end() && !it.value().isEmpty()) {
                local = std::move(it.value());
                it.value() = QVector<CatalogEntry>();
            }
        }
        if (local.isEmpty()) {
            // Already evacuated into m_mergedCache — only refresh filter/status.
            if (m_mergedCache && !m_mergedCache->isEmpty()
                && m_sourceLoadedAtMs.contains(sourceId)) {
                ++m_mergeGeneration;
                if (m_hooks.rebuildGenres)
                    m_hooks.rebuildGenres();
                if (m_hooks.applyFilter)
                    m_hooks.applyFilter(m_activeQuery);
                const SourcePluginInfo* source = m_sources->pluginById(sourceId);
                setCatalogStatus(QCoreApplication::translate("Core", "%1 · %2 games")
                                     .arg(source ? source->name : sourceId)
                                     .arg(m_catalog->count()));
                updateCatalogLoadingState();
            }
            return;
        }
        const quint64 generation = ++m_mergeGeneration;
        applyMergedCatalogResult(generation, std::move(local), {}, {});
        return;
    }

    if (loadedSourceIds.isEmpty())
        return;

    struct MergeResult {
        QVector<CatalogEntry> merged;
        QHash<QString, QVector<CatalogEntry>> installOffers;
        QHash<QString, QString> entryIdToOfferGroup;
    };

    const quint64 generation = ++m_mergeGeneration;
    const QStringList activeSourceIds = m_activeSourceIds;
    QHash<QString, QVector<CatalogEntry>> bySource;
    bySource.reserve(loadedSourceIds.size());
    for (const QString& sourceId : loadedSourceIds)
        bySource.insert(sourceId, m_catalogBySource.value(sourceId));

    QSet<QString> enabledIds(loadedSourceIds.begin(), loadedSourceIds.end());

    auto* watcher = new QFutureWatcher<MergeResult>(this);
    connect(watcher, &QFutureWatcher<MergeResult>::finished, this,
            [this, watcher, generation]() {
                MergeResult result = watcher->result();
                watcher->deleteLater();
                applyMergedCatalogResult(generation, std::move(result.merged),
                                         std::move(result.installOffers),
                                         std::move(result.entryIdToOfferGroup));
            });
    watcher->setFuture(QtConcurrent::run(
        [bySource = std::move(bySource), activeSourceIds, enabledIds]() -> MergeResult {
            MergeResult out;
            QHash<QString, QVector<CatalogEntry>> groups;
            QStringList groupOrder;

            // Steam rows key by app id; FreeTP often has no steamAppId yet and keys by title.
            // Map title → steam:* so "The Forest" from both sources becomes one card.
            QHash<QString, QString> titleToSteamKey;
            for (const QString& sourceId : activeSourceIds) {
                if (!enabledIds.contains(sourceId))
                    continue;
                const auto it = bySource.constFind(sourceId);
                if (it == bySource.cend())
                    continue;
                for (const CatalogEntry& entry : it.value()) {
                    const QString appId = entry.steamAppId.trimmed();
                    if (appId.isEmpty())
                        continue;
                    const QString titleKey = normalizeTitleKey(entry.title);
                    if (titleKey.isEmpty())
                        continue;
                    titleToSteamKey.insert(titleKey, QStringLiteral("steam:%1").arg(appId));
                }
            }

            for (const QString& sourceId : activeSourceIds) {
                if (!enabledIds.contains(sourceId))
                    continue;
                const auto it = bySource.constFind(sourceId);
                if (it == bySource.cend())
                    continue;
                for (const CatalogEntry& entry : it.value()) {
                    CatalogEntry copy = entry;
                    copy.id = repairCatalogEntryId(copy.id);
                    QString key = offerGroupKey(copy);
                    if (key.startsWith(QLatin1String("title:"))) {
                        const QString steamKey =
                            titleToSteamKey.value(normalizeTitleKey(copy.title));
                        if (!steamKey.isEmpty())
                            key = steamKey;
                    }
                    if (!groups.contains(key))
                        groupOrder.append(key);
                    groups[key].append(std::move(copy));
                }
            }

            out.merged.reserve(groups.size());
            for (const QString& key : groupOrder) {
                QVector<CatalogEntry> offers = groups.take(key);
                if (offers.isEmpty())
                    continue;

                QVector<CatalogEntry> uniqueOffers;
                QSet<QString> seenSources;
                for (CatalogEntry& offer : offers) {
                    if (seenSources.contains(offer.sourceId))
                        continue;
                    seenSources.insert(offer.sourceId);
                    uniqueOffers.append(std::move(offer));
                }
                offers = std::move(uniqueOffers);

                int bestIdx = 0;
                int bestScore = showcaseScore(offers.first());
                for (int i = 1; i < offers.size(); ++i) {
                    const int score = showcaseScore(offers.at(i));
                    if (score > bestScore) {
                        bestScore = score;
                        bestIdx = i;
                    }
                }
                CatalogEntry showcase = offers.at(bestIdx);
                // Keep showcase.id from the winning offer. Never swap in another offer's id -
                // that produced Frankenstein cards (Car Mechanic art + Undying Flower entryId).
                // Only fill missing fields from peers with the same normalized title.
                const QString showcaseTitleKey = normalizeTitleKey(showcase.title);
                for (const CatalogEntry& offer : offers) {
                    if (normalizeTitleKey(offer.title) != showcaseTitleKey)
                        continue;
                    if (showcase.remoteCoverUrl.isEmpty() && !offer.remoteCoverUrl.isEmpty())
                        showcase.remoteCoverUrl = offer.remoteCoverUrl;
                    if (showcase.coverUrl.isEmpty() && !offer.coverUrl.isEmpty())
                        showcase.coverUrl = offer.coverUrl;
                    if (showcase.steamAppId.isEmpty() && !offer.steamAppId.isEmpty())
                        showcase.steamAppId = offer.steamAppId;
                }

                out.entryIdToOfferGroup.insert(showcase.id, key);
                for (const CatalogEntry& offer : offers)
                    out.entryIdToOfferGroup.insert(offer.id, key);

                // Multi-source only — single-offer groups use the showcase row.
                if (offers.size() > 1)
                    out.installOffers.insert(key, std::move(offers));

                out.merged.append(std::move(showcase));
            }
            return out;
        }));
}

void CatalogController::applyMergedCatalogResult(
    quint64 generation, QVector<CatalogEntry> merged,
    QHash<QString, QVector<CatalogEntry>> installOffers,
    QHash<QString, QString> entryIdToOfferGroup)
{
    if (generation != m_mergeGeneration)
        return;

    m_installOffers = std::move(installOffers);
    m_entryIdToOfferGroup = std::move(entryIdToOfferGroup);

    {
        if (m_mergedCacheLock) {
            QWriteLocker locker(m_mergedCacheLock);
            *m_mergedCache = std::move(merged);
        } else {
            *m_mergedCache = std::move(merged);
        }
    }

    int enabledActiveCount = 0;
    QString singleEnabledId;
    bool waitingOnPeer = false;
    for (const QString& sourceId : m_activeSourceIds) {
        const SourcePluginInfo* source = m_sources->pluginById(sourceId);
        if (!source || !source->enabled)
            continue;
        ++enabledActiveCount;
        if (singleEnabledId.isEmpty())
            singleEnabledId = sourceId;
        if (m_loadingSourceIds.contains(sourceId))
            waitingOnPeer = true;
    }

    // Another enabled source is still downloading - don't push a partial catalog into
    // QML (freetp 2k then steamidra 100k reset was crashing GridView / Qt6QmlMeta).
    if (waitingOnPeer) {
        // Cache already swapped above - drop stale visibles and rebuild the id index so
        // cover/metadata lookups don't use indices from the previous layout.
        if (m_catalog)
            m_catalog->setVisibleIndicesPresorted({});
        if (m_hooks.rebuildIdIndex)
            m_hooks.rebuildIdIndex();
        updateCatalogLoadingState();
        return;
    }

    // Drop stale visible rows before async filter - old indices point at the previous
    // cache layout and GridView can crash if it binds through a full model reset later.
    if (m_catalog)
        m_catalog->setVisibleIndicesPresorted({});

    if (m_hooks.mergedEntriesReady)
        m_hooks.mergedEntriesReady(*m_mergedCache, m_activeSourceIds, m_activeQuery);

    if (enabledActiveCount == 1 && !singleEnabledId.isEmpty()) {
        const auto it = m_catalogBySource.find(singleEnabledId);
        if (it != m_catalogBySource.end() && !it.value().isEmpty())
            it.value() = QVector<CatalogEntry>();
    }

    if (m_hooks.rebuildIdIndex)
        m_hooks.rebuildIdIndex();
    if (m_hooks.rebuildGenres)
        m_hooks.rebuildGenres();
    if (m_hooks.applyFilter)
        m_hooks.applyFilter(m_activeQuery);
    if (m_hooks.warmCovers)
        m_hooks.warmCovers();

    if (enabledActiveCount == 1) {
        const SourcePluginInfo* source = m_sources->pluginById(singleEnabledId);
        setCatalogStatus(QCoreApplication::translate("Core", "%1 · %2 games")
                             .arg(source ? source->name : singleEnabledId)
                             .arg(m_catalog->count()));
    } else {
        setCatalogStatus(QCoreApplication::translate("Core", "%1 sources · %2 games")
                             .arg(enabledActiveCount)
                             .arg(m_catalog->count()));
    }
    updateCatalogLoadingState();
    if (m_hooks.catalogReady)
        m_hooks.catalogReady();
}

QVariantList CatalogController::installOffersForEntry(const QString& entryId) const
{
    QVariantList out;
    const auto appendOffer = [&](const CatalogEntry& offer) {
        QVariantMap row;
        row.insert(QStringLiteral("entryId"), offer.id);
        row.insert(QStringLiteral("sourceId"), offer.sourceId);
        row.insert(QStringLiteral("sourceName"), m_sources->nameForId(offer.sourceId));
        row.insert(QStringLiteral("title"), offer.title);
        row.insert(QStringLiteral("sizeBytes"), offer.sizeBytes);
        const QString sizeLabel =
            offer.sizeLabel.isEmpty() ? formatSizeLabelBytes(offer.sizeBytes) : offer.sizeLabel;
        row.insert(QStringLiteral("sizeLabel"), sizeLabel);
        out.append(row);
    };

    const QString groupKey = m_entryIdToOfferGroup.value(entryId);
    if (!groupKey.isEmpty()) {
        const QVector<CatalogEntry> offers = m_installOffers.value(groupKey);
        if (!offers.isEmpty()) {
            for (const CatalogEntry& offer : offers)
                appendOffer(offer);
            return out;
        }
    }

    // Single-offer: showcase row from merged cache.
    if (!m_mergedCache)
        return out;
    const auto findMerged = [&]() {
        for (const CatalogEntry& entry : *m_mergedCache) {
            if (entry.id == entryId) {
                appendOffer(entry);
                return;
            }
        }
    };
    if (m_mergedCacheLock) {
        QReadLocker locker(m_mergedCacheLock);
        findMerged();
    } else {
        findMerged();
    }
    return out;
}

std::optional<CatalogEntry> CatalogController::resolveInstallOffer(const QString& entryId,
                                                                   const QString& sourceId) const
{
    const QString groupKey = m_entryIdToOfferGroup.value(entryId);
    if (!groupKey.isEmpty()) {
        const QVector<CatalogEntry> offers = m_installOffers.value(groupKey);
        for (const CatalogEntry& offer : offers) {
            if (offer.sourceId == sourceId)
                return offer;
        }
        if (sourceId.isEmpty() && !offers.isEmpty())
            return offers.first();
    }

    if (!m_mergedCache)
        return std::nullopt;
    const auto findMerged = [&]() -> std::optional<CatalogEntry> {
        for (const CatalogEntry& entry : *m_mergedCache) {
            if (entry.id != entryId)
                continue;
            if (sourceId.isEmpty() || entry.sourceId == sourceId)
                return entry;
        }
        return std::nullopt;
    };
    if (m_mergedCacheLock) {
        QReadLocker locker(m_mergedCacheLock);
        return findMerged();
    }
    return findMerged();
}

const CatalogEntry* CatalogController::entryByIdDeep(const QString& entryId) const
{
    if (entryId.isEmpty())
        return nullptr;
    const QString resolved = repairCatalogEntryId(entryId);

    auto matchId = [&](const CatalogEntry& entry) {
        return entry.id == resolved || entry.id == entryId;
    };

    // Prefer install-offer snapshots: they keep FreeTP magnets/addons when steamidra is showcase.
    const QString groupKey = m_entryIdToOfferGroup.value(resolved);
    const QString groupKeyAlt =
        groupKey.isEmpty() && entryId != resolved ? m_entryIdToOfferGroup.value(entryId)
                                                  : QString();
    for (const QString& key : {groupKey, groupKeyAlt}) {
        if (key.isEmpty())
            continue;
        const auto offersIt = m_installOffers.constFind(key);
        if (offersIt == m_installOffers.cend())
            continue;
        for (const CatalogEntry& offer : offersIt.value()) {
            if (matchId(offer))
                return &offer;
        }
    }

    for (auto it = m_catalogBySource.cbegin(); it != m_catalogBySource.cend(); ++it) {
        for (const CatalogEntry& entry : it.value()) {
            if (matchId(entry))
                return &entry;
        }
    }
    return nullptr;
}

void CatalogController::requestCatalogLoad(const QString& sourceId)
{
    if (sourceId.isEmpty())
        return;
    if (m_catalogBySource.contains(sourceId)) {
        if (isSourceCacheFresh(sourceId))
            return;
        // Stale or polluted — drop and reload.
        m_catalogBySource.remove(sourceId);
        m_sourceLoadedAtMs.remove(sourceId);
    }
    if (m_loadingSourceIds.contains(sourceId)) {
        if (!m_catalogLoadQueue.contains(sourceId))
            m_catalogLoadQueue.append(sourceId);
        return;
    }

    m_loadingSourceIds.insert(sourceId);
    updateCatalogLoadingState();
    if (m_pluginHost && m_pluginHost->hasPlugin(sourceId)) {
        loadCatalogSourceNow(sourceId);
        return;
    }
    if (!m_catalogLoadQueue.contains(sourceId))
        m_catalogLoadQueue.append(sourceId);
    processCatalogLoadQueue();
}

void CatalogController::processCatalogLoadQueue()
{
    if (m_catalogHttpLoadActive)
        return;
    while (!m_catalogLoadQueue.isEmpty()) {
        const QString sourceId = m_catalogLoadQueue.takeFirst();
        if (isSourceCacheFresh(sourceId))
            continue;
        if (m_catalogBySource.contains(sourceId)) {
            m_catalogBySource.remove(sourceId);
            m_sourceLoadedAtMs.remove(sourceId);
        }
        if (m_loadingSourceIds.contains(sourceId) && m_pluginHost
            && m_pluginHost->hasPlugin(sourceId)) {
            return;
        }
        loadCatalogSourceNow(sourceId);
        return;
    }
    updateCatalogLoadingState();
}

void CatalogController::loadCatalogSourceNow(const QString& sourceId)
{
    struct DiskCatalogLoad {
        QVector<CatalogEntry> entries;
        QByteArray payloadSha;
        QByteArray etag;
        bool hadDiskPayload = false;
    };

    // Fast path: read+parse disk cache on a worker (not the UI thread), then revalidate.
    if (!m_catalogBySource.contains(sourceId)) {
        auto* watcher = new QFutureWatcher<DiskCatalogLoad>(this);
        m_inFlightPluginCatalogWatchers.append(watcher);
        const auto prepare = m_hooks.prepareEntry;
        connect(watcher, &QFutureWatcher<DiskCatalogLoad>::finished, this,
                [this, watcher, sourceId]() {
                    m_inFlightPluginCatalogWatchers.removeAll(watcher);
                    const DiskCatalogLoad loaded = watcher->result();
                    watcher->deleteLater();
                    if (!loaded.hadDiskPayload) {
                        // Fall through to plugin/network on the UI thread.
                        loadCatalogSourceNowFromNetwork(sourceId);
                        return;
                    }
                    if (!loaded.entries.isEmpty()) {
                        if (!loaded.payloadSha.isEmpty())
                            m_sourcePayloadSha.insert(sourceId, loaded.payloadSha);
                        storeCatalogForSource(sourceId, loaded.entries, /*prepareEntries=*/false);
                    }
                });
        watcher->setFuture(QtConcurrent::run([sourceId, prepare]() -> DiskCatalogLoad {
            DiskCatalogLoad out;
            QByteArray payload;
            QByteArray etag;
            if (!CatalogDiskCache::loadPayload(sourceId, &payload, &etag) || payload.isEmpty())
                return out;
            out.hadDiskPayload = true;
            out.etag = etag;
            out.payloadSha = CatalogDiskCache::payloadSha256(payload);
            // Plugin JSON uses schema + entries[]; parseCatalogFeed treats "entries" as Ryuu
            // and used to drop FreeTP magnets. Prefer the plugin parser when schema matches.
            QVector<CatalogEntry> entries;
            const QJsonDocument doc = QJsonDocument::fromJson(payload);
            if (doc.isObject()
                && doc.object()
                       .value(QStringLiteral("schema"))
                       .toString()
                       .startsWith(QStringLiteral("arachnel.plugin.catalog"))) {
                entries = parsePluginCatalogJson(payload, sourceId);
            }
            if (entries.isEmpty())
                entries = parseCatalogFeed(payload, sourceId);
            if (entries.isEmpty())
                entries = parsePluginCatalogJson(payload, sourceId);
            for (CatalogEntry& entry : entries) {
                entry.sourceId = sourceId;
                entry.id = repairCatalogEntryId(entry.id);
                if (prepare)
                    prepare(entry);
                else
                    prepareCatalogEntry(entry);
            }
            out.entries = std::move(entries);
            return out;
        }));
        return;
    }

    loadCatalogSourceNowFromNetwork(sourceId);
}

void CatalogController::loadCatalogSourceNowFromNetwork(const QString& sourceId)
{
    struct PluginCatalogLoad {
        QVector<CatalogEntry> entries;
        QByteArray payloadSha;
    };

    if (m_pluginHost) {
        if (m_pluginHost->hasPlugin(sourceId)) {
            auto* watcher = new QFutureWatcher<PluginCatalogLoad>(this);
            m_inFlightPluginCatalogWatchers.append(watcher);
            const auto prepare = m_hooks.prepareEntry;
            connect(watcher, &QFutureWatcher<PluginCatalogLoad>::finished, this,
                    [this, watcher, sourceId]() {
                        m_inFlightPluginCatalogWatchers.removeAll(watcher);
                        PluginCatalogLoad loaded = watcher->result();
                        watcher->deleteLater();
                        if (!loaded.payloadSha.isEmpty()
                            && m_sourcePayloadSha.value(sourceId) == loaded.payloadSha
                            && loaded.entries.isEmpty()) {
                            m_sourceLoadedAtMs.insert(sourceId,
                                                      QDateTime::currentMSecsSinceEpoch());
                            m_loadingSourceIds.remove(sourceId);
                            updateCatalogLoadingState();
                            processCatalogLoadQueue();
                            return;
                        }
                        if (!loaded.entries.isEmpty()) {
                            if (!loaded.payloadSha.isEmpty()
                                && m_sourcePayloadSha.value(sourceId) == loaded.payloadSha
                                && m_catalogBySource.contains(sourceId)) {
                                m_sourceLoadedAtMs.insert(sourceId,
                                                          QDateTime::currentMSecsSinceEpoch());
                                m_loadingSourceIds.remove(sourceId);
                                updateCatalogLoadingState();
                                processCatalogLoadQueue();
                                return;
                            }
                            if (!loaded.payloadSha.isEmpty())
                                m_sourcePayloadSha.insert(sourceId, loaded.payloadSha);
                            storeCatalogForSource(sourceId, std::move(loaded.entries),
                                                  /*prepareEntries=*/false);
                            return;
                        }
                        const QString url = m_sources->catalogUrlFor(sourceId);
                        if (!url.isEmpty()) {
                            m_catalogHttpLoadActive = true;
                            updateCatalogLoadingState();
                            QByteArray etag;
                            CatalogDiskCache::loadPayload(sourceId, nullptr, &etag);
                            m_loader->loadFeed(QUrl(url), sourceId, etag);
                            return;
                        }
                        m_loadingSourceIds.remove(sourceId);
                        if (m_activeSourceIds.contains(sourceId)) {
                            emit noticeRequested(QCoreApplication::translate(
                                "Core", "Catalog empty or unavailable: %1")
                                                     .arg(m_sources->nameForId(sourceId)));
                        }
                        rebuildMergedCatalog();
                    });
            PluginHost* host = m_pluginHost;
            const QByteArray expectedSha = m_sourcePayloadSha.value(sourceId);
            watcher->setFuture(QtConcurrent::run([host, sourceId, prepare, expectedSha]() {
                PluginCatalogLoad out;
                QByteArray payload = host->loadPluginCatalogPayload(sourceId, &out.payloadSha);
                if (payload.isEmpty())
                    return out;
                if (!expectedSha.isEmpty() && out.payloadSha == expectedSha)
                    return out;
                out.entries = parsePluginCatalogJson(payload, sourceId);
                payload.clear();
                for (CatalogEntry& entry : out.entries) {
                    entry.sourceId = sourceId;
                    entry.id = repairCatalogEntryId(entry.id);
                    if (prepare)
                        prepare(entry);
                    else
                        prepareCatalogEntry(entry);
                }
                return out;
            }));
            return;
        }
    }

    const QString url = m_sources->catalogUrlFor(sourceId);
    if (!url.isEmpty()) {
        m_catalogHttpLoadActive = true;
        updateCatalogLoadingState();
        QByteArray etag;
        CatalogDiskCache::loadPayload(sourceId, nullptr, &etag);
        m_loader->loadFeed(QUrl(url), sourceId, etag);
        return;
    }
    m_loadingSourceIds.remove(sourceId);
    emit noticeRequested(QCoreApplication::translate("Core", "No catalog URL configured for source %1")
                             .arg(sourceId));
    rebuildMergedCatalog();
}

void CatalogController::revalidateCatalogSource(const QString& sourceId, const QByteArray& etag)
{
    Q_UNUSED(etag);
    struct PluginCatalogLoad {
        QVector<CatalogEntry> entries;
        QByteArray payloadSha;
    };

    if (m_pluginHost && m_pluginHost->hasPlugin(sourceId)) {
        auto* watcher = new QFutureWatcher<PluginCatalogLoad>(this);
        m_inFlightPluginCatalogWatchers.append(watcher);
        const auto prepare = m_hooks.prepareEntry;
        const QByteArray expectedSha = m_sourcePayloadSha.value(sourceId);
        connect(watcher, &QFutureWatcher<PluginCatalogLoad>::finished, this,
                [this, watcher, sourceId]() {
                    m_inFlightPluginCatalogWatchers.removeAll(watcher);
                    PluginCatalogLoad loaded = watcher->result();
                    watcher->deleteLater();
                    if (!loaded.payloadSha.isEmpty()
                        && m_sourcePayloadSha.value(sourceId) == loaded.payloadSha
                        && loaded.entries.isEmpty()) {
                        m_sourceLoadedAtMs.insert(sourceId, QDateTime::currentMSecsSinceEpoch());
                        return;
                    }
                    if (loaded.entries.isEmpty())
                        return;
                    if (!loaded.payloadSha.isEmpty()
                        && m_sourcePayloadSha.value(sourceId) == loaded.payloadSha) {
                        m_sourceLoadedAtMs.insert(sourceId, QDateTime::currentMSecsSinceEpoch());
                        return;
                    }
                    if (!loaded.payloadSha.isEmpty())
                        m_sourcePayloadSha.insert(sourceId, loaded.payloadSha);
                    storeCatalogForSource(sourceId, std::move(loaded.entries),
                                          /*prepareEntries=*/false);
                });
        PluginHost* host = m_pluginHost;
        watcher->setFuture(QtConcurrent::run([host, sourceId, prepare, expectedSha]() {
            PluginCatalogLoad out;
            QByteArray payload = host->loadPluginCatalogPayload(sourceId, &out.payloadSha);
            if (payload.isEmpty())
                return out;
            if (!expectedSha.isEmpty() && out.payloadSha == expectedSha)
                return out;
            out.entries = parsePluginCatalogJson(payload, sourceId);
            payload.clear();
            for (CatalogEntry& entry : out.entries) {
                entry.sourceId = sourceId;
                entry.id = repairCatalogEntryId(entry.id);
                if (prepare)
                    prepare(entry);
                else
                    prepareCatalogEntry(entry);
            }
            return out;
        }));
        return;
    }

    const QString url = m_sources->catalogUrlFor(sourceId);
    if (url.isEmpty())
        return;
    // Keep chrome idle when we already showed disk cache; only block UI on cold load.
    if (!m_catalogBySource.contains(sourceId)) {
        m_catalogHttpLoadActive = true;
        m_loadingSourceIds.insert(sourceId);
        updateCatalogLoadingState();
    }
    m_loader->loadFeed(QUrl(url), sourceId, etag);
}

void CatalogController::updateCatalogLoadingState()
{
    emit catalogLoadingChanged(catalogLoading());
}

void CatalogController::setCatalogStatus(const QString& status)
{
    if (m_catalogStatus == status)
        return;
    m_catalogStatus = status;
    emit catalogStatusChanged(status);
}

void CatalogController::refreshCatalog(const QString& sourceId)
{
    m_catalogBySource.remove(sourceId);
    m_sourceLoadedAtMs.remove(sourceId);
    m_sourcePayloadSha.remove(sourceId);
    m_catalogCounts.remove(sourceId);
    CatalogDiskCache::remove(sourceId);
    emit catalogCountsChanged();
    m_loadingSourceIds.remove(sourceId);
    m_catalogLoadQueue.removeAll(sourceId);
    if (m_pluginHost) {
        if (ISourcePlugin* plugin = m_pluginHost->plugin(sourceId))
            plugin->resetCatalogCache();
    }
    if (m_activeSourceIds.contains(sourceId))
        requestCatalogLoad(sourceId);
}

void CatalogController::refreshSelectedCatalogs()
{
    for (const QString& id : m_activeSourceIds)
        refreshCatalog(id);
}

void CatalogController::selectAllEnabledSources()
{
    QStringList enabled;
    for (const SourcePluginInfo& source : m_sources->plugins()) {
        if (source.enabled)
            enabled.append(source.id);
    }
    if (enabled == m_activeSourceIds)
        return;
    m_activeSourceIds = enabled;
    emit activeCatalogSourcesChanged();
    rebuildMergedCatalog();
}

void CatalogController::setActiveCatalogSource(const QString& sourceId)
{
    // Unified catalog mode: selecting a single source still keeps all enabled sources active.
    Q_UNUSED(sourceId);
    selectAllEnabledSources();
}

void CatalogController::toggleCatalogSource(const QString& sourceId)
{
    Q_UNUSED(sourceId);
    selectAllEnabledSources();
}

void CatalogController::applyCatalogSearch(const QString& query)
{
    m_activeQuery = query;
    if (m_hooks.applyFilter)
        m_hooks.applyFilter(query);
}

void CatalogController::pruneDisabledCatalogSources()
{
    selectAllEnabledSources();
}

void CatalogController::selectCatalogSource(const QString& id, const QString& query)
{
    Q_UNUSED(id);
    m_activeQuery = query;
    selectAllEnabledSources();
    if (!query.isEmpty())
        applyCatalogSearch(query);
}

void CatalogController::clearCatalogView()
{
    m_activeSourceIds.clear();
    m_activeQuery.clear();
    {
        if (m_mergedCacheLock) {
            QWriteLocker locker(m_mergedCacheLock);
            m_mergedCache->clear();
        } else {
            m_mergedCache->clear();
        }
    }
    m_catalog->clear();
    m_installOffers.clear();
    m_entryIdToOfferGroup.clear();
    emit activeCatalogSourcesChanged();
    setCatalogStatus({});
    updateCatalogLoadingState();
}

void CatalogController::invalidateSourceCatalog(const QString& id)
{
    m_catalogBySource.remove(id);
    m_sourceLoadedAtMs.remove(id);
    m_sourcePayloadSha.remove(id);
    m_catalogCounts.remove(id);
    emit catalogCountsChanged();
    if (m_activeSourceIds.contains(id))
        rebuildMergedCatalog();
}

void CatalogController::prefetchCatalogCounts()
{
    m_catalogPrefetchQueue.clear();
    for (const SourcePluginInfo& source : m_sources->plugins()) {
        if (!source.enabled || m_catalogBySource.contains(source.id)
            || m_loadingSourceIds.contains(source.id))
            continue;
        m_catalogPrefetchQueue.append(m_pluginHost && m_pluginHost->hasPlugin(source.id)
                                          ? source.id
                                          : QStringLiteral("url:%1").arg(source.id));
    }
    startNextCatalogPrefetch();
}

void CatalogController::prefetchPluginCatalogCount(const QString& sourceId)
{
    if (!m_pluginHost || !m_pluginHost->hasPlugin(sourceId) || m_catalogBySource.contains(sourceId)
        || m_loadingSourceIds.contains(sourceId)) {
        startNextCatalogPrefetch();
        return;
    }
    auto* watcher = new QFutureWatcher<QVector<CatalogEntry>>(this);
    m_inFlightPluginCatalogWatchers.append(watcher);
    connect(watcher, &QFutureWatcher<QVector<CatalogEntry>>::finished, this,
            [this, watcher, sourceId]() {
                m_inFlightPluginCatalogWatchers.removeAll(watcher);
                const QVector<CatalogEntry> entries = watcher->result();
                watcher->deleteLater();
                if (!entries.isEmpty() && !m_catalogBySource.contains(sourceId)) {
                    m_catalogBySource.insert(sourceId, entries);
                    m_sourceLoadedAtMs.insert(sourceId, QDateTime::currentMSecsSinceEpoch());
                    m_catalogCounts.insert(sourceId, entries.size());
                    emit catalogCountsChanged();
                }
                startNextCatalogPrefetch();
            });
    PluginHost* host = m_pluginHost;
    watcher->setFuture(QtConcurrent::run([host, sourceId]() {
        return host->loadPluginCatalog(sourceId);
    }));
}

void CatalogController::startNextCatalogPrefetch()
{
    if (m_catalogPrefetchQueue.isEmpty())
        return;
    const QString item = m_catalogPrefetchQueue.takeFirst();
    if (!item.startsWith(QStringLiteral("url:"))) {
        prefetchPluginCatalogCount(item);
        return;
    }
    const QString sourceId = item.mid(4);
    const QString url = m_sources->catalogUrlFor(sourceId);
    if (url.isEmpty()) {
        startNextCatalogPrefetch();
        return;
    }
    m_catalogCounts.insert(sourceId, -1);
    emit catalogCountsChanged();
    m_probeLoader->loadFeed(QUrl(url), QStringLiteral("count:%1").arg(sourceId));
}

void CatalogController::waitForInFlightPluginCatalogLoads()
{
    m_catalogPrefetchQueue.clear();

    // List holds QFutureWatcher<PluginCatalogLoad|DiskCatalogLoad|QVector<...>>.
    // Waiting only on QVector skipped real plugin loads (unload race, #29).
    const QList<QObject*> watchers = m_inFlightPluginCatalogWatchers;
    m_inFlightPluginCatalogWatchers.clear();
    for (QObject* obj : watchers) {
        auto* base = dynamic_cast<QFutureWatcherBase*>(obj);
        if (!base)
            continue;
        QObject::disconnect(obj, nullptr, this, nullptr);
        base->waitForFinished();
        obj->deleteLater();
    }

    if (!m_loadingSourceIds.isEmpty()) {
        m_loadingSourceIds.clear();
        m_catalogHttpLoadActive = false;
        updateCatalogLoadingState();
    }
}

bool CatalogController::hasInFlightPluginCatalogLoads() const
{
    return !m_inFlightPluginCatalogWatchers.isEmpty();
}

} // namespace arachnel::core
