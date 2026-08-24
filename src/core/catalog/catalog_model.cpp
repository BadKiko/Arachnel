#include "catalog_model.h"

#include "catalog_genre_normalize.h"
#include "catalog_types.h"
#include "install_kind.h"

#include <QCoreApplication>
#include <QDate>
#include <QHash>
#include <QLocale>

#include <algorithm>

namespace arachnel::core {

namespace {

int steamCdnSelectableAddonCount(const CatalogEntry& entry)
{
    if (entry.sourceId != QStringLiteral("steamidra"))
        return entry.addons.size();
    int n = 0;
    for (const CatalogComponent& c : entry.addons) {
        if (!isSteamStoreDlcId(c.id))
            continue;
        if (c.kind != CatalogItemKind::Dlc && c.kind != CatalogItemKind::Addon)
            continue;
        ++n;
    }
    // Catalog stays id-light: dlcCount from relay until /dlcs fills addons.
    if (n == 0 && entry.dlcCount > 0)
        return entry.dlcCount;
    return n;
}

} // namespace

bool catalogEntryLess(const CatalogEntry& a, const CatalogEntry& b, CatalogModel::SortMode mode)
{
    switch (mode) {
    case CatalogModel::SortOldest:
        if (a.uploadDay != b.uploadDay)
            return a.uploadDay > 0 && (b.uploadDay == 0 || a.uploadDay < b.uploadDay);
        if (a.uploadDate != b.uploadDate)
            return a.uploadDate < b.uploadDate;
        break;
    case CatalogModel::SortTitleAsc:
        return a.titleLower < b.titleLower;
    case CatalogModel::SortTitleDesc:
        return a.titleLower > b.titleLower;
    case CatalogModel::SortPortableFirst:
        if (a.installKind != b.installKind) {
            const bool aPortable = (a.installKind == InstallKind::PortableArchive);
            const bool bPortable = (b.installKind == InstallKind::PortableArchive);
            if (aPortable != bPortable)
                return aPortable && !bPortable;
        }
        if (a.uploadDate != b.uploadDate)
            return a.uploadDate > b.uploadDate;
        break;
    case CatalogModel::SortNonPortableFirst:
        if (a.installKind != b.installKind) {
            const bool aPortable = (a.installKind == InstallKind::PortableArchive);
            const bool bPortable = (b.installKind == InstallKind::PortableArchive);
            if (aPortable != bPortable)
                return !aPortable && bPortable;
        }
        if (a.uploadDate != b.uploadDate)
            return a.uploadDate > b.uploadDate;
        break;
    case CatalogModel::SortSizeLargest:
    case CatalogModel::SortSizeSmallest: {
        if (a.sizeBytes != b.sizeBytes) {
            if (a.sizeBytes == 0)
                return false;
            if (b.sizeBytes == 0)
                return true;
            return mode == CatalogModel::SortSizeLargest ? (a.sizeBytes > b.sizeBytes)
                                                        : (a.sizeBytes < b.sizeBytes);
        }
        if (a.uploadDate != b.uploadDate)
            return a.uploadDate > b.uploadDate;
        break;
    }
    case CatalogModel::SortNewest:
    default:
        if (a.uploadDay != b.uploadDay)
            return a.uploadDay > b.uploadDay;
        if (a.uploadDate != b.uploadDate)
            return a.uploadDate > b.uploadDate;
        break;
    }
    return a.titleLower < b.titleLower;
}

namespace {

QChar catalogIndexLetter(const QString& titleLower)
{
    for (const QChar c : titleLower) {
        if (c.isDigit())
            return QLatin1Char('#');
        if (!c.isLetter())
            continue;
        // Latin A-Z for the alphabet scrubber; everything else maps to '#'.
        if (c.unicode() < 128)
            return c.toUpper();
        return QLatin1Char('#');
    }
    return QLatin1Char('#');
}

} // namespace

CatalogModel::CatalogModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int CatalogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_indices.size();
}

