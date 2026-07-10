#pragma once

#include "catalog_types.h"
#include "job_kind.h"
#include "job_model.h"
#include "settings_store.h"

#include <QObject>
#include <QString>

namespace arachnel::core {

class TorrentSession;

class JobOrchestrator : public QObject
{
    Q_OBJECT

public:
    JobOrchestrator(SettingsStore* settings, TorrentSession* torrent, JobModel* jobs,
                    QObject* parent = nullptr);

    QString startCatalogDownload(const CatalogEntry& entry, JobKind kind);
    QString startAddonDownload(const CatalogEntry& parent, const CatalogComponent& addon);
    void cancelJob(const QString& jobId);

signals:
    void downloadCompleted(const QString& jobId, const QString& entryId, const QString& sourceId,
                           const QString& savePath, JobKind kind);
    void downloadFailed(const QString& jobId, const QString& error);

private:
    QString formatBytes(qint64 bytes) const;
    QString formatSpeed(int bytesPerSec) const;
    QString formatEta(qint64 remainingBytes, int bytesPerSec) const;
    QString buildDetail(qint64 downloaded, qint64 total, int downloadRate, int numPeers,
                        const QString& state) const;
    QString pickMagnet(const QStringList& uris) const;
    QString createJob(const QString& title, JobKind kind, const QString& entryId,
                      const QString& sourceId, const QString& magnet, const QString& saveSubdir);

    void onTorrentProgress(const QString& jobId, int progress, qint64 downloaded, qint64 total,
                           int downloadRate, int numPeers, const QString& state);
    void onTorrentFinished(const QString& jobId, const QString& savePath);
    void onTorrentFailed(const QString& jobId, const QString& error);

    SettingsStore* m_settings = nullptr;
    TorrentSession* m_torrent = nullptr;
    JobModel* m_jobs = nullptr;
    QHash<QString, JobKind> m_jobKinds;
};

} // namespace arachnel::core
