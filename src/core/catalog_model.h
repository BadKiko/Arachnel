#pragma once

#include "catalog_types.h"
#include "install_kind.h"

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>

namespace arachnel::core {

class CatalogModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(int installKindFilter READ installKindFilter WRITE setInstallKindFilter NOTIFY installKindFilterChanged)

public:
    enum SortMode {
        SortNewest = 0,
        SortOldest,
        SortTitleAsc,
        SortTitleDesc,
        SortPortableFirst,
        SortNonPortableFirst,
    };
    Q_ENUM(SortMode)

    enum Role {
        EntryIdRole = Qt::UserRole + 1,
        TitleRole,
        CoverUrlRole,
        SourceIdRole,
        VersionRole,
        SizeLabelRole,
        DescriptionRole,
        GenresRole,
        InstallKindRole,
        InstallKindLabelRole,
        UploadDateRole,
        ItemKindRole,
        ItemKindLabelRole,
        AddonCountRole,
        HasAddonsRole,
        MetadataPendingRole,
    };
    Q_ENUM(Role)

    explicit CatalogModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_entries.size(); }
    int sortMode() const { return static_cast<int>(m_sortMode); }
    void setSortMode(int mode);

    int installKindFilter() const { return static_cast<int>(m_installKindFilter); }
    void setInstallKindFilter(int filter);

    void setEntries(QVector<CatalogEntry> entries);
    bool updateEntry(const CatalogEntry& entry);
    int indexOfEntry(const QString& id) const;
    const CatalogEntry* entryById(const QString& id) const;
    Q_INVOKABLE QVariantMap entryInfo(const QString& id) const;
    Q_INVOKABLE QVariantList addonsFor(const QString& entryId) const;
    void clear();

signals:
    void countChanged();
    void sortModeChanged();
    void installKindFilterChanged();

private:
    void sortEntries();
    QVariantMap toMap(const CatalogEntry& entry) const;

    QVector<CatalogEntry> m_entries;
    SortMode m_sortMode = SortNewest;
    int m_installKindFilter = -1; // -1 = all
};

} // namespace arachnel::core
