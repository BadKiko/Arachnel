#pragma once

#include "catalog_types.h"

#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

class CatalogFeedLoader : public QObject
{
    Q_OBJECT

public:
    explicit CatalogFeedLoader(QObject* parent = nullptr);

    void loadFeed(const QUrl& url, const QString& sourceId);

signals:
    void feedLoaded(const QString& sourceId, QVector<CatalogEntry> entries);
    void feedFailed(const QString& sourceId, const QString& error);

private:
    void handleFinished(QNetworkReply* reply);
    void abortActiveReply();

    QNetworkAccessManager* m_network = nullptr;
    QPointer<QNetworkReply> m_activeReply;
    quint64 m_requestSerial = 0;
};

QVector<CatalogEntry> parseCatalogFeed(const QByteArray& payload, const QString& sourceId);

} // namespace arachnel::core
