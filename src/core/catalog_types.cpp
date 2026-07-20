#include "catalog_types.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace arachnel::core {

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
