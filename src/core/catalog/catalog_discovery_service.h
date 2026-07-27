#pragma once

#include "catalog_shelf_model.h"
#include "catalog_types.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace arachnel::core {

/**
 * Discovery shelves from a precomputed feed JSON (backend later).
 * Does not scrape Steam or rebuild shelves from live metadata.
 */
class CatalogDiscoveryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString moodId READ moodId WRITE setMoodId NOTIFY moodIdChanged)
    Q_PROPERTY(bool moodActive READ moodActive NOTIFY moodIdChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool feedLoaded READ feedLoaded NOTIFY feedChanged)
    Q_PROPERTY(CatalogShelfModel* onFireShelf READ onFireShelf CONSTANT)
    Q_PROPERTY(CatalogShelfModel* newShelf READ newShelf CONSTANT)
    Q_PROPERTY(CatalogShelfModel* friendsShelf READ friendsShelf CONSTANT)
    Q_PROPERTY(CatalogShelfModel* soloShelf READ soloShelf CONSTANT)
    Q_PROPERTY(CatalogShelfModel* onlineFixShelf READ onlineFixShelf CONSTANT)

public:
    explicit CatalogDiscoveryService(QObject* parent = nullptr);

    void setCache(QVector<CatalogEntry>* cache);

    QString moodId() const { return m_moodId; }
    void setMoodId(const QString& moodId);
    bool moodActive() const { return !m_moodId.isEmpty(); }
    bool loading() const { return m_loading; }
    bool feedLoaded() const { return m_feedLoaded; }

    CatalogShelfModel* onFireShelf() const { return m_onFire; }
    CatalogShelfModel* newShelf() const { return m_new; }
    CatalogShelfModel* friendsShelf() const { return m_friends; }
    CatalogShelfModel* soloShelf() const { return m_solo; }
    CatalogShelfModel* onlineFixShelf() const { return m_onlineFix; }

    /** Reload discovery-feed.json from AppDataLocation (if present). */
    Q_INVOKABLE void refresh();
    void onEntryMetadataChanged(const QString& entryId);
    void onCatalogCacheRebuilt();

signals:
    void moodIdChanged();
    void loadingChanged();
    void feedChanged();
    void shelvesChanged();

private:
    void setLoading(bool loading);
    void bindShelves();
    void clearShelves();
    void reapplyFeed();
    bool loadFeedFromDisk();
    QVector<int> indicesForEntryIds(const QStringList& entryIds) const;
    QStringList entryIdsForShelf(const QString& shelfId) const;
    QStringList entryIdsForMood(const QString& moodId) const;

    QVector<CatalogEntry>* m_cache = nullptr;
    QJsonObject m_feed;
    QString m_moodId;
    bool m_loading = false;
    bool m_feedLoaded = false;

    CatalogShelfModel* m_onFire = nullptr;
    CatalogShelfModel* m_new = nullptr;
    CatalogShelfModel* m_friends = nullptr;
    CatalogShelfModel* m_solo = nullptr;
    CatalogShelfModel* m_onlineFix = nullptr;
};

} // namespace arachnel::core
