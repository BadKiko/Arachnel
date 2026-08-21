#pragma once

#include "friends_model.h"
#include "invite_service.h"
#include "presence_service.h"
#include "social_store.h"

#include <QObject>
#include <QVariantMap>

class QTimer;

namespace arachnel::core {

class SocialController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* friends READ friends CONSTANT)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY identityChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY identityChanged)
    Q_PROPERTY(QString relayBaseUrl READ relayBaseUrl WRITE setRelayBaseUrl NOTIFY relayBaseUrlChanged)
    Q_PROPERTY(bool relayConnected READ relayConnected NOTIFY relayStateChanged)
    Q_PROPERTY(QString relayStatus READ relayStatus NOTIFY relayStateChanged)
    Q_PROPERTY(QString pendingInviteCode READ pendingInviteCode NOTIFY pendingInviteChanged)
    Q_PROPERTY(QString pendingInviteExpiry READ pendingInviteExpiry NOTIFY pendingInviteChanged)
    // Global incoming game suggestion (rendered as a card overlay in AppWindow).
    Q_PROPERTY(QString suggestionFriend READ suggestionFriend NOTIFY suggestionNotificationChanged)
    Q_PROPERTY(QString suggestionGameId READ suggestionGameId NOTIFY suggestionNotificationChanged)
    Q_PROPERTY(QString suggestionGameTitle READ suggestionGameTitle NOTIFY suggestionNotificationChanged)

public:
    explicit SocialController(QObject* parent = nullptr);

    void initialize();
    void goOffline();

    QAbstractItemModel* friends() { return &m_friendsModel; }
    QString displayName() const { return m_store.identity().displayName; }
    QString deviceId() const { return m_store.identity().deviceId; }
    QString relayBaseUrl() const { return m_store.relayBaseUrl(); }
    bool relayConnected() const { return m_relayConnected; }
    QString relayStatus() const { return m_relayStatus; }
    QString pendingInviteCode() const { return m_pendingInviteCode; }
    QString pendingInviteExpiry() const { return m_pendingInviteExpiry; }
    QString suggestionFriend() const { return m_suggestionFriend; }
    QString suggestionGameId() const { return m_suggestionGameId; }
    QString suggestionGameTitle() const { return m_suggestionGameTitle; }

    void setDisplayName(const QString& name);
    void setRelayBaseUrl(const QString& url);
    void setLocalPresence(bool running, const QString& gameId, const QString& title,
                          const QString& coverUrl);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void createInviteCode();
    Q_INVOKABLE void acceptInviteCode(const QString& code);
    Q_INVOKABLE void removeFriend(const QString& friendId);
    Q_INVOKABLE void renameFriend(const QString& friendId, const QString& nickname);
    Q_INVOKABLE void suggestGame(const QString& friendId, const QString& gameId, const QString& title,
                                 const QString& coverUrl);
    Q_INVOKABLE void consumeSuggestionNotification();
    Q_INVOKABLE QVariantMap friendSummary(const QString& friendId) const;
    Q_INVOKABLE int onlineCount() const;

signals:
    void identityChanged();
    void relayBaseUrlChanged();
    void relayStateChanged();
    void pendingInviteChanged();
    void noticeRequested(const QString& message);
    void friendsChanged();
    void suggestionNotificationChanged();

private:
    void syncFriendsModel();
    void applyRemotePresence(const QVector<FriendEntry>& remoteFriends);
    const FriendEntry* findFriend(const QString& friendId) const;

    SocialStore m_store;
    FriendsModel m_friendsModel;
    PresenceService* m_presenceService = nullptr;
    InviteService* m_inviteService = nullptr;
    QTimer* m_pollTimer = nullptr;
    bool m_relayConnected = false;
    QString m_relayStatus;
    QString m_pendingInviteCode;
    QString m_pendingInviteExpiry;

    QString m_suggestionFriend;
    QString m_suggestionGameId;
    QString m_suggestionGameTitle;
};

} // namespace arachnel::core
