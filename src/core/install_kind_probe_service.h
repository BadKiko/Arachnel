#pragma once

#include "catalog_types.h"
#include "install_kind.h"

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QString>

namespace arachnel::core {

class PluginHost;

class InstallKindProbeService : public QObject
{
    Q_OBJECT

public:
    explicit InstallKindProbeService(PluginHost* pluginHost, QObject* parent = nullptr);

    std::optional<InstallKind> cachedKindForMagnet(const QString& magnetUri) const;
    void applyCachedKinds(QVector<CatalogEntry>& entries) const;

    void queueCatalog(const QString& sourceId, const QVector<CatalogEntry>& entries,
                      const QString& priorityQuery = {});
    void prioritizeEntry(const QString& sourceId, const QString& entryId,
                         const QString& magnetUri);

signals:
    void installKindResolved(const QString& entryId, InstallKind kind);

private:
    struct ProbeTask {
        QString entryId;
        QString sourceId;
        QString magnetUri;
        QString hashKey;
    };

    void enqueueTask(ProbeTask task, bool front = false);
    void pumpQueue();
    void persistCache() const;
    void loadCache();

    PluginHost* m_pluginHost = nullptr;
    QQueue<ProbeTask> m_queue;
    QSet<QString> m_queuedHashes;
    QSet<QString> m_inFlightHashes;
    QHash<QString, InstallKind> m_cacheByHash;
    int m_activeTasks = 0;

    static constexpr int kMaxConcurrent = 4;
};

} // namespace arachnel::core
