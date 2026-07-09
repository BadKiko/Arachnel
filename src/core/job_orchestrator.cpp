#include "job_orchestrator.h"

#include "torrent_session.h"

#include <QUuid>

namespace arachnel::core {

JobOrchestrator::JobOrchestrator(SettingsStore* settings, TorrentSession* torrent, JobModel* jobs,
                                 QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_torrent(torrent)
    , m_jobs(jobs)
{
    connect(m_torrent, &TorrentSession::torrentProgress, this, &JobOrchestrator::onTorrentProgress);
    connect(m_torrent, &TorrentSession::torrentFinished, this, &JobOrchestrator::onTorrentFinished);
    connect(m_torrent, &TorrentSession::torrentFailed, this, &JobOrchestrator::onTorrentFailed);
}

QString JobOrchestrator::pickMagnet(const QStringList& uris) const
{
    for (const QString& uri : uris) {
        if (uri.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive))
            return uri;
    }
    return uris.value(0);
}

QString JobOrchestrator::createJob(const QString& title, JobKind kind, const QString& entryId,
                                   const QString& sourceId, const QString& magnet,
                                   const QString& saveSubdir)
{
    const QString jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString savePath = m_settings->resolvedDownloadsRoot() + QLatin1Char('/') + saveSubdir;

    JobEntry job;
    job.id = jobId;
    job.title = title;
    job.kind = kind;
    job.status = QStringLiteral("queued");
    job.progress = 0;
    job.detail = QStringLiteral("Подготовка…");
    job.entryId = entryId;
    job.sourceId = sourceId;
    m_jobs->addJob(job);
    m_jobKinds.insert(jobId, kind);

    m_torrent->addMagnet(jobId, magnet, savePath);
    return jobId;
}

QString JobOrchestrator::startCatalogDownload(const CatalogEntry& entry, JobKind kind)
{
    const QString magnet = pickMagnet(entry.magnetUris);
    if (magnet.isEmpty())
        return {};

    const QString prefix = kind == JobKind::Update ? QStringLiteral("update") : QStringLiteral("install");
    const QString saveSubdir = QStringLiteral("%1/%2").arg(prefix, entry.id);
    const QString title =
        kind == JobKind::Update
            ? QStringLiteral("Обновление %1").arg(entry.title)
            : QStringLiteral("Загрузка %1").arg(entry.title);

    return createJob(title, kind, entry.id, entry.sourceId, magnet, saveSubdir);
}

QString JobOrchestrator::startAddonDownload(const CatalogEntry& parent,
                                            const CatalogComponent& addon)
{
    const QString magnet = pickMagnet(addon.magnetUris);
    if (magnet.isEmpty())
        return {};

    const QString saveSubdir = QStringLiteral("addons/%1/%2").arg(parent.id, addon.id);
    const QString title = QStringLiteral("Дополнение %1 — %2").arg(parent.title, addon.title);
    return createJob(title, JobKind::Download, addon.id, parent.sourceId, magnet, saveSubdir);
}

void JobOrchestrator::cancelJob(const QString& jobId)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    const int progress = m_jobs->data(m_jobs->index(row, 0), JobModel::ProgressRole).toInt();
    m_torrent->cancel(jobId);

    JobEntry job;
    job.id = jobId;
    job.title = m_jobs->data(m_jobs->index(row, 0), JobModel::TitleRole).toString();
    job.kind = static_cast<JobKind>(m_jobs->data(m_jobs->index(row, 0), JobModel::KindRole).toInt());
    job.status = QStringLiteral("cancelled");
    job.progress = progress;
    job.detail = QStringLiteral("Отменено");
    job.entryId = m_jobs->data(m_jobs->index(row, 0), JobModel::EntryIdRole).toString();
    job.sourceId = m_jobs->data(m_jobs->index(row, 0), JobModel::SourceIdRole).toString();
    m_jobs->updateJob(job);
    m_jobKinds.remove(jobId);
}

QString JobOrchestrator::formatBytes(qint64 bytes) const
{
    static const QStringList units{QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"),
                                   QStringLiteral("GB"), QStringLiteral("TB")};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', unit == 0 ? 0 : 1), units.at(unit));
}

