#include "catalog_shelf_model.h"

#include "install_kind.h"

namespace arachnel::core {

CatalogShelfModel::CatalogShelfModel(const QString& shelfId, QObject* parent)
    : QAbstractListModel(parent)
    , m_shelfId(shelfId)
{
}

int CatalogShelfModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_indices.size();
}

const CatalogEntry* CatalogShelfModel::entryAtRow(int row) const
{
    if (!m_source || row < 0 || row >= m_indices.size())
        return nullptr;
    const int cacheIndex = m_indices.at(row);
    if (cacheIndex < 0 || cacheIndex >= m_source->size())
        return nullptr;
    return &m_source->at(cacheIndex);
}

QVariant CatalogShelfModel::data(const QModelIndex& index, int role) const
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
    case UploadDateRole:
        return entry->uploadDate;
    case CurrentPlayersRole:
        return entry->currentPlayers;
    case HypeScoreRole:
        return entry->hypeScore;
    case InstallKindRole:
        return static_cast<int>(entry->installKind);
    case InstallKindLabelRole:
        return installKindLabel(entry->installKind);
    case MetadataPendingRole:
        return entry->metadataPending;
    case ScreenshotUrlsRole:
        return QVariant::fromValue(entry->screenshotUrls);
    default:
        return {};
    }
}

QHash<int, QByteArray> CatalogShelfModel::roleNames() const
{
    return {
        {EntryIdRole, "entryId"},
        {TitleRole, "title"},
        {CoverUrlRole, "coverUrl"},
        {SourceIdRole, "sourceId"},
        {VersionRole, "version"},
        {SizeLabelRole, "sizeLabel"},
        {UploadDateRole, "uploadDate"},
        {CurrentPlayersRole, "currentPlayers"},
        {HypeScoreRole, "hypeScore"},
        {InstallKindRole, "installKind"},
        {InstallKindLabelRole, "installKindLabel"},
        {MetadataPendingRole, "metadataPending"},
        {ScreenshotUrlsRole, "screenshotUrls"},
    };
}

void CatalogShelfModel::bindSource(const QVector<CatalogEntry>* source)
{
    m_source = source;
}

void CatalogShelfModel::setVisibleIndices(QVector<int> indices)
{
    if (indices == m_indices)
        return;

    const int oldCount = m_indices.size();
    const int newCount = indices.size();

    auto rebuildMap = [this]() {
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
    };

    if (newCount == 0) {
        if (oldCount > 0) {
            beginRemoveRows({}, 0, oldCount - 1);
            m_indices.clear();
            m_idToRow.clear();
            endRemoveRows();
            emit countChanged();
        }
        return;
    }

    if (oldCount == 0) {
        beginInsertRows({}, 0, newCount - 1);
        m_indices = std::move(indices);
        rebuildMap();
        endInsertRows();
        emit countChanged();
        return;
    }

    if (newCount > oldCount) {
        beginInsertRows({}, oldCount, newCount - 1);
        m_indices = std::move(indices);
        rebuildMap();
        endInsertRows();
        emit dataChanged(index(0), index(oldCount - 1));
    } else if (newCount < oldCount) {
        beginRemoveRows({}, newCount, oldCount - 1);
        m_indices = std::move(indices);
        rebuildMap();
        endRemoveRows();
        if (newCount > 0)
            emit dataChanged(index(0), index(newCount - 1));
    } else {
        m_indices = std::move(indices);
        rebuildMap();
        emit dataChanged(index(0), index(newCount - 1));
    }
    emit countChanged();
}

bool CatalogShelfModel::notifyEntryChanged(const QString& id)
{
    const auto it = m_idToRow.constFind(id);
    if (it == m_idToRow.cend())
        return false;
    const QModelIndex idx = index(it.value());
    emit dataChanged(idx, idx,
                     {CoverUrlRole, MetadataPendingRole, TitleRole, CurrentPlayersRole,
                      ScreenshotUrlsRole, SizeLabelRole, HypeScoreRole});
    return true;
}

void CatalogShelfModel::clear()
{
    if (m_indices.isEmpty()) {
        m_source = nullptr;
        return;
    }
    beginRemoveRows({}, 0, m_indices.size() - 1);
    m_indices.clear();
    m_idToRow.clear();
    m_source = nullptr;
    endRemoveRows();
    emit countChanged();
}

QVariantMap CatalogShelfModel::entryInfo(int row) const
{
    const CatalogEntry* entry = entryAtRow(row);
    if (!entry)
        return {};
    return {
        {QStringLiteral("entryId"), entry->id},
        {QStringLiteral("title"), entry->title},
        {QStringLiteral("coverUrl"), entry->coverUrl},
        {QStringLiteral("sourceId"), entry->sourceId},
        {QStringLiteral("version"), entry->version},
        {QStringLiteral("sizeLabel"), entry->sizeLabel},
        {QStringLiteral("uploadDate"), entry->uploadDate},
        {QStringLiteral("currentPlayers"), entry->currentPlayers},
        {QStringLiteral("hypeScore"), entry->hypeScore},
        {QStringLiteral("installKind"), static_cast<int>(entry->installKind)},
        {QStringLiteral("installKindLabel"), installKindLabel(entry->installKind)},
        {QStringLiteral("steamAppId"), entry->steamAppId},
        {QStringLiteral("metadataPending"), entry->metadataPending},
    };
}

} // namespace arachnel::core
