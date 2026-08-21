#include "social_store.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

namespace arachnel::core {

namespace {

QString socialDataDir()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

QString identityFilePath()
{
    return socialDataDir() + QStringLiteral("/social-identity.json");
}

QString friendsFilePath()
{
    return socialDataDir() + QStringLiteral("/friends.json");
}

QString relayFilePath()
{
    return socialDataDir() + QStringLiteral("/social-relay.json");
}

QString defaultRelayBaseUrl()
{
    return QStringLiteral("https://relay.badkiko.ru");
}

QString normalizeRelayBaseUrl(const QString& url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return defaultRelayBaseUrl();
    QUrl parsed = QUrl::fromUserInput(trimmed);
    if (!parsed.isValid() || parsed.host().isEmpty())
        return trimmed;
    if (parsed.scheme() == QLatin1String("http"))
        parsed.setScheme(QStringLiteral("https"));
    parsed.setPath(QString());
    parsed.setQuery(QString());
    parsed.setFragment(QString());
    QString out = parsed.toString(QUrl::RemoveUserInfo | QUrl::StripTrailingSlash);
    if (out.endsWith(QLatin1Char('/')))
        out.chop(1);
    return out;
}

QString isoNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QString randomSecretHex()
{
    QByteArray bytes(32, Qt::Uninitialized);
    for (int i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return QString::fromLatin1(bytes.toHex());
}

QString publicKeyFromPrivate(const QString& privateKey)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(privateKey.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QJsonObject friendToJson(const FriendEntry& entry)
{
    return {
        {QStringLiteral("friendId"), entry.friendId},
        {QStringLiteral("nickname"), entry.nickname},
        {QStringLiteral("publicKey"), entry.publicKey},
        {QStringLiteral("online"), entry.online},
        {QStringLiteral("currentGameId"), entry.currentGameId},
        {QStringLiteral("currentGameTitle"), entry.currentGameTitle},
        {QStringLiteral("currentGameCoverUrl"), entry.currentGameCoverUrl},
        {QStringLiteral("addedAt"), entry.addedAt},
        {QStringLiteral("lastSeenAt"), entry.lastSeenAt},
        {QStringLiteral("suggestedGameId"), entry.suggestedGameId},
        {QStringLiteral("suggestedGameTitle"), entry.suggestedGameTitle},
        {QStringLiteral("suggestedCoverUrl"), entry.suggestedCoverUrl},
        {QStringLiteral("suggestedAt"), entry.suggestedAt},
    };
}

FriendEntry friendFromJson(const QJsonObject& obj)
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
    entry.suggestedAt = obj.value(QStringLiteral("suggestedAt")).toString();
    return entry;
}

QJsonObject inviteToJson(const PendingInvite& invite)
{
    return {
        {QStringLiteral("inviteId"), invite.inviteId},
        {QStringLiteral("code"), invite.code},
        {QStringLiteral("createdAt"), invite.createdAt},
        {QStringLiteral("expiresAt"), invite.expiresAt},
        {QStringLiteral("outgoing"), invite.outgoing},
    };
}

PendingInvite inviteFromJson(const QJsonObject& obj)
{
    PendingInvite invite;
    invite.inviteId = obj.value(QStringLiteral("inviteId")).toString();
    invite.code = obj.value(QStringLiteral("code")).toString();
    invite.createdAt = obj.value(QStringLiteral("createdAt")).toString();
    invite.expiresAt = obj.value(QStringLiteral("expiresAt")).toString();
    invite.outgoing = obj.value(QStringLiteral("outgoing")).toBool(true);
    return invite;
}

} // namespace

SocialStore::SocialStore(QObject* parent)
    : QObject(parent)
{
}

void SocialStore::load()
{
    QFile identityFile(identityFilePath());
    if (identityFile.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(identityFile.readAll()).object();
        m_identity.deviceId = obj.value(QStringLiteral("deviceId")).toString();
        m_identity.displayName = obj.value(QStringLiteral("displayName")).toString();
        m_identity.publicKey = obj.value(QStringLiteral("publicKey")).toString();
        m_identity.privateKey = obj.value(QStringLiteral("privateKey")).toString();
        m_identity.createdAt = obj.value(QStringLiteral("createdAt")).toString();
    }

    QFile friendsFile(friendsFilePath());
    if (friendsFile.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(friendsFile.readAll()).object();
        const QJsonArray friendsArray = root.value(QStringLiteral("friends")).toArray();
        const QJsonArray invitesArray = root.value(QStringLiteral("pendingInvites")).toArray();
        m_friends.clear();
        m_pendingInvites.clear();
        m_friends.reserve(friendsArray.size());
        m_pendingInvites.reserve(invitesArray.size());
        for (const QJsonValue& value : friendsArray)
            m_friends.append(friendFromJson(value.toObject()));
        for (const QJsonValue& value : invitesArray)
            m_pendingInvites.append(inviteFromJson(value.toObject()));
    }

    QFile relayFile(relayFilePath());
    if (relayFile.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(relayFile.readAll()).object();
        m_relayBaseUrl = obj.value(QStringLiteral("relayBaseUrl")).toString().trimmed();
    }
    m_relayBaseUrl = normalizeRelayBaseUrl(m_relayBaseUrl);

    ensureIdentity();
}

void SocialStore::saveIdentity()
{
    QFile file(identityFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    const QJsonObject obj = {
        {QStringLiteral("deviceId"), m_identity.deviceId},
        {QStringLiteral("displayName"), m_identity.displayName},
        {QStringLiteral("publicKey"), m_identity.publicKey},
        {QStringLiteral("privateKey"), m_identity.privateKey},
        {QStringLiteral("createdAt"), m_identity.createdAt},
    };
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void SocialStore::saveFriends()
{
    QFile file(friendsFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    QJsonArray friendsArray;
    for (const FriendEntry& entry : std::as_const(m_friends))
        friendsArray.append(friendToJson(entry));
    QJsonArray invitesArray;
    for (const PendingInvite& invite : std::as_const(m_pendingInvites))
        invitesArray.append(inviteToJson(invite));
    const QJsonObject root = {
        {QStringLiteral("friends"), friendsArray},
        {QStringLiteral("pendingInvites"), invitesArray},
    };
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void SocialStore::saveRelayConfig()
{
    QFile file(relayFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    const QJsonObject obj = {
        {QStringLiteral("relayBaseUrl"), m_relayBaseUrl},
    };
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void SocialStore::ensureIdentity()
{
    bool changed = false;
    if (m_identity.deviceId.isEmpty()) {
        m_identity.deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        changed = true;
    }
    if (m_identity.privateKey.isEmpty()) {
        m_identity.privateKey = randomSecretHex();
        changed = true;
    }
    if (m_identity.publicKey.isEmpty()) {
        m_identity.publicKey = publicKeyFromPrivate(m_identity.privateKey);
        changed = true;
    }
    if (m_identity.createdAt.isEmpty()) {
        m_identity.createdAt = isoNow();
        changed = true;
    }
    if (m_identity.displayName.trimmed().isEmpty()) {
        m_identity.displayName = QStringLiteral("Arachnel %1").arg(m_identity.deviceId.first(6));
        changed = true;
    }
    if (changed) {
        saveIdentity();
        emit identityChanged();
    }
}

void SocialStore::setDisplayName(const QString& name)
{
    const QString next = name.trimmed();
    if (next.isEmpty() || next == m_identity.displayName)
        return;
    m_identity.displayName = next;
    saveIdentity();
    emit identityChanged();
}

void SocialStore::setRelayBaseUrl(const QString& url)
{
    const QString next = normalizeRelayBaseUrl(url);
    if (next == m_relayBaseUrl)
        return;
    m_relayBaseUrl = next;
    saveRelayConfig();
    emit relayConfigChanged();
}

void SocialStore::setFriends(QVector<FriendEntry> friends)
{
    m_friends = std::move(friends);
    saveFriends();
    emit friendsChanged();
}

void SocialStore::upsertFriend(const FriendEntry& entry)
{
    for (FriendEntry& existing : m_friends) {
        if (existing.friendId == entry.friendId) {
            existing = entry;
            saveFriends();
            emit friendsChanged();
            return;
        }
    }
    m_friends.append(entry);
    saveFriends();
    emit friendsChanged();
}

void SocialStore::removeFriend(const QString& friendId)
{
    for (int i = 0; i < m_friends.size(); ++i) {
        if (m_friends.at(i).friendId != friendId)
            continue;
        m_friends.removeAt(i);
        saveFriends();
        emit friendsChanged();
        return;
    }
}

void SocialStore::setPendingInvites(QVector<PendingInvite> invites)
{
    m_pendingInvites = std::move(invites);
    saveFriends();
    emit friendsChanged();
}

void SocialStore::upsertPendingInvite(const PendingInvite& invite)
{
    for (PendingInvite& existing : m_pendingInvites) {
        if (existing.inviteId == invite.inviteId) {
            existing = invite;
            saveFriends();
            emit friendsChanged();
            return;
        }
    }
    m_pendingInvites.append(invite);
    saveFriends();
    emit friendsChanged();
}

void SocialStore::removePendingInvite(const QString& inviteId)
{
    for (int i = 0; i < m_pendingInvites.size(); ++i) {
        if (m_pendingInvites.at(i).inviteId != inviteId)
            continue;
        m_pendingInvites.removeAt(i);
        saveFriends();
        emit friendsChanged();
        return;
    }
}

} // namespace arachnel::core
