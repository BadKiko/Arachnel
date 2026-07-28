#include "plugin_catalog_json.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace arachnel::core {

namespace {

QString itemKindToString(CatalogItemKind kind)
{
    switch (kind) {
    case CatalogItemKind::Dlc:
        return QStringLiteral("dlc");
    case CatalogItemKind::Addon:
        return QStringLiteral("addon");
    case CatalogItemKind::Game:
    default:
        return QStringLiteral("game");
    }
}

CatalogItemKind itemKindFromString(const QString& value)
{
    const QString n = value.trimmed().toLower();
    if (n == QStringLiteral("dlc"))
        return CatalogItemKind::Dlc;
    if (n == QStringLiteral("addon") || n == QStringLiteral("add-on"))
        return CatalogItemKind::Addon;
    return CatalogItemKind::Game;
}

QJsonObject componentToJson(const CatalogComponent& c)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), c.id);
    o.insert(QStringLiteral("title"), c.title);
    o.insert(QStringLiteral("fileSize"), c.fileSize);
    o.insert(QStringLiteral("uploadDate"), c.uploadDate);
    o.insert(QStringLiteral("kind"), itemKindToString(c.kind));
    o.insert(QStringLiteral("optional"), c.optional);
    o.insert(QStringLiteral("referer"), c.referer);
    o.insert(QStringLiteral("getfileUrl"), c.getfileUrl);
    o.insert(QStringLiteral("downloadUrl"), c.downloadUrl);
    o.insert(QStringLiteral("delivery"),
             c.delivery == ComponentDelivery::Direct ? QStringLiteral("direct")
                                                     : QStringLiteral("magnet"));
    QJsonArray uris;
    for (const QString& u : c.magnetUris)
        uris.append(u);
    if (!c.downloadUrl.isEmpty())
        uris.append(c.downloadUrl);
    o.insert(QStringLiteral("uris"), uris);
    return o;
}

CatalogComponent componentFromJson(const QJsonObject& o)
{
    CatalogComponent c;
    c.id = o.value(QStringLiteral("id")).toString();
    c.title = o.value(QStringLiteral("title")).toString();
    c.fileSize = o.value(QStringLiteral("fileSize")).toString();
    c.uploadDate = o.value(QStringLiteral("uploadDate")).toString();
    c.kind = itemKindFromString(o.value(QStringLiteral("kind")).toString());
    c.optional = o.value(QStringLiteral("optional")).toBool(false);
    c.referer = o.value(QStringLiteral("referer")).toString();
    c.getfileUrl = o.value(QStringLiteral("getfileUrl")).toString();
    c.downloadUrl = o.value(QStringLiteral("downloadUrl")).toString();
    const QString delivery = o.value(QStringLiteral("delivery")).toString().trimmed().toLower();
    c.delivery = delivery == QStringLiteral("direct") ? ComponentDelivery::Direct
                                                      : ComponentDelivery::Magnet;
    const QJsonArray uris = o.value(QStringLiteral("uris")).toArray();
    for (const QJsonValue& uri : uris) {
        const QString value = uri.toString();
        if (value.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive))
            c.magnetUris.append(value);
        else if (value.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
                 || value.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
            if (c.downloadUrl.isEmpty())
                c.downloadUrl = value;
        }
    }
    return c;
}

QJsonObject entryToJson(const CatalogEntry& e)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), e.id);
    o.insert(QStringLiteral("title"), e.title);
    o.insert(QStringLiteral("coverUrl"), e.coverUrl);
    o.insert(QStringLiteral("sourceId"), e.sourceId);
    o.insert(QStringLiteral("articleUrl"), e.sourcePageUrl);
    o.insert(QStringLiteral("version"), e.version);
    o.insert(QStringLiteral("fileSize"), e.sizeLabel);
    o.insert(QStringLiteral("sizeLabel"), e.sizeLabel);
    o.insert(QStringLiteral("description"), e.description);
    o.insert(QStringLiteral("genres"), e.genres);
    o.insert(QStringLiteral("steamAppId"), e.steamAppId);
    o.insert(QStringLiteral("trailerUrl"), e.trailerUrl);
    o.insert(QStringLiteral("trailerThumbnailUrl"), e.trailerThumbnailUrl);
    o.insert(QStringLiteral("uploadDate"), e.uploadDate);
    o.insert(QStringLiteral("parentEntryId"), e.parentEntryId);
    o.insert(QStringLiteral("kind"), itemKindToString(e.itemKind));
    o.insert(QStringLiteral("installKind"), static_cast<int>(e.installKind));
    o.insert(QStringLiteral("metadataPending"), e.metadataPending);
    o.insert(QStringLiteral("recommendationsTotal"), e.recommendationsTotal);
    o.insert(QStringLiteral("metacriticScore"), e.metacriticScore);
    o.insert(QStringLiteral("currentPlayers"), e.currentPlayers);
    o.insert(QStringLiteral("hypeScore"), e.hypeScore);

    QJsonArray magnets;
    for (const QString& m : e.magnetUris)
        magnets.append(m);
    o.insert(QStringLiteral("uris"), magnets);

    QJsonArray shots;
    for (const QString& s : e.screenshotUrls)
        shots.append(s);
    o.insert(QStringLiteral("screenshotUrls"), shots);

    QJsonArray addons;
    for (const CatalogComponent& c : e.addons)
        addons.append(componentToJson(c));
    o.insert(QStringLiteral("addons"), addons);
    return o;
}

