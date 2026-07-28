#pragma once

#include "catalog_types.h"
#include "cover_image_cache.h"

#include <functional>

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

class CatalogModel;
class GameMetadataService;
class SettingsStore;

// Owns cover policy: resolve → disk cache → file:-only model updates.
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
        bool interested = false;
        bool hqQueued = false;
    };

    void applyCoverToEntry(const QString& entryId, const QString& coverUrl, bool pending);
    void setPhase(const QString& entryId, Phase phase);
    void markPending(CatalogEntry* entry);
    void handleCacheReady(const QString& remoteUrl, const QString& localUrl);
    void handleCacheFailed(const QString& remoteUrl);
    void beginSteamPipeline(CatalogEntry* entry, CoverFetchPriority priority);
    void queueHqUpgrade(const QString& entryId);
    void queueHeroDownload(const QString& entryId, const QString& remoteUrl);
    void handleHeroReady(const QString& remoteUrl, const QString& localUrl);
    void handleHeroFailed(const QString& remoteUrl);
    void dropWaitersForEntry(const QString& entryId);
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
    QSet<QString> m_failedEntries;
};

} // namespace arachnel::core
