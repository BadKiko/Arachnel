#include "catalog_controller.h"

#include "catalog_feed_loader.h"
#include "catalog_model.h"
#include "catalog_parser.h"
#include "plugin_host.h"
#include "source_plugin_model.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QUrl>
#include <QtConcurrent>

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
{
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
    return !m_loadingSourceIds.isEmpty() || m_catalogHttpLoadActive;
}
QString CatalogController::catalogStatus() const { return m_catalogStatus; }
QString CatalogController::activeCatalogSourceId() const { return m_activeSourceIds.value(0); }
QStringList CatalogController::activeCatalogSourceIds() const { return m_activeSourceIds; }
int CatalogController::catalogEntryCount(const QString& id) const { return id.isEmpty() ? -1 : m_catalogBySource.contains(id) ? m_catalogBySource.value(id).size() : m_catalogCounts.value(id, -1); }
bool CatalogController::isCatalogSourceSelected(const QString& id) const { return m_activeSourceIds.contains(id); }
const QHash<QString, QVector<CatalogEntry>>& CatalogController::catalogsBySource() const { return m_catalogBySource; }

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
        m_mergedCache->clear();
        m_catalog->clear();
        if (m_hooks.rebuildIdIndex)
            m_hooks.rebuildIdIndex();
        if (m_hooks.rebuildGenres)
            m_hooks.rebuildGenres();
        setCatalogStatus({});
        updateCatalogLoadingState();
        return;
    }

    QVector<CatalogEntry> merged;
    for (const QString& sourceId : m_activeSourceIds) {
        const SourcePluginInfo* source = m_sources->pluginById(sourceId);
        if (!source || !source->enabled)
            continue;
        if (m_catalogBySource.contains(sourceId)) {
            const QVector<CatalogEntry>& entries = m_catalogBySource.value(sourceId);
            merged.reserve(merged.size() + entries.size());
            merged += entries;
        } else if (!m_loadingSourceIds.contains(sourceId)) {
            requestCatalogLoad(sourceId);
        }
    }

    for (CatalogEntry& entry : merged)
        entry.id = repairCatalogEntryId(entry.id);
    deduplicateCatalogEntries(merged);
    *m_mergedCache = std::move(merged);
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

    if (m_activeSourceIds.size() == 1) {
        const SourcePluginInfo* source = m_sources->pluginById(m_activeSourceIds.first());
        setCatalogStatus(QCoreApplication::translate("Core", "%1 · %2 games")
                             .arg(source ? source->name : m_activeSourceIds.first())
                             .arg(m_catalog->count()));
    } else {
        setCatalogStatus(QCoreApplication::translate("Core", "%1 sources · %2 games")
                             .arg(m_activeSourceIds.size())
                             .arg(m_catalog->count()));
    }
    updateCatalogLoadingState();
    if (m_hooks.catalogReady)
        m_hooks.catalogReady();
}

