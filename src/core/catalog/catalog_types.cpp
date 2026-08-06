#include "catalog_types.h"

#include "catalog_genre_normalize.h"

#include <QCoreApplication>
#include <QDate>
#include <QRegularExpression>
#include <QtMath>

namespace arachnel::core {

namespace {

double computeHypeScore(const CatalogEntry& entry)
{
    double score = 0.0;
    if (entry.currentPlayers > 0)
        score += qLn(1.0 + static_cast<double>(entry.currentPlayers)) * 12.0;
    if (entry.recommendationsTotal > 0)
        score += qLn(1.0 + static_cast<double>(entry.recommendationsTotal)) * 4.0;
    if (entry.metacriticScore > 0)
        score += entry.metacriticScore * 0.15;
    if (entry.uploadDay > 0) {
        const qint64 today = QDate::currentDate().toJulianDay();
        const qint64 age = qMax<qint64>(0, today - entry.uploadDay);
        score += qMax(0.0, 30.0 - static_cast<double>(age) * 0.35);
    }
    return score;
}

} // namespace

void prepareCatalogEntry(CatalogEntry& entry)
{
    entry.titleLower = entry.title.trimmed().toLower();
    entry.sizeBytes = parseSizeLabelBytes(entry.sizeLabel);
    const QDate day = QDate::fromString(entry.uploadDate.left(10), Qt::ISODate);
    entry.uploadDay = day.isValid() ? day.toJulianDay() : 0;

    entry.genreTokens.clear();
    const QStringList raw = entry.genres.split(QLatin1Char(','), Qt::SkipEmptyParts);
    entry.genreTokens.reserve(raw.size());
    for (QString token : raw) {
        token = token.trimmed();
        if (!token.isEmpty())
            entry.genreTokens.append(token);
    }
    entry.genreKeys = canonicalizeGenreTokens(entry.genreTokens);
    entry.playModeMask = playModeMaskFromEntry(entry.genreTokens, entry.installKind);
    for (const QString& key : entry.genreKeys) {
        if (key.compare(QLatin1String("Massively Multiplayer"), Qt::CaseInsensitive) == 0)
            entry.playModeMask |= kPlayModeMulti;
    }
    entry.hypeScore = computeHypeScore(entry);

    // List-resident row: drop cold fields (enriched again on open).
    entry.genreTokens.clear();
    entry.genreTokens.squeeze();
    entry.genres.clear();
    entry.description.clear();
    entry.screenshotUrls.clear();
    entry.screenshotUrls.squeeze();
    entry.trailerUrl.clear();
    entry.trailerThumbnailUrl.clear();
    entry.parentEntryId.clear();
    entry.uploadDate.clear();
    if (!entry.steamAppId.isEmpty()) {
        if (entry.coverUrl.startsWith(QLatin1String("http"), Qt::CaseInsensitive))
            entry.coverUrl.clear();
        entry.remoteCoverUrl.clear();
        bool hasTorrentMagnet = false;
        for (const QString& uri : entry.magnetUris) {
            if (uri.startsWith(QLatin1String("magnet:"), Qt::CaseInsensitive)) {
                hasTorrentMagnet = true;
                break;
            }
        }
        if (!hasTorrentMagnet) {
            entry.magnetUris.clear();
            entry.magnetUris.squeeze();
            entry.sourcePageUrl.clear();
        }
    }
    entry.genreKeys.squeeze();
}

quint8 playModeMaskFromEntry(const QStringList& genreTokens, InstallKind installKind)
{
    Q_UNUSED(installKind);
    quint8 mask = 0;
    for (const QString& raw : genreTokens) {
        const QString t = raw.trimmed().toLower();
        if (t.isEmpty())
            continue;
        if (t.contains(QLatin1String("single-player")) || t.contains(QLatin1String("singleplayer"))
            || t.contains(QStringLiteral("однопользовател")))
            mask |= kPlayModeSingle;
        if (t.contains(QLatin1String("co-op")) || t.contains(QLatin1String("coop"))
            || t.contains(QStringLiteral("кооп"))
            || t.contains(QLatin1String("shared/split"))
            || t.contains(QLatin1String("shared and split"))
            || t.contains(QLatin1String("split screen")))
            mask |= kPlayModeCoop;
        if (t.contains(QLatin1String("multi-player")) || t.contains(QLatin1String("multiplayer"))
            || t.contains(QLatin1String("online pvp")) || t == QLatin1String("pvp")
            || t.contains(QLatin1String("mmo"))
            || t.contains(QLatin1String("cross-platform multiplayer"))
            || t.contains(QStringLiteral("мультиплеер"))
            || t.contains(QLatin1String("massively multiplayer")))
            mask |= kPlayModeMulti;
        if (t.contains(QLatin1String("online co-op")) || t.contains(QLatin1String("online pvp")))
            mask |= kPlayModeMulti;
    }
    return mask;
}

bool catalogEntryHasOnlineFixAddon(const CatalogEntry& entry)
{
    for (const CatalogComponent& addon : entry.addons) {
        const QString title = addon.title.trimmed().toLower();
        if (title.isEmpty())
            continue;
        if (title.contains(QLatin1String("online fix")) || title.contains(QLatin1String("online-fix"))
            || title.contains(QLatin1String("onlinefix"))
            || (title.contains(QLatin1String("fix")) && title.contains(QLatin1String("repair")))
            || title.contains(QLatin1String("fix-repair")))
            return true;
    }
    return false;
}

qint64 parseSizeLabelBytes(const QString& label)
{
    static const QRegularExpression re(
        QStringLiteral(R"(^(\d+(?:\.\d+)?)\s*(B|KB|MB|GB|TB))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(label.trimmed());
    if (!match.hasMatch())
        return 0;
    double value = match.captured(1).toDouble();
    const QString unit = match.captured(2).toUpper();
    if (unit == QLatin1String("KB"))
        value *= 1024.0;
    else if (unit == QLatin1String("MB"))
        value *= 1024.0 * 1024.0;
    else if (unit == QLatin1String("GB"))
        value *= 1024.0 * 1024.0 * 1024.0;
    else if (unit == QLatin1String("TB"))
        value *= 1024.0 * 1024.0 * 1024.0 * 1024.0;
    return static_cast<qint64>(value);
}

QString formatSizeLabelBytes(qint64 bytes)
{
    if (bytes <= 0)
        return {};
    static const QStringList units{QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"),
                                   QStringLiteral("GB"), QStringLiteral("TB")};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', unit == 0 ? 0 : 1), units.at(unit));
}

QString catalogItemKindLabel(CatalogItemKind kind)
{
    switch (kind) {
    case CatalogItemKind::Game:
        return QCoreApplication::translate("Core", "Game");
    case CatalogItemKind::Dlc:
        return QStringLiteral("DLC");
    case CatalogItemKind::Addon:
        return QCoreApplication::translate("Core", "Add-on");
    }
    return QCoreApplication::translate("Core", "Component");
}

QString componentDeliveryLabel(ComponentDelivery delivery)
{
    switch (delivery) {
    case ComponentDelivery::Direct:
        return QCoreApplication::translate("Core", "Direct");
    case ComponentDelivery::Magnet:
        return QCoreApplication::translate("Core", "Torrent");
    }
    return QCoreApplication::translate("Core", "Download");
}

QString repairCatalogEntryId(const QString& entryId)
{
    if (entryId.startsWith(QStringLiteral("count:")))
        return entryId.mid(6);
    return entryId;
}

QString slugifyCatalogId(const QString& title, const QString& sourceId)
{
    const QString source = repairCatalogEntryId(sourceId);
    QString slug = title.toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9а-яё]+"), QRegularExpression::UseUnicodePropertiesOption),
                 QStringLiteral("-"));
    slug.replace(QRegularExpression(QStringLiteral("-+")), QStringLiteral("-"));
    slug = slug.trimmed();
    if (slug.startsWith(QLatin1Char('-')))
        slug.remove(0, 1);
    if (slug.endsWith(QLatin1Char('-')))
        slug.chop(1);
    if (slug.isEmpty())
        slug = QStringLiteral("entry");
    return QStringLiteral("%1-%2").arg(source, slug);
}

} // namespace arachnel::core
