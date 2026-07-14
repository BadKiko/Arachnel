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

namespace {

QString cacheFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/metadata-cache.json");
}

QString normalizeTitle(QString title)
{
    title = title.toLower();
    title.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QString());
    return title;
}

QString steamLanguageForUi(const QString& languageCode)
{
    const QString code = languageCode.trimmed().toLower();
    if (code == QStringLiteral("ru"))
        return QStringLiteral("russian");
    if (code == QStringLiteral("de"))
        return QStringLiteral("german");
    if (code == QStringLiteral("fr"))
        return QStringLiteral("french");
    if (code == QStringLiteral("es"))
        return QStringLiteral("spanish");
    if (code == QStringLiteral("zh") || code.startsWith(QStringLiteral("zh")))
        return QStringLiteral("schinese");
    return QStringLiteral("english");
}

QString pickTrailerUrl(const QJsonObject& movie)
{
    const QJsonObject mp4 = movie.value(QStringLiteral("mp4")).toObject();
    QString url = mp4.value(QStringLiteral("max")).toString();
    if (url.isEmpty())
        url = mp4.value(QStringLiteral("480")).toString();
    if (!url.isEmpty())
        return url;

    const QJsonObject webm = movie.value(QStringLiteral("webm")).toObject();
    url = webm.value(QStringLiteral("max")).toString();
    if (url.isEmpty())
        url = webm.value(QStringLiteral("480")).toString();
    if (!url.isEmpty())
        return url;

    url = movie.value(QStringLiteral("hls_h264")).toString();
    if (!url.isEmpty())
        return url;

    url = movie.value(QStringLiteral("dash_h264")).toString();
    return url;
}

QString buildSteamTrailerUrl(const QString& cdnPath)
{
    if (cdnPath.isEmpty())
        return {};
    return QStringLiteral("https://video.akamai.steamstatic.com/store_trailers/") + cdnPath;
}

QString pickTrailerFromStoreHighlight(const QJsonObject& highlight)
{
    const QJsonArray micro = highlight.value(QStringLiteral("microtrailer")).toArray();
    for (const QJsonValue& entry : micro) {
        const QJsonObject obj = entry.toObject();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("video/mp4"))
            continue;
        const QString url = buildSteamTrailerUrl(obj.value(QStringLiteral("filename")).toString());
        if (!url.isEmpty())
            return url;
    }
    for (const QJsonValue& entry : micro) {
        const QJsonObject obj = entry.toObject();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("video/webm"))
            continue;
        const QString url = buildSteamTrailerUrl(obj.value(QStringLiteral("filename")).toString());
        if (!url.isEmpty())
            return url;
    }

    const QJsonArray adaptive = highlight.value(QStringLiteral("adaptive_trailers")).toArray();
    for (const QJsonValue& entry : adaptive) {
        const QJsonObject obj = entry.toObject();
        if (obj.value(QStringLiteral("encoding")).toString() != QStringLiteral("hls_h264"))
            continue;
        const QString url = buildSteamTrailerUrl(obj.value(QStringLiteral("cdn_path")).toString());
        if (!url.isEmpty())
            return url;
    }
    return {};
}

QString pickTrailerFromStoreTrailers(const QJsonObject& trailers)
{
    const QJsonArray highlights = trailers.value(QStringLiteral("highlights")).toArray();
    for (const QJsonValue& value : highlights) {
        const QString url = pickTrailerFromStoreHighlight(value.toObject());
        if (!url.isEmpty())
            return url;
    }
    const QJsonArray other = trailers.value(QStringLiteral("other_trailers")).toArray();
    if (!other.isEmpty())
        return pickTrailerFromStoreHighlight(other.first().toObject());
    return {};
}

QString pickTrailerFromMovies(const QJsonArray& movies)
{
    for (const QJsonValue& value : movies) {
        const QJsonObject movie = value.toObject();
        if (!movie.value(QStringLiteral("highlight")).toBool())
            continue;
        const QString url = pickTrailerUrl(movie);
        if (!url.isEmpty())
            return url;
    }
    if (movies.isEmpty())
        return {};
    return pickTrailerUrl(movies.first().toObject());
}

QString pickTrailerThumbnailFromMovies(const QJsonArray& movies)
{
    for (const QJsonValue& value : movies) {
        const QJsonObject movie = value.toObject();
        if (!movie.value(QStringLiteral("highlight")).toBool())
            continue;
        const QString url = movie.value(QStringLiteral("thumbnail")).toString();
        if (!url.isEmpty())
            return url;
    }
    if (movies.isEmpty())
        return {};
    return movies.first().toObject().value(QStringLiteral("thumbnail")).toString();
}

