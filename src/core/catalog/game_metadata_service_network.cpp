#include "game_metadata_service.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace arachnel::core {

#include "game_metadata_service_helpers.h"

void GameMetadataService::requestStoreAssets(const QString& entryId, const QString& title,
                                             const QString& appId, MetadataFetchMode mode,
                                             const QStringList& remainingParentTerms,
                                             const QString& languageCode)
{
    const QString steamLanguage = steamLanguageForUi(languageCode);
    QJsonObject payload;
    QJsonArray ids;
    ids.append(QJsonObject{{QStringLiteral("appid"), appId.toLongLong()}});
    payload.insert(QStringLiteral("ids"), ids);
    payload.insert(QStringLiteral("context"),
                   QJsonObject{{QStringLiteral("language"), steamLanguage},
                               {QStringLiteral("country_code"), QStringLiteral("US")},
                               {QStringLiteral("steam_realm"), 1}});
    payload.insert(QStringLiteral("data_request"),
                   QJsonObject{{QStringLiteral("include_assets"), true},
                               {QStringLiteral("include_trailers"),
                                mode == MetadataFetchMode::Full}});

    QUrl url(QStringLiteral("https://api.steampowered.com/IStoreBrowseService/GetItems/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("input_json"),
                       QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("entryId", entryId);
    reply->setProperty("entryTitle", title);
    reply->setProperty("steamAppId", appId);
    reply->setProperty("fetchMode", static_cast<int>(mode));
    reply->setProperty("parentTerms", remainingParentTerms);
    reply->setProperty("languageCode", languageCode);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { handleAssetsFinished(reply); });
    ++m_activeRequests;
}

void GameMetadataService::requestAppDetails(const QString& entryId, const QString& title,
                                            const QString& appId, const QString& coverUrl,
                                            MetadataFetchMode mode, const QString& languageCode)
{
    const QString steamLanguage = steamLanguageForUi(languageCode);
    QUrl detailsUrl(QStringLiteral("https://store.steampowered.com/api/appdetails"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("appids"), appId);
    query.addQueryItem(QStringLiteral("l"), steamLanguage);
    detailsUrl.setQuery(query);

    QNetworkRequest request(detailsUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    QNetworkReply* detailsReply = m_network->get(request);
    detailsReply->setProperty("entryId", entryId);
    detailsReply->setProperty("entryTitle", title);
    detailsReply->setProperty("steamAppId", appId);
    detailsReply->setProperty("coverUrl", coverUrl);
    detailsReply->setProperty("fetchMode", static_cast<int>(mode));
    detailsReply->setProperty("languageCode", languageCode);
    connect(detailsReply, &QNetworkReply::finished, this,
            [this, detailsReply]() { handleDetailsFinished(detailsReply); });
    ++m_activeRequests;
}

void GameMetadataService::handleSearchFinished(QNetworkReply* reply)
{
    --m_activeRequests;

    const QString entryId = reply->property("entryId").toString();
    const QString entryTitle = reply->property("entryTitle").toString();
    const QString searchTerm = reply->property("searchTerm").toString();
    const int termIndex = reply->property("termIndex").toInt();
    const QStringList searchTerms = reply->property("searchTerms").toStringList();
    const auto mode = static_cast<MetadataFetchMode>(reply->property("fetchMode").toInt());
    const QString languageCode = reply->property("languageCode").toString();

    auto retryNextTerm = [&]() {
        reply->deleteLater();
        m_inFlight.remove(entryId);
        prependPending({entryId, entryTitle, searchTerms, termIndex + 1, mode, languageCode});
    };

    if (reply->error() != QNetworkReply::NoError) {
        retryNextTerm();
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();

    QJsonObject best = bestSearchItem(items, entryTitle);
    int score = best.isEmpty()
                    ? -1
                    : titleMatchScore(entryTitle, best.value(QStringLiteral("name")).toString());
    if (score < 500) {
        const QJsonObject byTerm = bestSearchItem(items, searchTerm);
        const int termScore =
            byTerm.isEmpty()
                ? -1
                : titleMatchScore(searchTerm, byTerm.value(QStringLiteral("name")).toString());
        if (termScore > score) {
            best = byTerm;
            score = termScore;
        }
    }

    if (best.isEmpty() || score < 500) {
        retryNextTerm();
        return;
    }

    reply->deleteLater();
    const QString appId = QString::number(best.value(QStringLiteral("id")).toInt());
    QStringList parentTerms;
    for (int i = termIndex + 1; i < searchTerms.size(); ++i)
        parentTerms.append(searchTerms.at(i));
    requestStoreAssets(entryId, entryTitle, appId, mode, parentTerms, languageCode);
    requestNext();
}

void GameMetadataService::handleAssetsFinished(QNetworkReply* reply)
{
    --m_activeRequests;

    const QString entryId = reply->property("entryId").toString();
    const QString entryTitle = reply->property("entryTitle").toString();
    const QString appId = reply->property("steamAppId").toString();
    const auto mode = static_cast<MetadataFetchMode>(reply->property("fetchMode").toInt());
    const QStringList parentTerms = reply->property("parentTerms").toStringList();
    const QString languageCode = reply->property("languageCode").toString();

    GameMetadata metadata = m_cache.value(entryTitle);
    metadata.steamAppId = appId;

    if (reply->error() == QNetworkReply::NoError) {
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray storeItems = root.value(QStringLiteral("response"))
                                          .toObject()
                                          .value(QStringLiteral("store_items"))
                                          .toArray();
        if (!storeItems.isEmpty()) {
            const QJsonObject storeItem = storeItems.first().toObject();
            const QJsonObject assets = storeItem.value(QStringLiteral("assets")).toObject();
            metadata.coverUrl = pickLibraryCover(assets);
            if (mode == MetadataFetchMode::Full) {
                const QJsonObject trailers = storeItem.value(QStringLiteral("trailers")).toObject();
                const QString trailer = pickTrailerFromStoreTrailers(trailers);
                if (!trailer.isEmpty())
                    metadata.trailerUrl = trailer;
                const QString thumbnail = pickTrailerThumbnailFromStoreTrailers(trailers);
                if (!thumbnail.isEmpty())
                    metadata.trailerThumbnailUrl = thumbnail;
            }
        }
    }
    reply->deleteLater();

    if (!metadata.trailerUrl.isEmpty() || !metadata.coverUrl.isEmpty()) {
        m_cache.insert(entryTitle, metadata);
        m_saveTimer->start();
    }

    if (metadata.coverUrl.isEmpty() && !parentTerms.isEmpty()
        && mode == MetadataFetchMode::CoverOnly) {
        m_inFlight.remove(entryId);
        prependPending({entryId, entryTitle, parentTerms, 0, mode, languageCode});
        return;
    }

    if (mode == MetadataFetchMode::CoverOnly) {
        if (metadata.coverUrl.isEmpty()) {
            failCover(entryId);
            return;
        }
        finishCover(entryId, entryTitle, metadata);
        requestNext();
        return;
    }

    requestAppDetails(entryId, entryTitle, appId, metadata.coverUrl, mode, languageCode);
    requestNext();
}

void GameMetadataService::handleDetailsFinished(QNetworkReply* reply)
{
    --m_activeRequests;

    const QString entryId = reply->property("entryId").toString();
    const QString entryTitle = reply->property("entryTitle").toString();
    const QString appId = reply->property("steamAppId").toString();
    const QString coverUrl = reply->property("coverUrl").toString();
    const QString languageCode = reply->property("languageCode").toString();

    GameMetadata metadata = m_cache.value(entryTitle);
    metadata.steamAppId = appId;
    metadata.coverUrl = coverUrl.isEmpty() ? metadata.coverUrl : coverUrl;
    metadata.descriptionLanguage = languageCode.trimmed().isEmpty() ? QStringLiteral("en")
                                                                    : languageCode.trimmed();

    if (reply->error() == QNetworkReply::NoError) {
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject appRoot = root.value(appId).toObject();
        if (appRoot.value(QStringLiteral("success")).toBool()) {
            const QJsonObject data = appRoot.value(QStringLiteral("data")).toObject();
            metadata.description = data.value(QStringLiteral("short_description")).toString();
            QStringList genres;
            for (const QJsonValue& genreValue : data.value(QStringLiteral("genres")).toArray())
                genres.append(genreValue.toObject().value(QStringLiteral("description")).toString());
            // Steam categories carry Single-player / Multi-player / Online Co-op / etc.
            for (const QJsonValue& catValue : data.value(QStringLiteral("categories")).toArray()) {
                const QString desc =
                    catValue.toObject().value(QStringLiteral("description")).toString().trimmed();
                if (!desc.isEmpty() && !genres.contains(desc))
                    genres.append(desc);
            }
            metadata.genres = genres.join(QStringLiteral(", "));
            metadata.screenshotUrls =
                parseScreenshotUrls(data.value(QStringLiteral("screenshots")).toArray());

            const QJsonArray movies = data.value(QStringLiteral("movies")).toArray();
            const QString moviesTrailer = pickTrailerFromMovies(movies);
            if (!moviesTrailer.isEmpty()) {
                if (metadata.trailerUrl.isEmpty())
                    metadata.trailerUrl = moviesTrailer;
                else if (moviesTrailer.contains(QStringLiteral(".mp4"))
                         && !metadata.trailerUrl.contains(QStringLiteral(".mp4")))
                    metadata.trailerUrl = moviesTrailer;
            }
            const QString moviesThumbnail = pickTrailerThumbnailFromMovies(movies);
            if (metadata.trailerThumbnailUrl.isEmpty() && !moviesThumbnail.isEmpty())
                metadata.trailerThumbnailUrl = moviesThumbnail;
        }
    }

    reply->deleteLater();
    m_cache.insert(entryTitle, metadata);
    m_saveTimer->start();
    emit metadataReady(entryId, metadata);

    if (!appId.isEmpty() && metadata.sizeLabel.isEmpty()) {
        requestDepotSize(entryId, entryTitle, appId);
        return;
    }

    m_inFlight.remove(entryId);
    tryDeferredFull(entryId);
    requestNext();
}

void GameMetadataService::requestDepotSize(const QString& entryId, const QString& title,
                                           const QString& appId)
{
    if (entryId.isEmpty() || appId.isEmpty()) {
        m_inFlight.remove(entryId);
        requestNext();
        return;
    }

    QUrl url(QStringLiteral("https://api.steamcmd.net/v1/info/%1").arg(appId));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("entryId", entryId);
    reply->setProperty("entryTitle", title);
    reply->setProperty("steamAppId", appId);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { handleDepotSizeFinished(reply); });
    ++m_activeRequests;
}

void GameMetadataService::handleDepotSizeFinished(QNetworkReply* reply)
{
    --m_activeRequests;

    const QString entryId = reply->property("entryId").toString();
    const QString entryTitle = reply->property("entryTitle").toString();
    const QString appId = reply->property("steamAppId").toString();

    GameMetadata metadata = m_cache.value(entryTitle);
    if (metadata.steamAppId.isEmpty())
        metadata.steamAppId = appId;

    if (reply->error() == QNetworkReply::NoError) {
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject app =
            root.value(QStringLiteral("data")).toObject().value(appId).toObject();
        const QJsonObject depots = app.value(QStringLiteral("depots")).toObject();
        qint64 total = 0;
        for (auto it = depots.constBegin(); it != depots.constEnd(); ++it) {
            bool ok = false;
            it.key().toLongLong(&ok);
            if (!ok)
                continue;
            const QJsonObject depot = it.value().toObject();
            if (depot.isEmpty())
                continue;
            // Skip DLC depots and non-English language packs.
            if (depot.contains(QStringLiteral("dlcappid"))) {
                const QJsonValue dlc = depot.value(QStringLiteral("dlcappid"));
                if (!(dlc.isNull() || (dlc.isString() && dlc.toString().isEmpty())))
                    continue;
            }
            const QJsonObject config = depot.value(QStringLiteral("config")).toObject();
            const QString language = config.value(QStringLiteral("language")).toString().trimmed();
            if (!language.isEmpty()
                && language.compare(QStringLiteral("english"), Qt::CaseInsensitive) != 0)
                continue;
            const QJsonObject pub =
                depot.value(QStringLiteral("manifests")).toObject().value(QStringLiteral("public")).toObject();
            const qint64 size = pub.value(QStringLiteral("size")).toString().toLongLong();
            if (size > 0)
                total += size;
        }
        if (total > 0)
            metadata.sizeLabel = formatSizeLabelBytes(total);
    }

    reply->deleteLater();
    m_cache.insert(entryTitle, metadata);
    m_saveTimer->start();
    m_inFlight.remove(entryId);
    emit metadataReady(entryId, metadata);
    tryDeferredFull(entryId);
    requestNext();
}

} // namespace arachnel::core
