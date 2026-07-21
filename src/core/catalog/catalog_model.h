#pragma once

#include "catalog_types.h"
#include "install_kind.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace arachnel::core {

class CatalogModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)

public:
    enum SortMode {
        SortNewest = 0,
        SortOldest,
        SortTitleAsc,
        SortTitleDesc,
        SortPortableFirst,
        SortNonPortableFirst,
        SortSizeLargest,
        SortSizeSmallest,
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

    int count() const { return m_indices.size(); }
    int sortMode() const { return static_cast<int>(m_sortMode); }
    void setSortMode(int mode);
    /** Update sort mode without resorting (caller will setVisibleIndices next). */
    void setSortModeQuiet(int mode);

    /** Bind to cache storage; must outlive visible indices. */
    void bindSource(const QVector<CatalogEntry>* source);
    /** Show cache rows by index (sorted in-place by current sortMode). No deep copy. */
    void setVisibleIndices(QVector<int> indices);
    /** Notify a visible row that its cache entry changed. Returns false if not visible. */
    bool notifyEntryChanged(const QString& id);
    int indexOfEntry(const QString& id) const;
    const CatalogEntry* entryById(const QString& id) const;
    Q_INVOKABLE QVariantMap entryInfo(const QString& id) const;
    Q_INVOKABLE QVariantList addonsFor(const QString& entryId) const;
    void clear();

signals:
    void countChanged();
    void sortModeChanged();

private:
    void sortIndices();
    void rebuildIdMap();
    const CatalogEntry* entryAtRow(int row) const;
    QVariantMap toMap(const CatalogEntry& entry) const;

    const QVector<CatalogEntry>* m_source = nullptr;
    QVector<int> m_indices;
    QHash<QString, int> m_idToRow;
    SortMode m_sortMode = SortNewest;
};

} // namespace arachnel::core
