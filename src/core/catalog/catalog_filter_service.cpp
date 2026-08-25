#include "catalog_filter_service.h"

#include "catalog_genre_normalize.h"
#include "crash_log.h"
#include "install_kind.h"

#include <QDate>
#include <QElapsedTimer>
#include <QHash>
#include <QReadLocker>
#include <QThreadPool>
#include <QTimer>
#include <QWriteLocker>

#include <algorithm>
#include <numeric>

namespace arachnel::core {

CatalogFilterService::CatalogFilterService(CatalogModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
{
    m_refilterTimer = new QTimer(this);
    m_refilterTimer->setSingleShot(true);
    m_refilterTimer->setInterval(120);
    connect(m_refilterTimer, &QTimer::timeout, this, [this]() { applyFilter(m_activeQuery); });
}

int CatalogFilterService::normalizePlayModeFilter(int filter)
{
    if (filter <= 0)
        return 0;
    if (filter == 1)
        return 1;
    return 2;
}

quint32 CatalogFilterService::genreBitForFilter(const QString& genre) const
{
    const QString trimmed = genre.trimmed();
    if (trimmed.isEmpty())
        return 0;
    quint32 bit = curatedGenreBit(trimmed);
    if (bit)
        return bit;
    return curatedGenreBit(canonicalizeGenreToken(trimmed));
}

quint8 CatalogFilterService::internSourceSlot(const QString& sourceId)
{
    for (int i = 0; i < m_sourceIdsBySlot.size(); ++i) {
        if (m_sourceIdsBySlot.at(i) == sourceId)
            return static_cast<quint8>(i);
    }
    if (m_sourceIdsBySlot.size() >= 32)
        return 31;
    m_sourceIdsBySlot.append(sourceId);
    return static_cast<quint8>(m_sourceIdsBySlot.size() - 1);
}

void CatalogFilterService::refreshAvailableGenres()
{
    QStringList genres = curatedGenreKeysFromBits(m_presentGenreBits);
    const QString current = m_genreFilter.trimmed();
    if (!current.isEmpty() && !genres.contains(current))
        genres.prepend(current);
    if (genres == m_availableGenres)
        return;
    m_availableGenres = std::move(genres);
    emit availableGenresChanged();
}

bool CatalogFilterService::rowMatches(const CatalogFilterRow& row, const FilterSnapshot& snap)
{
    if ((row.flags & kFilterFlagGame) == 0)
        return false;

    if (snap.checkSource && (snap.sourceMask & (quint32(1) << row.sourceSlot)) == 0)
        return false;

    if (snap.typeFilter >= 0) {
        const int kind = static_cast<int>(row.installKind);
        if (snap.typeFilter == 2) {
            if (kind != static_cast<int>(InstallKind::BundledFix)
                && kind != static_cast<int>(InstallKind::FixDownload))
                return false;
        } else if (kind != snap.typeFilter) {
            return false;
        }
    }

    if (snap.sizeFilter > 0 && row.sizeBytes > 0) {
        constexpr qint64 kGb = 1024LL * 1024 * 1024;
        switch (snap.sizeFilter) {
        case 1:
            if (row.sizeBytes >= kGb)
                return false;
            break;
        case 2:
            if (row.sizeBytes < kGb || row.sizeBytes >= 5 * kGb)
                return false;
            break;
        case 3:
            if (row.sizeBytes < 5 * kGb || row.sizeBytes >= 20 * kGb)
                return false;
            break;
        case 4:
            if (row.sizeBytes < 20 * kGb)
                return false;
            break;
        default:
            break;
        }
    }

    if (snap.recencyFilter > 0) {
        if (row.uploadDay <= 0 || row.uploadDay < snap.cutoffDay)
            return false;
    }

    if (snap.hasAddonsFilter && (row.flags & kFilterFlagHasAddons) == 0)
        return false;

    if (snap.genreBit && (row.genreBits & snap.genreBit) == 0)
        return false;

    if (snap.playModeFilter == 1) {
        if ((row.playModeMask & kPlayModeSingle) == 0 && row.playModeMask != 0)
            return false;
    } else if (snap.playModeFilter == 2) {
        if ((row.playModeMask & kPlayModeTogether) == 0)
            return false;
    }

    return true;
}

void CatalogFilterService::rebuildFilterTable()
{
    auto fill = [this](QVector<CatalogEntry>* cachePtr) {
        m_sourceIdsBySlot.clear();
        m_presentGenreBits = 0;
        if (!cachePtr) {
            m_rows.clear();
            m_searchEntries.clear();
            m_titleLowers.clear();
            return;
        }
        const int n = cachePtr->size();
        m_rows.resize(n);
        m_searchEntries.resize(n);
        m_titleLowers.resize(n);
        m_sourceIdsBySlot.reserve(8);
        QHash<QString, quint8> intern;
        intern.reserve(8);
        for (int i = 0; i < n; ++i) {
            const CatalogEntry& entry = cachePtr->at(i);
            quint8 slot = 0;
            const auto it = intern.constFind(entry.sourceId);
            if (it != intern.cend()) {
                slot = it.value();
            } else if (m_sourceIdsBySlot.size() < 32) {
                slot = static_cast<quint8>(m_sourceIdsBySlot.size());
                intern.insert(entry.sourceId, slot);
                m_sourceIdsBySlot.append(entry.sourceId);
            } else {
                slot = 31;
            }
            m_rows[i] = catalogFilterRowFromEntry(entry, slot);
            m_searchEntries[i] = CatalogSearchEntry::fromEntry(entry);
            m_titleLowers[i] = entry.titleLower;
            m_presentGenreBits |= entry.genreBits;
        }
    };

    if (m_cacheLock) {
        QWriteLocker locker(m_cacheLock);
        fill(m_cache);
    } else {
        fill(m_cache);
    }
    refreshAvailableGenres();
}

void CatalogFilterService::syncFilterRow(int cacheIndex)
{
    if (!m_cache || cacheIndex < 0)
        return;
    CatalogFilterRow row;
    CatalogSearchEntry searchEntry;
    QString titleLower;
    quint32 bits = 0;
    bool inRange = false;
    if (m_cacheLock) {
        QWriteLocker locker(m_cacheLock);
        if (cacheIndex >= m_cache->size())
            return;
        if (m_rows.size() != m_cache->size() || m_searchEntries.size() != m_cache->size()) {
            locker.unlock();
            rebuildFilterTable();
            return;
        }
        const CatalogEntry& entry = m_cache->at(cacheIndex);
        const quint8 slot = internSourceSlot(entry.sourceId);
        row = catalogFilterRowFromEntry(entry, slot);
        searchEntry = CatalogSearchEntry::fromEntry(entry);
        titleLower = entry.titleLower;
        bits = entry.genreBits;
        m_rows[cacheIndex] = row;
        m_searchEntries[cacheIndex] = std::move(searchEntry);
        m_titleLowers[cacheIndex] = titleLower;
        inRange = true;
    } else {
        if (cacheIndex >= m_cache->size())
            return;
        if (m_rows.size() != m_cache->size() || m_searchEntries.size() != m_cache->size()) {
            rebuildFilterTable();
            return;
        }
        const CatalogEntry& entry = m_cache->at(cacheIndex);
        const quint8 slot = internSourceSlot(entry.sourceId);
        row = catalogFilterRowFromEntry(entry, slot);
        searchEntry = CatalogSearchEntry::fromEntry(entry);
        m_rows[cacheIndex] = row;
        m_searchEntries[cacheIndex] = std::move(searchEntry);
        m_titleLowers[cacheIndex] = entry.titleLower;
        bits = entry.genreBits;
        inRange = true;
    }
    if (!inRange)
        return;
    const quint32 before = m_presentGenreBits;
    m_presentGenreBits |= bits;
    if (m_presentGenreBits != before)
        refreshAvailableGenres();
}

void CatalogFilterService::applyFilterResult(quint64 generation, QVector<int> indices,
                                             qint64 elapsedMs, int cacheSize, const QString& needle)
{
    if (generation != m_filterGeneration.load(std::memory_order_relaxed) || !m_model)
        return;
    if (m_cache && cacheSize != m_cache->size())
        return;
    m_model->setVisibleIndicesPresorted(std::move(indices));
    if (elapsedMs >= 16 || (!needle.isEmpty() && m_model->count() == 0)) {
        logDiagnostic(QStringLiteral("applyCatalogFilter: %1ms cache=%2 visible=%3 needle=\"%4\"")
                          .arg(elapsedMs)
                          .arg(cacheSize)
                          .arg(m_model->count())
                          .arg(needle));
    }
}

void CatalogFilterService::applyFilter(const QString& query)
{
    if (!m_model || !m_cache)
        return;

    bool needRebuild = false;
    if (m_cacheLock) {
        QReadLocker locker(m_cacheLock);
        needRebuild = m_rows.size() != m_cache->size() || m_searchEntries.size() != m_cache->size();
    } else {
        needRebuild = m_rows.size() != m_cache->size() || m_searchEntries.size() != m_cache->size();
    }
    if (needRebuild)
        rebuildFilterTable();

    m_activeQuery = query;
    m_filterCutoffDay = 0;
    if (m_recencyFilter > 0) {
        const int days = m_recencyFilter == 1   ? 7
                         : m_recencyFilter == 2 ? 30
                         : m_recencyFilter == 3 ? 90
                                                 : 365;
        m_filterCutoffDay = QDate::currentDate().addDays(-days).toJulianDay();
    }

    const quint64 generation = ++m_filterGeneration;
    m_model->bindSource(m_cache);

    quint32 sourceMask = 0;
    bool checkSource = !m_hiddenSourceIds.isEmpty() && !m_sourceIdsBySlot.isEmpty();
    if (checkSource) {
        for (int i = 0; i < m_sourceIdsBySlot.size(); ++i) {
            if (!m_hiddenSourceIds.contains(m_sourceIdsBySlot.at(i)))
                sourceMask |= (quint32(1) << i);
        }
        if (sourceMask == 0) {
            // All interned sources hidden.
        } else {
            quint32 all = 0;
            for (int i = 0; i < m_sourceIdsBySlot.size(); ++i)
                all |= (quint32(1) << i);
            if (sourceMask == all)
                checkSource = false;
        }
    }

    FilterSnapshot snap;
    snap.query = ParsedSearchQuery::parse(query);
    snap.cutoffDay = m_filterCutoffDay;
    snap.typeFilter = m_typeFilter;
    snap.sizeFilter = m_sizeFilter;
    snap.recencyFilter = m_recencyFilter;
    snap.hasAddonsFilter = m_hasAddonsFilter;
    snap.genreBit = genreBitForFilter(m_genreFilter);
    snap.playModeFilter = m_playModeFilter;
    snap.sourceMask = sourceMask;
    snap.checkSource = checkSource;
    snap.sortMode = m_model->sortMode();
    snap.anySideFilter = m_typeFilter >= 0 || m_sizeFilter > 0 || m_recencyFilter > 0
        || m_hasAddonsFilter || snap.genreBit != 0 || m_playModeFilter > 0 || checkSource;

    QVector<CatalogEntry>* cachePtr = m_cache;
    QReadWriteLock* lock = m_cacheLock;
    const QVector<CatalogFilterRow>* rowsPtr = &m_rows;
    const QVector<CatalogSearchEntry>* searchPtr = &m_searchEntries;

    QThreadPool::globalInstance()->start(
        [this, generation, snap, cachePtr, lock, rowsPtr, searchPtr]() {
            QElapsedTimer timer;
            timer.start();

            struct ScoredIndex {
                int index = 0;
                int score = 0;
            };
            QVector<ScoredIndex> scoredMatches;
            QVector<int> indices;
            int cacheSize = 0;
            CatalogModel::SortMode mode = CatalogModel::SortNewest;
            const bool hasSearch = !snap.query.isEmpty;

            auto scanCache = [&]() -> bool {
                if (!cachePtr || !rowsPtr || !searchPtr)
                    return false;
                cacheSize = cachePtr->size();
                if (rowsPtr->size() != cacheSize || searchPtr->size() != cacheSize)
                    return false;

                if (!hasSearch && !snap.anySideFilter) {
                    indices.resize(cacheSize);
                    std::iota(indices.begin(), indices.end(), 0);
                } else if (!hasSearch) {
                    indices.reserve(cacheSize);
                    for (int i = 0; i < cacheSize; ++i) {
                        if ((i & 0x3FF) == 0
                            && generation != m_filterGeneration.load(std::memory_order_relaxed)) {
                            return false;
                        }
                        if (!rowMatches(rowsPtr->at(i), snap))
                            continue;
                        indices.append(i);
                    }
                } else {
                    scoredMatches.reserve(qMin(cacheSize, 1024));
                    for (int i = 0; i < cacheSize; ++i) {
                        if ((i & 0x3FF) == 0
                            && generation != m_filterGeneration.load(std::memory_order_relaxed)) {
                            return false;
                        }
                        if (!rowMatches(rowsPtr->at(i), snap))
                            continue;

                        const int score =
                            scoreCatalogMatch(searchPtr->at(i), cachePtr->at(i), snap.query);
                        if (score <= 0)
                            continue;

                        scoredMatches.append({i, score});
                    }
                }
                mode = static_cast<CatalogModel::SortMode>(snap.sortMode);
                return true;
            };

            if (lock) {
                QReadLocker locker(lock);
                if (!scanCache())
                    return;
            } else {
                if (!scanCache())
                    return;
            }

            if (generation != m_filterGeneration.load(std::memory_order_relaxed))
                return;

            if (hasSearch) {
                auto sortScored = [&](const ScoredIndex& a, const ScoredIndex& b) {
                    if (a.score != b.score)
                        return a.score > b.score;
                    if (cachePtr)
                        return catalogEntryLess(cachePtr->at(a.index), cachePtr->at(b.index), mode);
                    return a.index < b.index;
                };

                if (lock && cachePtr) {
                    QReadLocker locker(lock);
                    if (cacheSize != cachePtr->size())
                        return;
                    std::stable_sort(scoredMatches.begin(), scoredMatches.end(), sortScored);
                } else {
                    std::stable_sort(scoredMatches.begin(), scoredMatches.end(), sortScored);
                }

                indices.reserve(scoredMatches.size());
                for (const ScoredIndex& sm : scoredMatches)
                    indices.append(sm.index);
            } else {
                if (lock && cachePtr) {
                    QReadLocker locker(lock);
                    if (cacheSize != cachePtr->size())
                        return;
                    std::stable_sort(indices.begin(), indices.end(),
                                     [cachePtr, mode](int ai, int bi) {
                                         return catalogEntryLess(cachePtr->at(ai), cachePtr->at(bi),
                                                                 mode);
                                     });
                } else if (cachePtr) {
                    std::stable_sort(indices.begin(), indices.end(),
                                     [cachePtr, mode](int ai, int bi) {
                                         return catalogEntryLess(cachePtr->at(ai), cachePtr->at(bi),
                                                                 mode);
                                     });
                }
            }

            if (generation != m_filterGeneration.load(std::memory_order_relaxed))
                return;

            const qint64 ms = timer.elapsed();
            const QString queryStr = snap.query.rawQuery;
            QTimer::singleShot(0, this,
                               [this, generation, indices = std::move(indices), ms, cacheSize,
                                queryStr]() mutable {
                                   applyFilterResult(generation, std::move(indices), ms, cacheSize,
                                                     queryStr);
                               });
        });
}

void CatalogFilterService::scheduleRefilter()
{
    if (m_refilterTimer)
        m_refilterTimer->start();
}

void CatalogFilterService::notifyFiltersChanged()
{
    emit filtersChanged();
    applyFilter(m_activeQuery);
}

void CatalogFilterService::setTypeFilter(int filter)
{
    const int next = (filter < -1 || filter > 2) ? -1 : filter;
    if (m_typeFilter == next)
        return;
    m_typeFilter = next;
    notifyFiltersChanged();
}

void CatalogFilterService::setSizeFilter(int filter)
{
    const int next = qBound(0, filter, 4);
    if (m_sizeFilter == next)
        return;
    m_sizeFilter = next;
    notifyFiltersChanged();
}

void CatalogFilterService::setRecencyFilter(int filter)
{
    const int next = qBound(0, filter, 4);
    if (m_recencyFilter == next)
        return;
    m_recencyFilter = next;
    notifyFiltersChanged();
}

void CatalogFilterService::setHasAddonsFilter(bool enabled)
{
    if (m_hasAddonsFilter == enabled)
        return;
    m_hasAddonsFilter = enabled;
    notifyFiltersChanged();
}

void CatalogFilterService::setGenreFilter(const QString& genre)
{
    QString next = genre.trimmed();
    if (!next.isEmpty() && genreBitForFilter(next) == 0)
        next.clear();
    if (m_genreFilter == next)
        return;
    m_genreFilter = next;
    notifyFiltersChanged();
}

void CatalogFilterService::setPlayModeFilter(int filter)
{
    const int next = normalizePlayModeFilter(filter);
    if (m_playModeFilter == next)
        return;
    m_playModeFilter = next;
    notifyFiltersChanged();
}

void CatalogFilterService::setHiddenSourceIds(QStringList ids)
{
    ids.removeAll(QString());
    ids.removeDuplicates();
    if (ids == m_hiddenSourceIds)
        return;
    m_hiddenSourceIds = std::move(ids);
    notifyFiltersChanged();
}

void CatalogFilterService::setSourceHidden(const QString& sourceId, bool hidden)
{
    const QString id = sourceId.trimmed();
    if (id.isEmpty())
        return;
    const int at = m_hiddenSourceIds.indexOf(id);
    if (hidden) {
        if (at >= 0)
            return;
        m_hiddenSourceIds.append(id);
    } else {
        if (at < 0)
            return;
        m_hiddenSourceIds.removeAt(at);
    }
    notifyFiltersChanged();
}

int CatalogFilterService::activeFilterCount() const
{
    int count = 0;
    if (m_typeFilter >= 0)
        ++count;
    if (m_sizeFilter > 0)
        ++count;
    if (m_recencyFilter > 0)
        ++count;
    if (m_hasAddonsFilter)
        ++count;
    if (!m_genreFilter.isEmpty())
        ++count;
    if (m_playModeFilter > 0)
        ++count;
    count += m_hiddenSourceIds.size();
    return count;
}

void CatalogFilterService::clearFilters()
{
    const bool hiddenChanged = !m_hiddenSourceIds.isEmpty();
    m_hiddenSourceIds.clear();
    setFilters(-1, 0, 0, false, {}, 0);
    if (hiddenChanged && m_typeFilter == -1 && m_sizeFilter == 0 && m_recencyFilter == 0
        && !m_hasAddonsFilter && m_genreFilter.isEmpty() && m_playModeFilter == 0) {
        // setFilters no-op'd because side filters were already clear.
        notifyFiltersChanged();
    }
}

void CatalogFilterService::setFilters(int typeFilter, int sizeFilter, int recencyFilter,
                                      bool hasAddonsFilter, const QString& genreFilter,
                                      int playModeFilter)
{
    const int nextType = (typeFilter < -1 || typeFilter > 2) ? -1 : typeFilter;
    const int nextSize = qBound(0, sizeFilter, 4);
    const int nextRecency = qBound(0, recencyFilter, 4);
    QString nextGenre = genreFilter.trimmed();
    if (!nextGenre.isEmpty() && genreBitForFilter(nextGenre) == 0)
        nextGenre.clear();
    const int nextPlay = normalizePlayModeFilter(playModeFilter);
    if (m_typeFilter == nextType && m_sizeFilter == nextSize && m_recencyFilter == nextRecency
        && m_hasAddonsFilter == hasAddonsFilter && m_genreFilter == nextGenre
        && m_playModeFilter == nextPlay)
        return;

    m_typeFilter = nextType;
    m_sizeFilter = nextSize;
    m_recencyFilter = nextRecency;
    m_hasAddonsFilter = hasAddonsFilter;
    m_genreFilter = nextGenre;
    m_playModeFilter = nextPlay;
    notifyFiltersChanged();
}

void CatalogFilterService::applyPresentation(int sortMode, int typeFilter, int sizeFilter,
                                             int recencyFilter, bool hasAddonsFilter,
                                             const QString& genreFilter, int playModeFilter)
{
    if (m_model)
        m_model->setSortModeQuiet(sortMode);

    const int nextType = (typeFilter < -1 || typeFilter > 2) ? -1 : typeFilter;
    const int nextSize = qBound(0, sizeFilter, 4);
    const int nextRecency = qBound(0, recencyFilter, 4);
    QString nextGenre = genreFilter.trimmed();
    if (!nextGenre.isEmpty() && genreBitForFilter(nextGenre) == 0)
        nextGenre.clear();
    const int nextPlay = normalizePlayModeFilter(playModeFilter);
    const bool changed = m_typeFilter != nextType || m_sizeFilter != nextSize
        || m_recencyFilter != nextRecency || m_hasAddonsFilter != hasAddonsFilter
        || m_genreFilter != nextGenre || m_playModeFilter != nextPlay;

    m_typeFilter = nextType;
    m_sizeFilter = nextSize;
    m_recencyFilter = nextRecency;
    m_hasAddonsFilter = hasAddonsFilter;
    m_genreFilter = nextGenre;
    m_playModeFilter = nextPlay;

    if (changed)
        emit filtersChanged();
    applyFilter(m_activeQuery);
}

} // namespace arachnel::core
