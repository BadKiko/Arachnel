#pragma once

#include "catalog_types.h"
#include "install_kind.h"

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QString>

class QTimer;

namespace arachnel::core {

class InstallAnalyzer;

class InstallKindProbeService : public QObject
{
    Q_OBJECT

public:
    explicit InstallKindProbeService(InstallAnalyzer* analyzer, QObject* parent = nullptr);

    std::optional<InstallKind> cachedKindForMagnet(const QString& magnetUri) const;
    void applyCachedKinds(QVector<CatalogEntry>& entries) const;

    void queueCatalog(const QString& sourceId, const QVector<CatalogEntry>& entries,
                      const QString& priorityQuery = {});
    void prioritizeEntry(const QString& sourceId, const QString& entryId,
                         const QString& magnetUri);

    void setBackgroundProbesEnabled(bool enabled);

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

    InstallAnalyzer* m_analyzer = nullptr;
    QQueue<ProbeTask> m_queue;
    QSet<QString> m_queuedHashes;
    QSet<QString> m_inFlightHashes;
    QHash<QString, InstallKind> m_cacheByHash;
    int m_activeTasks = 0;
    bool m_probesEnabled = true;
    QTimer* m_persistTimer = nullptr;

    static constexpr int kMaxConcurrent = 1;
};

} // namespace arachnel::core
