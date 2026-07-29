#pragma once

#include "catalog_types.h"

#include <functional>
#include <optional>

#include <QHash>
#include <QList>
#include <QObject>
#include <QReadWriteLock>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVector>
#include <QtGlobal>

class QTimer;

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
    static constexpr qint64 kCatalogCacheTtlMs = 15 * 60 * 1000;

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

    void setMergedCacheLock(QReadWriteLock* lock) { m_mergedCacheLock = lock; }

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
    void selectAllEnabledSources();
    void applyCatalogSearch(const QString& query);
    void pruneDisabledCatalogSources();
    void selectCatalogSource(const QString& sourceId, const QString& query = {});
    void clearCatalogView();
    void invalidateSourceCatalog(const QString& sourceId);
    void prefetchCatalogCounts();
    /** Block until in-flight plugin->catalog() futures finish (call before unloading DLLs). */
    void waitForInFlightPluginCatalogLoads();
    bool hasInFlightPluginCatalogLoads() const;

    /** Install offers for a merged catalog entry (same steamAppId / title across sources). */
    QVariantList installOffersForEntry(const QString& entryId) const;
    /** Resolve a specific source offer (may differ from the merged showcase entry). */
    std::optional<CatalogEntry> resolveInstallOffer(const QString& entryId,
                                                    const QString& sourceId) const;

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
    bool isSourceCacheFresh(const QString& sourceId) const;
    void refreshStaleSources();
    void revalidateCatalogSource(const QString& sourceId, const QByteArray& etag = {});
    static QString offerGroupKey(const CatalogEntry& entry);
    static QString normalizeTitleKey(const QString& title);
    static int showcaseScore(const CatalogEntry& entry);

    CatalogModel* m_catalog = nullptr;
    SourcePluginModel* m_sources = nullptr;
    PluginHost* m_pluginHost = nullptr;
    QVector<CatalogEntry>* m_mergedCache = nullptr;
    QReadWriteLock* m_mergedCacheLock = nullptr;
    Hooks m_hooks;
    CatalogFeedLoader* m_loader = nullptr;
    CatalogFeedLoader* m_probeLoader = nullptr;
    QTimer* m_cacheTtlTimer = nullptr;
    QHash<QString, QVector<CatalogEntry>> m_catalogBySource;
    QHash<QString, qint64> m_sourceLoadedAtMs;
    QHash<QString, QByteArray> m_sourcePayloadSha;
    /** groupKey -> all source offers for that game. */
    QHash<QString, QVector<CatalogEntry>> m_installOffers;
    /** Any known entry id (showcase or offer) -> groupKey. */
    QHash<QString, QString> m_entryIdToOfferGroup;
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
