#pragma once

#include "catalog_types.h"
#include "cover_image_cache.h"

#include <functional>

#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace arachnel::core {

class CatalogModel;
class GameMetadataService;
class SettingsStore;

struct CoverCoordinatorStats {
    qint64 requests = 0;
    qint64 cancels = 0;
    qint64 applied = 0;
    qint64 abandoned = 0;
    qint64 dupSuppressed = 0;
    qint64 ladderAdvances = 0;
    qint64 applyLatencySumMs = 0;
    qint64 applyLatencyMaxMs = 0;
    int applyP50Ms = 0;
    int applyP95Ms = 0;
    int interested = 0;
    int plansActive = 0;
};

// Viewport-first cover policy: one CoverPlan per entry, one in-flight URL.
// Never puts https into CatalogEntry.coverUrl (file: only).
class CatalogCoverCoordinator : public QObject
{
    Q_OBJECT

public:
    using EntryLookup = std::function<CatalogEntry*(const QString&)>;
    using EntryList = std::function<QVector<CatalogEntry>&()>;

    CatalogCoverCoordinator(CoverImageCache* coverCache, GameMetadataService* metadataService,
                            SettingsStore* settings, CatalogModel* catalog, EntryLookup findEntry,
                            EntryList entries, QObject* parent = nullptr);

    void warmCatalogCovers(const QString& sourceId, const QString& query, int limit);
    void warmActiveCatalogCovers(const QStringList& sourceIds, const QString& query,
                                 int limit = 12);

    void rebuildRemoteCoverIndex();
    void clearFailedCoverHints();

    void setCacheLock(QReadWriteLock* lock) { m_cacheLock = lock; }

    void requestCatalogCover(const QString& entryId,
                             CoverFetchPriority priority = CoverFetchPriority::Visible);
    void cancelCatalogCover(const QString& entryId);
    void invalidateCatalogCover(const QString& entryId);

    void requestCatalogHeroCover(const QString& entryId);
    QString heroCoverUrl(const QString& entryId) const;

    void applyCover(const QString& entryId, const QString& coverUrl);
    void ensureDiskCover(const QString& entryId, const QString& remoteUrl,
                         CoverFetchPriority priority = CoverFetchPriority::Visible);

    CoverCoordinatorStats stats() const;
    QVariantMap statsMap() const;
    void resetStats();
    QString metricsText() const;

signals:
    void coverApplied(const QString& entryId, const QString& coverUrl);
    void heroCoverApplied(const QString& entryId, const QString& coverUrl);

private:
    enum class PlanPhase {
        Idle = 0,
        Fetching,
        Showing,
        Done,
        Failed,
    };

    struct CoverPlan {
        PlanPhase phase = PlanPhase::Idle;
        CoverFetchPriority priority = CoverFetchPriority::Visible;
        quint64 generation = 0;
        bool interested = false;
        QStringList urls; // ordered ladder (network only; disk hits applied immediately)
        int index = 0;
        QString activeRemote;
        QString appliedLocal;
        bool hqQueued = false;
    };

    void applyCoverToEntry(const QString& entryId, const QString& coverUrl, bool pending);
    void markPending(CatalogEntry* entry);
    void clearPendingFlag(CatalogEntry* entry);
    void handleCacheReady(const QString& remoteUrl, const QString& localUrl);
    void handleCacheFailed(const QString& remoteUrl);
    void handleMetadataCover(const QString& entryId, const QString& coverUrl);

    void buildPlanUrls(CatalogEntry* entry, CoverPlan& plan) const;
    void startOrContinuePlan(const QString& entryId, CoverPlan& plan);
    void advancePlan(const QString& entryId, CoverPlan& plan);
    void ensureCurrentUrl(const QString& entryId, CoverPlan& plan);
    void releasePlanRemote(const QString& entryId, CoverPlan& plan);
    void dropWaiter(const QString& entryId, const QString& remoteUrl);
    void queueHeroDownload(const QString& entryId, const QString& remoteUrl);
    void handleHeroReady(const QString& remoteUrl, const QString& localUrl);
    void handleHeroFailed(const QString& remoteUrl);

    void noteApplyLatency(const QString& entryId);
    void noteApplySample(qint64 ms);
    void refreshApplyPercentiles();

    QString resolveCatalogRemote(const CatalogEntry& entry) const;
    QString steamThumbUrl(const QString& steamAppId) const;
    QString steamHqUrl(const QString& steamAppId) const;
    QString steamHeroUrl(const QString& steamAppId) const;
    QString steamHeaderUrl(const QString& steamAppId) const;
    QString firstCached(const QStringList& remotes) const;
    static bool isHttpUrl(const QString& url);
    static bool isHqUrl(const QString& url);

    CoverImageCache* m_coverCache = nullptr;
    GameMetadataService* m_metadataService = nullptr;
    SettingsStore* m_settings = nullptr;
    CatalogModel* m_catalog = nullptr;
    QReadWriteLock* m_cacheLock = nullptr;
    EntryLookup m_findEntry;
    EntryList m_entries;

    QHash<QString, CoverPlan> m_plans;
    QHash<QString, QSet<QString>> m_waiters;     // remote → entryIds
    QHash<QString, QSet<QString>> m_heroWaiters; // remote → entryIds
    QHash<QString, QString> m_heroLocal;         // entryId → file:
    QHash<QString, QString> m_remoteBySteamAppId;
    QSet<QString> m_failedEntries;
    QHash<QString, qint64> m_requestAtMs;

    qint64 m_requests = 0;
    qint64 m_cancels = 0;
    qint64 m_applied = 0;
    qint64 m_abandoned = 0;
    qint64 m_dupSuppressed = 0;
    qint64 m_ladderAdvances = 0;
    qint64 m_applyLatencySumMs = 0;
    qint64 m_applyLatencyMaxMs = 0;
    int m_applyP50Ms = 0;
    int m_applyP95Ms = 0;
    QVector<int> m_recentApplyMs;
    int m_recentApplyWrite = 0;
    static constexpr int kApplyLatencyWindow = 64;
};

} // namespace arachnel::core
