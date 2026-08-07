#pragma once

#include "catalog_types.h"
#include "install_kind.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <functional>

namespace arachnel::core {

class CatalogModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    /** Index rail stops for the current sort: [{label, row}, ...]. */
    Q_PROPERTY(QVariantList scrubStops READ scrubStops NOTIFY scrubStopsChanged)

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
        CurrentPlayersRole,
        HypeScoreRole,
        ScreenshotUrlsRole,
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
    /** Fallback for non-showcase ids (e.g. FreeTP offer under a steamidra card). */
    void setExtraEntryLookup(std::function<const CatalogEntry*(const QString&)> lookup);
    /** Show cache rows by index (sorted in-place by current sortMode). No deep copy. */
    void setVisibleIndices(QVector<int> indices);
    /** Same as setVisibleIndices but indices are already ordered for current sortMode. */
    void setVisibleIndicesPresorted(QVector<int> indices);
    /**
     * Replace visible rows without beginResetModel (QML GridView/ListView can crash in
     * Qt6QmlMeta when reset runs during a bind after a huge catalog merge).
     */
    void replaceVisibleIndices(QVector<int> indices, bool alreadySorted);
    /** Notify a visible row that its cache entry changed. Returns false if not visible.
     *  Empty roles → cover/metadata/players only (not all roles — avoids QML rebinding
     *  screenshotUrls/description on every cover apply). */
    bool notifyEntryChanged(const QString& id, const QList<int>& roles = {});
    Q_INVOKABLE int indexOfEntry(const QString& id) const;
    const CatalogEntry* entryById(const QString& id) const;
    Q_INVOKABLE QVariantMap entryInfo(const QString& id) const;
    Q_INVOKABLE QVariantList addonsFor(const QString& entryId) const;
    /** First visible row whose title starts with letter (A-Z) or '#' for digits/other. */
    Q_INVOKABLE int indexForLetter(const QString& letter) const;
    QVariantList scrubStops() const;
    void clear();

signals:
    void countChanged();
    void sortModeChanged();
    void scrubStopsChanged();

private:
    void rebuildIdMap();
    void invalidateScrubStops() const;
    QVariantList buildScrubStops() const;
    const CatalogEntry* entryAtRow(int row) const;
    QVariantMap toMap(const CatalogEntry& entry) const;

    const QVector<CatalogEntry>* m_source = nullptr;
    std::function<const CatalogEntry*(const QString&)> m_extraEntryLookup;
    QVector<int> m_indices;
    QHash<QString, int> m_idToRow;
    SortMode m_sortMode = SortNewest;
    mutable QVariantList m_scrubStops;
    mutable bool m_scrubStopsDirty = true;
};

/** Shared by CatalogModel sort and async CatalogFilterService. */
bool catalogEntryLess(const CatalogEntry& a, const CatalogEntry& b, CatalogModel::SortMode mode);

} // namespace arachnel::core
