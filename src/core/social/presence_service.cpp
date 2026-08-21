#include "presence_service.h"
#include "social_http.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

namespace arachnel::core {

namespace {

FriendEntry friendFromRelayJson(const QJsonObject& obj)
{
    FriendEntry entry;
    entry.friendId = obj.value(QStringLiteral("friendId")).toString();
    entry.nickname = obj.value(QStringLiteral("nickname")).toString();
    entry.publicKey = obj.value(QStringLiteral("publicKey")).toString();
    entry.online = obj.value(QStringLiteral("online")).toBool(false);
    entry.currentGameId = obj.value(QStringLiteral("currentGameId")).toString();
    entry.currentGameTitle = obj.value(QStringLiteral("currentGameTitle")).toString();
    entry.currentGameCoverUrl = obj.value(QStringLiteral("currentGameCoverUrl")).toString();
    entry.addedAt = obj.value(QStringLiteral("addedAt")).toString();
    entry.lastSeenAt = obj.value(QStringLiteral("lastSeenAt")).toString();
    entry.suggestedGameId = obj.value(QStringLiteral("suggestedGameId")).toString();
    entry.suggestedGameTitle = obj.value(QStringLiteral("suggestedGameTitle")).toString();
    entry.suggestedCoverUrl = obj.value(QStringLiteral("suggestedCoverUrl")).toString();
    if (entry.suggestedCoverUrl.isEmpty())
        entry.suggestedCoverUrl = obj.value(QStringLiteral("suggestedGameCoverUrl")).toString();
    if (entry.suggestedCoverUrl.isEmpty())
        entry.suggestedCoverUrl = obj.value(QStringLiteral("coverUrl")).toString();
    const QJsonObject suggested = obj.value(QStringLiteral("suggestedGame")).toObject();
    if (!suggested.isEmpty()) {
        if (entry.suggestedGameId.isEmpty())
            entry.suggestedGameId = suggested.value(QStringLiteral("id")).toString();
        if (entry.suggestedGameTitle.isEmpty())
            entry.suggestedGameTitle = suggested.value(QStringLiteral("title")).toString();
        if (entry.suggestedCoverUrl.isEmpty())
            entry.suggestedCoverUrl = suggested.value(QStringLiteral("coverUrl")).toString();
    }
    entry.suggestedAt = obj.value(QStringLiteral("suggestedAt")).toString();
    return entry;
}

} // namespace

PresenceService::PresenceService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void PresenceService::setIdentity(const SocialIdentity& identity)
{
    m_identity = identity;
}

void PresenceService::setRelayBaseUrl(const QString& url)
{
    m_relayBaseUrl = url.trimmed();
}

void PresenceService::setLocalPresence(const PresenceSnapshot& presence)
{
    m_localPresence = presence;
}

void PresenceService::publish()
{
    const QString endpoint = joinRelayUrl(m_relayBaseUrl, QStringLiteral("/v1/social/presence"));
    if (endpoint.isEmpty()) {
        emit relayStateChanged(false, tr("Set a relay URL in Friends settings"));
        return;
    }

    // Relay only accepts https covers. Library covers are often file:// cache paths -
    // sending those makes the whole presence publish fail, so friends never see the game.
    QString cover = m_localPresence.currentGameCoverUrl.trimmed();
    if (!cover.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
        cover.clear();

    const QJsonObject payload = {
        {QStringLiteral("deviceId"), m_identity.deviceId},
        {QStringLiteral("publicKey"), m_identity.publicKey},
        {QStringLiteral("displayName"), m_identity.displayName},
        {QStringLiteral("online"), m_localPresence.online},
        {QStringLiteral("currentGameId"), m_localPresence.currentGameId},
        {QStringLiteral("currentGameTitle"), m_localPresence.currentGameTitle},
        {QStringLiteral("currentGameCoverUrl"), cover},
        {QStringLiteral("sentAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    };

    QNetworkReply* reply = m_network->post(makeRelayRequest(QUrl(endpoint), true),
                                           QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { handlePublishFinished(reply); });
}

void PresenceService::refresh()
{
    const QString endpoint = joinRelayUrl(m_relayBaseUrl, QStringLiteral("/v1/social/presence"));
    if (endpoint.isEmpty()) {
        emit relayStateChanged(false, tr("Set a relay URL in Friends settings"));
        return;
    }

    QUrl url(endpoint);
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("deviceId"), m_identity.deviceId);
    query.addQueryItem(QStringLiteral("publicKey"), m_identity.publicKey);
    url.setQuery(query);

    QNetworkReply* reply = m_network->get(makeRelayRequest(url, false));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { handleRefreshFinished(reply); });
}

void PresenceService::handlePublishFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_connected = false;
        emit relayStateChanged(false, describeRelayError(reply));
        return;
    }
    m_connected = true;
    emit relayStateChanged(true, tr("Relay connected"));
}

void PresenceService::handleRefreshFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_connected = false;
        emit relayStateChanged(false, describeRelayError(reply));
        return;
    }

    QVector<FriendEntry> friends;
    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray items = root.value(QStringLiteral("friends")).toArray();
    friends.reserve(items.size());
    for (const QJsonValue& value : items)
        friends.append(friendFromRelayJson(value.toObject()));
    m_connected = true;
    emit relayStateChanged(true, tr("Relay connected"));
    emit friendsPresenceReceived(std::move(friends));
}

} // namespace arachnel::core
