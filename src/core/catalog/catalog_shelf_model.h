#pragma once

#include "catalog_types.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

namespace arachnel::core {

/** Lightweight list model over catalog cache indices (discovery shelves). */
class CatalogShelfModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString shelfId READ shelfId CONSTANT)

public:
    enum Role {
        EntryIdRole = Qt::UserRole + 1,
        TitleRole,
        CoverUrlRole,
        SourceIdRole,
        VersionRole,
        SizeLabelRole,
        UploadDateRole,
        CurrentPlayersRole,
        HypeScoreRole,
        InstallKindRole,
        InstallKindLabelRole,
        MetadataPendingRole,
    };
    Q_ENUM(Role)

    explicit CatalogShelfModel(const QString& shelfId, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_indices.size(); }
    QString shelfId() const { return m_shelfId; }

    void bindSource(const QVector<CatalogEntry>* source);
    void setVisibleIndices(QVector<int> indices);
    bool notifyEntryChanged(const QString& id);
    void clear();

    Q_INVOKABLE QVariantMap entryInfo(int row) const;

signals:
    void countChanged();

private:
    const CatalogEntry* entryAtRow(int row) const;

    QString m_shelfId;
    const QVector<CatalogEntry>* m_source = nullptr;
    QVector<int> m_indices;
    QHash<QString, int> m_idToRow;
};

} // namespace arachnel::core