void CatalogController::requestCatalogLoad(const QString& sourceId)
{
    if (sourceId.isEmpty())
        return;
    if (m_catalogBySource.contains(sourceId)) {
        if (!catalogCacheHasPollutedIds(m_catalogBySource.value(sourceId)))
            return;
        m_catalogBySource.remove(sourceId);
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
        if (m_catalogBySource.contains(sourceId)
            && !catalogCacheHasPollutedIds(m_catalogBySource.value(sourceId))) {
            continue;
        }
        if (m_catalogBySource.contains(sourceId))
            m_catalogBySource.remove(sourceId);
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
        if (ISourcePlugin* plugin = m_pluginHost->plugin(sourceId)) {
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
            watcher->setFuture(QtConcurrent::run([plugin]() { return plugin->catalog(); }));
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

void CatalogController::refreshSelectedCatalogs() { for (const QString& id : m_activeSourceIds) refreshCatalog(id); }
void CatalogController::setActiveCatalogSource(const QString& sourceId) {
    const QString id = sourceId.trimmed();
    if (id.isEmpty()) { clearCatalogView(); return; }
    if (m_activeSourceIds == QStringList {id}) { rebuildMergedCatalog(); return; }
    m_activeSourceIds = {id}; emit activeCatalogSourcesChanged();
    m_catalogBySource.contains(id) ? rebuildMergedCatalog() : requestCatalogLoad(id);
}
void CatalogController::toggleCatalogSource(const QString& sourceId) {
    if (sourceId.isEmpty()) return;
    if (m_activeSourceIds.contains(sourceId)) {
        if (m_activeSourceIds.size() <= 1) return;
        m_activeSourceIds.removeAll(sourceId); emit activeCatalogSourcesChanged(); rebuildMergedCatalog(); return;
    }
    const SourcePluginInfo* source = m_sources->pluginById(sourceId);
    if (!source || !source->enabled) return;
    m_activeSourceIds.append(sourceId); emit activeCatalogSourcesChanged();
    m_catalogBySource.contains(sourceId) ? rebuildMergedCatalog() : requestCatalogLoad(sourceId);
}
void CatalogController::applyCatalogSearch(const QString& query) {
    m_activeQuery = query; if (m_hooks.applyFilter) m_hooks.applyFilter(query);
}
void CatalogController::pruneDisabledCatalogSources() {
    QStringList valid;
    for (const QString& id : m_activeSourceIds) if (m_sources->isSourceEnabled(id)) valid.append(id);
    if (valid.isEmpty()) {
        const QString first = m_sources->firstEnabledId();
        if (first.isEmpty()) {
            if (m_activeSourceIds.isEmpty()) return;
            m_activeSourceIds.clear(); emit activeCatalogSourcesChanged(); rebuildMergedCatalog(); return;
        }
        valid = {first};
    }
    if (valid == m_activeSourceIds) return;
    m_activeSourceIds = valid; emit activeCatalogSourcesChanged();
    m_catalogBySource.contains(valid.first()) ? rebuildMergedCatalog() : requestCatalogLoad(valid.first());
}
void CatalogController::selectCatalogSource(const QString& id, const QString& query) {
    if (id.isEmpty()) return;
    m_activeQuery = query; setActiveCatalogSource(id); if (!query.isEmpty()) applyCatalogSearch(query);
}
void CatalogController::clearCatalogView() {
    m_activeSourceIds.clear(); m_activeQuery.clear(); m_mergedCache->clear(); m_catalog->clear();
    emit activeCatalogSourcesChanged(); setCatalogStatus({}); updateCatalogLoadingState();
}
void CatalogController::invalidateSourceCatalog(const QString& id) {
    m_catalogBySource.remove(id); m_catalogCounts.remove(id); emit catalogCountsChanged();
    if (m_activeSourceIds.contains(id)) rebuildMergedCatalog();
}
void CatalogController::prefetchCatalogCounts()
{
    m_catalogPrefetchQueue.clear();
    for (const SourcePluginInfo& source : m_sources->plugins()) {
        if (!source.enabled || m_catalogBySource.contains(source.id))
            continue;
        m_catalogPrefetchQueue.append(m_pluginHost && m_pluginHost->hasPlugin(source.id)
                                          ? source.id
                                          : QStringLiteral("url:%1").arg(source.id));
    }
    startNextCatalogPrefetch();
}
void CatalogController::prefetchPluginCatalogCount(const QString& sourceId)
{
    ISourcePlugin* plugin = m_pluginHost ? m_pluginHost->plugin(sourceId) : nullptr;
    if (!plugin) {
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
                if (!entries.isEmpty()) {
                    m_catalogBySource.insert(sourceId, entries);
                    m_catalogCounts.insert(sourceId, entries.size());
                    emit catalogCountsChanged();
                }
                startNextCatalogPrefetch();
            });
    watcher->setFuture(QtConcurrent::run([plugin]() { return plugin->catalog(); }));
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
    // Snapshot — finished handlers may mutate the list while we wait.
    const QList<QObject*> watchers = m_inFlightPluginCatalogWatchers;
    for (QObject* obj : watchers) {
        auto* watcher = dynamic_cast<QFutureWatcher<QVector<CatalogEntry>>*>(obj);
        if (!watcher)
            continue;
        watcher->waitForFinished();
    }
}

} // namespace arachnel::core
