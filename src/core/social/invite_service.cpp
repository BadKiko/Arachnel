#include "invite_service.h"
#include "social_http.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

namespace arachnel::core {

namespace {

FriendEntry friendFromReply(const QJsonObject& root)
{
    const QJsonObject obj = root.value(QStringLiteral("friend")).toObject();
    FriendEntry entry;
    entry.friendId = obj.value(QStringLiteral("friendId")).toString();
    entry.nickname = obj.value(QStringLiteral("nickname")).toString();
    entry.publicKey = obj.value(QStringLiteral("publicKey")).toString();
    entry.addedAt = obj.value(QStringLiteral("addedAt")).toString();
    entry.lastSeenAt = obj.value(QStringLiteral("lastSeenAt")).toString();
    entry.online = obj.value(QStringLiteral("online")).toBool(false);
    entry.currentGameId = obj.value(QStringLiteral("currentGameId")).toString();
    entry.currentGameTitle = obj.value(QStringLiteral("currentGameTitle")).toString();
    entry.currentGameCoverUrl = obj.value(QStringLiteral("currentGameCoverUrl")).toString();
    return entry;
}

} // namespace

InviteService::InviteService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void InviteService::setIdentity(const SocialIdentity& identity)
{
    m_identity = identity;
}

void InviteService::setRelayBaseUrl(const QString& url)
{
    m_relayBaseUrl = url.trimmed();
}

void InviteService::createInviteCode()
{
    const QString endpoint = joinRelayUrl(m_relayBaseUrl, QStringLiteral("/v1/social/invites/create"));
    if (endpoint.isEmpty()) {
        emit requestFailed(tr("Set a relay URL in Friends settings"));
        return;
    }

    const QJsonObject payload = {
        {QStringLiteral("deviceId"), m_identity.deviceId},
        {QStringLiteral("publicKey"), m_identity.publicKey},
        {QStringLiteral("displayName"), m_identity.displayName},
    };
    QNetworkReply* reply = m_network->post(makeRelayRequest(QUrl(endpoint), true),
                                           QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { handleCreateFinished(reply); });
}

void InviteService::acceptInviteCode(const QString& code)
{
    const QString endpoint = joinRelayUrl(m_relayBaseUrl, QStringLiteral("/v1/social/invites/accept"));
    if (endpoint.isEmpty()) {
        emit requestFailed(tr("Set a relay URL in Friends settings"));
        return;
    }

    const QJsonObject payload = {
        {QStringLiteral("code"), code.trimmed()},
        {QStringLiteral("deviceId"), m_identity.deviceId},
        {QStringLiteral("publicKey"), m_identity.publicKey},
        {QStringLiteral("displayName"), m_identity.displayName},
    };
    QNetworkReply* reply = m_network->post(makeRelayRequest(QUrl(endpoint), true),
                                           QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { handleAcceptFinished(reply); });
}

void InviteService::removeFriend(const QString& friendId)
{
    const QString endpoint = joinRelayUrl(m_relayBaseUrl, QStringLiteral("/v1/social/friends/remove"));
    if (endpoint.isEmpty()) {
        emit requestFailed(tr("Set a relay URL in Friends settings"));
        return;
    }

    const QString fid = friendId.trimmed();
    if (fid.isEmpty()) {
        emit requestFailed(tr("Friend not found"));
        return;
    }

    const QJsonObject payload = {
        {QStringLiteral("deviceId"), m_identity.deviceId},
        {QStringLiteral("publicKey"), m_identity.publicKey},
        {QStringLiteral("displayName"), m_identity.displayName},
        {QStringLiteral("friendId"), fid},
    };
    QNetworkReply* reply = m_network->post(makeRelayRequest(QUrl(endpoint), true),
                                           QJsonDocument(payload).toJson(QJsonDocument::Compact));
    reply->setProperty("friendId", fid);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { handleRemoveFinished(reply); });
}

void InviteService::suggestGame(const QString& friendId, const QString& gameId, const QString& title,
                                const QString& coverUrl)
{
    const QString endpoint = joinRelayUrl(m_relayBaseUrl, QStringLiteral("/v1/social/suggestions"));
    if (endpoint.isEmpty()) {
        emit requestFailed(tr("Set a relay URL in Friends settings"));
        return;
    }

    QString cover = coverUrl.trimmed();
    if (!cover.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
        cover.clear();

    const QJsonObject payload = {
        {QStringLiteral("fromDeviceId"), m_identity.deviceId},
        {QStringLiteral("fromPublicKey"), m_identity.publicKey},
        {QStringLiteral("friendId"), friendId},
        {QStringLiteral("gameId"), gameId},
        {QStringLiteral("title"), title},
        {QStringLiteral("coverUrl"), cover},
        {QStringLiteral("sentAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    };
    QNetworkReply* reply = m_network->post(makeRelayRequest(QUrl(endpoint), true),
                                           QJsonDocument(payload).toJson(QJsonDocument::Compact));
    reply->setProperty("friendId", friendId);
    reply->setProperty("title", title);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { handleSuggestFinished(reply); });
}

void InviteService::handleCreateFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit requestFailed(describeRelayError(reply));
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    PendingInvite invite;
    invite.inviteId = root.value(QStringLiteral("inviteId")).toString();
    invite.code = root.value(QStringLiteral("code")).toString();
    invite.expiresAt = root.value(QStringLiteral("expiresAt")).toString();
    invite.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    invite.outgoing = true;
    emit inviteCodeReady(invite);
}

void InviteService::handleAcceptFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit requestFailed(describeRelayError(reply));
        return;
    }

    emit friendAccepted(friendFromReply(QJsonDocument::fromJson(reply->readAll()).object()));
}

void InviteService::handleSuggestFinished(QNetworkReply* reply)
{
    const QString friendId = reply->property("friendId").toString();
    const QString title = reply->property("title").toString();
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit requestFailed(describeRelayError(reply));
        return;
    }
    emit suggestionSent(friendId, title);
}

void InviteService::handleRemoveFinished(QNetworkReply* reply)
{
    const QString friendId = reply->property("friendId").toString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError && status != 404) {
        emit requestFailed(describeRelayError(reply));
        return;
    }
    emit friendRemoved(friendId);
}

} // namespace arachnel::core