CatalogEntry entryFromJson(const QJsonObject& o, const QString& defaultSourceId)
{
    CatalogEntry e;
    e.id = o.value(QStringLiteral("id")).toString();
    e.title = o.value(QStringLiteral("title")).toString();
    e.coverUrl = o.value(QStringLiteral("coverUrl")).toString();
    e.sourceId = o.value(QStringLiteral("sourceId")).toString();
    if (e.sourceId.isEmpty())
        e.sourceId = defaultSourceId;
    e.sourcePageUrl = o.value(QStringLiteral("articleUrl")).toString();
    if (e.sourcePageUrl.isEmpty())
        e.sourcePageUrl = o.value(QStringLiteral("sourcePageUrl")).toString();
    e.version = o.value(QStringLiteral("version")).toString();
    e.sizeLabel = o.value(QStringLiteral("sizeLabel")).toString();
    if (e.sizeLabel.isEmpty())
        e.sizeLabel = o.value(QStringLiteral("fileSize")).toString();
    e.description = o.value(QStringLiteral("description")).toString();
    e.genres = o.value(QStringLiteral("genres")).toString();
    e.steamAppId = o.value(QStringLiteral("steamAppId")).toString();
    e.trailerUrl = o.value(QStringLiteral("trailerUrl")).toString();
    e.trailerThumbnailUrl = o.value(QStringLiteral("trailerThumbnailUrl")).toString();
    e.uploadDate = o.value(QStringLiteral("uploadDate")).toString();
    if (e.version.isEmpty() && !e.uploadDate.isEmpty())
        e.version = e.uploadDate.left(10);
    e.parentEntryId = o.value(QStringLiteral("parentEntryId")).toString();
    e.itemKind = itemKindFromString(o.value(QStringLiteral("kind")).toString());
    e.metadataPending = o.value(QStringLiteral("metadataPending")).toBool(false);
    e.recommendationsTotal = o.value(QStringLiteral("recommendationsTotal")).toInt(0);
    e.metacriticScore = o.value(QStringLiteral("metacriticScore")).toInt(0);
    e.currentPlayers = o.value(QStringLiteral("currentPlayers")).toInt(-1);
    e.hypeScore = o.value(QStringLiteral("hypeScore")).toDouble(0.0);

    const int rawKind = o.value(QStringLiteral("installKind")).toInt(-1);
    if (rawKind >= static_cast<int>(InstallKind::PortableArchive)
        && rawKind <= static_cast<int>(InstallKind::FixDownload)) {
        e.installKind = static_cast<InstallKind>(rawKind);
    }

    const QJsonArray uris = o.value(QStringLiteral("uris")).toArray();
    for (const QJsonValue& uri : uris)
        e.magnetUris.append(uri.toString());

    const QJsonArray shots = o.value(QStringLiteral("screenshotUrls")).toArray();
    for (const QJsonValue& s : shots)
        e.screenshotUrls.append(s.toString());

    const QJsonArray addons = o.value(QStringLiteral("addons")).toArray();
    e.addons.reserve(addons.size());
    for (const QJsonValue& v : addons) {
        if (v.isObject())
            e.addons.append(componentFromJson(v.toObject()));
    }

    if (e.id.isEmpty() && !e.steamAppId.isEmpty())
        e.id = QStringLiteral("steam-%1").arg(e.steamAppId);
    return e;
}

} // namespace

QByteArray serializePluginCatalogJson(const QVector<CatalogEntry>& entries)
{
    QJsonArray arr;
    for (const CatalogEntry& e : entries)
        arr.append(entryToJson(e));
    QJsonObject root;
    root.insert(QStringLiteral("schema"), QStringLiteral("arachnel.plugin.catalog.v1"));
    root.insert(QStringLiteral("entries"), arr);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QVector<CatalogEntry> parsePluginCatalogJson(const QByteArray& json,
                                             const QString& defaultSourceId)
{
    QVector<CatalogEntry> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonArray arr;
    if (doc.isObject()) {
        const QJsonObject root = doc.object();
        if (root.contains(QStringLiteral("entries")))
            arr = root.value(QStringLiteral("entries")).toArray();
        else if (root.contains(QStringLiteral("downloads")))
            arr = root.value(QStringLiteral("downloads")).toArray();
        else if (root.contains(QStringLiteral("games")))
            arr = root.value(QStringLiteral("games")).toArray();
    } else if (doc.isArray()) {
        arr = doc.array();
    }
    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject())
            continue;
        CatalogEntry e = entryFromJson(v.toObject(), defaultSourceId);
        prepareCatalogEntry(e);
        out.append(e);
    }
    return out;
}

} // namespace arachnel::core
