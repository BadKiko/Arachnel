#include "workshop_service.h"

#include "cover_image_cache.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

namespace arachnel::core {

namespace {

constexpr qint64 kCacheTtlSecs = 6 * 60 * 60;
constexpr int kMaxScreenshots = 12;
constexpr int kMaxScreenshotConcurrency = 4;
const QString kUserAgent = QStringLiteral("Arachnel/0.1");

QString formatSize(qint64 bytes)
{
    if (bytes <= 0)
        return {};
    const double mb = double(bytes) / (1024.0 * 1024.0);
    if (mb >= 1024.0)
        return QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 1);
    if (mb >= 10.0)
        return QStringLiteral("%1 MB").arg(qRound(mb));
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
}

QString normalizeUgcUrl(QString url)
{
    url.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    const int q = url.indexOf(QLatin1Char('?'));
    if (q >= 0)
        url = url.left(q);
    if (!url.endsWith(QLatin1Char('/')))
        url.append(QLatin1Char('/'));
    return url;
}

QStringList extractWorkshopScreenshots(const QByteArray& htmlBytes)
{
    const QString html = QString::fromUtf8(htmlBytes);
    static const QRegularExpression previewRe(
        QStringLiteral(
            R"('previewid'\s*:\s*'[^']+'\s*,\s*'url'\s*:\s*'(https://images\.steamusercontent\.com/ugc/[0-9A-Fa-f]+/[0-9A-Fa-f]+/?))"),
        QRegularExpression::CaseInsensitiveOption);

    QStringList out;
    QSet<QString> seen;
    auto it = previewRe.globalMatch(html);
    while (it.hasNext()) {
        const QString url = normalizeUgcUrl(it.next().captured(1));
        if (url.isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        out.append(url);
        if (out.size() >= kMaxScreenshots)
            break;
    }

    if (out.isEmpty()) {
        static const QRegularExpression mainRe(
            QStringLiteral(
                R"(id="previewImageMain"[^>]*src="(https://images\.steamusercontent\.com/ugc/[0-9A-Fa-f]+/[0-9A-Fa-f]+/?))"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = mainRe.match(html);
        if (m.hasMatch())
            out.append(normalizeUgcUrl(m.captured(1)));
    }
    return out;
}

QStringList mergePreviewFirst(const QString& previewUrl, QStringList shots)
{
    if (previewUrl.isEmpty())
        return shots;
    const QString normalized = normalizeUgcUrl(previewUrl);
    shots.removeAll(normalized);
    // preview_url from the API often has a different host/path shape - keep raw if normalize differs.
    shots.removeAll(previewUrl);
    shots.prepend(previewUrl);
    while (shots.size() > kMaxScreenshots)
        shots.removeLast();
    return shots;
}

} // namespace

WorkshopService::WorkshopService(CoverImageCache* covers, QObject* parent)
    : QObject(parent)
    , m_covers(covers)
    , m_network(new QNetworkAccessManager(this))
{
    if (m_covers) {
        connect(m_covers, &CoverImageCache::ready, this,
                [this](const QString& remoteUrl, const QString& localUrl) {
                    emit previewReady(remoteUrl, localUrl);
                    for (auto it = m_itemsByApp.begin(); it != m_itemsByApp.end(); ++it) {
                        for (WorkshopItem& item : it.value()) {
                            if (item.previewUrl == remoteUrl)
                                item.localPreviewUrl = localUrl;
                        }
                    }
                });
    }
}

void WorkshopService::requestPage(const QString& steamAppId, int page)
{
    const QString appId = steamAppId.trimmed();
    if (appId.isEmpty() || page < 1)
        return;

    const QString flightKey = appId + QLatin1Char(':') + QString::number(page);
    if (m_inFlight.contains(flightKey))
        return;

    QVector<WorkshopItem> cached;
    bool hasMore = false;
    if (loadCache(appId, page, &cached, &hasMore)) {
        applyScreenshotCaches(&cached);
        applyLocalPreviews(&cached);
        if (page <= 1)
            m_itemsByApp.insert(appId, cached);
        else {
            auto& all = m_itemsByApp[appId];
            for (const WorkshopItem& item : cached) {
                bool found = false;
                for (const WorkshopItem& existing : all) {
                    if (existing.publishedFileId == item.publishedFileId) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    all.append(item);
            }
        }
        emitPage(appId, page, cached, hasMore);
        enqueueScreenshotFetches(appId, cached);
        return;
    }

    m_inFlight.insert(flightKey);
    fetchBrowsePage(appId, page);
}

QVariantList WorkshopService::itemsForApp(const QString& steamAppId) const
{
    QVariantList out;
    const auto it = m_itemsByApp.constFind(steamAppId.trimmed());
    if (it == m_itemsByApp.cend())
        return out;
    for (const WorkshopItem& item : it.value())
        out.append(itemToMap(item));
    return out;
}

void WorkshopService::requestPreview(const QString& previewUrl)
{
    if (!m_covers || previewUrl.isEmpty())
        return;
    const QString local = m_covers->localUrlFor(previewUrl);
    if (!local.isEmpty()) {
        emit previewReady(previewUrl, local);
        return;
    }
    m_covers->ensure(previewUrl, CoverFetchPriority::Visible);
}

void WorkshopService::releasePreview(const QString& previewUrl)
{
    if (m_covers && !previewUrl.isEmpty())
        m_covers->release(previewUrl);
}

QString WorkshopService::localPreviewUrl(const QString& previewUrl) const
{
    if (!m_covers || previewUrl.isEmpty())
        return {};
    return m_covers->localUrlFor(previewUrl);
}

std::optional<WorkshopItem> WorkshopService::itemById(const QString& steamAppId,
                                                      const QString& publishedFileId) const
{
    const auto it = m_itemsByApp.constFind(steamAppId.trimmed());
    if (it == m_itemsByApp.cend())
        return std::nullopt;
    for (const WorkshopItem& item : it.value()) {
        if (item.publishedFileId == publishedFileId)
            return item;
    }
    return std::nullopt;
}

void WorkshopService::fetchBrowsePage(const QString& steamAppId, int page)
{
    QUrl url(QStringLiteral("https://steamcommunity.com/workshop/browse/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("appid"), steamAppId);
    query.addQueryItem(QStringLiteral("browsesort"), QStringLiteral("trend"));
    query.addQueryItem(QStringLiteral("section"), QStringLiteral("readytouseitems"));
    query.addQueryItem(QStringLiteral("actualsort"), QStringLiteral("trend"));
    query.addQueryItem(QStringLiteral("p"), QString::number(page));
    query.addQueryItem(QStringLiteral("numperpage"), QStringLiteral("30"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("steamAppId", steamAppId);
    reply->setProperty("page", page);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QString appId = reply->property("steamAppId").toString();
        const int page = reply->property("page").toInt();
        const QString flightKey = appId + QLatin1Char(':') + QString::number(page);
        m_inFlight.remove(flightKey);

        if (reply->error() != QNetworkReply::NoError) {
            emit pageFailed(appId, page, reply->errorString());
            reply->deleteLater();
            return;
        }

        const QString html = QString::fromUtf8(reply->readAll());
        reply->deleteLater();

        static const QRegularExpression idRe(
            QStringLiteral(R"(sharedfiles/filedetails/\?id=(\d+))"));
        QStringList ids;
        QSet<QString> seen;
        auto it = idRe.globalMatch(html);
        while (it.hasNext()) {
            const QString id = it.next().captured(1);
            if (seen.contains(id))
                continue;
            seen.insert(id);
            ids.append(id);
        }

        if (ids.isEmpty()) {
            emitPage(appId, page, {}, false);
            saveCache(appId, page, {}, false);
            return;
        }

        fetchDetails(appId, page, ids);
    });
}

void WorkshopService::fetchDetails(const QString& steamAppId, int page, const QStringList& ids)
{
    QUrl url(QStringLiteral(
        "https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/"));
    QUrlQuery body;
    body.addQueryItem(QStringLiteral("itemcount"), QString::number(ids.size()));
    for (int i = 0; i < ids.size(); ++i) {
        body.addQueryItem(QStringLiteral("publishedfileids[%1]").arg(i), ids.at(i));
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QNetworkReply* reply = m_network->post(request, body.query(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("steamAppId", steamAppId);
    reply->setProperty("page", page);
    reply->setProperty("idCount", ids.size());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QString appId = reply->property("steamAppId").toString();
        const int page = reply->property("page").toInt();
        const int idCount = reply->property("idCount").toInt();

        if (reply->error() != QNetworkReply::NoError) {
            emit pageFailed(appId, page, reply->errorString());
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        const QJsonArray details =
            doc.object().value(QStringLiteral("response")).toObject().value(
                QStringLiteral("publishedfiledetails")).toArray();

        QVector<WorkshopItem> items;
        items.reserve(details.size());
        for (const QJsonValue& v : details) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("result")).toInt() != 1)
                continue;
            WorkshopItem item;
            item.publishedFileId = o.value(QStringLiteral("publishedfileid")).toString();
            item.title = o.value(QStringLiteral("title")).toString();
            item.description = o.value(QStringLiteral("description")).toString();
            item.previewUrl = o.value(QStringLiteral("preview_url")).toString();
            item.fileSize = o.value(QStringLiteral("file_size")).toVariant().toLongLong();
            item.timeUpdated = o.value(QStringLiteral("time_updated")).toVariant().toLongLong();
            item.subscriptions = o.value(QStringLiteral("subscriptions")).toInt();
            if (item.publishedFileId.isEmpty() || item.title.isEmpty())
                continue;
            if (!item.previewUrl.isEmpty())
                item.screenshotUrls = QStringList{item.previewUrl};
            items.append(item);
        }

        applyScreenshotCaches(&items);
        applyLocalPreviews(&items);
        const bool hasMore = idCount >= 30;
        if (page <= 1)
            m_itemsByApp.insert(appId, items);
        else {
            auto& all = m_itemsByApp[appId];
            for (const WorkshopItem& item : items) {
                bool found = false;
                for (WorkshopItem& existing : all) {
                    if (existing.publishedFileId == item.publishedFileId) {
                        existing = item;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    all.append(item);
            }
        }
        saveCache(appId, page, items, hasMore);
        emitPage(appId, page, items, hasMore);
        enqueueScreenshotFetches(appId, items);

        if (m_covers) {
            for (const WorkshopItem& item : items) {
                for (const QString& url : item.screenshotUrls) {
                    if (!url.isEmpty() && m_covers->localUrlFor(url).isEmpty())
                        m_covers->ensure(url, CoverFetchPriority::Warm);
                }
            }
        }
    });
}

void WorkshopService::emitPage(const QString& steamAppId, int page,
                               const QVector<WorkshopItem>& items, bool hasMore)
{
    QVariantList list;
    list.reserve(items.size());
    for (const WorkshopItem& item : items)
        list.append(itemToMap(item));
    emit pageReady(steamAppId, page, list, hasMore);
}

bool WorkshopService::loadCache(const QString& steamAppId, int page, QVector<WorkshopItem>* out,
                                bool* hasMore)
{
    QFile f(cachePath(steamAppId, page));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    const QJsonObject root = doc.object();
    const qint64 fetchedAt = root.value(QStringLiteral("fetchedAt")).toVariant().toLongLong();
    if (fetchedAt <= 0
        || QDateTime::currentSecsSinceEpoch() - fetchedAt > kCacheTtlSecs)
        return false;

    *hasMore = root.value(QStringLiteral("hasMore")).toBool();
    out->clear();
    for (const QJsonValue& v : root.value(QStringLiteral("items")).toArray()) {
        const QJsonObject o = v.toObject();
        WorkshopItem item;
        item.publishedFileId = o.value(QStringLiteral("id")).toString();
        item.title = o.value(QStringLiteral("title")).toString();
        item.description = o.value(QStringLiteral("description")).toString();
        item.previewUrl = o.value(QStringLiteral("previewUrl")).toString();
        item.fileSize = o.value(QStringLiteral("fileSize")).toVariant().toLongLong();
        item.timeUpdated = o.value(QStringLiteral("timeUpdated")).toVariant().toLongLong();
        item.subscriptions = o.value(QStringLiteral("subscriptions")).toInt();
        for (const QJsonValue& shot : o.value(QStringLiteral("screenshotUrls")).toArray()) {
            const QString url = shot.toString();
            if (!url.isEmpty())
                item.screenshotUrls.append(url);
        }
        item.screenshotsResolved = o.value(QStringLiteral("screenshotsResolved")).toBool();
        if (item.screenshotUrls.isEmpty() && !item.previewUrl.isEmpty())
            item.screenshotUrls = QStringList{item.previewUrl};
        if (!item.publishedFileId.isEmpty())
            out->append(item);
    }
    return true;
}

void WorkshopService::saveCache(const QString& steamAppId, int page,
                                const QVector<WorkshopItem>& items, bool hasMore)
{
    const QString path = cachePath(steamAppId, page);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonArray arr;
    for (const WorkshopItem& item : items) {
        QJsonArray shots;
        for (const QString& url : item.screenshotUrls)
            shots.append(url);
        arr.append(QJsonObject{
            {QStringLiteral("id"), item.publishedFileId},
            {QStringLiteral("title"), item.title},
            {QStringLiteral("description"), item.description},
            {QStringLiteral("previewUrl"), item.previewUrl},
            {QStringLiteral("fileSize"), item.fileSize},
            {QStringLiteral("timeUpdated"), item.timeUpdated},
            {QStringLiteral("subscriptions"), item.subscriptions},
            {QStringLiteral("screenshotUrls"), shots},
            {QStringLiteral("screenshotsResolved"), item.screenshotsResolved},
        });
    }
    QJsonObject root;
    root.insert(QStringLiteral("fetchedAt"), QDateTime::currentSecsSinceEpoch());
    root.insert(QStringLiteral("hasMore"), hasMore);
    root.insert(QStringLiteral("items"), arr);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString WorkshopService::cachePath(const QString& steamAppId, int page) const
{
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/workshop-cache/") + steamAppId;
    return root + QStringLiteral("/page-%1.json").arg(page);
}

QString WorkshopService::screenshotCachePath(const QString& publishedFileId) const
{
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/workshop-cache/screenshots");
    return root + QLatin1Char('/') + publishedFileId + QStringLiteral(".json");
}

bool WorkshopService::loadScreenshotCache(const QString& publishedFileId, QStringList* out) const
{
    if (!out || publishedFileId.isEmpty())
        return false;
    QFile f(screenshotCachePath(publishedFileId));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    const QJsonObject root = doc.object();
    const qint64 fetchedAt = root.value(QStringLiteral("fetchedAt")).toVariant().toLongLong();
    if (fetchedAt <= 0
        || QDateTime::currentSecsSinceEpoch() - fetchedAt > kCacheTtlSecs)
        return false;
    out->clear();
    for (const QJsonValue& v : root.value(QStringLiteral("urls")).toArray()) {
        const QString url = v.toString();
        if (!url.isEmpty())
            out->append(url);
    }
    return !out->isEmpty();
}

void WorkshopService::saveScreenshotCache(const QString& publishedFileId,
                                          const QStringList& urls) const
{
    if (publishedFileId.isEmpty())
        return;
    const QString path = screenshotCachePath(publishedFileId);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonArray arr;
    for (const QString& url : urls)
        arr.append(url);
    QJsonObject root;
    root.insert(QStringLiteral("fetchedAt"), QDateTime::currentSecsSinceEpoch());
    root.insert(QStringLiteral("urls"), arr);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void WorkshopService::applyScreenshotCaches(QVector<WorkshopItem>* items)
{
    if (!items)
        return;
    for (WorkshopItem& item : *items) {
        if (item.screenshotsResolved && item.screenshotUrls.size() > 1)
            continue;
        QStringList cached;
        if (!loadScreenshotCache(item.publishedFileId, &cached))
            continue;
        item.screenshotUrls = mergePreviewFirst(item.previewUrl, cached);
        item.screenshotsResolved = true;
    }
}

void WorkshopService::enqueueScreenshotFetches(const QString& steamAppId,
                                               const QVector<WorkshopItem>& items)
{
    for (const WorkshopItem& item : items) {
        if (item.publishedFileId.isEmpty() || item.screenshotsResolved)
            continue;
        if (m_screenshotInFlight.contains(item.publishedFileId))
            continue;
        m_screenshotQueue.enqueue(ScreenshotJob{steamAppId, item.publishedFileId});
        m_screenshotInFlight.insert(item.publishedFileId);
    }
    pumpScreenshotQueue();
}

void WorkshopService::pumpScreenshotQueue()
{
    while (m_screenshotActive < kMaxScreenshotConcurrency && !m_screenshotQueue.isEmpty()) {
        const ScreenshotJob job = m_screenshotQueue.dequeue();
        ++m_screenshotActive;
        fetchItemScreenshots(job);
    }
}

void WorkshopService::fetchItemScreenshots(const ScreenshotJob& job)
{
    QUrl url(QStringLiteral("https://steamcommunity.com/sharedfiles/filedetails/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("id"), job.publishedFileId);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("steamAppId", job.steamAppId);
    reply->setProperty("publishedFileId", job.publishedFileId);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QString appId = reply->property("steamAppId").toString();
        const QString fileId = reply->property("publishedFileId").toString();
        --m_screenshotActive;
        m_screenshotInFlight.remove(fileId);

        QStringList shots;
        if (reply->error() == QNetworkReply::NoError)
            shots = extractWorkshopScreenshots(reply->readAll());
        reply->deleteLater();

        QString previewUrl;
        if (auto item = itemById(appId, fileId))
            previewUrl = item->previewUrl;
        if (shots.isEmpty() && !previewUrl.isEmpty())
            shots = QStringList{previewUrl};
        else
            shots = mergePreviewFirst(previewUrl, shots);

        if (!shots.isEmpty())
            saveScreenshotCache(fileId, shots);
        applyScreenshots(appId, fileId, shots);
        pumpScreenshotQueue();
    });
}

void WorkshopService::applyScreenshots(const QString& steamAppId, const QString& publishedFileId,
                                       const QStringList& urls)
{
    auto it = m_itemsByApp.find(steamAppId);
    if (it == m_itemsByApp.end())
        return;
    for (WorkshopItem& item : it.value()) {
        if (item.publishedFileId != publishedFileId)
            continue;
        item.screenshotUrls = urls;
        item.screenshotsResolved = true;
        if (m_covers) {
            for (const QString& url : urls) {
                if (!url.isEmpty() && m_covers->localUrlFor(url).isEmpty())
                    m_covers->ensure(url, CoverFetchPriority::Warm);
            }
        }
        emit itemUpdated(steamAppId, itemToMap(item));
        return;
    }
}

QVariantMap WorkshopService::itemToMap(const WorkshopItem& item) const
{
    QVariantList urls;
    if (!item.screenshotUrls.isEmpty()) {
        for (const QString& remote : item.screenshotUrls) {
            if (urls.size() >= kMaxScreenshots)
                break;
            if (remote.isEmpty())
                continue;
            const QString local = m_covers ? m_covers->localUrlFor(remote) : QString();
            urls.append(local.isEmpty() ? remote : local);
        }
    } else if (!item.localPreviewUrl.isEmpty()) {
        urls.append(item.localPreviewUrl);
    } else if (!item.previewUrl.isEmpty()) {
        urls.append(item.previewUrl);
    }

    return {
        {QStringLiteral("publishedFileId"), item.publishedFileId},
        {QStringLiteral("title"), item.title},
        {QStringLiteral("description"), item.description},
        {QStringLiteral("previewUrl"), item.previewUrl},
        {QStringLiteral("localPreviewUrl"), item.localPreviewUrl},
        {QStringLiteral("fileSize"), item.fileSize},
        {QStringLiteral("sizeLabel"), formatSize(item.fileSize)},
        {QStringLiteral("timeUpdated"), item.timeUpdated},
        {QStringLiteral("subscriptions"), item.subscriptions},
        {QStringLiteral("previewUrls"), urls},
        {QStringLiteral("screenshotsResolved"), item.screenshotsResolved},
    };
}

void WorkshopService::applyLocalPreviews(QVector<WorkshopItem>* items)
{
    if (!m_covers || !items)
        return;
    for (WorkshopItem& item : *items) {
        if (!item.previewUrl.isEmpty())
            item.localPreviewUrl = m_covers->localUrlFor(item.previewUrl);
    }
}

} // namespace arachnel::core
