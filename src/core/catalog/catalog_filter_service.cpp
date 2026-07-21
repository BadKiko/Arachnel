#include "catalog_filter_service.h"

#include "crash_log.h"
#include "install_kind.h"

#include <QDate>
#include <QElapsedTimer>
#include <QHash>

namespace arachnel::core {

CatalogFilterService::CatalogFilterService(CatalogModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
{
    m_refilterTimer = new QTimer(this);
    m_refilterTimer->setSingleShot(true);
    m_refilterTimer->setInterval(50);
    connect(m_refilterTimer, &QTimer::timeout, this, [this]() { applyFilter(m_activeQuery); });
}

bool CatalogFilterService::entryMatches(const CatalogEntry& entry) const
{
    if (m_typeFilter >= 0) {
        const int kind = static_cast<int>(entry.installKind);
        if (m_typeFilter == 2) {
            if (kind != static_cast<int>(InstallKind::BundledFix)
                && kind != static_cast<int>(InstallKind::FixDownload))
                return false;
        } else if (kind != m_typeFilter) {
            return false;
        }
    }

    if (m_sizeFilter > 0) {
        const qint64 bytes = entry.sizeBytes;
        if (bytes <= 0)
            return false;
        constexpr qint64 kGb = 1024LL * 1024 * 1024;
        switch (m_sizeFilter) {
        case 1:
            if (bytes >= kGb)
                return false;
            break;
        case 2:
            if (bytes < kGb || bytes >= 5 * kGb)
                return false;
            break;
        case 3:
            if (bytes < 5 * kGb || bytes >= 20 * kGb)
                return false;
            break;
        case 4:
            if (bytes < 20 * kGb)
                return false;
            break;
        default:
            break;
        }
    }

    if (m_recencyFilter > 0) {
        if (entry.uploadDay <= 0 || entry.uploadDay < m_filterCutoffDay)
            return false;
    }

    if (m_hasAddonsFilter && entry.addons.isEmpty())
        return false;

    if (!m_genreFilter.isEmpty()) {
        bool matched = false;
        for (const QString& token : entry.genreTokens) {
            if (token.compare(m_genreFilter, Qt::CaseInsensitive) == 0) {
                matched = true;
                break;
            }
        }
        if (!matched)
            return false;
    }

    if (m_playModeFilter > 0) {
        const quint8 need = m_playModeFilter == 1   ? kPlayModeSingle
                            : m_playModeFilter == 2 ? kPlayModeCoop
                                                     : kPlayModeMulti;
        if ((entry.playModeMask & need) == 0)
            return false;
    }

    return true;
}

void CatalogFilterService::applyFilter(const QString& query)
{
    if (!m_model || !m_cache)
        return;

    QElapsedTimer timer;
    timer.start();

    m_filterNeedle = query.trimmed().toLower();
    m_filterCutoffDay = 0;
    if (m_recencyFilter > 0) {
        const int days = m_recencyFilter == 1   ? 7
                         : m_recencyFilter == 2 ? 30
                         : m_recencyFilter == 3 ? 90
                                                 : 365;
        m_filterCutoffDay = QDate::currentDate().addDays(-days).toJulianDay();
    }

    m_model->bindSource(m_cache);
    QVector<int> indices;
    indices.reserve(m_cache->size());
    for (int i = 0; i < m_cache->size(); ++i) {
        const CatalogEntry& entry = m_cache->at(i);
        if (!m_filterNeedle.isEmpty() && !entry.titleLower.contains(m_filterNeedle))
            continue;
        if (!entryMatches(entry))
            continue;
        indices.append(i);
    }
    m_model->setVisibleIndices(std::move(indices));

    const qint64 ms = timer.elapsed();
    if (ms >= 16) {
        logDiagnostic(QStringLiteral("applyCatalogFilter: %1ms cache=%2 visible=%3")
                          .arg(ms)
                          .arg(m_cache->size())
                          .arg(m_model->count()));
    }
}

void CatalogFilterService::scheduleRefilter()
{
    if (m_refilterTimer)
        m_refilterTimer->start();
}

void CatalogFilterService::rebuildAvailableGenres()
{
    if (!m_cache)
        return;

    QHash<QString, int> counts;
    for (const auto& entry : *m_cache) {
        for (const QString& token : entry.genreTokens) {
            const QString lower = token.toLower();
            if (lower.contains(QLatin1String("single-player"))
                || lower.contains(QLatin1String("multi-player"))
                || lower.contains(QLatin1String("co-op")) || lower.contains(QLatin1String("coop"))
                || lower.contains(QLatin1String("online pvp")) || lower == QLatin1String("pvp")
                || lower.contains(QLatin1String("mmo"))
                || lower.contains(QLatin1String("cross-platform"))
                || lower.contains(QLatin1String("online fix"))
                || lower.contains(QLatin1String("shared/split"))
                || lower.contains(QLatin1String("lan pvp")) || lower.contains(QLatin1String("lan co-op"))
                || lower.contains(QStringLiteral("однопользовател"))
                || lower.contains(QStringLiteral("мультиплеер"))
                || lower.contains(QStringLiteral("кооп")))
                continue;
            ++counts[token];
        }
    }

    QStringList genres;
    genres.reserve(counts.size());
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        if (it.value() > 0)
            genres.append(it.key());
    }
    genres.sort(Qt::CaseInsensitive);
    if (genres == m_availableGenres)
        return;
    m_availableGenres = std::move(genres);
    emit availableGenresChanged();
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
    const QString next = genre.trimmed();
    if (m_genreFilter == next)
        return;
    m_genreFilter = next;
    notifyFiltersChanged();
}

void CatalogFilterService::setPlayModeFilter(int filter)
{
    const int next = qBound(0, filter, 3);
    if (m_playModeFilter == next)
        return;
    m_playModeFilter = next;
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
    return count;
}

void CatalogFilterService::clearFilters()
{
    setFilters(-1, 0, 0, false, {}, 0);
}

void CatalogFilterService::setFilters(int typeFilter, int sizeFilter, int recencyFilter,
                                      bool hasAddonsFilter, const QString& genreFilter,
                                      int playModeFilter)
{
    const int nextType = (typeFilter < -1 || typeFilter > 2) ? -1 : typeFilter;
    const int nextSize = qBound(0, sizeFilter, 4);
    const int nextRecency = qBound(0, recencyFilter, 4);
    const QString nextGenre = genreFilter.trimmed();
    const int nextPlay = qBound(0, playModeFilter, 3);
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
    const QString nextGenre = genreFilter.trimmed();
    const int nextPlay = qBound(0, playModeFilter, 3);
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
