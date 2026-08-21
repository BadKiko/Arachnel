#include "core_controller_impl.h"

#include "deep_link.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QUrl>

namespace arachnel::core {

QString CoreController::pendingDeepLinkGameId() const
{
    return m_pendingDeepLinkGameId;
}

QString CoreController::gameShareUrl(const QString& entryId) const
{
    const QString id = entryId.trimmed();
    if (id.isEmpty())
        return {};
    // HTTPS wrapper so messengers (Telegram, etc.) keep a clickable link.
    // Landing page on discover.badkiko.ru redirects to arachnel://game/<id>.
    return QStringLiteral("https://discover.badkiko.ru/open/game/%1")
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(id)));
}

void CoreController::shareGameLink(const QString& entryId)
{
    const QString url = gameShareUrl(entryId);
    if (url.isEmpty())
        return;

    if (QGuiApplication* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        if (QClipboard* clipboard = gui->clipboard())
            clipboard->setText(url);
    }
    showNotice(QCoreApplication::translate("Core", "Link copied"));
}

void CoreController::toggleBookmark(const QString& entryId)
{
    const QString id = entryId.trimmed();
    if (id.isEmpty())
        return;
    const QVariantMap info = entryDetails(id);
    const QString title = info.value(QStringLiteral("title")).toString();
    const QString coverUrl = info.value(QStringLiteral("coverUrl")).toString();
    QString sourceName = info.value(QStringLiteral("sourceName")).toString();
    if (sourceName.isEmpty())
        sourceName = info.value(QStringLiteral("sourceId")).toString();
    m_settings.toggleBookmark(id, title, coverUrl, sourceName);
}

void CoreController::requestDeepLink(const QString& rawOrUrl)
{
    const QString gameId = arachnel::parseDeepLinkGameId(rawOrUrl);
    if (gameId.isEmpty()) {
        // Empty payload from a secondary instance: just raise the window.
        if (rawOrUrl.trimmed().isEmpty()) {
            emit activationRequested();
            arachnel::forceActivateApplicationWindows();
        }
        return;
    }

    emit activationRequested();
    arachnel::forceActivateApplicationWindows();
    if (m_pendingDeepLinkGameId != gameId) {
        m_pendingDeepLinkGameId = gameId;
        emit pendingDeepLinkChanged();
    }
    emit deepLinkRequested(gameId);
}

void CoreController::consumePendingDeepLink()
{
    if (m_pendingDeepLinkGameId.isEmpty())
        return;
    m_pendingDeepLinkGameId.clear();
    emit pendingDeepLinkChanged();
}

void CoreController::forceActivateMainWindow()
{
    arachnel::forceActivateApplicationWindows();
}

} // namespace arachnel::core
