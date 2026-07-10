#pragma once

#include "catalog_types.h"

#include <QObject>
#include <QHash>
#include <QSet>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

struct GameMetadata {
    QString coverUrl;
    QString description;
    QString genres;
    QString steamAppId;
};

enum class MetadataFetchMode {
    CoverOnly = 0,
    Full,
};

class GameMetadataService : public QObject
{
    Q_OBJECT

public:
    explicit GameMetadataService(QObject* parent = nullptr);

    void loadCache();
    void saveCache();

    void queueFetch(const QString& entryId, const QString& title, MetadataFetchMode mode);
    GameMetadata metadataForTitle(const QString& title) const;

signals:
    void coverReady(const QString& entryId, const QString& coverUrl);
    void metadataReady(const QString& entryId, const GameMetadata& metadata);

private:
    struct PendingRequest {
        QString entryId;
        QString title;
        MetadataFetchMode mode;
    };

    void requestNext();
    void handleSearchFinished(QNetworkReply* reply);
    void handleDetailsFinished(QNetworkReply* reply);
    void finishCover(const QString& entryId, const QString& title, const GameMetadata& metadata);

    QNetworkAccessManager* m_network = nullptr;
    QHash<QString, GameMetadata> m_cache;
    QList<PendingRequest> m_pending;
    QSet<QString> m_queuedIds;
    int m_activeRequests = 0;

    static constexpr int kMaxConcurrent = 3;
    static constexpr int kMaxQueueSize = 48;
};

} // namespace arachnel::core
