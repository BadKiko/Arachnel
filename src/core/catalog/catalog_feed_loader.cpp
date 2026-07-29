#include "catalog_feed_loader.h"

#include "catalog_disk_cache.h"
#include "catalog_parser.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QVariant>
#include <QtConcurrent>

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

void CatalogFeedLoader::loadFeed(const QUrl& url, const QString& sourceId, const QByteArray& etag)
{
    cancelActive();

    const quint64 serial = ++m_requestSerial;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    request.setTransferTimeout(30000);
    if (!etag.isEmpty())
        request.setRawHeader("If-None-Match", etag);
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

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus == 304) {
        emit feedNotModified(sourceId);
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit feedFailed(sourceId, reply->errorString());
        reply->deleteLater();
        return;
    }

    const QByteArray payload = reply->readAll();
    const QByteArray etag = reply->rawHeader("ETag");
    reply->deleteLater();

    const bool countOnly = sourceId.startsWith(QStringLiteral("count:"));
    const quint64 capturedSerial = serial;

    (void)QtConcurrent::run([this, sourceId, payload, etag, countOnly, capturedSerial]() {
        if (countOnly) {
            const int count = catalogFeedQuickCount(payload);
            QTimer::singleShot(0, this, [this, sourceId, count, capturedSerial]() {
                if (capturedSerial != m_requestSerial)
                    return;
                if (count < 0) {
                    emit feedFailed(sourceId,
                                    QCoreApplication::translate(
                                        "Core", "Catalog is empty or format not recognized"));
                    return;
                }
                emit feedCountLoaded(sourceId, count);
            });
            return;
        }

        const QString validationError = catalogFeedValidationError(payload);
        if (!validationError.isEmpty()) {
            QTimer::singleShot(0, this, [this, sourceId, validationError, capturedSerial]() {
                if (capturedSerial != m_requestSerial)
                    return;
                emit feedFailed(sourceId, validationError);
            });
            return;
        }

        QString parseSourceId = sourceId;
        if (parseSourceId.startsWith(QStringLiteral("count:")))
            parseSourceId = parseSourceId.mid(6);

        QVector<CatalogEntry> entries = parseCatalogFeed(payload, parseSourceId);
        const QByteArray sha = CatalogDiskCache::payloadSha256(payload);
        if (!entries.isEmpty() && !sourceId.startsWith(QStringLiteral("validate:")))
            CatalogDiskCache::savePayload(parseSourceId, payload, etag);

        QTimer::singleShot(0, this,
                           [this, sourceId, entries = std::move(entries), sha,
                            capturedSerial]() mutable {
                               if (capturedSerial != m_requestSerial)
                                   return;
                               if (entries.isEmpty()) {
                                   emit feedFailed(sourceId,
                                                   QCoreApplication::translate(
                                                       "Core",
                                                       "Catalog is empty or format not recognized"));
                                   return;
                               }
                               emit feedLoaded(sourceId, std::move(entries), sha);
                           });
    });
}

} // namespace arachnel::core
