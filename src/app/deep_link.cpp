#include "deep_link.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIODevice>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QUrl>
#include <QWindow>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <QSettings>
#endif

namespace arachnel {
namespace {

QString localServerName()
{
    return QStringLiteral("Arachnel-Arachnel-single-instance");
}

#if defined(Q_OS_WIN)
void forceActivateHwnd(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return;

    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);
    else
        ShowWindow(hwnd, SW_SHOW);

    const HWND foreground = GetForegroundWindow();
    const DWORD thisTid = GetCurrentThreadId();
    DWORD fgTid = 0;
    if (foreground)
        fgTid = GetWindowThreadProcessId(foreground, nullptr);

    if (fgTid && fgTid != thisTid)
        AttachThreadInput(fgTid, thisTid, TRUE);

    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    SetActiveWindow(hwnd);

    if (fgTid && fgTid != thisTid)
        AttachThreadInput(fgTid, thisTid, FALSE);
}
#endif

} // namespace

void forceActivateApplicationWindows()
{
    QGuiApplication* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (!gui)
        return;

    const QWindowList windows = gui->topLevelWindows();
    for (QWindow* window : windows) {
        if (!window)
            continue;
        // Skip transient dialogs without a title when a main window exists.
        if (window->visibility() == QWindow::Hidden)
            window->show();
        if (window->windowStates() & Qt::WindowMinimized)
            window->showNormal();
        else
            window->show();
        window->raise();
        window->requestActivate();
#if defined(Q_OS_WIN)
        forceActivateHwnd(reinterpret_cast<HWND>(window->winId()));
#endif
    }
}

void registerGameDeepLinkProtocol()
{
#if defined(Q_OS_WIN)
    QSettings classes(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\arachnel"),
                      QSettings::NativeFormat);
    classes.setValue(QStringLiteral("."), QStringLiteral("URL:Arachnel Protocol"));
    classes.setValue(QStringLiteral("URL Protocol"), QString());
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    classes.setValue(QStringLiteral("shell/open/command/."),
                     QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1")));
#else
    const QString apps =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (apps.isEmpty())
        return;
    QDir().mkpath(apps);
    const QString desktopPath = apps + QStringLiteral("/arachnel-url-handler.desktop");
    const QString execPath = QCoreApplication::applicationFilePath();
    const QByteArray body =
        QByteArrayLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=Arachnel URL Handler\n"
                          "Exec=\"")
        + QFile::encodeName(execPath) + QByteArrayLiteral("\" %u\n"
                                                          "NoDisplay=true\n"
                                                          "MimeType=x-scheme-handler/arachnel;\n"
                                                          "StartupNotify=false\n");
    QFile file(desktopPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(body);
        file.close();
    }
#endif
}

QString findDeepLinkArgument(const QStringList& arguments)
{
    for (const QString& arg : arguments) {
        if (arg.startsWith(QStringLiteral("arachnel:"), Qt::CaseInsensitive))
            return arg.trimmed();
    }
    return {};
}

QString parseDeepLinkGameId(const QString& rawOrUrl)
{
    const QString trimmed = rawOrUrl.trimmed();
    if (trimmed.isEmpty())
        return {};

    if (!trimmed.contains(QLatin1Char(':'))
        && !trimmed.contains(QLatin1Char('/'))) {
        return trimmed;
    }

    QUrl url(trimmed);
    if (!url.isValid())
        return {};

    // HTTPS share links: https://discover.badkiko.ru/open/game/<id>
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        || url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0) {
        const QString host = url.host().toLower();
        if (host == QStringLiteral("discover.badkiko.ru")
            || host == QStringLiteral("open.badkiko.ru")) {
            QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
            if (!parts.isEmpty() && parts.first().compare(QStringLiteral("open"), Qt::CaseInsensitive) == 0)
                parts.removeFirst();
            if (!parts.isEmpty() && parts.first().compare(QStringLiteral("game"), Qt::CaseInsensitive) == 0)
                parts.removeFirst();
            if (parts.isEmpty())
                return {};
            return QUrl::fromPercentEncoding(parts.first().toUtf8()).trimmed();
        }
        return {};
    }

    if (url.scheme().compare(QStringLiteral("arachnel"), Qt::CaseInsensitive) != 0)
        return {};

    QStringList parts;
    if (!url.host().isEmpty())
        parts.append(url.host());
    const QStringList pathParts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    parts.append(pathParts);

    if (parts.isEmpty())
        return {};

    if (parts.first().compare(QStringLiteral("game"), Qt::CaseInsensitive) == 0) {
        if (parts.size() < 2)
            return {};
        return QUrl::fromPercentEncoding(parts.at(1).toUtf8()).trimmed();
    }

    return QUrl::fromPercentEncoding(parts.first().toUtf8()).trimmed();
}

SingleInstanceGuard::SingleInstanceGuard(QObject* parent)
    : QObject(parent)
    , m_serverName(localServerName())
{
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    if (m_server) {
        m_server->close();
        QLocalServer::removeServer(m_serverName);
    }
}

bool SingleInstanceGuard::tryBecomePrimary()
{
    QLocalSocket probe;
    probe.connectToServer(m_serverName);
    if (probe.waitForConnected(400)) {
        probe.disconnectFromServer();
        return false;
    }

    QLocalServer::removeServer(m_serverName);

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_serverName)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QLocalServer::newConnection, this, &SingleInstanceGuard::onNewConnection);
    return true;
}

void SingleInstanceGuard::forwardToPrimary(const QString& payload)
{
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    if (!socket.waitForConnected(1500))
        return;

#if defined(Q_OS_WIN)
    // Secondary was launched by the user (browser / shell) and briefly owns
    // foreground rights. Hand them to the primary so SetForegroundWindow works.
    AllowSetForegroundWindow(ASFW_ANY);
#endif

    // Trailing newline so an empty deep-link still wakes readyRead (raise window).
    socket.write(payload.toUtf8() + '\n');
    socket.flush();
    socket.waitForBytesWritten(1500);
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.waitForDisconnected(1500);
}

void SingleInstanceGuard::onNewConnection()
{
    if (!m_server)
        return;
    while (QLocalSocket* socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() { readSocket(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
            if (socket->bytesAvailable() > 0)
                readSocket(socket);
            socket->deleteLater();
        });
        if (socket->bytesAvailable() > 0)
            readSocket(socket);
    }
}

void SingleInstanceGuard::readSocket(QLocalSocket* socket)
{
    if (!socket)
        return;
    const QByteArray data = socket->readAll();
    // One shot: readyRead + disconnected can both fire for the same payload.
    socket->disconnect(this);
    const QString payload = QString::fromUtf8(data).trimmed();
    emit messageReceived(payload);
}

} // namespace arachnel