const CatalogEntry* CatalogModel::entryAtRow(int row) const
{
    if (!m_source || row < 0 || row >= m_indices.size())
        return nullptr;
    const int cacheIndex = m_indices.at(row);
    if (cacheIndex < 0 || cacheIndex >= m_source->size())
        return nullptr;
    return &m_source->at(cacheIndex);
}

QVariant CatalogModel::data(const QModelIndex& index, int role) const
{
    const CatalogEntry* entry = entryAtRow(index.row());
    if (!entry)
        return {};

    switch (role) {
    case EntryIdRole:
        return entry->id;
    case TitleRole:
        return entry->title;
    case CoverUrlRole:
        return entry->coverUrl;
    case SourceIdRole:
        return entry->sourceId;
    case VersionRole:
        return entry->version;
    case SizeLabelRole:
        return entry->sizeLabel;
    case DescriptionRole:
        return entry->description;
    case GenresRole:
        return genreLabelsFromBits(entry->genreBits);
    case InstallKindRole:
        return static_cast<int>(entry->installKind);
    case InstallKindLabelRole:
        return installKindLabel(entry->installKind);
    case UploadDateRole:
        return entry->uploadDate;
    case ItemKindRole:
        return static_cast<int>(entry->itemKind);
    case ItemKindLabelRole:
        return catalogItemKindLabel(entry->itemKind);
    case AddonCountRole:
        return steamCdnSelectableAddonCount(*entry);
    case HasAddonsRole:
        return steamCdnSelectableAddonCount(*entry) > 0;
    case MetadataPendingRole:
        return entry->metadataPending;
    case CurrentPlayersRole:
        return entry->currentPlayers;
    case HypeScoreRole:
        return entry->hypeScore;
    case ScreenshotUrlsRole:
        // List/grid cards must not bind screenshots — peek/details use entryInfo().
        // Returning empty here avoids QStringList wrapping on every cover dataChanged.
        return QVariant::fromValue(QStringList{});
    default:
        return {};
    }
}

QHash<int, QByteArray> CatalogModel::roleNames() const
{
    return {
        {EntryIdRole, "entryId"},
        {TitleRole, "title"},
        {CoverUrlRole, "coverUrl"},
        {SourceIdRole, "sourceId"},
        {VersionRole, "version"},
        {SizeLabelRole, "sizeLabel"},
        {DescriptionRole, "description"},
        {GenresRole, "genres"},
        {InstallKindRole, "installKind"},
        {InstallKindLabelRole, "installKindLabel"},
        {UploadDateRole, "uploadDate"},
        {ItemKindRole, "itemKind"},
        {ItemKindLabelRole, "itemKindLabel"},
        {AddonCountRole, "addonCount"},
        {HasAddonsRole, "hasAddons"},
        {MetadataPendingRole, "metadataPending"},
        {CurrentPlayersRole, "currentPlayers"},
        {HypeScoreRole, "hypeScore"},
        {ScreenshotUrlsRole, "screenshotUrls"},
    };
}

void CatalogModel::setSortModeQuiet(int mode)
{
    const auto next = static_cast<SortMode>(
        qBound(static_cast<int>(SortNewest), mode, static_cast<int>(SortSizeSmallest)));
    if (m_sortMode == next)
        return;
    m_sortMode = next;
    emit sortModeChanged();
    invalidateScrubStops();
}

void CatalogModel::setSortMode(int mode)
{
    const auto next = static_cast<SortMode>(
        qBound(static_cast<int>(SortNewest), mode, static_cast<int>(SortSizeSmallest)));
    if (m_sortMode == next)
        return;

    m_sortMode = next;
    if (!m_indices.isEmpty() && m_source)
        replaceVisibleIndices(m_indices, false);
    emit sortModeChanged();
    invalidateScrubStops();
}

void CatalogModel::bindSource(const QVector<CatalogEntry>* source)
{
    m_source = source;
}

