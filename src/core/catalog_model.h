#pragma once

#include "install_kind.h"

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>

namespace arachnel::core {

struct CatalogEntry {
    QString id;
    QString title;
    QString coverUrl;
    QString sourceId;
    QString version;
    QString sizeLabel;
    QString description;
    QString genres;
    InstallKind installKind = InstallKind::PortableArchive;
};

class CatalogModel : public QAbstractListModel
{
    Q_OBJECT

public:
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
    };
    Q_ENUM(Role)

    explicit CatalogModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QVector<CatalogEntry> entries);
    const CatalogEntry* entryById(const QString& id) const;
    Q_INVOKABLE QVariantMap entryInfo(const QString& id) const;
    void clear();

private:
    QVariantMap toMap(const CatalogEntry& entry) const;

    QVector<CatalogEntry> m_entries;
};

} // namespace arachnel::core
