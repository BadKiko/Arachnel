#pragma once

#include "social_types.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace arachnel::core {

class SocialStore : public QObject
{
    Q_OBJECT

public:
    explicit SocialStore(QObject* parent = nullptr);

    const SocialIdentity& identity() const { return m_identity; }
    const QVector<FriendEntry>& friends() const { return m_friends; }
    const QVector<PendingInvite>& pendingInvites() const { return m_pendingInvites; }
    QString relayBaseUrl() const { return m_relayBaseUrl; }

    void load();
    void saveIdentity();
    void saveFriends();
    void saveRelayConfig();

    void ensureIdentity();
    void setDisplayName(const QString& name);
    void setRelayBaseUrl(const QString& url);
    void setFriends(QVector<FriendEntry> friends);
    void upsertFriend(const FriendEntry& entry);
    void removeFriend(const QString& friendId);
    void setPendingInvites(QVector<PendingInvite> invites);
    void upsertPendingInvite(const PendingInvite& invite);
    void removePendingInvite(const QString& inviteId);

signals:
    void identityChanged();
    void friendsChanged();
    void relayConfigChanged();

private:
    SocialIdentity m_identity;
    QVector<FriendEntry> m_friends;
    QVector<PendingInvite> m_pendingInvites;
    QString m_relayBaseUrl{QStringLiteral("https://relay.badkiko.ru")};
};

} // namespace arachnel::core
