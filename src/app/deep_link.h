#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QLocalServer;

namespace arachnel {

/** arachnel://game/<entryId> helpers + OS protocol registration + single-instance. */
QString makeGameShareUrl(const QString& entryId);
QString parseGameIdFromDeepLink(const QString& raw);
QString findDeepLinkArgument(const QStringList& args);

void registerGameDeepLinkProtocol();

class SingleInstanceGuard : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstanceGuard(QObject* parent = nullptr);
    ~SingleInstanceGuard() override;

    /** True if this process should keep running (became the primary instance). */
    bool tryBecomePrimary();
    /** Forward a deep-link URL (may be empty) to the primary and return. */
    bool forwardToPrimary(const QString& deepLinkUrl);

signals:
    void messageReceived(const QString& deepLinkUrl);

private:
    static QString serverName();
    void onNewConnection();

    QLocalServer* m_server = nullptr;
};

} // namespace arachnel
