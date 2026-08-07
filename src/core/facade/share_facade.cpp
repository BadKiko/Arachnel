#include "core_controller_impl.h"

#include "deep_link.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>

namespace arachnel::core {

QString CoreController::pendingDeepLinkGameId() const
{
    return m_pendingDeepLinkGameId;
}

QString CoreController::gameShareUrl(const QString& entryId) const
{
    return arachnel::makeGameShareUrl(entryId);
}

void CoreController::shareGameLink(const QString& entryId)
{
    const QString url = arachnel::makeGameShareUrl(entryId);
    if (url.isEmpty())
        return;

    if (QGuiApplication* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        if (QClipboard* clipboard = gui->clipboard())
            clipboard->setText(url);
    }
    showNotice(QCoreApplication::translate("Core", "Link copied"), false);
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
    const QString id = arachnel::parseGameIdFromDeepLink(rawOrUrl);
    if (id.isEmpty() && !rawOrUrl.trimmed().isEmpty()) {
        // Already a bare game id from an internal forward.
        const QString bare = rawOrUrl.trimmed();
        if (!bare.contains(QLatin1String("://"))) {
            m_pendingDeepLinkGameId = bare;
            emit pendingDeepLinkChanged();
            emit deepLinkRequested(bare);
            emit activationRequested();
            return;
        }
        emit activationRequested();
        return;
    }
    if (id.isEmpty()) {
        emit activationRequested();
        return;
    }
    m_pendingDeepLinkGameId = id;
    emit pendingDeepLinkChanged();
    emit deepLinkRequested(id);
    emit activationRequested();
}

void CoreController::consumePendingDeepLink()
{
    if (m_pendingDeepLinkGameId.isEmpty())
        return;
    m_pendingDeepLinkGameId.clear();
    emit pendingDeepLinkChanged();
}

} // namespace arachnel::core
