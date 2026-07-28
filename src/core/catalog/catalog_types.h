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
    /** Canonical genre keys (Action, RPG, …) derived from genreTokens. */
    QStringList genreKeys;
    /** Bitmask: Single=1, Co-op=2, Multiplayer=4 (from Steam categories / genres). */
    quint8 playModeMask = 0;

    // Steam-enrichment discovery signals (any catalog source; keyed by steamAppId).
    int recommendationsTotal = 0;
    int metacriticScore = 0; // 0 = unknown
    qint64 releaseDay = 0; // Julian day; 0 if unknown
    int currentPlayers = -1; // -1 = unknown / not fetched
    qint64 playersFetchedAt = 0; // unix secs
    double hypeScore = 0.0;
};

constexpr quint8 kPlayModeSingle = 1;
constexpr quint8 kPlayModeCoop = 2;
constexpr quint8 kPlayModeMulti = 4;

/** Derive play-mode bits from genre/category tokens (+ online-fix installKind fallback). */
quint8 playModeMaskFromEntry(const QStringList& genreTokens, InstallKind installKind);
/** True when catalog lists an Online Fix / repair add-on (not plain BundledFix install kind). */
bool catalogEntryHasOnlineFixAddon(const CatalogEntry& entry);

QString catalogItemKindLabel(CatalogItemKind kind);
QString repairCatalogEntryId(const QString& entryId);
QString slugifyCatalogId(const QString& title, const QString& sourceId);

/** Parse FreeTP-style size labels ("2.71 GB", "512 MB") into bytes; 0 if unknown. */
qint64 parseSizeLabelBytes(const QString& label);

/** Format byte counts as "512 MB" / "67.5 GB" for catalog size chips. */
QString formatSizeLabelBytes(qint64 bytes);

/** Fill titleLower / sizeBytes / uploadDay / genreTokens from primary fields. */
void prepareCatalogEntry(CatalogEntry& entry);

} // namespace arachnel::core
