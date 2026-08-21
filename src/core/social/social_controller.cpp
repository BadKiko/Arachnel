#include "social_controller.h"

#include <QDateTime>
#include <QTimer>

namespace arachnel::core {

SocialController::SocialController(QObject* parent)
    : QObject(parent)
    , m_store(this)
    , m_friendsModel(this)
    , m_presenceService(new PresenceService(this))
    , m_inviteService(new InviteService(this))
    , m_pollTimer(new QTimer(this))
{
    // Keep relay UI responsive; relay rate limits are per-minute and allow
    // frequent presence sync without noticeable spam.
    m_pollTimer->setInterval(5000);
    connect(m_pollTimer, &QTimer::timeout, this, &SocialController::refresh);

    connect(&m_store, &SocialStore::identityChanged, this, [this]() {
        m_presenceService->setIdentity(m_store.identity());
        m_inviteService->setIdentity(m_store.identity());
        emit identityChanged();
    });
    connect(&m_store, &SocialStore::friendsChanged, this, [this]() {
        syncFriendsModel();
        emit friendsChanged();
    });
    connect(&m_store, &SocialStore::relayConfigChanged, this, [this]() {
        m_presenceService->setRelayBaseUrl(m_store.relayBaseUrl());
        m_inviteService->setRelayBaseUrl(m_store.relayBaseUrl());
        emit relayBaseUrlChanged();
    });
    connect(m_presenceService, &PresenceService::relayStateChanged, this,
            [this](bool connected, const QString& status) {
                m_relayConnected = connected;
                m_relayStatus = status;
                emit relayStateChanged();
            });
    connect(m_presenceService, &PresenceService::requestFailed, this,
            [this](const QString& message) { emit noticeRequested(message); });
    connect(m_presenceService, &PresenceService::friendsPresenceReceived, this,
            &SocialController::applyRemotePresence);
    connect(m_inviteService, &InviteService::requestFailed, this,
            [this](const QString& message) { emit noticeRequested(message); });
    connect(m_inviteService, &InviteService::inviteCodeReady, this, [this](const PendingInvite& invite) {
        m_pendingInviteCode = invite.code;
        m_pendingInviteExpiry = invite.expiresAt;
        m_store.upsertPendingInvite(invite);
        emit pendingInviteChanged();
        emit noticeRequested(tr("Friend code ready"));
    });
    connect(m_inviteService, &InviteService::friendAccepted, this, [this](FriendEntry entry) {
        if (entry.friendId.isEmpty())
            entry.friendId = entry.publicKey.left(16);
        if (entry.addedAt.isEmpty())
            entry.addedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (entry.nickname.isEmpty())
            entry.nickname = tr("New friend");
        m_store.upsertFriend(entry);
        emit noticeRequested(tr("Friend added"));
    });
    connect(m_inviteService, &InviteService::friendRemoved, this, [this](const QString& friendId) {
        if (!friendId.isEmpty())
            m_store.removeFriend(friendId);
        emit noticeRequested(tr("Friend removed"));
    });
    connect(m_inviteService, &InviteService::suggestionSent, this,
            [this](const QString&, const QString& title) {
                emit noticeRequested(tr("Suggestion sent: %1").arg(title));
            });
}

void SocialController::initialize()
{
    m_store.load();
    m_presenceService->setIdentity(m_store.identity());
    m_presenceService->setRelayBaseUrl(m_store.relayBaseUrl());
    m_inviteService->setIdentity(m_store.identity());
    m_inviteService->setRelayBaseUrl(m_store.relayBaseUrl());
    // App open = online. Game fields are filled separately when a title is running.
    PresenceSnapshot presence;
    presence.online = true;
    m_presenceService->setLocalPresence(presence);
    syncFriendsModel();
    m_relayStatus = m_store.relayBaseUrl().isEmpty() ? tr("Relay URL not set") : tr("Ready");
    emit relayStateChanged();
    if (!m_store.relayBaseUrl().isEmpty()) {
        m_pollTimer->start();
        refresh();
    }
}

void SocialController::goOffline()
{
    m_pollTimer->stop();
    PresenceSnapshot presence;
    presence.online = false;
    m_presenceService->setLocalPresence(presence);
    if (!m_store.relayBaseUrl().isEmpty())
        m_presenceService->publish();
}

void SocialController::setDisplayName(const QString& name)
{
    m_store.setDisplayName(name);
    // identityChanged already refreshed PresenceService identity; push now so friends see it.
    if (!m_store.relayBaseUrl().isEmpty())
        m_presenceService->publish();
}

void SocialController::setRelayBaseUrl(const QString& url)
{
    m_store.setRelayBaseUrl(url);
    if (m_store.relayBaseUrl().isEmpty()) {
        m_pollTimer->stop();
        m_relayConnected = false;
        m_relayStatus = tr("Relay URL not set");
        emit relayStateChanged();
        return;
    }
    m_pollTimer->start();
    refresh();
}

void SocialController::setLocalPresence(bool running, const QString& gameId, const QString& title,
                                        const QString& coverUrl)
{
    PresenceSnapshot presence;
    // Still in the launcher = online, even with no game running.
    presence.online = true;
    if (running) {
        presence.currentGameId = gameId;
        presence.currentGameTitle = title;
        presence.currentGameCoverUrl = coverUrl;
    }
    m_presenceService->setLocalPresence(presence);
    if (!m_store.relayBaseUrl().isEmpty())
        m_presenceService->publish();
}

void SocialController::refresh()
{
    if (m_store.relayBaseUrl().isEmpty()) {
        m_relayConnected = false;
        m_relayStatus = tr("Relay URL not set");
        emit relayStateChanged();
        return;
    }
    m_presenceService->publish();
    m_presenceService->refresh();
}

void SocialController::createInviteCode()
{
    m_inviteService->createInviteCode();
}

void SocialController::acceptInviteCode(const QString& code)
{
    const QString trimmed = code.trimmed();
    if (trimmed.isEmpty()) {
        emit noticeRequested(tr("Enter a friend code"));
        return;
    }
    m_inviteService->acceptInviteCode(trimmed);
}

void SocialController::removeFriend(const QString& friendId)
{
    const QString fid = friendId.trimmed();
    if (fid.isEmpty())
        return;
    if (m_store.relayBaseUrl().isEmpty()) {
        m_store.removeFriend(fid);
        return;
    }
    m_inviteService->removeFriend(fid);
}

void SocialController::renameFriend(const QString& friendId, const QString& nickname)
{
    const QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty())
        return;
    QVector<FriendEntry> friends = m_store.friends();
    for (FriendEntry& entry : friends) {
        if (entry.friendId != friendId)
            continue;
        entry.nickname = trimmed;
        m_store.setFriends(std::move(friends));
        return;
    }
}

