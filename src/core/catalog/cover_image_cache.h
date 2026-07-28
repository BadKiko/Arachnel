#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

enum class CoverFetchPriority {
    Upgrade = 0, // background HQ polish
    Warm = 1,    // prefetch after catalog ready
    Visible = 2, // on-screen cards
};

// Downloads remote covers once and serves them from AppData/cover-cache/.
// Never exposes https to callers of localUrlFor after ensure succeeds — only file: URLs.
class CoverImageCache : public QObject
{
    Q_OBJECT

public:
    explicit CoverImageCache(QObject* parent = nullptr);

    // file:///… if already on disk, otherwise empty.
    QString localUrlFor(const QString& remoteUrl) const;
    bool has(const QString& remoteUrl) const;
    bool hasFailed(const QString& remoteUrl) const;

    // If cached → emit ready (queued). Else enqueue download by priority.
    // Failed URLs (negative cache) emit failed immediately and do not hit the network.
    void ensure(const QString& remoteUrl,
                CoverFetchPriority priority = CoverFetchPriority::Visible);

    // Delete disk file, clear negative cache for this URL, drop from pending.
    // In-flight replies are marked ignored so they cannot rewrite the file.
    void remove(const QString& remoteUrl);
    void clearFailed(const QString& remoteUrl);

signals:
    void ready(const QString& remoteUrl, const QString& localUrl);
    void failed(const QString& remoteUrl);

private:
    struct PendingItem {
        QString url;
        CoverFetchPriority priority = CoverFetchPriority::Visible;
    };

    void handleFinished(QNetworkReply* reply);
    void startNext();
    void enqueue(const QString& remoteUrl, CoverFetchPriority priority);
    void markFailed(const QString& remoteUrl);
    void loadNegativeCache();
    void persistNegativeCache() const;
    QString cacheDir() const;
    QString failedCachePath() const;
    QString filePathFor(const QString& remoteUrl) const;
    static QString extensionFor(const QString& remoteUrl);
    static bool isRemoteHttpUrl(const QString& url);

    QNetworkAccessManager* m_network = nullptr;
    QSet<QString> m_inFlight;
    QSet<QString> m_ignoreWhenFinished;
    QHash<QString, CoverFetchPriority> m_pendingPriority;
    QVector<PendingItem> m_pending;
    QHash<QString, qint64> m_failedAtMs; // remoteUrl → epoch ms
    QHash<QString, QPointer<QNetworkReply>> m_replies;
    int m_active = 0;
    bool m_negativeDirty = false;

    static constexpr int kMaxConcurrent = 8;
    static constexpr qint64 kNegativeTtlMs = 7LL * 24 * 60 * 60 * 1000;
};

} // namespace arachnel::core
