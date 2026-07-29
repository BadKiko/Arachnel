#include "catalog_controller.h"

#include "catalog_feed_loader.h"
#include "catalog_model.h"
#include "catalog_parser.h"
#include "plugin_host.h"
#include "source_plugin_model.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFutureWatcher>
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
            [this](const QString& sourceId, const QVector<CatalogEntry>& entries) {
                m_catalogHttpLoadActive = false;
                storeCatalogForSource(sourceId, entries);
                processCatalogLoadQueue();
            });
    connect(m_loader, &CatalogFeedLoader::feedFailed, this,
            [this](const QString& sourceId, const QString& error) {
                m_catalogHttpLoadActive = false;
                m_loadingSourceIds.remove(sourceId);
                if (m_activeSourceIds.contains(sourceId)) {
                    emit noticeRequested(
                        QCoreApplication::translate("Core", "Catalog error: %1").arg(error));
                }
                rebuildMergedCatalog();
                processCatalogLoadQueue();
            });
    connect(m_probeLoader, &CatalogFeedLoader::feedLoaded, this,
            [this](const QString& tag, const QVector<CatalogEntry>& entries) {
                if (!tag.startsWith(QStringLiteral("count:")))
                    return;
                const QString sourceId = tag.mid(6);
                m_catalogCounts.insert(sourceId, entries.size());
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
    if (!entry.coverUrl.isEmpty())
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
        refreshCatalog(sourceId);
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

void CatalogController::storeCatalogForSource(const QString& sourceId, QVector<CatalogEntry> entries)
{
    normalizeCatalogSourceIds(entries, sourceId);
    for (CatalogEntry& entry : entries) {
        prepareCatalogEntry(entry);
        if (m_hooks.prepareEntry)
            m_hooks.prepareEntry(entry);
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
    m_installOffers.clear();
    m_entryIdToOfferGroup.clear();

    if (m_activeSourceIds.isEmpty()) {
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

    QHash<QString, QVector<CatalogEntry>> groups;
    QStringList groupOrder;

    for (const QString& sourceId : m_activeSourceIds) {
        const SourcePluginInfo* source = m_sources->pluginById(sourceId);
        if (!source || !source->enabled)
            continue;
        if (m_catalogBySource.contains(sourceId)) {
            const QVector<CatalogEntry>& entries = m_catalogBySource.value(sourceId);
            for (const CatalogEntry& entry : entries) {
                CatalogEntry copy = entry;
                copy.id = repairCatalogEntryId(copy.id);
                const QString key = offerGroupKey(copy);
                if (!groups.contains(key))
                    groupOrder.append(key);
                groups[key].append(std::move(copy));
            }
        } else if (!m_loadingSourceIds.contains(sourceId)) {
            requestCatalogLoad(sourceId);
        }
    }

    QVector<CatalogEntry> merged;
    merged.reserve(groups.size());
    for (const QString& key : groupOrder) {
        QVector<CatalogEntry> offers = groups.value(key);
        if (offers.isEmpty())
            continue;

        // Prefer distinct sources; keep first occurrence per sourceId.
        QVector<CatalogEntry> uniqueOffers;
        QSet<QString> seenSources;
        for (const CatalogEntry& offer : offers) {
            if (seenSources.contains(offer.sourceId))
                continue;
            seenSources.insert(offer.sourceId);
            uniqueOffers.append(offer);
        }
        offers = std::move(uniqueOffers);
        m_installOffers.insert(key, offers);

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
        // Prefer a stable steam-* id when available for discovery matching.
        for (const CatalogEntry& offer : offers) {
            if (offer.id.startsWith(QStringLiteral("steam-")) && !offer.steamAppId.isEmpty()) {
                showcase.id = offer.id;
                showcase.steamAppId = offer.steamAppId;
                if (!offer.coverUrl.isEmpty())
                    showcase.coverUrl = offer.coverUrl;
                break;
            }
        }
        // If showcase itself has no steamAppId, steal it from any offer that does.
        if (showcase.steamAppId.isEmpty()) {
            for (const CatalogEntry& offer : offers) {
                if (!offer.steamAppId.isEmpty()) {
                    showcase.steamAppId = offer.steamAppId;
                    break;
                }
            }
        }

        m_entryIdToOfferGroup.insert(showcase.id, key);
        for (const CatalogEntry& offer : offers)
            m_entryIdToOfferGroup.insert(offer.id, key);

        merged.append(std::move(showcase));
    }

    {
        if (m_mergedCacheLock) {
            QWriteLocker locker(m_mergedCacheLock);
            *m_mergedCache = std::move(merged);
        } else {
            *m_mergedCache = std::move(merged);
        }
    }
    if (m_hooks.mergedEntriesReady)
        m_hooks.mergedEntriesReady(*m_mergedCache, m_activeSourceIds, m_activeQuery);
    if (m_hooks.rebuildIdIndex)
        m_hooks.rebuildIdIndex();
    if (m_hooks.applyFilter)
        m_hooks.applyFilter(m_activeQuery);
    if (m_hooks.rebuildGenres)
        m_hooks.rebuildGenres();
    if (m_hooks.warmCovers)
        m_hooks.warmCovers();

    const int sourceCount = m_activeSourceIds.size();
    if (sourceCount == 1) {
        const SourcePluginInfo* source = m_sources->pluginById(m_activeSourceIds.first());
        setCatalogStatus(QCoreApplication::translate("Core", "%1 · %2 games")
                             .arg(source ? source->name : m_activeSourceIds.first())
                             .arg(m_catalog->count()));
    } else {
        setCatalogStatus(QCoreApplication::translate("Core", "%1 sources · %2 games")
                             .arg(sourceCount)
                             .arg(m_catalog->count()));
    }
    updateCatalogLoadingState();
    if (m_hooks.catalogReady)
        m_hooks.catalogReady();
}

QVariantList CatalogController::installOffersForEntry(const QString& entryId) const
{
    QVariantList out;
    const QString groupKey = m_entryIdToOfferGroup.value(entryId);
    if (groupKey.isEmpty())
        return out;

    const QVector<CatalogEntry> offers = m_installOffers.value(groupKey);
    for (const CatalogEntry& offer : offers) {
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
    }
    return out;
}

std::optional<CatalogEntry> CatalogController::resolveInstallOffer(const QString& entryId,
                                                                   const QString& sourceId) const
{
    const QString groupKey = m_entryIdToOfferGroup.value(entryId);
    if (groupKey.isEmpty())
        return std::nullopt;

    const QVector<CatalogEntry> offers = m_installOffers.value(groupKey);
    for (const CatalogEntry& offer : offers) {
        if (offer.sourceId == sourceId)
            return offer;
    }
    if (sourceId.isEmpty() && offers.size() == 1)
        return offers.first();
    return std::nullopt;
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
    if (m_pluginHost) {
        if (m_pluginHost->hasPlugin(sourceId)) {
            auto* watcher = new QFutureWatcher<QVector<CatalogEntry>>(this);
            m_inFlightPluginCatalogWatchers.append(watcher);
            connect(watcher, &QFutureWatcher<QVector<CatalogEntry>>::finished, this,
                    [this, watcher, sourceId]() {
                        m_inFlightPluginCatalogWatchers.removeAll(watcher);
                        const QVector<CatalogEntry> entries = watcher->result();
                        watcher->deleteLater();
                        if (!entries.isEmpty()) {
                            storeCatalogForSource(sourceId, entries);
                            return;
                        }
                        const QString url = m_sources->catalogUrlFor(sourceId);
                        if (!url.isEmpty()) {
                            m_catalogHttpLoadActive = true;
                            updateCatalogLoadingState();
                            m_loader->loadFeed(QUrl(url), sourceId);
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
            watcher->setFuture(QtConcurrent::run([host, sourceId]() {
                return host->loadPluginCatalog(sourceId);
            }));
            return;
        }
    }

    const QString url = m_sources->catalogUrlFor(sourceId);
    if (!url.isEmpty()) {
        m_catalogHttpLoadActive = true;
        updateCatalogLoadingState();
        m_loader->loadFeed(QUrl(url), sourceId);
        return;
    }
    m_loadingSourceIds.remove(sourceId);
    emit noticeRequested(QCoreApplication::translate("Core", "No catalog URL configured for source %1")
                             .arg(sourceId));
    rebuildMergedCatalog();
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
    m_catalogCounts.remove(sourceId);
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

    const QList<QObject*> watchers = m_inFlightPluginCatalogWatchers;
    for (QObject* obj : watchers) {
        auto* watcher = dynamic_cast<QFutureWatcher<QVector<CatalogEntry>>*>(obj);
        if (!watcher)
            continue;
        QObject::disconnect(watcher, &QFutureWatcher<QVector<CatalogEntry>>::finished, this,
                            nullptr);
        watcher->waitForFinished();
        m_inFlightPluginCatalogWatchers.removeAll(watcher);
        watcher->deleteLater();
    }
}

bool CatalogController::hasInFlightPluginCatalogLoads() const
{
    return !m_inFlightPluginCatalogWatchers.isEmpty();
}

} // namespace arachnel::core
