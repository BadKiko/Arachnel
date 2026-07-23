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

void GameMetadataService::setSizeApiBaseUrl(const QString& baseUrl)
{
    QString trimmed = baseUrl.trimmed();
    while (trimmed.endsWith(QLatin1Char('/')))
        trimmed.chop(1);
    m_sizeApiBaseUrl = trimmed;
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
        metadata.sizeLabel = obj.value(QStringLiteral("sizeLabel")).toString();
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
        obj.insert(QStringLiteral("sizeLabel"), it->sizeLabel);
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
    queueFetch(entryId, request.title, MetadataFetchMode::Full, request.languageCode,
               request.knownSteamAppId);
}

void GameMetadataService::startKnownAppFetch(const QString& entryId, const QString& title,
                                             const QString& appId, MetadataFetchMode mode,
                                             const QString& languageCode,
                                             const GameMetadata& cached)
{
    m_inFlight.insert(entryId);
    if (mode == MetadataFetchMode::CoverOnly) {
        requestStoreAssets(entryId, title, appId, mode, {}, languageCode);
        return;
    }
    if (isVerticalLibraryCover(cached.coverUrl) || !cached.coverUrl.isEmpty())
        requestAppDetails(entryId, title, appId, cached.coverUrl, mode, languageCode);
    else
        requestStoreAssets(entryId, title, appId, mode, {}, languageCode);
}

void GameMetadataService::queueFetch(const QString& entryId, const QString& title,
                                     MetadataFetchMode mode, const QString& languageCode,
                                     const QString& knownSteamAppId)
{
    if (entryId.isEmpty() || title.isEmpty())
        return;

    const QString uiLanguage = languageCode.trimmed().isEmpty() ? QStringLiteral("en")
                                                                  : languageCode.trimmed();
    GameMetadata cached = m_cache.value(title);
    if (!knownSteamAppId.isEmpty() && cached.steamAppId != knownSteamAppId) {
        cached.steamAppId = knownSteamAppId;
        m_cache.insert(title, cached);
    }
    const QString appId =
        !knownSteamAppId.isEmpty() ? knownSteamAppId.trimmed() : cached.steamAppId.trimmed();

    if (mode == MetadataFetchMode::CoverOnly && isVerticalLibraryCover(cached.coverUrl)) {
        emit coverReady(entryId, cached.coverUrl);
        return;
    }
    if (mode == MetadataFetchMode::Full && isVerticalLibraryCover(cached.coverUrl)
        && !cached.description.isEmpty()
        && cached.descriptionLanguage.compare(uiLanguage, Qt::CaseInsensitive) == 0) {
        if (hasCachedMedia(cached)) {
            emit metadataReady(entryId, cached);
            if (cached.sizeLabel.isEmpty() && !appId.isEmpty()) {
                m_inFlight.insert(entryId);
                requestDepotSize(entryId, title, appId);
            }
            return;
        }
        if (!appId.isEmpty() && needsMediaRefresh(cached)) {
            startKnownAppFetch(entryId, title, appId, mode, uiLanguage, cached);
            return;
        }
        if (!cached.screenshotUrls.isEmpty() || !cached.trailerUrl.isEmpty()) {
            emit metadataReady(entryId, cached);
            if (cached.sizeLabel.isEmpty() && !appId.isEmpty()) {
                m_inFlight.insert(entryId);
                requestDepotSize(entryId, title, appId);
            }
            return;
        }
    }

    if (m_inFlight.contains(entryId)) {
        if (mode == MetadataFetchMode::Full)
            m_deferredFull.insert(entryId, {title, uiLanguage, appId});
        return;
    }

    const int existing = indexOfPending(entryId);
    if (existing >= 0) {
        PendingRequest req = m_pending.takeAt(existing);
        if (mode == MetadataFetchMode::Full)
            req.mode = MetadataFetchMode::Full;
        req.languageCode = uiLanguage;
        if (!appId.isEmpty())
            req.knownSteamAppId = appId;
        m_pending.prepend(std::move(req));
        requestNext();
        return;
    }

    // Catalog already knows the Steam app (steamidra) — skip flaky store search.
    if (!appId.isEmpty()) {
        if (mode == MetadataFetchMode::CoverOnly && isVerticalLibraryCover(cached.coverUrl)) {
            emit coverReady(entryId, cached.coverUrl);
            return;
        }
        startKnownAppFetch(entryId, title, appId, mode, uiLanguage, cached);
        return;
    }

    if (mode == MetadataFetchMode::CoverOnly && !cached.steamAppId.isEmpty()
        && !isVerticalLibraryCover(cached.coverUrl)) {
        m_inFlight.insert(entryId);
        requestStoreAssets(entryId, title, cached.steamAppId, mode,
                           searchTermsFor(title).mid(1), uiLanguage);
        return;
    }

    prependPending({entryId, title, searchTermsFor(title), 0, mode, uiLanguage, {}});
}

void GameMetadataService::requestNext()
{
    while (m_activeRequests < kMaxConcurrent && !m_pending.isEmpty()) {
        const PendingRequest request = m_pending.takeFirst();
        if (!request.knownSteamAppId.isEmpty()) {
            startKnownAppFetch(request.entryId, request.title, request.knownSteamAppId,
                               request.mode, request.languageCode, m_cache.value(request.title));
            continue;
        }

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

} // namespace arachnel::core
