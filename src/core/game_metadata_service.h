#pragma once

#include "catalog_types.h"

#include <QObject>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QTimer>

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

    // Visible cards should call this — newest requests jump the queue (lazy-load priority).
    void queueFetch(const QString& entryId, const QString& title, MetadataFetchMode mode);
    // Drop pending (not in-flight) work when a GridView delegate is recycled.
    // Returns true if a queued request was removed.
    bool cancelPending(const QString& entryId);
    void clearCachedCover(const QString& title);
    GameMetadata metadataForTitle(const QString& title) const;

signals:
    void coverReady(const QString& entryId, const QString& coverUrl);
    void metadataReady(const QString& entryId, const GameMetadata& metadata);

private:
    struct PendingRequest {
        QString entryId;
        QString title;
        QStringList searchTerms;
        int termIndex = 0;
        MetadataFetchMode mode = MetadataFetchMode::CoverOnly;
    };

    void requestNext();
    void prependPending(PendingRequest request);
    void failCover(const QString& entryId);
    void handleSearchFinished(QNetworkReply* reply);
    void handleAssetsFinished(QNetworkReply* reply);
    void handleDetailsFinished(QNetworkReply* reply);
    void finishCover(const QString& entryId, const QString& title, const GameMetadata& metadata);
    void requestStoreAssets(const QString& entryId, const QString& title, const QString& appId,
                            MetadataFetchMode mode, const QStringList& remainingParentTerms = {});
    void requestAppDetails(const QString& entryId, const QString& title, const QString& appId,
                           const QString& coverUrl, MetadataFetchMode mode);
    int indexOfPending(const QString& entryId) const;

    QNetworkAccessManager* m_network = nullptr;
    QHash<QString, GameMetadata> m_cache;
    QList<PendingRequest> m_pending;
    QSet<QString> m_inFlight;
    int m_activeRequests = 0;
    QTimer* m_saveTimer = nullptr;

    // Few parallel Steam calls; queue is short and priority-ordered.
    static constexpr int kMaxConcurrent = 4;
    static constexpr int kMaxQueueSize = 16;
};

} // namespace arachnel::core
