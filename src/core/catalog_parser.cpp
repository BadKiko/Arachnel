#include "catalog_parser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace arachnel::core {

namespace {

CatalogItemKind itemKindFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("dlc"))
        return CatalogItemKind::Dlc;
    if (normalized == QStringLiteral("addon") || normalized == QStringLiteral("add-on"))
        return CatalogItemKind::Addon;
    return CatalogItemKind::Game;
}

CatalogComponent parseComponent(const QJsonObject& obj, const QString& sourceId,
                               const QString& parentId)
{
    CatalogComponent component;
    const QString title = obj.value(QStringLiteral("title")).toString();
    component.title = title;
    component.id = obj.value(QStringLiteral("id")).toString();
    if (component.id.isEmpty())
        component.id = slugifyCatalogId(title, sourceId + QStringLiteral("-addon"));
    component.fileSize = obj.value(QStringLiteral("fileSize")).toString();
    component.uploadDate = obj.value(QStringLiteral("uploadDate")).toString();
    component.kind = itemKindFromString(obj.value(QStringLiteral("kind")).toString());

    const QString delivery = obj.value(QStringLiteral("delivery")).toString().trimmed().toLower();
    if (delivery == QStringLiteral("direct"))
        component.delivery = ComponentDelivery::Direct;
    else
        component.delivery = ComponentDelivery::Magnet;

    component.referer = obj.value(QStringLiteral("referer")).toString();
    component.getfileUrl = obj.value(QStringLiteral("getfileUrl")).toString();
    component.optional = obj.value(QStringLiteral("optional")).toBool(false);

    const QJsonArray uris = obj.value(QStringLiteral("uris")).toArray();
    for (const QJsonValue& uri : uris) {
        const QString value = uri.toString();
        if (value.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive))
            component.magnetUris.append(value);
        else if (value.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
                 || value.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive))
            component.downloadUrl = value;
    }

    if (component.id == parentId)
        component.id += QStringLiteral("-component");

    return component;
}

CatalogEntry parseDownloadObject(const QJsonObject& obj, const QString& sourceId)
{
    CatalogEntry entry;
    const QString title = obj.value(QStringLiteral("title")).toString();
    entry.title = title;
    entry.id = obj.value(QStringLiteral("id")).toString();
    if (entry.id.isEmpty())
        entry.id = slugifyCatalogId(title, sourceId);
    entry.sourceId = sourceId;
    entry.sizeLabel = obj.value(QStringLiteral("fileSize")).toString();
    entry.uploadDate = obj.value(QStringLiteral("uploadDate")).toString();
    entry.version = entry.uploadDate.left(10);
    entry.itemKind = itemKindFromString(obj.value(QStringLiteral("kind")).toString());
    entry.parentEntryId = obj.value(QStringLiteral("parentTitle")).toString();
    entry.metadataPending = false;

    const int rawInstallKind = obj.value(QStringLiteral("installKind")).toInt(-1);
    if (rawInstallKind >= static_cast<int>(InstallKind::PortableArchive)
        && rawInstallKind <= static_cast<int>(InstallKind::FixDownload)) {
        entry.installKind = static_cast<InstallKind>(rawInstallKind);
    }

    const QJsonArray uris = obj.value(QStringLiteral("uris")).toArray();
    for (const QJsonValue& uri : uris)
        entry.magnetUris.append(uri.toString());

    const QJsonArray addons = obj.value(QStringLiteral("addons")).toArray();
    entry.addons.reserve(addons.size());
    for (const QJsonValue& addonValue : addons) {
        entry.addons.append(parseComponent(addonValue.toObject(), sourceId, entry.id));
    }

    return entry;
}

void attachOrphanAddons(QVector<CatalogEntry>& entries)
{
    QHash<QString, int> titleToIndex;
    for (int i = 0; i < entries.size(); ++i)
        titleToIndex.insert(entries.at(i).title, i);

    QVector<int> absorbed;
    for (int i = 0; i < entries.size(); ++i) {
        CatalogEntry& entry = entries[i];
        if (entry.itemKind == CatalogItemKind::Game || entry.parentEntryId.isEmpty())
            continue;

        const int parentIndex = titleToIndex.value(entry.parentEntryId, -1);
        if (parentIndex < 0 || parentIndex == i)
            continue;

        CatalogComponent component;
        component.id = entry.id;
        component.title = entry.title;
        component.fileSize = entry.sizeLabel;
        component.uploadDate = entry.uploadDate;
        component.kind = entry.itemKind;
        component.magnetUris = entry.magnetUris;
        entries[parentIndex].addons.append(component);
        absorbed.append(i);
    }

    for (int i = absorbed.size() - 1; i >= 0; --i)
        entries.removeAt(absorbed.at(i));
}

} // namespace

QVector<CatalogEntry> parseCatalogFeed(const QByteArray& payload, const QString& sourceId)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject())
        return {};

    const QJsonObject root = document.object();
    const QJsonArray downloads = root.value(QStringLiteral("downloads")).toArray();
    QVector<CatalogEntry> entries;
    entries.reserve(downloads.size());

    for (const QJsonValue& value : downloads) {
        if (!value.isObject())
            continue;
        entries.append(parseDownloadObject(value.toObject(), sourceId));
    }

    attachOrphanAddons(entries);

    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [](const CatalogEntry& entry) {
                                     return entry.itemKind != CatalogItemKind::Game;
                                 }),
                  entries.end());

    return entries;
}

} // namespace arachnel::core
