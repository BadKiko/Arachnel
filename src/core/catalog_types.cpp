#include "catalog_types.h"

#include <QCoreApplication>

#include <QRegularExpression>

namespace arachnel::core {

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
