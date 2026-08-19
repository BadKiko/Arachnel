#pragma once

#include "social_types.h"

#include <QObject>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

class PresenceService : public QObject
{
    Q_OBJECT

public:
    explicit PresenceService(QObject* parent = nullptr);

    void setIdentity(const SocialIdentity& identity);
    void setRelayBaseUrl(const QString& url);
    void setLocalPresence(const PresenceSnapshot& presence);
    QString relayBaseUrl() const { return m_relayBaseUrl; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void publish();

signals:
    void relayStateChanged(bool connected, const QString& status);
    void friendsPresenceReceived(QVector<FriendEntry> friends);
    void requestFailed(const QString& message);

private:
    void handlePublishFinished(QNetworkReply* reply);
    void handleRefreshFinished(QNetworkReply* reply);

    QNetworkAccessManager* m_network = nullptr;
    SocialIdentity m_identity;
    PresenceSnapshot m_localPresence;
    QString m_relayBaseUrl;
    bool m_connected = false;
};

} // namespace arachnel::core
