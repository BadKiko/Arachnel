#include "catalog_model.h"

#include "catalog_types.h"
#include "install_kind.h"

namespace arachnel::core {

CatalogModel::CatalogModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int CatalogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant CatalogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const auto& entry = m_entries.at(index.row());
    switch (role) {
    case EntryIdRole:
        return entry.id;
    case TitleRole:
        return entry.title;
    case CoverUrlRole:
        return entry.coverUrl;
    case SourceIdRole:
        return entry.sourceId;
    case VersionRole:
        return entry.version;
    case SizeLabelRole:
        return entry.sizeLabel;
    case DescriptionRole:
        return entry.description;
    case GenresRole:
        return entry.genres;
    case InstallKindRole:
        return static_cast<int>(entry.installKind);
    case InstallKindLabelRole:
        return installKindLabel(entry.installKind);
    case UploadDateRole:
        return entry.uploadDate;
    case ItemKindRole:
        return static_cast<int>(entry.itemKind);
    case ItemKindLabelRole:
        return catalogItemKindLabel(entry.itemKind);
    case AddonCountRole:
        return entry.addons.size();
    case HasAddonsRole:
        return !entry.addons.isEmpty();
    case MetadataPendingRole:
        return entry.metadataPending;
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

void CatalogModel::setEntries(QVector<CatalogEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

const CatalogEntry* CatalogModel::entryById(const QString& id) const
{
    for (const auto& entry : m_entries) {
        if (entry.id == id)
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
        });
    }
    return addons;
}

void CatalogModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

} // namespace arachnel::core
