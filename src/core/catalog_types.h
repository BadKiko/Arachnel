#pragma once

#include "install_kind.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

enum class CatalogItemKind {
    Game = 0,
    Dlc,
    Addon,
};

struct CatalogComponent {
    QString id;
    QString title;
    QStringList magnetUris;
    QString fileSize;
    QString uploadDate;
    CatalogItemKind kind = CatalogItemKind::Dlc;
};

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
    QStringList magnetUris;
    QString uploadDate;
    QString parentEntryId;
    CatalogItemKind itemKind = CatalogItemKind::Game;
    QVector<CatalogComponent> addons;
    bool metadataPending = false;
};

QString catalogItemKindLabel(CatalogItemKind kind);
QString slugifyCatalogId(const QString& title, const QString& sourceId);

} // namespace arachnel::core