void SocialController::suggestGame(const QString& friendId, const QString& gameId, const QString& title,
                                   const QString& coverUrl)
{
    if (friendId.trimmed().isEmpty() || gameId.trimmed().isEmpty())
        return;
    m_inviteService->suggestGame(friendId.trimmed(), gameId.trimmed(), title.trimmed(), coverUrl);
}

QVariantMap SocialController::friendSummary(const QString& friendId) const
{
    const FriendEntry* entry = findFriend(friendId);
    if (!entry)
        return {};
    return {
        {QStringLiteral("friendId"), entry->friendId},
        {QStringLiteral("nickname"), entry->nickname},
        {QStringLiteral("online"), entry->online},
        {QStringLiteral("currentGameId"), entry->currentGameId},
        {QStringLiteral("currentGameTitle"), entry->currentGameTitle},
        {QStringLiteral("currentGameCoverUrl"), entry->currentGameCoverUrl},
        {QStringLiteral("suggestedGameId"), entry->suggestedGameId},
        {QStringLiteral("suggestedGameTitle"), entry->suggestedGameTitle},
        {QStringLiteral("suggestedCoverUrl"), entry->suggestedCoverUrl},
        {QStringLiteral("lastSeenAt"), entry->lastSeenAt},
    };
}

int SocialController::onlineCount() const
{
    int count = 0;
    for (const FriendEntry& entry : m_store.friends()) {
        if (entry.online)
            ++count;
    }
    return count;
}

void SocialController::syncFriendsModel()
{
    m_friendsModel.setFriends(m_store.friends());
}

void SocialController::applyRemotePresence(const QVector<FriendEntry>& remoteFriends)
{
    const QVector<FriendEntry> localFriends = m_store.friends();
    QVector<FriendEntry> merged;
    merged.reserve(remoteFriends.size());

    for (const FriendEntry& remote : remoteFriends) {
        FriendEntry entry = remote;
        if (entry.friendId.isEmpty())
            entry.friendId = entry.publicKey.left(16);

        const FriendEntry* local = findFriend(entry.friendId);
        if (!local && !entry.publicKey.isEmpty()) {
            for (const FriendEntry& candidate : localFriends) {
                if (candidate.publicKey == entry.publicKey) {
                    local = &candidate;
                    break;
                }
            }
        }

        // Relay displayName is source of truth so renames reach friends.
        if (entry.nickname.isEmpty() && local && !local->nickname.isEmpty())
            entry.nickname = local->nickname;
        if (entry.nickname.isEmpty())
            entry.nickname = tr("Friend");
        if (entry.addedAt.isEmpty())
            entry.addedAt = local && !local->addedAt.isEmpty()
                                ? local->addedAt
                                : QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        if (!remote.suggestedGameId.isEmpty()) {
            const bool arrived = (!local || local->suggestedAt != remote.suggestedAt)
                                && !remote.suggestedAt.isEmpty();
            entry.suggestedGameId = remote.suggestedGameId;
            entry.suggestedGameTitle = remote.suggestedGameTitle;
            if (!remote.suggestedCoverUrl.isEmpty())
                entry.suggestedCoverUrl = remote.suggestedCoverUrl;
            entry.suggestedAt = remote.suggestedAt;
            if (arrived) {
                const QString title =
                    remote.suggestedGameTitle.isEmpty() ? remote.suggestedGameId : remote.suggestedGameTitle;
                m_suggestionFriend = entry.nickname;
                m_suggestionGameId = remote.suggestedGameId;
                m_suggestionGameTitle = title;
                emit suggestionNotificationChanged();
            }
        } else if (local) {
            entry.suggestedGameId = local->suggestedGameId;
            entry.suggestedGameTitle = local->suggestedGameTitle;
            entry.suggestedCoverUrl = local->suggestedCoverUrl;
            entry.suggestedAt = local->suggestedAt;
        }

        merged.append(entry);
    }
    m_store.setFriends(std::move(merged));
}

void SocialController::consumeSuggestionNotification()
{
    if (m_suggestionGameId.isEmpty() && m_suggestionGameTitle.isEmpty() && m_suggestionFriend.isEmpty())
        return;
    m_suggestionFriend.clear();
    m_suggestionGameId.clear();
    m_suggestionGameTitle.clear();
    emit suggestionNotificationChanged();
}

const FriendEntry* SocialController::findFriend(const QString& friendId) const
{
    const QVector<FriendEntry>& friends = m_store.friends();
    for (const FriendEntry& entry : friends) {
        if (entry.friendId == friendId)
            return &entry;
    }
    return nullptr;
}

} // namespace arachnel::core
