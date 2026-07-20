#include "catalog_model.h"

#include "catalog_types.h"
#include "install_kind.h"

#include <algorithm>

namespace arachnel::core {

namespace {

bool catalogIndexLess(const CatalogEntry& a, const CatalogEntry& b, CatalogModel::SortMode mode)
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
        return entry->genres;
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
        return entry->addons.size();
    case HasAddonsRole:
        return !entry->addons.isEmpty();
    case MetadataPendingRole:
        return entry->metadataPending;
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
}

void CatalogModel::setSortMode(int mode)
{
    const auto next = static_cast<SortMode>(
        qBound(static_cast<int>(SortNewest), mode, static_cast<int>(SortSizeSmallest)));
    if (m_sortMode == next)
        return;

    m_sortMode = next;
    if (!m_indices.isEmpty() && m_source) {
        beginResetModel();
        sortIndices();
        rebuildIdMap();
        endResetModel();
    }
    emit sortModeChanged();
}

void CatalogModel::bindSource(const QVector<CatalogEntry>* source)
{
    m_source = source;
}

void CatalogModel::sortIndices()
{
    if (!m_source)
        return;
    std::stable_sort(m_indices.begin(), m_indices.end(), [this](int ai, int bi) {
        return catalogIndexLess(m_source->at(ai), m_source->at(bi), m_sortMode);
    });
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
    beginResetModel();
    m_indices = std::move(indices);
    sortIndices();
    rebuildIdMap();
    endResetModel();
    emit countChanged();
}

bool CatalogModel::notifyEntryChanged(const QString& id)
{
    const auto it = m_idToRow.constFind(id);
    if (it == m_idToRow.cend())
        return false;
    const QModelIndex idx = index(it.value());
    emit dataChanged(idx, idx);
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
        return nullptr;
    const QString resolved = repairCatalogEntryId(id);
    for (const auto& entry : *m_source) {
        if (entry.id == resolved || entry.id == id)
            return &entry;
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
        {QStringLiteral("genres"), entry.genres},
        {QStringLiteral("sizeLabel"), entry.sizeLabel},
        {QStringLiteral("installKind"), static_cast<int>(entry.installKind)},
        {QStringLiteral("installKindLabel"), installKindLabel(entry.installKind)},
        {QStringLiteral("uploadDate"), entry.uploadDate},
        {QStringLiteral("itemKind"), static_cast<int>(entry.itemKind)},
        {QStringLiteral("itemKindLabel"), catalogItemKindLabel(entry.itemKind)},
        {QStringLiteral("addonCount"), entry.addons.size()},
        {QStringLiteral("hasAddons"), !entry.addons.isEmpty()},
        {QStringLiteral("metadataPending"), entry.metadataPending},
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

    QVariantList addons;
    addons.reserve(entry->addons.size());
    for (const auto& addon : entry->addons) {
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
        });
    }
    return addons;
}

void CatalogModel::clear()
{
    if (m_indices.isEmpty()) {
        m_source = nullptr;
        return;
    }
    beginResetModel();
    m_indices.clear();
    m_idToRow.clear();
    m_source = nullptr;
    endResetModel();
    emit countChanged();
}

} // namespace arachnel::core
