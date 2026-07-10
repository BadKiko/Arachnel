#include "game_metadata_service.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

namespace arachnel::core {

namespace {

QString cacheFilePath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/metadata-cache.json");
}

QString steamLibraryCover(const QString& appId)
{
    return QStringLiteral(
               "https://shared.fastly.steamstatic.com/store_item_assets/steam/apps/%1/"
               "library_600x900.jpg")
        .arg(appId);
}

} // namespace

GameMetadataService::GameMetadataService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    loadCache();
}

void GameMetadataService::loadCache()
{
    QFile file(cacheFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QJsonObject obj = it.value().toObject();
        GameMetadata metadata;
        metadata.coverUrl = obj.value(QStringLiteral("coverUrl")).toString();
        metadata.description = obj.value(QStringLiteral("description")).toString();
        metadata.genres = obj.value(QStringLiteral("genres")).toString();
        metadata.steamAppId = obj.value(QStringLiteral("steamAppId")).toString();
        m_cache.insert(it.key(), metadata);
    }
}

void GameMetadataService::saveCache()
{
    QJsonObject root;
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        const GameMetadata& metadata = it.value();
        QJsonObject obj;
        obj.insert(QStringLiteral("coverUrl"), metadata.coverUrl);
        obj.insert(QStringLiteral("description"), metadata.description);
        obj.insert(QStringLiteral("genres"), metadata.genres);
        obj.insert(QStringLiteral("steamAppId"), metadata.steamAppId);
        root.insert(it.key(), obj);
    }

    QFile file(cacheFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

GameMetadata GameMetadataService::metadataForTitle(const QString& title) const
{
    return m_cache.value(title);
}

void GameMetadataService::queueFetch(const QString& entryId, const QString& title,
                                     MetadataFetchMode mode)
{
    if (entryId.isEmpty() || title.isEmpty() || m_queuedIds.contains(entryId))
        return;

    const GameMetadata cached = m_cache.value(title);
    if (mode == MetadataFetchMode::CoverOnly && !cached.coverUrl.isEmpty()) {
        emit coverReady(entryId, cached.coverUrl);
        return;
    }
    if (mode == MetadataFetchMode::Full && !cached.coverUrl.isEmpty()
        && !cached.description.isEmpty()) {
        emit metadataReady(entryId, cached);
        return;
    }

    if (m_pending.size() >= kMaxQueueSize)
        return;

    m_queuedIds.insert(entryId);
    m_pending.append({entryId, title, mode});
    requestNext();
}

void GameMetadataService::requestNext()
{
    while (m_activeRequests < kMaxConcurrent && !m_pending.isEmpty()) {
        const PendingRequest request = m_pending.takeFirst();

        QUrl url(QStringLiteral("https://store.steampowered.com/api/storesearch/"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("term"), request.title);
        query.addQueryItem(QStringLiteral("cc"), QStringLiteral("US"));
        query.addQueryItem(QStringLiteral("l"), QStringLiteral("english"));
        url.setQuery(query);

        QNetworkRequest netRequest(url);
        netRequest.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
        QNetworkReply* reply = m_network->get(netRequest);
        reply->setProperty("entryId", request.entryId);
        reply->setProperty("entryTitle", request.title);
        reply->setProperty("fetchMode", static_cast<int>(request.mode));
        connect(reply, &QNetworkReply::finished, this,
                [this, reply]() { handleSearchFinished(reply); });
        ++m_activeRequests;
    }
}

void GameMetadataService::finishCover(const QString& entryId, const QString& title,
                                      const GameMetadata& metadata)
{
    m_cache.insert(title, metadata);
    saveCache();
    m_queuedIds.remove(entryId);
    emit coverReady(entryId, metadata.coverUrl);
}

void GameMetadataService::handleSearchFinished(QNetworkReply* reply)
{
    --m_activeRequests;

    const QString entryId = reply->property("entryId").toString();
    const QString entryTitle = reply->property("entryTitle").toString();
    const auto mode = static_cast<MetadataFetchMode>(reply->property("fetchMode").toInt());

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        m_queuedIds.remove(entryId);
        requestNext();
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    reply->deleteLater();

    if (items.isEmpty()) {
        m_queuedIds.remove(entryId);
        requestNext();
        return;
    }

    const QJsonObject first = items.first().toObject();
    const QString appId = QString::number(first.value(QStringLiteral("id")).toInt());

    GameMetadata metadata = m_cache.value(entryTitle);
    metadata.steamAppId = appId;

    const QString tinyImage = first.value(QStringLiteral("tiny_image")).toString();
    if (!tinyImage.isEmpty())
        metadata.coverUrl = tinyImage;
    else if (!appId.isEmpty())
        metadata.coverUrl = steamLibraryCover(appId);

    if (mode == MetadataFetchMode::CoverOnly) {
        finishCover(entryId, entryTitle, metadata);
        requestNext();
        return;
    }

    QUrl detailsUrl(QStringLiteral("https://store.steampowered.com/api/appdetails"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("appids"), appId);
    query.addQueryItem(QStringLiteral("l"), QStringLiteral("english"));
    detailsUrl.setQuery(query);

    QNetworkRequest request(detailsUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    QNetworkReply* detailsReply = m_network->get(request);
    detailsReply->setProperty("entryId", entryId);
    detailsReply->setProperty("entryTitle", entryTitle);
    detailsReply->setProperty("steamAppId", appId);
    detailsReply->setProperty("coverUrl", metadata.coverUrl);
    connect(detailsReply, &QNetworkReply::finished, this,
            [this, detailsReply]() { handleDetailsFinished(detailsReply); });
    ++m_activeRequests;

    requestNext();
}

void GameMetadataService::handleDetailsFinished(QNetworkReply* reply)
{
    --m_activeRequests;

    const QString entryId = reply->property("entryId").toString();
    const QString entryTitle = reply->property("entryTitle").toString();
    const QString appId = reply->property("steamAppId").toString();

    GameMetadata metadata;
    metadata.steamAppId = appId;
    metadata.coverUrl = reply->property("coverUrl").toString();
    if (metadata.coverUrl.isEmpty() && !appId.isEmpty())
        metadata.coverUrl = steamLibraryCover(appId);

    if (reply->error() == QNetworkReply::NoError) {
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject appRoot = root.value(appId).toObject();
        if (appRoot.value(QStringLiteral("success")).toBool()) {
            const QJsonObject data = appRoot.value(QStringLiteral("data")).toObject();
            metadata.description = data.value(QStringLiteral("short_description")).toString();
            const QString headerImage = data.value(QStringLiteral("header_image")).toString();
            if (!headerImage.isEmpty())
                metadata.coverUrl = headerImage;

            QStringList genres;
            const QJsonArray genreArray = data.value(QStringLiteral("genres")).toArray();
            genres.reserve(genreArray.size());
            for (const QJsonValue& genreValue : genreArray)
                genres.append(genreValue.toObject().value(QStringLiteral("description")).toString());
            metadata.genres = genres.join(QStringLiteral(", "));
        }
    }

    reply->deleteLater();
    m_cache.insert(entryTitle, metadata);
    saveCache();
    m_queuedIds.remove(entryId);
    emit metadataReady(entryId, metadata);
    requestNext();
}

} // namespace arachnel::core
