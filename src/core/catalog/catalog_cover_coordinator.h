#pragma once

#include "catalog_types.h"

#include <functional>

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

class CatalogModel;
class CoverImageCache;
class GameMetadataService;
class SettingsStore;

// Coordinates catalog-card cover metadata requests and local disk caching.
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
    void requestCatalogCover(const QString& entryId);
    void cancelCatalogCover(const QString& entryId);
    void invalidateCatalogCover(const QString& entryId);
    void applyCover(const QString& entryId, const QString& coverUrl);
    void ensureDiskCover(const QString& entryId, const QString& remoteUrl);

signals:
    // Lets the facade synchronize a matching library game's persisted cover URL.
    void coverApplied(const QString& entryId, const QString& coverUrl);

private:
    void applyCoverToEntry(const QString& entryId, const QString& coverUrl);
    void markCoverPending(CatalogEntry* entry);
    void handleCoverDownloadFailed(const QString& remoteUrl);
    QStringList steamCoverCandidates(const QString& steamAppId) const;
    QString nextSteamCoverFallback(const QString& entryId, const QString& failedUrl) const;
    static bool isRemoteLibraryCover(const QString& url);
    static bool isAllowedSteamCoverUrl(const QString& url);

    CoverImageCache* m_coverCache = nullptr;
    GameMetadataService* m_metadataService = nullptr;
    SettingsStore* m_settings = nullptr;
    CatalogModel* m_catalog = nullptr;
    EntryLookup m_findEntry;
    EntryList m_entries;
    QHash<QString, QSet<QString>> m_coverWaiters;
    // entryId -> last remote URL we tried (for fallback chain).
    QHash<QString, QString> m_coverAttempt;
    // Do not keep retrying entries that exhausted Steam CDN fallbacks.
    QSet<QString> m_coverGiveUp;
};

} // namespace arachnel::core
