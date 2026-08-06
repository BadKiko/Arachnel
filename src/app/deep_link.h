#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QLocalServer;
class QLocalSocket;

namespace arachnel {

/** Register arachnel:// URL protocol (Windows HKCU; Linux best-effort desktop mime). */
void registerGameDeepLinkProtocol();

/** First argv entry that looks like arachnel://… (empty if none). */
QString findDeepLinkArgument(const QStringList& arguments);

/** Parse game entry id from arachnel://game/<id> (or bare id). Empty if invalid. */
QString parseDeepLinkGameId(const QString& rawOrUrl);

/**
 * Single-instance gate via QLocalServer.
 * Secondary processes forward an optional deep-link payload and exit.
 */
class SingleInstanceGuard : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstanceGuard(QObject* parent = nullptr);
    ~SingleInstanceGuard() override;

    bool tryBecomePrimary();
    void forwardToPrimary(const QString& payload);

signals:
    void messageReceived(const QString& payload);

private:
    void onNewConnection();
    void readSocket(QLocalSocket* socket);

    QString m_serverName;
    QLocalServer* m_server = nullptr;
};

} // namespace arachnel
