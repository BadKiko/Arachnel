#pragma once

#include <QString>
#include <QVector>

namespace arachnel::core {

struct SocialIdentity {
    QString deviceId;
    QString displayName;
    QString publicKey;
    QString privateKey;
    QString createdAt;
};

struct FriendEntry {
    QString friendId;
    QString nickname;
    QString publicKey;
    bool online = false;
    QString currentGameId;
    QString currentGameTitle;
    QString currentGameCoverUrl;
    QString addedAt;
    QString lastSeenAt;
    QString suggestedGameId;
    QString suggestedGameTitle;
    QString suggestedCoverUrl;
    QString suggestedAt;
};

struct PendingInvite {
    QString inviteId;
    QString code;
    QString createdAt;
    QString expiresAt;
    bool outgoing = true;
};

struct PresenceSnapshot {
    bool online = false;
    QString currentGameId;
    QString currentGameTitle;
    QString currentGameCoverUrl;
};

} // namespace arachnel::core
