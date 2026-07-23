#pragma once

#include "catalog_types.h"

#include <functional>

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

class CatalogFeedLoader;
class CatalogModel;
class PluginHost;
class SourcePluginModel;

// Catalog loading, cache merging, source selection, and count prefetching.
class CatalogController : public QObject
{
    Q_OBJECT

public:
    struct Hooks {
        std::function<void(CatalogEntry&)> prepareEntry;
        std::function<void(QVector<CatalogEntry>&, const QStringList&, const QString&)> mergedEntriesReady;
        std::function<void()> rebuildIdIndex;
        std::function<void(const QString&)> applyFilter;
        std::function<void()> rebuildGenres;
        std::function<void()> warmCovers;
        std::function<void()> catalogReady;
    };

    CatalogController(CatalogModel* catalog, SourcePluginModel* sources, PluginHost* pluginHost,
                      QVector<CatalogEntry>* mergedCache, Hooks hooks = {}, QObject* parent = nullptr);

    bool catalogLoading() const;
    QString catalogStatus() const;
    QString activeCatalogSourceId() const;
    QStringList activeCatalogSourceIds() const;
    int catalogEntryCount(const QString& sourceId) const;
    bool isCatalogSourceSelected(const QString& sourceId) const;
    const QHash<QString, QVector<CatalogEntry>>& catalogsBySource() const;

    void requestCatalogLoad(const QString& sourceId);
    void processCatalogLoadQueue();
    void loadCatalogSourceNow(const QString& sourceId);
    void commitCatalogLoad(const QString& sourceId, QVector<CatalogEntry> entries);
    void storeCatalogForSource(const QString& sourceId, QVector<CatalogEntry> entries);
    void rebuildMergedCatalog();
    void refreshCatalog(const QString& sourceId);
    void refreshSelectedCatalogs();
    void setActiveCatalogSource(const QString& sourceId);
    void toggleCatalogSource(const QString& sourceId);
    void applyCatalogSearch(const QString& query);
    void pruneDisabledCatalogSources();
    void selectCatalogSource(const QString& sourceId, const QString& query = {});
    void clearCatalogView();
    void invalidateSourceCatalog(const QString& sourceId);
    void prefetchCatalogCounts();
    /** Block until in-flight plugin->catalog() futures finish (call before unloading DLLs). */
    void waitForInFlightPluginCatalogLoads();

signals:
    void catalogLoadingChanged(bool loading);
    void catalogStatusChanged(const QString& status);
    void activeCatalogSourcesChanged();
    void catalogCountsChanged();
    void noticeRequested(const QString& message);

private:
    void updateCatalogLoadingState();
    void setCatalogStatus(const QString& status);
    void prefetchPluginCatalogCount(const QString& sourceId);
    void startNextCatalogPrefetch();
    static void normalizeCatalogSourceIds(QVector<CatalogEntry>& entries, const QString& sourceId);

    CatalogModel* m_catalog = nullptr;
    SourcePluginModel* m_sources = nullptr;
    PluginHost* m_pluginHost = nullptr;
    QVector<CatalogEntry>* m_mergedCache = nullptr;
    Hooks m_hooks;
    CatalogFeedLoader* m_loader = nullptr;
    CatalogFeedLoader* m_probeLoader = nullptr;
    QHash<QString, QVector<CatalogEntry>> m_catalogBySource;
    QHash<QString, int> m_catalogCounts;
    QStringList m_catalogPrefetchQueue;
    QStringList m_activeSourceIds;
    QStringList m_catalogLoadQueue;
    QSet<QString> m_loadingSourceIds;
    QString m_activeQuery;
    QString m_catalogStatus;
    bool m_catalogHttpLoadActive = false;
    QList<QObject*> m_inFlightPluginCatalogWatchers;
};

} // namespace arachnel::core
