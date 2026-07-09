#pragma once

#include "catalog_types.h"

#include <QObject>
#include <QHash>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

struct GameMetadata {
    QString coverUrl;
    QString description;
    QString genres;
    QString steamAppId;
};

class GameMetadataService : public QObject
{
    Q_OBJECT

public:
    explicit GameMetadataService(QObject* parent = nullptr);

    void enrichEntries(QVector<CatalogEntry>& entries);
    GameMetadata metadataForTitle(const QString& title) const;

signals:
    void entryEnriched(const QString& entryId);
    void enrichmentFinished();

private:
    void requestNext();
    void handleSearchFinished(QNetworkReply* reply);
    void handleDetailsFinished(QNetworkReply* reply);

    QNetworkAccessManager* m_network = nullptr;
    QHash<QString, GameMetadata> m_cache;
    QVector<CatalogEntry*> m_pending;
    int m_activeRequests = 0;
    static constexpr int kMaxConcurrent = 4;
};

} // namespace arachnel::core
