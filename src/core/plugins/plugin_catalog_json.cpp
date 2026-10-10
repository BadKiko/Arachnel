#include "plugin_catalog_json.h"

#include "json_row_scanner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent>

#include <algorithm>

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
    o.insert(QStringLiteral("contentAvailable"), c.contentAvailable);
    o.insert(QStringLiteral("coverUrl"), c.coverUrl);
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
    QJsonArray shots;
    for (const QString& u : c.screenshotUrls)
        shots.append(u);
    if (!shots.isEmpty())
        o.insert(QStringLiteral("screenshotUrls"), shots);
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
    c.contentAvailable = o.value(QStringLiteral("contentAvailable")).toBool(true);
    if (o.contains(QStringLiteral("hasManifest")))
        c.contentAvailable = o.value(QStringLiteral("hasManifest")).toBool(true);
    c.coverUrl = o.value(QStringLiteral("coverUrl")).toString();
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
    for (const QJsonValue& shot : o.value(QStringLiteral("screenshotUrls")).toArray()) {
        const QString u = shot.toString().trimmed();
        if (!u.isEmpty())
            c.screenshotUrls.append(u);
    }
    return c;
}

QJsonObject entryToJson(const CatalogEntry& e)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), e.id);
    o.insert(QStringLiteral("title"), e.title);
    o.insert(QStringLiteral("coverUrl"),
             !e.remoteCoverUrl.isEmpty() ? e.remoteCoverUrl : e.coverUrl);
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
    o.insert(QStringLiteral("hasWorkshop"), e.hasWorkshop);
    o.insert(QStringLiteral("dlcCount"), e.dlcCount);
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
    {
        const QString feedCover = o.value(QStringLiteral("coverUrl")).toString().trimmed();
        if (feedCover.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
            || feedCover.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
            e.remoteCoverUrl = feedCover;
        } else {
            e.coverUrl = feedCover;
        }
    }
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
    e.hasWorkshop = o.value(QStringLiteral("hasWorkshop")).toBool(false);
    e.dlcCount = o.value(QStringLiteral("dlcCount")).toInt(0);
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
    return parsePluginCatalogJsonEx(json, defaultSourceId, nullptr);
}

namespace {

// A feed this big as one QJsonDocument is a DOM several times its size (a 49 MB catalog peaked
// at ~1 GB, and the heap stayed committed afterwards). Cut the rows out without parsing them
// and parse them in small batches, in parallel; only one batch is ever a DOM.
constexpr qsizetype kStreamingThreshold = 4 * 1024 * 1024;
constexpr qsizetype kRowsPerBatch = 1024;

bool parseRowsStreaming(const QByteArray& json, const QString& defaultSourceId,
                        QVector<CatalogEntry>* out, QStringList* supersedes)
{
    const char* data = json.constData();
    const qsizetype n = json.size();
    jsonrows::Range range;
    QVector<jsonrows::RowSpan> spans;
    const QByteArrayList rowKeys{QByteArrayLiteral("entries"), QByteArrayLiteral("downloads"),
                                 QByteArrayLiteral("games")};
    if (!jsonrows::locateRows(data, n, rowKeys, &range)
        || !jsonrows::collectRowSpans(data, n, range, &spans) || spans.isEmpty()) {
        return false;
    }

    if (supersedes) {
        jsonrows::Range sup;
        if (jsonrows::locateRows(data, n, {QByteArrayLiteral("supersedes")}, &sup) && sup.valid()) {
            const QByteArray arr = QByteArray("[") + QByteArray::fromRawData(data + sup.begin, sup.end - sup.begin) + "]";
            for (const QJsonValue& v : QJsonDocument::fromJson(arr).array()) {
                const QString id = v.toString();
                if (!id.isEmpty())
                    supersedes->append(id);
            }
        }
    }

    struct Batch {
        qsizetype first;
        qsizetype last;
    };
    QVector<Batch> batches;
    for (qsizetype i = 0; i < spans.size(); i += kRowsPerBatch)
        batches.append({i, std::min<qsizetype>(i + kRowsPerBatch, spans.size())});

    const auto parseBatch = [&](const Batch& b) {
        QByteArray input;
        input.reserve((spans.at(b.last - 1).end - spans.at(b.first).begin) + (b.last - b.first) + 2);
        input.append('[');
        for (qsizetype i = b.first; i < b.last; ++i) {
            if (i > b.first)
                input.append(',');
            input.append(data + spans.at(i).begin, spans.at(i).end - spans.at(i).begin);
        }
        input.append(']');
        QVector<CatalogEntry> rows;
        const QJsonArray arr = QJsonDocument::fromJson(input).array();
        rows.reserve(arr.size());
        for (const QJsonValue& v : arr) {
            if (v.isObject())
                rows.append(entryFromJson(v.toObject(), defaultSourceId));
        }
        return rows;
    };
    QVector<QVector<CatalogEntry>> parts =
        QtConcurrent::blockingMapped<QVector<QVector<CatalogEntry>>>(batches, parseBatch);

    qsizetype total = 0;
    for (const auto& part : std::as_const(parts))
        total += part.size();
    out->reserve(total);
    for (auto& part : parts) {
        for (CatalogEntry& e : part)
            out->append(std::move(e));
        part.clear();
        part.squeeze();
    }
    return true;
}

} // namespace

QVector<CatalogEntry> parsePluginCatalogJsonEx(const QByteArray& json,
                                               const QString& defaultSourceId,
                                               QStringList* supersedes)
{
    QVector<CatalogEntry> out;
    if (json.size() >= kStreamingThreshold && parseRowsStreaming(json, defaultSourceId, &out, supersedes))
        return out;
    out.clear();
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonArray arr;
    if (doc.isObject()) {
        const QJsonObject root = doc.object();
        if (supersedes) {
            for (const QJsonValue& v : root.value(QStringLiteral("supersedes")).toArray()) {
                const QString id = v.toString();
                if (!id.isEmpty())
                    supersedes->append(id);
            }
        }
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
        out.append(e);
    }
    return out;
}

} // namespace arachnel::core
