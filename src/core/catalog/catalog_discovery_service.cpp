#include "catalog_discovery_service.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace arachnel::core {

namespace {

QString discoveryFeedPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/discovery-feed.json");
}

QStringList stringListFromJson(const QJsonValue& value)
{
    QStringList out;
    if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        out.reserve(arr.size());
        for (const QJsonValue& v : arr) {
            const QString id = v.toString().trimmed();
            if (!id.isEmpty())
                out.append(id);
        }
        return out;
    }
    if (value.isObject()) {
        // Allow { "entryIds": [...] } objects per shelf.
        return stringListFromJson(value.toObject().value(QStringLiteral("entryIds")));
    }
    return out;
}

} // namespace

CatalogDiscoveryService::CatalogDiscoveryService(QObject* parent)
    : QObject(parent)
    , m_onFire(new CatalogShelfModel(QStringLiteral("onFire"), this))
    , m_new(new CatalogShelfModel(QStringLiteral("new"), this))
    , m_friends(new CatalogShelfModel(QStringLiteral("friends"), this))
    , m_solo(new CatalogShelfModel(QStringLiteral("solo"), this))
    , m_onlineFix(new CatalogShelfModel(QStringLiteral("onlineFix"), this))
{
}

void CatalogDiscoveryService::setCache(QVector<CatalogEntry>* cache)
{
    m_cache = cache;
    bindShelves();
    if (!m_feedLoaded)
        loadFeedFromDisk();
    else
        reapplyFeed();
}

void CatalogDiscoveryService::setMoodId(const QString& moodId)
{
    const QString next = moodId.trimmed().toLower();
    if (m_moodId == next)
        return;
    m_moodId = next;
    emit moodIdChanged();
    reapplyFeed();
}

void CatalogDiscoveryService::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void CatalogDiscoveryService::refresh()
{
    setLoading(true);
    loadFeedFromDisk();
    setLoading(false);
}

void CatalogDiscoveryService::onCatalogCacheRebuilt()
{
    bindShelves();
    if (!m_feedLoaded)
        loadFeedFromDisk();
    else
        reapplyFeed();
}

void CatalogDiscoveryService::onEntryMetadataChanged(const QString& entryId)
{
    // Update card fields only — never reshuffle shelf membership.
    m_onFire->notifyEntryChanged(entryId);
    m_new->notifyEntryChanged(entryId);
    m_friends->notifyEntryChanged(entryId);
    m_solo->notifyEntryChanged(entryId);
    m_onlineFix->notifyEntryChanged(entryId);
}

void CatalogDiscoveryService::bindShelves()
{
    m_onFire->bindSource(m_cache);
    m_new->bindSource(m_cache);
    m_friends->bindSource(m_cache);
    m_solo->bindSource(m_cache);
    m_onlineFix->bindSource(m_cache);
}

void CatalogDiscoveryService::clearShelves()
{
    m_onFire->setVisibleIndices({});
    m_new->setVisibleIndices({});
    m_friends->setVisibleIndices({});
    m_solo->setVisibleIndices({});
    m_onlineFix->setVisibleIndices({});
}

QVector<int> CatalogDiscoveryService::indicesForEntryIds(const QStringList& entryIds) const
{
    QVector<int> indices;
    if (!m_cache || entryIds.isEmpty())
        return indices;

    QHash<QString, int> idToIndex;
    idToIndex.reserve(m_cache->size());
    for (int i = 0; i < m_cache->size(); ++i)
        idToIndex.insert(m_cache->at(i).id, i);

    indices.reserve(entryIds.size());
    for (const QString& id : entryIds) {
        const auto it = idToIndex.constFind(id);
        if (it == idToIndex.cend())
            continue;
        indices.append(it.value());
    }
    return indices;
}

QStringList CatalogDiscoveryService::entryIdsForShelf(const QString& shelfId) const
{
    const QJsonObject shelves = m_feed.value(QStringLiteral("shelves")).toObject();
    QStringList ids = stringListFromJson(shelves.value(shelfId));
    // Backward compat: older feeds used "onFire" for hits.
    if (ids.isEmpty() && shelfId == QLatin1String("hits"))
        ids = stringListFromJson(shelves.value(QStringLiteral("onFire")));
    return ids;
}

QStringList CatalogDiscoveryService::entryIdsForMood(const QString& moodId) const
{
    const QJsonObject filters = m_feed.value(QStringLiteral("filters")).toObject();
    return stringListFromJson(filters.value(moodId));
}

void CatalogDiscoveryService::reapplyFeed()
{
    if (!m_cache || !m_feedLoaded) {
        clearShelves();
        emit shelvesChanged();
        return;
    }

    bindShelves();

    if (!m_moodId.isEmpty()) {
        const QVector<int> filtered = indicesForEntryIds(entryIdsForMood(m_moodId));
        m_onFire->setVisibleIndices(filtered);
        m_new->setVisibleIndices({});
        m_friends->setVisibleIndices({});
        m_solo->setVisibleIndices({});
        m_onlineFix->setVisibleIndices({});
    } else {
        m_onFire->setVisibleIndices(indicesForEntryIds(entryIdsForShelf(QStringLiteral("hits"))));
        m_new->setVisibleIndices(indicesForEntryIds(entryIdsForShelf(QStringLiteral("new"))));
        m_friends->setVisibleIndices(indicesForEntryIds(entryIdsForShelf(QStringLiteral("friends"))));
        m_solo->setVisibleIndices(indicesForEntryIds(entryIdsForShelf(QStringLiteral("solo"))));
        m_onlineFix->setVisibleIndices({});
    }

    emit shelvesChanged();
}

bool CatalogDiscoveryService::loadFeedFromDisk()
{
    const QString path = discoveryFeedPath();
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        m_feed = {};
        if (m_feedLoaded) {
            m_feedLoaded = false;
            emit feedChanged();
        }
        clearShelves();
        emit shelvesChanged();
        return false;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        m_feed = {};
        m_feedLoaded = false;
        emit feedChanged();
        clearShelves();
        emit shelvesChanged();
        return false;
    }

    m_feed = doc.object();
    m_feedLoaded = true;
    emit feedChanged();
    reapplyFeed();
    return true;
}

} // namespace arachnel::core
