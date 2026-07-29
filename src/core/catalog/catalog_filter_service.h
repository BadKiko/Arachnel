#pragma once

#include "catalog_model.h"
#include "catalog_types.h"

#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <atomic>
#include <cstdint>

namespace arachnel::core {

/** Catalog filter + presentation state (type/size/recency/genre/play-mode). */
class CatalogFilterService : public QObject
{
    Q_OBJECT
public:
    explicit CatalogFilterService(CatalogModel* model, QObject* parent = nullptr);

    void setCache(QVector<CatalogEntry>* cache) { m_cache = cache; }
    /** Shared with CatalogController: write on merge/clear, read while snapshotting for filter. */
    void setCacheLock(QReadWriteLock* lock) { m_cacheLock = lock; }
    void setActiveQuery(const QString& query) { m_activeQuery = query; }
    QString activeQuery() const { return m_activeQuery; }

    int typeFilter() const { return m_typeFilter; }
    void setTypeFilter(int filter);
    int sizeFilter() const { return m_sizeFilter; }
    void setSizeFilter(int filter);
    int recencyFilter() const { return m_recencyFilter; }
    void setRecencyFilter(int filter);
    bool hasAddonsFilter() const { return m_hasAddonsFilter; }
    void setHasAddonsFilter(bool enabled);
    QString genreFilter() const { return m_genreFilter; }
    void setGenreFilter(const QString& genre);
    int playModeFilter() const { return m_playModeFilter; }
    void setPlayModeFilter(int filter);

    int activeFilterCount() const;
    QStringList availableGenres() const { return m_availableGenres; }

    void clearFilters();
    void setFilters(int typeFilter, int sizeFilter, int recencyFilter, bool hasAddonsFilter,
                    const QString& genreFilter, int playModeFilter = 0);
    void applyPresentation(int sortMode, int typeFilter, int sizeFilter, int recencyFilter,
                           bool hasAddonsFilter, const QString& genreFilter,
                           int playModeFilter = 0);

    void applyFilter(const QString& query);
    void rebuildAvailableGenres();
    void scheduleRefilter();

signals:
    void filtersChanged();
    void availableGenresChanged();

private:
    struct FilterSnapshot {
        QString needle;
        qint64 cutoffDay = 0;
        int typeFilter = -1;
        int sizeFilter = 0;
        int recencyFilter = 0;
        bool hasAddonsFilter = false;
        QString genreFilter;
        int playModeFilter = 0;
        int sortMode = 0;
        bool anySideFilter = false;
    };

    static bool entryMatches(const CatalogEntry& entry, const FilterSnapshot& snap);
    void notifyFiltersChanged();
    void applyFilterResult(quint64 generation, QVector<int> indices, qint64 elapsedMs, int cacheSize,
                           const QString& needle);
    void applyGenreResult(quint64 generation, QStringList genres);

    CatalogModel* m_model = nullptr;
    QVector<CatalogEntry>* m_cache = nullptr;
    QReadWriteLock* m_cacheLock = nullptr;
    QString m_activeQuery;
    QString m_filterNeedle;
    qint64 m_filterCutoffDay = 0;
    QStringList m_availableGenres;
    QTimer* m_refilterTimer = nullptr;
    std::atomic<quint64> m_filterGeneration{0};
    std::atomic<quint64> m_genreGeneration{0};
    int m_typeFilter = -1;
    int m_sizeFilter = 0;
    int m_recencyFilter = 0;
    bool m_hasAddonsFilter = false;
    QString m_genreFilter;
    int m_playModeFilter = 0;
};

} // namespace arachnel::core