QStringList parseScreenshotUrls(const QJsonArray& screenshots, int maxCount = 8)
{
    QStringList urls;
    urls.reserve(maxCount);
    for (const QJsonValue& value : screenshots) {
        if (urls.size() >= maxCount)
            break;
        const QString url = value.toObject().value(QStringLiteral("path_full")).toString();
        if (!url.isEmpty())
            urls.append(url);
    }
    return urls;
}

bool hasCachedMedia(const GameMetadata& metadata)
{
    return !metadata.screenshotUrls.isEmpty() && !metadata.trailerUrl.isEmpty();
}

bool needsMediaRefresh(const GameMetadata& metadata)
{
    if (metadata.trailerUrl.isEmpty() || metadata.screenshotUrls.isEmpty())
        return true;
    // Older builds cached microtrailers on the image CDN (404).
    if (metadata.trailerUrl.contains(QStringLiteral("shared.akamai.steamstatic.com"))
        && metadata.trailerUrl.contains(QStringLiteral("microtrailer")))
        return true;
    return !metadata.trailerUrl.isEmpty() && metadata.trailerThumbnailUrl.isEmpty();
}

bool isVerticalLibraryCover(const QString& url)
{
    return url.contains(QStringLiteral("library_capsule"))
        || url.contains(QStringLiteral("library_600x900"));
}

QString buildAssetUrl(const QString& urlFormat, const QString& filename)
{
    if (filename.isEmpty())
        return {};
    QString path = urlFormat;
    path.replace(QStringLiteral("${FILENAME}"), filename);
    if (path.startsWith(QStringLiteral("http")))
        return path;
    return QStringLiteral("https://shared.akamai.steamstatic.com/store_item_assets/") + path;
}

QString pickTrailerThumbnailFromStoreHighlight(const QJsonObject& highlight)
{
    const QString format = highlight.value(QStringLiteral("trailer_url_format")).toString();
    const QString medium = highlight.value(QStringLiteral("screenshot_medium")).toString();
    QString url = buildAssetUrl(format, medium);
    if (!url.isEmpty())
        return url;
    return buildAssetUrl(format, highlight.value(QStringLiteral("screenshot_full")).toString());
}

QString pickTrailerThumbnailFromStoreTrailers(const QJsonObject& trailers)
{
    const QJsonArray highlights = trailers.value(QStringLiteral("highlights")).toArray();
    for (const QJsonValue& value : highlights) {
        const QString url = pickTrailerThumbnailFromStoreHighlight(value.toObject());
        if (!url.isEmpty())
            return url;
    }
    const QJsonArray other = trailers.value(QStringLiteral("other_trailers")).toArray();
    if (!other.isEmpty())
        return pickTrailerThumbnailFromStoreHighlight(other.first().toObject());
    return {};
}

QString pickLibraryCover(const QJsonObject& assets)
{
    const QString format = assets.value(QStringLiteral("asset_url_format")).toString();
    if (format.isEmpty())
        return {};
    for (const QString& key : {QStringLiteral("library_capsule"), QStringLiteral("library_capsule_2x")}) {
        const QString url = buildAssetUrl(format, assets.value(key).toString());
        if (!url.isEmpty())
            return url;
    }
    return {};
}

int titleMatchScore(const QString& wanted, const QString& candidate)
{
    const QString a = normalizeTitle(wanted);
    const QString b = normalizeTitle(candidate);
    if (a.isEmpty() || b.isEmpty())
        return -1;
    if (a == b)
        return 1000;
    if (b.startsWith(a) || a.startsWith(b))
        return 800 - qAbs(a.size() - b.size());
    if (b.contains(a) || a.contains(b))
        return 500 - qAbs(a.size() - b.size());
    return -1;
}

QJsonObject bestSearchItem(const QJsonArray& items, const QString& title)
{
    QJsonObject best;
    int bestScore = -1;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("type")).toString() != QStringLiteral("app"))
            continue;
        const int score = titleMatchScore(title, item.value(QStringLiteral("name")).toString());
        if (score > bestScore) {
            bestScore = score;
            best = item;
        }
    }
    return best;
}

void appendUnique(QStringList& terms, const QString& term)
{
    const QString t = term.simplified();
    if (!t.isEmpty() && !terms.contains(t, Qt::CaseInsensitive))
        terms.append(t);
}

