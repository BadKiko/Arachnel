#include "deep_link.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUrl>

#if defined(Q_OS_WIN)
#include <QSettings>
#endif

namespace arachnel {
namespace {

constexpr auto kScheme = "arachnel";
constexpr auto kGameHost = "game";

} // namespace

QString makeGameShareUrl(const QString& entryId)
{
    const QString id = entryId.trimmed();
    if (id.isEmpty())
        return {};
    // arachnel://game/<id> — encode path segment so spaces/odd chars survive.
    const QByteArray encoded = QUrl::toPercentEncoding(id, QByteArray(), QByteArray("/"));
    return QStringLiteral("%1://%2/%3")
        .arg(QLatin1String(kScheme), QLatin1String(kGameHost), QString::fromUtf8(encoded));
}

QString parseGameIdFromDeepLink(const QString& raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty())
        return {};

    QUrl url(trimmed);
    if (!url.isValid())
        return {};
    if (url.scheme().compare(QLatin1String(kScheme), Qt::CaseInsensitive) != 0)
        return {};

    // arachnel://game/<id>  or  arachnel:game/<id>
    QString path = url.path();
    QString host = url.host();
    if (host.compare(QLatin1String(kGameHost), Qt::CaseInsensitive) == 0) {
        if (path.startsWith(QLatin1Char('/')))
            path.remove(0, 1);
    } else if (host.isEmpty()) {
        // arachnel:/game/id or path-only
        if (path.startsWith(QLatin1Char('/')))
            path.remove(0, 1);
        if (path.startsWith(QLatin1String("game/"), Qt::CaseInsensitive))
            path = path.mid(5);
        else if (path.compare(QLatin1String("game"), Qt::CaseInsensitive) == 0)
            path.clear();
        else
            return {};
    } else {
        return {};
    }

    const QString id = QUrl::fromPercentEncoding(path.toUtf8()).trimmed();
    return id;
}

QString findDeepLinkArgument(const QStringList& args)
{
    for (int i = 1; i < args.size(); ++i) {
        const QString& arg = args.at(i);
        if (arg.startsWith(QLatin1String("--")))
            continue;
        if (!parseGameIdFromDeepLink(arg).isEmpty())
            return arg.trimmed();
    }
    return {};
}

void registerGameDeepLinkProtocol()
{
#if defined(Q_OS_WIN)
    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    if (appPath.isEmpty())
        return;

    QSettings classes(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\arachnel"),
                      QSettings::NativeFormat);
    classes.setValue(QStringLiteral("."), QStringLiteral("URL:Arachnel Protocol"));
    classes.setValue(QStringLiteral("URL Protocol"), QString());

    QSettings command(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\arachnel\\shell\\open\\command"),
        QSettings::NativeFormat);
    command.setValue(QStringLiteral("."),
                     QStringLiteral("\"%1\" \"%2\"").arg(appPath, QStringLiteral("%1")));
#else
    // Linux: x-scheme-handler via .desktop (packaging/linux/arachnel.desktop).
#endif
}

QString SingleInstanceGuard::serverName()
{
    const QByteArray hash = QCryptographicHash::hash(
        (QDir::homePath() + QLatin1Char('|') + QStringLiteral("ArachnelDeepLink")).toUtf8(),
        QCryptographicHash::Sha1);
    return QStringLiteral("arachnel-si-%1").arg(QString::fromLatin1(hash.toHex().left(16)));
}

SingleInstanceGuard::SingleInstanceGuard(QObject* parent)
    : QObject(parent)
{
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    if (m_server) {
        m_server->close();
        QLocalServer::removeServer(serverName());
    }
}

bool SingleInstanceGuard::tryBecomePrimary()
{
    const QString name = serverName();

    QLocalSocket probe;
    probe.connectToServer(name);
    if (probe.waitForConnected(150)) {
        probe.disconnectFromServer();
        return false;
    }

    QLocalServer::removeServer(name);
    m_server = new QLocalServer(this);
    if (!m_server->listen(name)) {
        delete m_server;
        m_server = nullptr;
        return true; // proceed anyway rather than blocking launch
    }

    QObject::connect(m_server, &QLocalServer::newConnection, this,
                     &SingleInstanceGuard::onNewConnection);
    return true;
}

bool SingleInstanceGuard::forwardToPrimary(const QString& deepLinkUrl)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(500))
        return false;

    const QByteArray payload = deepLinkUrl.toUtf8() + '\n';
    socket.write(payload);
    socket.flush();
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return true;
}

void SingleInstanceGuard::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket* client = m_server->nextPendingConnection();
        if (!client)
            continue;
        QObject::connect(client, &QLocalSocket::readyRead, this, [this, client]() {
            const QByteArray raw = client->readAll();
            QString text = QString::fromUtf8(raw).trimmed();
            // Strip trailing newline fragments.
            while (text.endsWith(QLatin1Char('\n')) || text.endsWith(QLatin1Char('\r')))
                text.chop(1);
            emit messageReceived(text);
            client->disconnectFromServer();
            client->deleteLater();
        });
        QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
    }
}

} // namespace arachnel