void CatalogModel::setExtraEntryLookup(std::function<const CatalogEntry*(const QString&)> lookup)
{
    m_extraEntryLookup = std::move(lookup);
}

void CatalogModel::rebuildIdMap()
{
    m_idToRow.clear();
    m_idToRow.reserve(m_indices.size());
    if (!m_source)
        return;
    for (int row = 0; row < m_indices.size(); ++row) {
        const int cacheIndex = m_indices.at(row);
        if (cacheIndex < 0 || cacheIndex >= m_source->size())
            continue;
        m_idToRow.insert(m_source->at(cacheIndex).id, row);
    }
}

void CatalogModel::setVisibleIndices(QVector<int> indices)
{
    replaceVisibleIndices(std::move(indices), false);
}

void CatalogModel::setVisibleIndicesPresorted(QVector<int> indices)
{
    replaceVisibleIndices(std::move(indices), true);
}

void CatalogModel::beginBulkUpdate()
{
    if (++m_bulkDepth != 1)
        return;
    m_bulkUpdating = true;
    emit bulkUpdatingChanged();
}

void CatalogModel::endBulkUpdate()
{
    if (m_bulkDepth <= 0)
        return;
    if (--m_bulkDepth != 0)
        return;
    m_bulkUpdating = false;
    emit bulkUpdatingChanged();
}

void CatalogModel::replaceVisibleIndices(QVector<int> indices, bool alreadySorted)
{
    if (!alreadySorted && m_source)
        std::stable_sort(indices.begin(), indices.end(), [this](int ai, int bi) {
            if (!m_source || ai < 0 || bi < 0 || ai >= m_source->size() || bi >= m_source->size())
                return ai < bi;
            return catalogEntryLess(m_source->at(ai), m_source->at(bi), m_sortMode);
        });

    if (indices == m_indices)
        return;

    const int oldCount = m_indices.size();
    const int newCount = indices.size();
    // GridView/ListView + required-property delegates crash in Qt6QmlMeta on a
    // 100k-row insert/remove. Unbind first (see CatalogScrollViews).
    const bool bulky = std::max(oldCount, newCount) >= 4096;
    if (bulky)
        beginBulkUpdate();

    if (newCount == 0) {
        if (oldCount > 0) {
            beginRemoveRows({}, 0, oldCount - 1);
            m_indices.clear();
            m_idToRow.clear();
            endRemoveRows();
            emit countChanged();
            invalidateScrubStops();
        }
        if (bulky)
            endBulkUpdate();
        return;
    }

    if (oldCount == 0) {
        beginInsertRows({}, 0, newCount - 1);
        m_indices = std::move(indices);
        rebuildIdMap();
        endInsertRows();
        emit countChanged();
        invalidateScrubStops();
        if (bulky)
            endBulkUpdate();
        return;
    }

    // Replace the list. Prefix insert/remove + dataChanged morphs old cards in
    // place, which looks like the filter is being applied live across the grid.
    beginRemoveRows({}, 0, oldCount - 1);
    m_indices.clear();
    m_idToRow.clear();
    endRemoveRows();
    beginInsertRows({}, 0, newCount - 1);
    m_indices = std::move(indices);
    rebuildIdMap();
    endInsertRows();
    emit countChanged();
    invalidateScrubStops();
    if (bulky)
        endBulkUpdate();
}

bool CatalogModel::notifyEntryChanged(const QString& id, const QList<int>& roles)
{
    const auto it = m_idToRow.constFind(id);
    if (it == m_idToRow.cend())
        return false;
    const QModelIndex idx = index(it.value());
    if (roles.isEmpty()) {
        emit dataChanged(idx, idx,
                         {CoverUrlRole, MetadataPendingRole, TitleRole, CurrentPlayersRole,
                          HypeScoreRole});
    } else {
        emit dataChanged(idx, idx, roles);
    }
    return true;
}

int CatalogModel::indexOfEntry(const QString& id) const
{
    return m_idToRow.value(id, -1);
}

