#include "game_metadata_service.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace arachnel::core {

GameMetadataService::GameMetadataService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

GameMetadata GameMetadataService::metadataForTitle(const QString& title) const
{
    return m_cache.value(title);
}

void GameMetadataService::enrichEntries(QVector<CatalogEntry>& entries)
{
    m_pending.clear();
    for (auto& entry : entries) {
        if (!entry.coverUrl.isEmpty() && !entry.description.isEmpty()) {
            entry.metadataPending = false;
            continue;
        }

        const GameMetadata cached = m_cache.value(entry.title);
        if (!cached.coverUrl.isEmpty()) {
            entry.coverUrl = cached.coverUrl;
            entry.description = cached.description;
            entry.genres = cached.genres;
            entry.metadataPending = false;
            continue;
        }

        m_pending.append(&entry);
    }

    if (m_pending.isEmpty()) {
        emit enrichmentFinished();
        return;
    }

    requestNext();
}

void GameMetadataService::requestNext()
{
    while (m_activeRequests < kMaxConcurrent && !m_pending.isEmpty()) {
        CatalogEntry* entry = m_pending.takeFirst();
        entry->metadataPending = true;

        QUrl url(QStringLiteral("https://store.steampowered.com/api/storesearch/"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("term"), entry->title);
        query.addQueryItem(QStringLiteral("cc"), QStringLiteral("US"));
        query.addQueryItem(QStringLiteral("l"), QStringLiteral("english"));
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
        QNetworkReply* reply = m_network->get(request);
        reply->setProperty("entryId", entry->id);
        reply->setProperty("entryTitle", entry->title);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply]() { handleSearchFinished(reply); });
        ++m_activeRequests;
    }
}

void GameMetadataService::handleSearchFinished(QNetworkReply* reply)
{
    --m_activeRequests;

    const QString entryId = reply->property("entryId").toString();
    const QString entryTitle = reply->property("entryTitle").toString();

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        emit entryEnriched(entryId);
        if (m_activeRequests == 0 && m_pending.isEmpty())
            emit enrichmentFinished();
        requestNext();
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    reply->deleteLater();

    if (items.isEmpty()) {
        emit entryEnriched(entryId);
        if (m_activeRequests == 0 && m_pending.isEmpty())
            emit enrichmentFinished();
        requestNext();
        return;
    }

    const QJsonObject first = items.first().toObject();
    const QString appId = QString::number(first.value(QStringLiteral("id")).toInt());

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
    connect(detailsReply, &QNetworkReply::finished, this,
            [this, detailsReply]() { handleDetailsFinished(detailsReply); });
    ++m_activeRequests;

    if (m_activeRequests == 0 && m_pending.isEmpty())
        emit enrichmentFinished();
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
    metadata.coverUrl =
        QStringLiteral("https://shared.fastly.steamstatic.com/store_item_assets/steam/apps/%1/"
                       "library_600x900.jpg")
            .arg(appId);

    if (reply->error() == QNetworkReply::NoError) {
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject appRoot = root.value(appId).toObject();
        if (appRoot.value(QStringLiteral("success")).toBool()) {
            const QJsonObject data = appRoot.value(QStringLiteral("data")).toObject();
            metadata.description = data.value(QStringLiteral("short_description")).toString();
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
    emit entryEnriched(entryId);

    if (m_activeRequests == 0 && m_pending.isEmpty())
        emit enrichmentFinished();
    requestNext();
}

} // namespace arachnel::core
