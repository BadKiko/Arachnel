#pragma once

#include "social_types.h"

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

class InviteService : public QObject
{
    Q_OBJECT

public:
    explicit InviteService(QObject* parent = nullptr);

    void setIdentity(const SocialIdentity& identity);
    void setRelayBaseUrl(const QString& url);

    Q_INVOKABLE void createInviteCode();
    Q_INVOKABLE void acceptInviteCode(const QString& code);
    Q_INVOKABLE void removeFriend(const QString& friendId);
    Q_INVOKABLE void suggestGame(const QString& friendId, const QString& gameId,
                                 const QString& title, const QString& coverUrl);

signals:
    void inviteCodeReady(const PendingInvite& invite);
    void friendAccepted(const FriendEntry& entry);
    void friendRemoved(const QString& friendId);
    void suggestionSent(const QString& friendId, const QString& title);
    void requestFailed(const QString& message);

private:
    void handleCreateFinished(QNetworkReply* reply);
    void handleAcceptFinished(QNetworkReply* reply);
    void handleRemoveFinished(QNetworkReply* reply);
    void handleSuggestFinished(QNetworkReply* reply);

    QNetworkAccessManager* m_network = nullptr;
    SocialIdentity m_identity;
    QString m_relayBaseUrl;
};

} // namespace arachnel::core
