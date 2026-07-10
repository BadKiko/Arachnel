#pragma once

#include "catalog_types.h"
#include "job_kind.h"
#include "job_model.h"
#include "job_store.h"
#include "settings_store.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace arachnel::core {

class TorrentSession;

class JobOrchestrator : public QObject
{
    Q_OBJECT

public:
    JobOrchestrator(SettingsStore* settings, JobStore* jobStore, TorrentSession* torrent,
                    JobModel* jobs, QObject* parent = nullptr);

    void restoreJobs();
    void flushPersistence();
    QString startCatalogDownload(const CatalogEntry& entry, JobKind kind);
    QString startAddonDownload(const CatalogEntry& parent, const CatalogComponent& addon);
    void cancelJob(const QString& jobId);
    void toggleJobPause(const QString& jobId);
    void clearFinishedJobs();

signals:
    void downloadCompleted(const QString& jobId, const QString& entryId, const QString& sourceId,
                           const QString& savePath, JobKind kind);
    void downloadFailed(const QString& jobId, const QString& error);

private:
    QString pickMagnet(const QStringList& uris) const;
    QString createJob(const QString& title, JobKind kind, const QString& entryId,
                      const QString& sourceId, const QString& magnet, const QString& saveSubdir,
                      const QString& coverUrl);
    void startTorrent(const JobEntry& job);
    void persistJob(const JobEntry& job);
    JobEntry jobFromModelRow(int row) const;
    void updateJobInModel(const JobEntry& job);

    QString formatBytes(qint64 bytes) const;
    QString formatSpeed(int bytesPerSec) const;
    QString formatEta(qint64 remainingBytes, int bytesPerSec) const;
    QString buildDetail(qint64 downloaded, qint64 total, int downloadRate, int numPeers,
                        const QString& state) const;

    void onTorrentProgress(const QString& jobId, int progress, qint64 downloaded, qint64 total,
                           int downloadRate, int numPeers, const QString& state);
    void onTorrentFinished(const QString& jobId, const QString& savePath);
    void onTorrentFailed(const QString& jobId, const QString& error);

    SettingsStore* m_settings = nullptr;
    JobStore* m_jobStore = nullptr;
    TorrentSession* m_torrent = nullptr;
    JobModel* m_jobs = nullptr;
    QHash<QString, JobKind> m_jobKinds;
    QTimer m_persistTimer;
    bool m_dirty = false;
};

} // namespace arachnel::core
