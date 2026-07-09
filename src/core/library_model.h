#pragma once

#include "install_kind.h"

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>

namespace arachnel::core {

struct LibraryGame {
    QString id;
    QString title;
    QString coverUrl;
    QString sourceId;
    QString sourceName;
    QString version;
    QString installPath;
    QString description;
    QString genres;
    QString sizeLabel;
    InstallKind installKind = InstallKind::PortableArchive;
    bool hasUpdate = false;
};

class LibraryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        GameIdRole = Qt::UserRole + 1,
        TitleRole,
        CoverUrlRole,
        SourceIdRole,
        SourceNameRole,
        VersionRole,
        InstallPathRole,
        DescriptionRole,
        GenresRole,
        SizeLabelRole,
        InstallKindRole,
        InstallKindLabelRole,
        HasUpdateRole,
    };
    Q_ENUM(Role)

    explicit LibraryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setGames(QVector<LibraryGame> games);
    const LibraryGame* gameById(const QString& id) const;
    Q_INVOKABLE QVariantMap gameAt(int row) const;
    Q_INVOKABLE QVariantMap gameInfo(const QString& id) const;
    Q_INVOKABLE int updateCount() const;

private:
    QVariantMap toMap(const LibraryGame& game) const;

    QVector<LibraryGame> m_games;
};

} // namespace arachnel::core
