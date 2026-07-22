#include "catalog_feed_loader.h"

#include "catalog_parser.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVariant>
#include <QCoreApplication>

namespace arachnel::core {

CatalogFeedLoader::CatalogFeedLoader(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void CatalogFeedLoader::cancelActive()
{
    if (!m_activeReply)
        return;
    QNetworkReply* reply = m_activeReply.data();
    m_activeReply.clear();
    if (!reply)
        return;
    reply->disconnect(this);
    reply->abort();
    delete reply;
}

void CatalogFeedLoader::loadFeed(const QUrl& url, const QString& sourceId)
{
    cancelActive();

    const quint64 serial = ++m_requestSerial;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    // Qt 6.7+: abort hung catalog downloads so UI does not spin forever.
    request.setTransferTimeout(30000);
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("sourceId", sourceId);
    reply->setProperty("requestSerial", QVariant::fromValue(serial));
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleFinished(reply); });
}

void CatalogFeedLoader::handleFinished(QNetworkReply* reply)
{
    const QString sourceId = reply->property("sourceId").toString();
    const quint64 serial = reply->property("requestSerial").toULongLong();
    const bool isActive = (m_activeReply == reply);
    if (isActive)
        m_activeReply.clear();

    if (serial != m_requestSerial || !isActive) {
        reply->deleteLater();
        return;
    }

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit feedFailed(sourceId, reply->errorString());
        reply->deleteLater();
        return;
    }

    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    const QString validationError = catalogFeedValidationError(payload);
    if (!validationError.isEmpty()) {
        emit feedFailed(sourceId, validationError);
        return;
    }

  QString parseSourceId = sourceId;
    if (parseSourceId.startsWith(QStringLiteral("count:")))
        parseSourceId = parseSourceId.mid(6);

    const QVector<CatalogEntry> entries = parseCatalogFeed(payload, parseSourceId);

    if (entries.isEmpty()) {
        emit feedFailed(sourceId, QCoreApplication::translate("Core", "Catalog is empty or format not recognized"));
        return;
    }

    emit feedLoaded(sourceId, entries);
}

} // namespace arachnel::core