const CatalogEntry* CatalogModel::entryById(const QString& id) const
{
    const int row = indexOfEntry(id);
    if (row >= 0)
        return entryAtRow(row);
    if (!m_source)
        return m_extraEntryLookup ? m_extraEntryLookup(id) : nullptr;
    const QString resolved = repairCatalogEntryId(id);
    for (const auto& entry : *m_source) {
        if (entry.id == resolved || entry.id == id)
            return &entry;
    }
    if (m_extraEntryLookup) {
        if (const CatalogEntry* extra = m_extraEntryLookup(resolved))
            return extra;
        if (resolved != id)
            return m_extraEntryLookup(id);
    }
    return nullptr;
}

QVariantMap CatalogModel::toMap(const CatalogEntry& entry) const
{
    return {
        {QStringLiteral("gameId"), entry.id},
        {QStringLiteral("entryId"), entry.id},
        {QStringLiteral("title"), entry.title},
        {QStringLiteral("coverUrl"), entry.coverUrl},
        {QStringLiteral("sourceId"), entry.sourceId},
        {QStringLiteral("sourceName"), entry.sourceId},
        {QStringLiteral("sourcePageUrl"), entry.sourcePageUrl},
        {QStringLiteral("steamAppId"), entry.steamAppId},
        {QStringLiteral("trailerUrl"), entry.trailerUrl},
        {QStringLiteral("trailerThumbnailUrl"), entry.trailerThumbnailUrl},
        {QStringLiteral("screenshotUrls"), QVariant::fromValue(entry.screenshotUrls)},
        {QStringLiteral("version"), entry.version},
        {QStringLiteral("installPath"), QString()},
        {QStringLiteral("description"), entry.description},
        {QStringLiteral("genres"), genreLabelsFromBits(entry.genreBits)},
        {QStringLiteral("hasDrm"), entry.hasDrm},
        {QStringLiteral("sizeLabel"), entry.sizeLabel},
        {QStringLiteral("installKind"), static_cast<int>(entry.installKind)},
        {QStringLiteral("installKindLabel"), installKindLabel(entry.installKind)},
        {QStringLiteral("uploadDate"), entry.uploadDate},
        {QStringLiteral("itemKind"), static_cast<int>(entry.itemKind)},
        {QStringLiteral("itemKindLabel"), catalogItemKindLabel(entry.itemKind)},
        {QStringLiteral("addonCount"), steamCdnSelectableAddonCount(entry)},
        {QStringLiteral("hasAddons"), steamCdnSelectableAddonCount(entry) > 0},
        {QStringLiteral("metadataPending"), entry.metadataPending},
        {QStringLiteral("hasWorkshop"), entry.hasWorkshop},
        {QStringLiteral("currentPlayers"), entry.currentPlayers},
        {QStringLiteral("hypeScore"), entry.hypeScore},
        {QStringLiteral("hasUpdate"), false},
        {QStringLiteral("installed"), false},
    };
}

QVariantMap CatalogModel::entryInfo(const QString& id) const
{
    const CatalogEntry* entry = entryById(id);
    if (!entry)
        return {};
    return toMap(*entry);
}

QVariantList CatalogModel::addonsFor(const QString& entryId) const
{
    const CatalogEntry* entry = entryById(entryId);
    if (!entry)
        return {};

    const bool steamCdnOnly = entry->sourceId == QStringLiteral("steamidra");
    QVariantList addons;
    addons.reserve(entry->addons.size());
    for (const auto& addon : entry->addons) {
        // Steam CDN picker is Store DLC only - never surface zip/magnet packaging rows.
        if (steamCdnOnly && !isSteamStoreDlcId(addon.id))
            continue;
        if (steamCdnOnly && addon.kind != CatalogItemKind::Dlc
            && addon.kind != CatalogItemKind::Addon)
            continue;
        addons.append(QVariantMap{
            {QStringLiteral("id"), addon.id},
            {QStringLiteral("title"), addon.title},
            {QStringLiteral("fileSize"), addon.fileSize},
            {QStringLiteral("uploadDate"), addon.uploadDate},
            {QStringLiteral("kind"), static_cast<int>(addon.kind)},
            {QStringLiteral("kindLabel"), catalogItemKindLabel(addon.kind)},
            {QStringLiteral("delivery"), static_cast<int>(addon.delivery)},
            {QStringLiteral("deliveryLabel"), componentDeliveryLabel(addon.delivery)},
            {QStringLiteral("optional"), addon.optional},
            {QStringLiteral("contentAvailable"), addon.contentAvailable},
            {QStringLiteral("coverUrl"), addon.coverUrl},
            {QStringLiteral("screenshotUrls"), QVariant::fromValue(addon.screenshotUrls)},
        });
    }
    return addons;
}

