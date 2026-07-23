#include "catalog_types.h"

#include <QCoreApplication>
#include <QDate>
#include <QRegularExpression>

namespace arachnel::core {

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
    entry.playModeMask = playModeMaskFromEntry(entry.genreTokens, entry.installKind);
}

quint8 playModeMaskFromEntry(const QStringList& genreTokens, InstallKind installKind)
{
    quint8 mask = 0;
    for (const QString& raw : genreTokens) {
        const QString t = raw.trimmed().toLower();
        if (t.isEmpty())
            continue;
        if (t.contains(QLatin1String("single-player")) || t.contains(QLatin1String("singleplayer"))
            || t.contains(QStringLiteral("однопользовател")))
            mask |= kPlayModeSingle;
        if (t.contains(QLatin1String("co-op")) || t.contains(QLatin1String("coop"))
            || t.contains(QStringLiteral("кооп")))
            mask |= kPlayModeCoop;
        if (t.contains(QLatin1String("multi-player")) || t.contains(QLatin1String("multiplayer"))
            || t.contains(QLatin1String("online pvp")) || t == QLatin1String("pvp")
            || t.contains(QLatin1String("mmo"))
            || t.contains(QLatin1String("cross-platform"))
            || t.contains(QLatin1String("online fix"))
            || t.contains(QStringLiteral("мультиплеер")))
            mask |= kPlayModeMulti;
        // Online Co-op / Online PvP are online multiplayer modes.
        if (t.contains(QLatin1String("online co-op")) || t.contains(QLatin1String("online pvp"))
            || t.contains(QLatin1String("online fix")))
            mask |= kPlayModeMulti;
    }
    // FreeTP/steamidra mark online-capable titles as BundledFix / FixDownload.
    if (mask == 0
        && (installKind == InstallKind::BundledFix || installKind == InstallKind::FixDownload))
        mask |= kPlayModeMulti;
    return mask;
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
