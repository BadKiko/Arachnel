#pragma once

#include "catalog_types.h"
#include "job_kind.h"
#include "job_model.h"
#include "job_store.h"
#include "plugin_interface.h"
#include "settings_store.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace arachnel::core {

class HttpDownloadSession;
class TorrentSession;

class JobOrchestrator : public QObject
{
    Q_OBJECT

public:
    JobOrchestrator(SettingsStore* settings, JobStore* jobStore, TorrentSession* torrent,
                    HttpDownloadSession* http, JobModel* jobs, QObject* parent = nullptr);

    void restoreJobs();
    void flushPersistence();
    QString startCatalogDownload(const CatalogEntry& entry, JobKind kind,
                                 const QString& libraryId = {});
    QString startAddonDownload(const CatalogEntry& parent, const CatalogComponent& addon);
    /** Create a job slot for plugin-owned downloads (no torrent/HTTP session). */
    QString startPluginOwnedDownload(const CatalogEntry& entry, JobKind kind,
                                     const QString& libraryId = {});
    void reportPluginProgress(const QString& jobId, const OwnedDownloadProgress& progress);
    void completePluginDownload(const QString& jobId, const QString& installPath);
    void failPluginDownload(const QString& jobId, const QString& error);
    void cancelJob(const QString& jobId);
    void toggleJobPause(const QString& jobId);
    void removeJob(const QString& jobId);
    void retryJob(const QString& jobId);
    void clearFinishedJobs();
    void setJobPhase(const QString& jobId, const QString& status, const QString& detail);

signals:
    void downloadCompleted(const QString& jobId, const QString& entryId, const QString& sourceId,
                           const QString& artifactPath, JobKind kind, const QString& libraryId);
    void downloadFailed(const QString& jobId, const QString& error);

private:
    QString pickMagnet(const QStringList& uris) const;
    QString createJob(const QString& title, JobKind kind, const QString& entryId,
                      const QString& sourceId, const QString& downloadUri, const QString& saveSubdir,
                      const QString& coverUrl, const QString& libraryId,
                      const QString& parentEntryId = {}, bool httpDownload = false,
                      const QString& referer = {});
    void startTorrent(const JobEntry& job);
    void startHttp(const JobEntry& job);
    void persistJob(const JobEntry& job);
    JobEntry jobFromModelRow(int row) const;
    void updateJobInModel(const JobEntry& job);
    QString findActiveJobId(const QString& entryId, const QString& parentEntryId) const;
    QString findExistingJobId(const QString& entryId, const QString& parentEntryId) const;

    QString formatBytes(qint64 bytes) const;
    QString formatSpeed(int bytesPerSec) const;
    QString formatEta(qint64 remainingBytes, int bytesPerSec) const;
    QString buildDetail(qint64 downloaded, qint64 total, int downloadRate, int numPeers,
                        const QString& state) const;
    QString buildHttpDetail(qint64 downloaded, qint64 total) const;
    QString buildTransferDetail(qint64 downloaded, qint64 total, int downloadRate) const;

    void onTorrentProgress(const QString& jobId, int progress, qint64 downloaded, qint64 total,
                           int downloadRate, int numPeers, const QString& state);
    void onTorrentFinished(const QString& jobId, const QString& savePath);
    void onTorrentFailed(const QString& jobId, const QString& error);
    void onHttpProgress(const QString& jobId, int progress, qint64 downloaded, qint64 total);
    void onHttpFinished(const QString& jobId, const QString& filePath);
    void onHttpFailed(const QString& jobId, const QString& error);

    struct SpeedSample {
        qint64 bytes = 0;
        qint64 ms = 0;
        int rate = 0;
    };

    SettingsStore* m_settings = nullptr;
    JobStore* m_jobStore = nullptr;
    TorrentSession* m_torrent = nullptr;
    HttpDownloadSession* m_http = nullptr;
    JobModel* m_jobs = nullptr;
    QHash<QString, JobKind> m_jobKinds;
    QHash<QString, SpeedSample> m_pluginSpeed;
    QTimer m_persistTimer;
    bool m_dirty = false;
};

} // namespace arachnel::core
