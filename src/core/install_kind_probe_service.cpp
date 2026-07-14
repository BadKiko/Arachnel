#include "install_kind_probe_service.h"
#include "plugin_host.h"
#include "torrent_metadata_fetcher.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>

namespace arachnel::core {

namespace {

QString cacheFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/install-kind-cache.json");
}

QString pickMagnet(const QStringList& uris)
{
    for (const QString& uri : uris) {
        if (uri.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive))
            return uri;
    }
    return uris.value(0);
}

} // namespace

InstallKindProbeService::InstallKindProbeService(PluginHost* pluginHost, QObject* parent)
    : QObject(parent)
    , m_pluginHost(pluginHost)
{
    loadCache();

    m_persistTimer = new QTimer(this);
    m_persistTimer->setSingleShot(true);
    m_persistTimer->setInterval(5000);
    connect(m_persistTimer, &QTimer::timeout, this, &InstallKindProbeService::persistCache);
}

std::optional<InstallKind> InstallKindProbeService::cachedKindForMagnet(
    const QString& magnetUri) const
{
    const QString hashKey = magnetInfoHashKey(magnetUri);
    if (hashKey.isEmpty())
        return std::nullopt;
    const auto it = m_cacheByHash.constFind(hashKey);
    if (it == m_cacheByHash.constEnd())
        return std::nullopt;
    return it.value();
}

void InstallKindProbeService::applyCachedKinds(QVector<CatalogEntry>& entries) const
{
    for (CatalogEntry& entry : entries) {
        const std::optional<InstallKind> cached = cachedKindForMagnet(pickMagnet(entry.magnetUris));
        if (cached)
            entry.installKind = *cached;
    }
}

void InstallKindProbeService::queueCatalog(const QString& sourceId,
                                           const QVector<CatalogEntry>& entries,
                                           const QString& priorityQuery)
{
    if (!m_pluginHost || !m_pluginHost->hasPlugin(sourceId))
        return;

    const QString needle = priorityQuery.trimmed().toLower();
    QVector<const CatalogEntry*> ordered;
    ordered.reserve(entries.size());
    for (const CatalogEntry& entry : entries)
        ordered.append(&entry);

    std::stable_sort(ordered.begin(), ordered.end(),
                     [&needle](const CatalogEntry* a, const CatalogEntry* b) {
                         if (needle.isEmpty())
                             return false;
                         const bool aMatch = a->title.toLower().contains(needle);
                         const bool bMatch = b->title.toLower().contains(needle);
                         if (aMatch == bMatch)
                             return false;
                         return aMatch && !bMatch;
                     });

    for (const CatalogEntry* entry : ordered) {
        if (entry->sourceId != sourceId)
            continue;
        const QString magnet = pickMagnet(entry->magnetUris);
        const QString hashKey = magnetInfoHashKey(magnet);
        if (magnet.isEmpty() || hashKey.isEmpty())
            continue;
        if (m_cacheByHash.contains(hashKey))
            continue;

        ProbeTask task;
        task.entryId = entry->id;
        task.sourceId = sourceId;
        task.magnetUri = magnet;
        task.hashKey = hashKey;
        enqueueTask(task, false);
    }

    pumpQueue();
}

void InstallKindProbeService::prioritizeEntry(const QString& sourceId, const QString& entryId,
                                              const QString& magnetUri)
{
    if (!m_pluginHost || !m_pluginHost->hasPlugin(sourceId))
        return;

    const QString hashKey = magnetInfoHashKey(magnetUri);
    if (magnetUri.isEmpty() || hashKey.isEmpty())
        return;

    const auto cached = m_cacheByHash.constFind(hashKey);
    if (cached != m_cacheByHash.constEnd()) {
        emit installKindResolved(entryId, cached.value());
        return;
    }

    ProbeTask task;
    task.entryId = entryId;
    task.sourceId = sourceId;
    task.magnetUri = magnetUri;
    task.hashKey = hashKey;
    enqueueTask(task, true);
    pumpQueue();
}

void InstallKindProbeService::setBackgroundProbesEnabled(bool enabled)
{
    if (m_probesEnabled == enabled)
        return;
    m_probesEnabled = enabled;
    if (enabled)
        pumpQueue();
}

void InstallKindProbeService::enqueueTask(ProbeTask task, bool front)
{
    if (m_queuedHashes.contains(task.hashKey) || m_inFlightHashes.contains(task.hashKey))
        return;

    m_queuedHashes.insert(task.hashKey);
    if (front)
        m_queue.prepend(task);
    else
        m_queue.enqueue(task);
}

void InstallKindProbeService::pumpQueue()
{
    if (!m_probesEnabled)
        return;

    while (m_activeTasks < kMaxConcurrent && !m_queue.isEmpty()) {
        const ProbeTask task = m_queue.dequeue();
        m_queuedHashes.remove(task.hashKey);
        m_inFlightHashes.insert(task.hashKey);
        ++m_activeTasks;

        QtConcurrent::run([this, task]() {
            QStringList fileNames;
            if (const std::optional<QStringList> fetched = fetchMagnetFileNames(task.magnetUri))
                fileNames = *fetched;

            QTimer::singleShot(0, this, [this, task, fileNames]() {
                m_inFlightHashes.remove(task.hashKey);
                --m_activeTasks;

                InstallKind kind = InstallKind::PortableArchive;
                bool resolved = false;
                if (!fileNames.isEmpty()) {
                    if (ISourcePlugin* plugin = m_pluginHost->plugin(task.sourceId)) {
                        kind = plugin->detectInstallKindFromFileNames(fileNames);
                        resolved = true;
                    }
                }

                if (resolved && !task.hashKey.isEmpty()) {
                    m_cacheByHash.insert(task.hashKey, kind);
                    m_persistTimer->start();
                    emit installKindResolved(task.entryId, kind);
                }

                pumpQueue();
            });
        });
    }
}

void InstallKindProbeService::persistCache() const
{
    QJsonObject root;
    for (auto it = m_cacheByHash.constBegin(); it != m_cacheByHash.constEnd(); ++it)
        root.insert(it.key(), static_cast<int>(it.value()));

    QFile file(cacheFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void InstallKindProbeService::loadCache()
{
    QFile file(cacheFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return;

    const QJsonObject root = document.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const int raw = it.value().toInt(static_cast<int>(InstallKind::PortableArchive));
        if (raw >= static_cast<int>(InstallKind::PortableArchive)
            && raw <= static_cast<int>(InstallKind::FixDownload)) {
            m_cacheByHash.insert(it.key(), static_cast<InstallKind>(raw));
        }
    }
}

} // namespace arachnel::core
