#pragma once

#include "catalog_types.h"
#include "cover_image_cache.h"

#include <functional>

#include <QHash>
#include <QObject>
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
    qint64 stuckRecoveries = 0;
    qint64 abandoned = 0;
    qint64 orphanReclaims = 0;
    qint64 applyLatencySumMs = 0;
    qint64 applyLatencyMaxMs = 0;
    int applyP50Ms = 0;
    int applyP95Ms = 0;
    int interested = 0;
    int downloading = 0;
};

// Owns cover policy: resolve → disk cache → file:-only model updates.
// Prefer catalog/relay remoteCoverUrl, then Steam CDN/metadata.
// Progressive: thumb first, then HQ upgrade. Never puts https into CatalogEntry.coverUrl.
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
                                 int limit = 24);

    /** Rebuild steamAppId → remoteCoverUrl index (call after catalog merge). */
    void rebuildRemoteCoverIndex();
    /** Drop negative cache entries so a fresh catalog can retry covers. */
    void clearFailedCoverHints();

    void requestCatalogCover(const QString& entryId,
                             CoverFetchPriority priority = CoverFetchPriority::Visible);
    void cancelCatalogCover(const QString& entryId);
    void invalidateCatalogCover(const QString& entryId);

    // Wide Steam banner for discovery hero (library_hero → header), file: only.
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
    enum class Phase {
        Idle = 0,
        Resolving,
        DownloadingThumb,
        ShowingThumb,
        DownloadingHq,
        Done,
        Failed,
    };

    struct EntryCoverState {
        Phase phase = Phase::Idle;
        CoverFetchPriority priority = CoverFetchPriority::Visible;
        QString activeRemote;
        QString catalogRemote; // relay/catalog URL being tried (or already failed)
        bool interested = false;
        bool hqQueued = false;
        bool catalogRemoteTried = false;
    };

    void applyCoverToEntry(const QString& entryId, const QString& coverUrl, bool pending);
    void setPhase(const QString& entryId, Phase phase);
    void markPending(CatalogEntry* entry);
    void handleCacheReady(const QString& remoteUrl, const QString& localUrl);
    void handleCacheFailed(const QString& remoteUrl);
    void beginSteamPipeline(CatalogEntry* entry, CoverFetchPriority priority);
    void beginMetadataPipeline(CatalogEntry* entry, CoverFetchPriority priority);
    bool beginCatalogRemote(CatalogEntry* entry, CoverFetchPriority priority);
    void continueAfterCatalogMiss(CatalogEntry* entry, CoverFetchPriority priority);
    void queueHqUpgrade(const QString& entryId);
    void queueHeroDownload(const QString& entryId, const QString& remoteUrl);
    void handleHeroReady(const QString& remoteUrl, const QString& localUrl);
    void handleHeroFailed(const QString& remoteUrl);
    void dropWaitersForEntry(const QString& entryId);
    bool entryHasWaiter(const QString& entryId) const;
    bool tryResumeKnownCover(CatalogEntry* entry, CoverFetchPriority priority);
    QSet<QString> takeWaitersForRemote(const QString& remoteUrl);
    void noteApplyLatency(const QString& entryId);
    void noteApplySample(qint64 ms);
    void refreshApplyPercentiles();
    QString resolveCatalogRemote(const CatalogEntry& entry) const;
    QString firstCachedSteamCover(const QString& steamAppId) const;
    QString steamThumbUrl(const QString& steamAppId) const;
    QString steamHqUrl(const QString& steamAppId) const;
    QString steamHeroUrl(const QString& steamAppId) const;
    QString steamHeaderUrl(const QString& steamAppId) const;
    QString firstCached(const QStringList& remotes) const;
    QString firstViableRemote(const QStringList& remotes) const;
    static bool isHttpUrl(const QString& url);
    static bool looksLikeSteamCover(const QString& url);

    CoverImageCache* m_coverCache = nullptr;
    GameMetadataService* m_metadataService = nullptr;
    SettingsStore* m_settings = nullptr;
    CatalogModel* m_catalog = nullptr;
    EntryLookup m_findEntry;
    EntryList m_entries;

    QHash<QString, EntryCoverState> m_state;
    QHash<QString, QSet<QString>> m_waiters;      // remote → entryIds
    QHash<QString, QSet<QString>> m_heroWaiters;  // remote → entryIds
    QHash<QString, QString> m_heroLocal;          // entryId → file:
    QHash<QString, QString> m_remoteBySteamAppId; // steamAppId → remoteCoverUrl
    QSet<QString> m_failedEntries;
    QHash<QString, qint64> m_requestAtMs;

    qint64 m_requests = 0;
    qint64 m_cancels = 0;
    qint64 m_applied = 0;
    qint64 m_stuckRecoveries = 0;
    qint64 m_abandoned = 0;
    qint64 m_orphanReclaims = 0;
    qint64 m_applyLatencySumMs = 0;
    qint64 m_applyLatencyMaxMs = 0;
    int m_applyP50Ms = 0;
    int m_applyP95Ms = 0;
    QVector<int> m_recentApplyMs;
    int m_recentApplyWrite = 0;
    static constexpr int kApplyLatencyWindow = 64;
};

} // namespace arachnel::core