QString JobOrchestrator::formatSpeed(int bytesPerSec) const
{
    if (bytesPerSec <= 0)
        return QStringLiteral("—");
    return QStringLiteral("%1/s").arg(formatBytes(bytesPerSec));
}

QString JobOrchestrator::formatEta(qint64 remainingBytes, int bytesPerSec) const
{
    if (bytesPerSec <= 0 || remainingBytes <= 0)
        return QStringLiteral("—");
    const qint64 seconds = remainingBytes / bytesPerSec;
    if (seconds < 60)
        return QStringLiteral("%1 с").arg(seconds);
    if (seconds < 3600)
        return QStringLiteral("%1 мин").arg(seconds / 60);
    return QStringLiteral("%1 ч").arg(seconds / 3600);
}

QString JobOrchestrator::buildDetail(qint64 downloaded, qint64 total, int downloadRate,
                                     int numPeers, const QString& state) const
{
    const qint64 remaining = qMax<qint64>(0, total - downloaded);
    return QStringLiteral("%1 · %2 · %3 peers · %4")
        .arg(formatBytes(downloaded), total > 0 ? formatBytes(total) : QStringLiteral("?"),
             QString::number(numPeers), state);
}

void JobOrchestrator::onTorrentProgress(const QString& jobId, int progress, qint64 downloaded,
                                        qint64 total, int downloadRate, int numPeers,
                                        const QString& state)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job;
    job.id = jobId;
    job.title = m_jobs->data(m_jobs->index(row, 0), JobModel::TitleRole).toString();
    job.kind = static_cast<JobKind>(m_jobs->data(m_jobs->index(row, 0), JobModel::KindRole).toInt());
    job.status = state;
    job.progress = progress;
    job.detail = buildDetail(downloaded, total, downloadRate, numPeers, state);
    job.bytesDownloaded = downloaded;
    job.totalBytes = total;
    job.entryId = m_jobs->data(m_jobs->index(row, 0), JobModel::EntryIdRole).toString();
    job.sourceId = m_jobs->data(m_jobs->index(row, 0), JobModel::SourceIdRole).toString();
    m_jobs->updateJob(job);
}

void JobOrchestrator::onTorrentFinished(const QString& jobId, const QString& savePath)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    const QString entryId = m_jobs->data(m_jobs->index(row, 0), JobModel::EntryIdRole).toString();
    const QString sourceId = m_jobs->data(m_jobs->index(row, 0), JobModel::SourceIdRole).toString();
    const JobKind kind = m_jobKinds.value(jobId, JobKind::Download);

    JobEntry job;
    job.id = jobId;
    job.title = m_jobs->data(m_jobs->index(row, 0), JobModel::TitleRole).toString();
    job.kind = kind;
    job.status = QStringLiteral("completed");
    job.progress = 100;
    job.detail = QStringLiteral("Загрузка завершена");
    job.entryId = entryId;
    job.sourceId = sourceId;
    m_jobs->updateJob(job);

    emit downloadCompleted(jobId, entryId, sourceId, savePath, kind);
    m_jobKinds.remove(jobId);
}

void JobOrchestrator::onTorrentFailed(const QString& jobId, const QString& error)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row >= 0) {
        JobEntry job;
        job.id = jobId;
        job.title = m_jobs->data(m_jobs->index(row, 0), JobModel::TitleRole).toString();
        job.kind = m_jobKinds.value(jobId, JobKind::Download);
        job.status = QStringLiteral("failed");
        job.progress = m_jobs->data(m_jobs->index(row, 0), JobModel::ProgressRole).toInt();
        job.detail = error;
        job.entryId = m_jobs->data(m_jobs->index(row, 0), JobModel::EntryIdRole).toString();
        job.sourceId = m_jobs->data(m_jobs->index(row, 0), JobModel::SourceIdRole).toString();
        m_jobs->updateJob(job);
    }

    emit downloadFailed(jobId, error);
    m_jobKinds.remove(jobId);
}

} // namespace arachnel::core
