#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

enum class CoverFetchPriority {
    Upgrade = 0, // background HQ polish
    Warm = 1,    // prefetch after catalog ready
    Visible = 2, // on-screen cards
};

struct CoverCacheStats {
    qint64 ensureCalls = 0;
    qint64 cacheHits = 0;
    qint64 negativeHits = 0;
    qint64 downloadsStarted = 0;
    qint64 downloadsOk = 0;
    qint64 downloadsFail = 0;
    qint64 preempts = 0;
    qint64 latencySumMs = 0;
    qint64 latencyMaxMs = 0;
    int pending = 0;
    int active = 0;
    int p50Ms = 0;
    int p95Ms = 0;
};

// Downloads remote covers once and serves them from AppData/cover-cache/.
// Never exposes https to callers of localUrlFor after ensure succeeds - only file: URLs.
class CoverImageCache : public QObject
{
    Q_OBJECT

public:
    explicit CoverImageCache(QObject* parent = nullptr);

    // file:///… if already on disk / memory, otherwise empty.
    QString localUrlFor(const QString& remoteUrl) const;
    bool has(const QString& remoteUrl) const;
    bool hasFailed(const QString& remoteUrl) const;

    // If cached → emit ready (queued). Else enqueue download by priority.
    // Failed URLs (negative cache) emit failed immediately and do not hit the network.
    void ensure(const QString& remoteUrl,
                CoverFetchPriority priority = CoverFetchPriority::Visible);

    // Drop from pending queue. In-flight is demoted so Visible can preempt the slot.
    void release(const QString& remoteUrl);
    bool isInFlight(const QString& remoteUrl) const;

    // Delete disk file, clear negative cache for this URL, drop from pending.
    // In-flight replies are marked ignored so they cannot rewrite the file.
    void remove(const QString& remoteUrl);
    void clearFailed(const QString& remoteUrl);
    void clearAllFailed();
    void noteCacheHit(); // coordinator applied a disk/memory hit without ensure()

    CoverCacheStats stats() const;
    QVariantMap statsMap() const;
    void resetStats();

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
    void removePending(const QString& remoteUrl);
    void trimPending();
    void preemptForVisible();
    void markFailed(const QString& remoteUrl);
    void loadNegativeCache();
    void persistNegativeCache() const;
    void schedulePersistNegativeCache();
    void rememberPositive(const QString& remoteUrl, const QString& localUrl) const;
    void noteLatency(qint64 ms);
    void refreshPercentiles();
    QString cacheDir() const;
    QString failedCachePath() const;
    QString filePathFor(const QString& remoteUrl) const;
    static QString extensionFor(const QString& remoteUrl);
    static bool isRemoteHttpUrl(const QString& url);

    QNetworkAccessManager* m_network = nullptr;
    QSet<QString> m_inFlight;
    QSet<QString> m_ignoreWhenFinished;
    QSet<QString> m_preempted; // aborted for Visible - requeue, do not fail
    QHash<QString, CoverFetchPriority> m_pendingPriority;
    QHash<QString, CoverFetchPriority> m_inFlightPriority;
    QHash<QString, qint64> m_downloadStartedAtMs;
    QVector<PendingItem> m_pending;
    QHash<QString, qint64> m_failedAtMs; // remoteUrl → epoch ms
    mutable QHash<QString, QString> m_positiveLocal; // remoteUrl → file:
    QHash<QString, QPointer<QNetworkReply>> m_replies;
    QTimer m_persistTimer;
    int m_active = 0;
    mutable bool m_negativeDirty = false;

    qint64 m_ensureCalls = 0;
    qint64 m_cacheHits = 0;
    qint64 m_negativeHits = 0;
    qint64 m_downloadsStarted = 0;
    qint64 m_downloadsOk = 0;
    qint64 m_downloadsFail = 0;
    qint64 m_preempts = 0;
    qint64 m_latencySumMs = 0;
    qint64 m_latencyMaxMs = 0;
    int m_p50Ms = 0;
    int m_p95Ms = 0;
    QVector<int> m_recentLatenciesMs;
    int m_recentWrite = 0;

    // Viewport covers first; keep queue short so scroll-churn cannot bury them.
    static constexpr int kMaxConcurrent = 8;
    static constexpr int kMaxPending = 48;
    static constexpr int kLatencyWindow = 64;
    // Soft negative TTL - catalog refresh also clears via clearAllFailed().
    static constexpr qint64 kNegativeTtlMs = 60LL * 60 * 1000;
};
} // namespace arachnel::core