int CatalogModel::indexForLetter(const QString& letter) const
{
    if (!m_source || letter.isEmpty() || m_indices.isEmpty())
        return -1;

    const QChar target = letter.at(0).toUpper();
    for (int row = 0; row < m_indices.size(); ++row) {
        const CatalogEntry* entry = entryAtRow(row);
        if (!entry)
            continue;
        if (catalogIndexLetter(entry->titleLower) == target)
            return row;
    }
    return -1;
}

void CatalogModel::invalidateScrubStops() const
{
    m_scrubStopsDirty = true;
    emit const_cast<CatalogModel*>(this)->scrubStopsChanged();
}

QVariantList CatalogModel::scrubStops() const
{
    if (!m_scrubStopsDirty)
        return m_scrubStops;
    m_scrubStops = buildScrubStops();
    m_scrubStopsDirty = false;
    return m_scrubStops;
}

namespace {

QVariantMap scrubStop(const QString& label, int row)
{
    return QVariantMap{
        {QStringLiteral("label"), label},
        {QStringLiteral("row"), row},
    };
}

QString monthScrubLabel(const QDate& day, int currentYear)
{
    const QLocale locale;
    if (day.year() == currentYear)
        return locale.toString(day, QStringLiteral("MMM"));
    if (day.month() == 1)
        return QString::number(day.year());
    return locale.toString(day, QStringLiteral("MMM yy"));
}

QString sizeScrubLabel(qint64 bytes)
{
    if (bytes >= (50LL << 30))
        return QStringLiteral("50G+");
    if (bytes >= (20LL << 30))
        return QStringLiteral("20G");
    if (bytes >= (10LL << 30))
        return QStringLiteral("10G");
    if (bytes >= (5LL << 30))
        return QStringLiteral("5G");
    if (bytes >= (1LL << 30))
        return QStringLiteral("1G");
    if (bytes >= (500LL << 20))
        return QStringLiteral("500M");
    if (bytes >= (100LL << 20))
        return QStringLiteral("100M");
    return QStringLiteral("<100M");
}

} // namespace