QStringList searchTermsFor(const QString& title)
{
    QStringList terms;
    appendUnique(terms, title);

    QString cleaned = title;
    cleaned.replace(QRegularExpression(QStringLiteral(R"(\([^)]*\))")), QString());
    cleaned.replace(QRegularExpression(QStringLiteral(R"(\[[^\]]*\])")), QString());
    cleaned = cleaned.simplified();
    appendUnique(terms, cleaned);

    QString repackStripped = cleaned;
    repackStripped.remove(QRegularExpression(QStringLiteral(R"(\|.*)"),
                                           QRegularExpression::DotMatchesEverythingOption));
    repackStripped.remove(
        QRegularExpression(QStringLiteral(R"(\s+PC\s*$)"), QRegularExpression::CaseInsensitiveOption));
    repackStripped.remove(
        QRegularExpression(QStringLiteral(R"(RePack.*)"), QRegularExpression::CaseInsensitiveOption));
    repackStripped = repackStripped.simplified();
    appendUnique(terms, repackStripped);

    if (cleaned.contains(QLatin1Char(':')))
        appendUnique(terms, cleaned.section(QLatin1Char(':'), 0, 0).trimmed());

    QString stripped = cleaned;
    stripped.replace(
        QRegularExpression(
            QStringLiteral(
                R"(\s+(alpha|beta|demo|epoch|mod|multiplayer|singleplayer|goty|)"
                R"(deluxe|definitive|remastered|complete|edition|pack|dlc)\s*$)"),
            QRegularExpression::CaseInsensitiveOption),
        QString());
    stripped = stripped.simplified();
    appendUnique(terms, stripped);

    const QStringList words = stripped.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.size() >= 2)
        appendUnique(terms, words.at(0) + QLatin1Char(' ') + words.at(1));
    if (!words.isEmpty())
        appendUnique(terms, words.at(0));
    return terms;
}

} // namespace

GameMetadataService::GameMetadataService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_saveTimer(new QTimer(this))
{
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(2000);
    connect(m_saveTimer, &QTimer::timeout, this, &GameMetadataService::saveCache);
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
        metadata.descriptionLanguage = obj.value(QStringLiteral("descriptionLanguage")).toString();
        metadata.genres = obj.value(QStringLiteral("genres")).toString();
        metadata.steamAppId = obj.value(QStringLiteral("steamAppId")).toString();
        metadata.trailerUrl = obj.value(QStringLiteral("trailerUrl")).toString();
        metadata.trailerThumbnailUrl = obj.value(QStringLiteral("trailerThumbnailUrl")).toString();
        for (const QJsonValue& shot : obj.value(QStringLiteral("screenshotUrls")).toArray())
            metadata.screenshotUrls.append(shot.toString());
        if (!isVerticalLibraryCover(metadata.coverUrl))
            metadata.coverUrl.clear();
        m_cache.insert(it.key(), metadata);
    }
}

