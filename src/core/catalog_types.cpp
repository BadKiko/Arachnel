#include "catalog_types.h"

#include <QRegularExpression>

namespace arachnel::core {

QString catalogItemKindLabel(CatalogItemKind kind)
{
    switch (kind) {
    case CatalogItemKind::Game:
        return QStringLiteral("Игра");
    case CatalogItemKind::Dlc:
        return QStringLiteral("DLC");
    case CatalogItemKind::Addon:
        return QStringLiteral("Дополнение");
    }
    return QStringLiteral("Компонент");
}

QString componentDeliveryLabel(ComponentDelivery delivery)
{
    switch (delivery) {
    case ComponentDelivery::Direct:
        return QStringLiteral("Прямая");
    case ComponentDelivery::Magnet:
        return QStringLiteral("Торрент");
    }
    return QStringLiteral("Загрузка");
}

QString slugifyCatalogId(const QString& title, const QString& sourceId)
{
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
    return QStringLiteral("%1-%2").arg(sourceId, slug);
}

} // namespace arachnel::core
