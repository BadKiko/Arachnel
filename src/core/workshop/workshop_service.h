#pragma once

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

class CoverImageCache;

struct WorkshopItem {
    QString publishedFileId;
    QString title;
    QString description;
    QString previewUrl;
    QString localPreviewUrl;
    QStringList screenshotUrls;
    bool screenshotsResolved = false;
    qint64 fileSize = 0;
    qint64 timeUpdated = 0;
    int subscriptions = 0;
};

class WorkshopService : public QObject
{
    Q_OBJECT

public:
    explicit WorkshopService(CoverImageCache* covers, QObject* parent = nullptr);

    void requestPage(const QString& steamAppId, int page);
    QVariantList itemsForApp(const QString& steamAppId) const;
    void requestPreview(const QString& previewUrl);
    void releasePreview(const QString& previewUrl);
    QString localPreviewUrl(const QString& previewUrl) const;
    std::optional<WorkshopItem> itemById(const QString& steamAppId,
                                         const QString& publishedFileId) const;

signals:
    void pageReady(const QString& steamAppId, int page, const QVariantList& items, bool hasMore);
    void pageFailed(const QString& steamAppId, int page, const QString& error);
    void previewReady(const QString& previewUrl, const QString& localUrl);
    void itemUpdated(const QString& steamAppId, const QVariantMap& item);

private:
    struct ScreenshotJob {
        QString steamAppId;
        QString publishedFileId;
    };

    void fetchBrowsePage(const QString& steamAppId, int page);
    void fetchDetails(const QString& steamAppId, int page, const QStringList& ids);
    void emitPage(const QString& steamAppId, int page, const QVector<WorkshopItem>& items,
                  bool hasMore);
    bool loadCache(const QString& steamAppId, int page, QVector<WorkshopItem>* out, bool* hasMore);
    void saveCache(const QString& steamAppId, int page, const QVector<WorkshopItem>& items,
                   bool hasMore);
    QString cachePath(const QString& steamAppId, int page) const;
    QString screenshotCachePath(const QString& publishedFileId) const;
    bool loadScreenshotCache(const QString& publishedFileId, QStringList* out) const;
    void saveScreenshotCache(const QString& publishedFileId, const QStringList& urls) const;
    void applyScreenshotCaches(QVector<WorkshopItem>* items);
    void enqueueScreenshotFetches(const QString& steamAppId, const QVector<WorkshopItem>& items);
    void pumpScreenshotQueue();
    void fetchItemScreenshots(const ScreenshotJob& job);
    void applyScreenshots(const QString& steamAppId, const QString& publishedFileId,
                          const QStringList& urls);
    QVariantMap itemToMap(const WorkshopItem& item) const;
    void applyLocalPreviews(QVector<WorkshopItem>* items);

    CoverImageCache* m_covers = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    QHash<QString, QVector<WorkshopItem>> m_itemsByApp;
    QSet<QString> m_inFlight;
    QSet<QString> m_screenshotInFlight;
    QQueue<ScreenshotJob> m_screenshotQueue;
    int m_screenshotActive = 0;
};

} // namespace arachnel::core