void GameMetadataService::saveCache()
{
    QJsonObject root;
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        QJsonObject obj;
        obj.insert(QStringLiteral("coverUrl"), it->coverUrl);
        obj.insert(QStringLiteral("description"), it->description);
        obj.insert(QStringLiteral("descriptionLanguage"), it->descriptionLanguage);
        obj.insert(QStringLiteral("genres"), it->genres);
        obj.insert(QStringLiteral("steamAppId"), it->steamAppId);
        obj.insert(QStringLiteral("trailerUrl"), it->trailerUrl);
        obj.insert(QStringLiteral("trailerThumbnailUrl"), it->trailerThumbnailUrl);
        QJsonArray screenshots;
        for (const QString& url : it->screenshotUrls)
            screenshots.append(url);
        obj.insert(QStringLiteral("screenshotUrls"), screenshots);
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

void GameMetadataService::clearCachedCover(const QString& title)
{
    auto it = m_cache.find(title);
    if (it == m_cache.end())
        return;
    it->coverUrl.clear();
    m_saveTimer->start();
}

int GameMetadataService::indexOfPending(const QString& entryId) const
{
    for (int i = 0; i < m_pending.size(); ++i) {
        if (m_pending.at(i).entryId == entryId)
            return i;
    }
    return -1;
}

bool GameMetadataService::cancelPending(const QString& entryId)
{
    const int idx = indexOfPending(entryId);
    if (idx < 0)
        return false;
    m_pending.removeAt(idx);
    return true;
}

void GameMetadataService::prependPending(PendingRequest request)
{
    if (request.termIndex < 0 || request.termIndex >= request.searchTerms.size()) {
        failCover(request.entryId);
        return;
    }

    const int existing = indexOfPending(request.entryId);
    if (existing >= 0)
        m_pending.removeAt(existing);

    m_pending.prepend(std::move(request));
    while (m_pending.size() > kMaxQueueSize) {
        const PendingRequest evicted = m_pending.takeLast();
        failCover(evicted.entryId);
    }
    requestNext();
}

void GameMetadataService::failCover(const QString& entryId)
{
    m_inFlight.remove(entryId);
    emit coverReady(entryId, QString());
    tryDeferredFull(entryId);
    requestNext();
}

void GameMetadataService::tryDeferredFull(const QString& entryId)
{
    const auto it = m_deferredFull.constFind(entryId);
    if (it == m_deferredFull.cend())
        return;

    const DeferredFullRequest request = it.value();
    m_deferredFull.remove(entryId);
    queueFetch(entryId, request.title, MetadataFetchMode::Full, request.languageCode);
}

void GameMetadataService::queueFetch(const QString& entryId, const QString& title,
                                     MetadataFetchMode mode, const QString& languageCode)
{
    if (entryId.isEmpty() || title.isEmpty())
        return;

    const QString uiLanguage = languageCode.trimmed().isEmpty() ? QStringLiteral("en")
                                                                  : languageCode.trimmed();
    const GameMetadata cached = m_cache.value(title);
    if (mode == MetadataFetchMode::CoverOnly && isVerticalLibraryCover(cached.coverUrl)) {
        emit coverReady(entryId, cached.coverUrl);
        return;
    }
    if (mode == MetadataFetchMode::Full && isVerticalLibraryCover(cached.coverUrl)
        && !cached.description.isEmpty()
        && cached.descriptionLanguage.compare(uiLanguage, Qt::CaseInsensitive) == 0) {
        if (hasCachedMedia(cached)) {
            emit metadataReady(entryId, cached);
            return;
        }
        if (!cached.steamAppId.isEmpty() && needsMediaRefresh(cached)) {
            m_inFlight.insert(entryId);
            requestStoreAssets(entryId, title, cached.steamAppId, mode, {}, uiLanguage);
            return;
        }
        if (!cached.screenshotUrls.isEmpty() || !cached.trailerUrl.isEmpty()) {
            emit metadataReady(entryId, cached);
            return;
        }
    }

    if (m_inFlight.contains(entryId)) {
        if (mode == MetadataFetchMode::Full)
            m_deferredFull.insert(entryId, {title, uiLanguage});
        return;
    }

    const int existing = indexOfPending(entryId);
    if (existing >= 0) {
        PendingRequest req = m_pending.takeAt(existing);
        if (mode == MetadataFetchMode::Full)
            req.mode = MetadataFetchMode::Full;
        req.languageCode = uiLanguage;
        m_pending.prepend(std::move(req));
        requestNext();
        return;
    }

    if (mode == MetadataFetchMode::CoverOnly && !cached.steamAppId.isEmpty()
        && !isVerticalLibraryCover(cached.coverUrl)) {
        m_inFlight.insert(entryId);
        requestStoreAssets(entryId, title, cached.steamAppId, mode,
                           searchTermsFor(title).mid(1), uiLanguage);
        return;
    }

    prependPending({entryId, title, searchTermsFor(title), 0, mode, uiLanguage});
}

void GameMetadataService::requestNext()
{
    while (m_activeRequests < kMaxConcurrent && !m_pending.isEmpty()) {
        const PendingRequest request = m_pending.takeFirst();
        m_inFlight.insert(request.entryId);

        const QString term = request.searchTerms.value(request.termIndex);
        const QString steamLanguage = steamLanguageForUi(request.languageCode);
        QUrl url(QStringLiteral("https://store.steampowered.com/api/storesearch/"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("term"), term);
        query.addQueryItem(QStringLiteral("cc"), QStringLiteral("US"));
        query.addQueryItem(QStringLiteral("l"), steamLanguage);
        url.setQuery(query);

        QNetworkRequest netRequest(url);
        netRequest.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
        QNetworkReply* reply = m_network->get(netRequest);
        reply->setProperty("entryId", request.entryId);
        reply->setProperty("entryTitle", request.title);
        reply->setProperty("searchTerm", term);
        reply->setProperty("termIndex", request.termIndex);
        reply->setProperty("searchTerms", request.searchTerms);
        reply->setProperty("fetchMode", static_cast<int>(request.mode));
        reply->setProperty("languageCode", request.languageCode);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply]() { handleSearchFinished(reply); });
        ++m_activeRequests;
    }
}

void GameMetadataService::finishCover(const QString& entryId, const QString& title,
                                      const GameMetadata& metadata)
{
    m_cache.insert(title, metadata);
    m_saveTimer->start();
    m_inFlight.remove(entryId);
    emit coverReady(entryId, metadata.coverUrl);
    tryDeferredFull(entryId);
}

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
    m_inFlight.remove(entryId);
    emit metadataReady(entryId, metadata);
    tryDeferredFull(entryId);
    requestNext();
}

} // namespace arachnel::core
