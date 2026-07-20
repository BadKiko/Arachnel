#pragma once

#include "install_kind.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace arachnel::core {

enum class CatalogItemKind {
    Game = 0,
    Dlc,
    Addon,
};

enum class ComponentDelivery {
    Magnet = 0,
    Direct,
};

QString componentDeliveryLabel(ComponentDelivery delivery);

struct CatalogComponent {
    QString id;
    QString title;
    QStringList magnetUris;
    QString downloadUrl;
    QString referer;
    QString getfileUrl;
    QString fileSize;
    QString uploadDate;
    CatalogItemKind kind = CatalogItemKind::Dlc;
    ComponentDelivery delivery = ComponentDelivery::Magnet;
    bool optional = false;
};

struct CatalogEntry {
    QString id;
    QString title;
    QString coverUrl;
    QString sourceId;
    QString sourcePageUrl;
    QString version;
    QString sizeLabel;
    QString description;
    QString genres;
    QString steamAppId;
    QString trailerUrl;
    QString trailerThumbnailUrl;
    QStringList screenshotUrls;
    InstallKind installKind = InstallKind::PortableArchive;
    QStringList magnetUris;
    QString uploadDate;
    QString parentEntryId;
    CatalogItemKind itemKind = CatalogItemKind::Game;
    QVector<CatalogComponent> addons;
    bool metadataPending = false;

    // Precomputed for filter/sort hot paths (filled by prepareCatalogEntry).
    QString titleLower;
    qint64 sizeBytes = 0;
    qint64 uploadDay = 0; // Julian day; 0 if unknown
    QStringList genreTokens;
};

QString catalogItemKindLabel(CatalogItemKind kind);
QString repairCatalogEntryId(const QString& entryId);
QString slugifyCatalogId(const QString& title, const QString& sourceId);

/** Parse FreeTP-style size labels ("2.71 GB", "512 MB") into bytes; 0 if unknown. */
qint64 parseSizeLabelBytes(const QString& label);

/** Fill titleLower / sizeBytes / uploadDay / genreTokens from primary fields. */
void prepareCatalogEntry(CatalogEntry& entry);

} // namespace arachnel::core