QVariantList CatalogModel::buildScrubStops() const
{
    QVariantList stops;
    if (!m_source || m_indices.isEmpty())
        return stops;

    constexpr int kMaxStops = 18;

    switch (m_sortMode) {
    case SortTitleAsc:
    case SortTitleDesc: {
        // One pass for A-Z + '#'.
        static const char kLetters[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        QHash<QChar, int> firstRow;
        firstRow.reserve(28);
        for (int row = 0; row < m_indices.size(); ++row) {
            const CatalogEntry* entry = entryAtRow(row);
            if (!entry)
                continue;
            const QChar letter = catalogIndexLetter(entry->titleLower);
            if (!firstRow.contains(letter))
                firstRow.insert(letter, row);
        }
        for (const char* p = kLetters; *p; ++p) {
            const QChar letter = QLatin1Char(*p);
            const auto it = firstRow.constFind(letter);
            if (it != firstRow.cend())
                stops.append(scrubStop(QString(letter), it.value()));
        }
        break;
    }
    case SortSizeLargest:
    case SortSizeSmallest: {
        QString lastLabel;
        for (int row = 0; row < m_indices.size(); ++row) {
            const CatalogEntry* entry = entryAtRow(row);
            if (!entry)
                continue;
            const QString label = sizeScrubLabel(entry->sizeBytes);
            if (label == lastLabel)
                continue;
            lastLabel = label;
            stops.append(scrubStop(label, row));
        }
        if (stops.size() > kMaxStops) {
            QVariantList thinned;
            thinned.reserve(kMaxStops);
            for (int i = 0; i < kMaxStops; ++i) {
                const int idx = i * (stops.size() - 1) / (kMaxStops - 1);
                thinned.append(stops.at(idx));
            }
            stops = std::move(thinned);
        }
        break;
    }
    case SortPortableFirst:
    case SortNonPortableFirst: {
        int portableRow = -1;
        int otherRow = -1;
        for (int row = 0; row < m_indices.size(); ++row) {
            const CatalogEntry* entry = entryAtRow(row);
            if (!entry)
                continue;
            const bool portable = entry->installKind == InstallKind::PortableArchive;
            if (portable && portableRow < 0)
                portableRow = row;
            if (!portable && otherRow < 0)
                otherRow = row;
            if (portableRow >= 0 && otherRow >= 0)
                break;
        }
        const QString portableLabel =
            QCoreApplication::translate("Core", "Portable");
        const QString otherLabel =
            QCoreApplication::translate("Core", "Installer");
        if (m_sortMode == SortPortableFirst) {
            if (portableRow >= 0)
                stops.append(scrubStop(portableLabel, portableRow));
            if (otherRow >= 0)
                stops.append(scrubStop(otherLabel, otherRow));
        } else {
            if (otherRow >= 0)
                stops.append(scrubStop(otherLabel, otherRow));
            if (portableRow >= 0)
                stops.append(scrubStop(portableLabel, portableRow));
        }
        break;
    }
    case SortNewest:
    case SortOldest:
    default: {
        struct Boundary {
            QString ym;
            int row = 0;
        };
        QVector<Boundary> boundaries;
        boundaries.reserve(64);
        QString lastYm;
        for (int row = 0; row < m_indices.size(); ++row) {
            const CatalogEntry* entry = entryAtRow(row);
            if (!entry)
                continue;
            QString ym = entry->uploadDate.left(7);
            if (ym.size() < 7)
                ym = QStringLiteral("?");
            if (ym == lastYm)
                continue;
            lastYm = ym;
            boundaries.push_back({ym, row});
        }

        QVector<Boundary> picked;
        if (boundaries.size() <= kMaxStops) {
            picked = boundaries;
        } else {
            picked.reserve(kMaxStops);
            for (int i = 0; i < kMaxStops; ++i) {
                const int idx = i * (boundaries.size() - 1) / (kMaxStops - 1);
                picked.push_back(boundaries.at(idx));
            }
        }

        const int currentYear = QDate::currentDate().year();
        QString lastLabel;
        for (const Boundary& b : picked) {
            QString label = b.ym;
            const QDate day = QDate::fromString(b.ym + QStringLiteral("-01"), Qt::ISODate);
            if (day.isValid())
                label = monthScrubLabel(day, currentYear);
            else if (b.ym == QLatin1String("?"))
                label = QCoreApplication::translate("Core", "Unknown");
            if (label == lastLabel)
                continue;
            lastLabel = label;
            stops.append(scrubStop(label, b.row));
        }
        break;
    }
    }

    return stops;
}

void CatalogModel::clear()
{
    if (m_indices.isEmpty()) {
        m_source = nullptr;
        invalidateScrubStops();
        return;
    }
    const bool bulky = m_indices.size() >= 4096;
    if (bulky)
        beginBulkUpdate();
    beginRemoveRows({}, 0, m_indices.size() - 1);
    m_indices.clear();
    m_idToRow.clear();
    m_source = nullptr;
    endRemoveRows();
    emit countChanged();
    invalidateScrubStops();
    if (bulky)
        endBulkUpdate();
}

} // namespace arachnel::core
