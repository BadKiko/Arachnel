#include "job_orchestrator.h"

#include "job_status.h"
#include "torrent_session.h"

#include <QDateTime>
#include <QUuid>

namespace arachnel::core {

namespace {

QString isoNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

} // namespace

JobOrchestrator::JobOrchestrator(SettingsStore* settings, JobStore* jobStore,
                                 TorrentSession* torrent, JobModel* jobs, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_jobStore(jobStore)
    , m_torrent(torrent)
    , m_jobs(jobs)
{
    connect(m_torrent, &TorrentSession::torrentProgress, this, &JobOrchestrator::onTorrentProgress);
    connect(m_torrent, &TorrentSession::torrentFinished, this, &JobOrchestrator::onTorrentFinished);
    connect(m_torrent, &TorrentSession::torrentFailed, this, &JobOrchestrator::onTorrentFailed);

    m_persistTimer.setInterval(3000);
    m_persistTimer.setSingleShot(true);
    connect(&m_persistTimer, &QTimer::timeout, this, [this]() {
        if (!m_dirty)
            return;
        m_jobStore->save();
        m_dirty = false;
    });
}

void JobOrchestrator::restoreJobs()
{
    QVector<JobEntry> jobs = m_jobStore->jobs();
    for (auto& job : jobs) {
        if (isJobTerminal(job.status))
            continue;
        if (isJobQueued(job.status) || isJobActive(job.status))
            job.status = QStringLiteral("starting");
        m_jobKinds.insert(job.id, job.kind);
    }

    m_jobStore->setJobs(jobs);
    m_jobs->setJobs(jobs);

    for (const auto& job : jobs) {
        if (isJobTerminal(job.status))
            continue;
        const bool wasPaused = isJobPaused(job.status);
        startTorrent(job);
        if (wasPaused)
            m_torrent->setPaused(job.id, true);
    }
}

void JobOrchestrator::flushPersistence()
{
    m_persistTimer.stop();
    m_dirty = false;

    QVector<JobEntry> jobs;
    jobs.reserve(m_jobs->rowCount());
    for (int i = 0; i < m_jobs->rowCount(); ++i)
        jobs.append(jobFromModelRow(i));
    m_jobStore->setJobs(jobs);
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
                                   const QString& saveSubdir, const QString& coverUrl)
{
    const QString jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString savePath = m_settings->resolvedDownloadsRoot() + QLatin1Char('/') + saveSubdir;

    JobEntry job;
    job.id = jobId;
    job.title = title;
    job.kind = kind;
    job.status = QStringLiteral("starting");
    job.progress = 0;
    job.detail = QStringLiteral("Подключение…");
    job.entryId = entryId;
    job.sourceId = sourceId;
    job.magnetUri = magnet;
    job.savePath = savePath;
    job.coverUrl = coverUrl;
    job.createdAt = isoNow();
    m_jobKinds.insert(jobId, kind);

    m_jobs->addJob(job);
    m_jobStore->upsertJob(job);
    startTorrent(job);
    return jobId;
}

void JobOrchestrator::startTorrent(const JobEntry& job)
{
    if (!m_torrent->addJob(job.id, job.magnetUri, job.savePath)) {
        JobEntry failed = job;
        failed.status = QStringLiteral("failed");
        failed.detail = QStringLiteral("Не удалось начать торрент");
        failed.completedAt = isoNow();
        updateJobInModel(failed);
        persistJob(failed);
        emit downloadFailed(job.id, failed.detail);
        m_jobKinds.remove(job.id);
    }
}

void JobOrchestrator::persistJob(const JobEntry& job)
{
    m_jobStore->upsertJob(job);
    m_dirty = true;
    if (!m_persistTimer.isActive())
        m_persistTimer.start();
}

JobEntry JobOrchestrator::jobFromModelRow(int row) const
{
    JobEntry job;
    const QModelIndex idx = m_jobs->index(row, 0);
    job.id = m_jobs->data(idx, JobModel::JobIdRole).toString();
    job.title = m_jobs->data(idx, JobModel::TitleRole).toString();
    job.kind = static_cast<JobKind>(m_jobs->data(idx, JobModel::KindRole).toInt());
    job.status = m_jobs->data(idx, JobModel::StatusRole).toString();
    job.progress = m_jobs->data(idx, JobModel::ProgressRole).toInt();
    job.detail = m_jobs->data(idx, JobModel::DetailRole).toString();
    job.bytesDownloaded = m_jobs->data(idx, JobModel::BytesDownloadedRole).toLongLong();
    job.totalBytes = m_jobs->data(idx, JobModel::TotalBytesRole).toLongLong();
    job.entryId = m_jobs->data(idx, JobModel::EntryIdRole).toString();
    job.sourceId = m_jobs->data(idx, JobModel::SourceIdRole).toString();
    job.magnetUri = m_jobs->data(idx, JobModel::MagnetUriRole).toString();
    job.savePath = m_jobs->data(idx, JobModel::SavePathRole).toString();
    job.coverUrl = m_jobs->data(idx, JobModel::CoverUrlRole).toString();
    job.createdAt = m_jobs->data(idx, JobModel::CreatedAtRole).toString();
    job.completedAt = m_jobs->data(idx, JobModel::CompletedAtRole).toString();
    return job;
}

void JobOrchestrator::updateJobInModel(const JobEntry& job)
{
    m_jobs->updateJob(job);
}

QString JobOrchestrator::startCatalogDownload(const CatalogEntry& entry, JobKind kind)
{
    const QString magnet = pickMagnet(entry.magnetUris);
    if (magnet.isEmpty())
        return {};

    for (int i = 0; i < m_jobs->rowCount(); ++i) {
        const QString entryId = m_jobs->data(m_jobs->index(i, 0), JobModel::EntryIdRole).toString();
        const QString status = m_jobs->data(m_jobs->index(i, 0), JobModel::StatusRole).toString();
        if (entryId == entry.id && isJobInProgress(status))
            return m_jobs->data(m_jobs->index(i, 0), JobModel::JobIdRole).toString();
    }

    const QString prefix =
        kind == JobKind::Update ? QStringLiteral("update") : QStringLiteral("install");
    const QString saveSubdir = QStringLiteral("%1/%2").arg(prefix, entry.id);
    const QString title = kind == JobKind::Update
                              ? QStringLiteral("Обновление %1").arg(entry.title)
                              : QStringLiteral("Загрузка %1").arg(entry.title);

    return createJob(title, kind, entry.id, entry.sourceId, magnet, saveSubdir, entry.coverUrl);
}

QString JobOrchestrator::startAddonDownload(const CatalogEntry& parent,
                                            const CatalogComponent& addon)
{
    const QString magnet = pickMagnet(addon.magnetUris);
    if (magnet.isEmpty())
        return {};

    const QString saveSubdir = QStringLiteral("addons/%1/%2").arg(parent.id, addon.id);
    const QString title = QStringLiteral("Дополнение %1 — %2").arg(parent.title, addon.title);
    return createJob(title, JobKind::Download, addon.id, parent.sourceId, magnet, saveSubdir,
                     parent.coverUrl);
}

void JobOrchestrator::cancelJob(const QString& jobId)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    if (isJobTerminal(job.status))
        return;

    if (isJobRunning(job.status) || job.status == QStringLiteral("starting"))
        m_torrent->cancel(jobId, true);
    else
        m_torrent->removeResumeFile(jobId);

    job.status = QStringLiteral("cancelled");
    job.detail = QStringLiteral("Отменено");
    job.completedAt = isoNow();
    updateJobInModel(job);
    persistJob(job);
    m_jobKinds.remove(jobId);
}

void JobOrchestrator::toggleJobPause(const QString& jobId)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    if (isJobTerminal(job.status))
        return;

    if (job.status == QStringLiteral("paused")) {
        m_torrent->setPaused(jobId, false);
        job.status = QStringLiteral("starting");
        job.detail = QStringLiteral("Возобновление…");
    } else if (isJobRunning(job.status)) {
        m_torrent->setPaused(jobId, true);
        job.status = QStringLiteral("paused");
        job.detail = QStringLiteral("Пауза");
    } else {
        return;
    }

    updateJobInModel(job);
    persistJob(job);
}

void JobOrchestrator::clearFinishedJobs()
{
    QVector<JobEntry> remaining;
    remaining.reserve(m_jobs->rowCount());
    for (int i = 0; i < m_jobs->rowCount(); ++i) {
        const JobEntry job = jobFromModelRow(i);
        if (isJobTerminal(job.status))
            m_jobKinds.remove(job.id);
        else
            remaining.append(job);
    }
    m_jobs->setJobs(remaining);
    m_jobStore->setJobs(remaining);
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
    const QString eta = formatEta(remaining, downloadRate);
    return QStringLiteral("%1 / %2 · %3 · %4 peers · ETA %5")
        .arg(formatBytes(downloaded), total > 0 ? formatBytes(total) : QStringLiteral("?"),
             formatSpeed(downloadRate), QString::number(numPeers), eta);
}

void JobOrchestrator::onTorrentProgress(const QString& jobId, int progress, qint64 downloaded,
                                        qint64 total, int downloadRate, int numPeers,
                                        const QString& state)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    job.progress = progress;
    job.bytesDownloaded = downloaded;
    job.totalBytes = total;

    if (job.status == QStringLiteral("paused")) {
        job.detail = buildDetail(downloaded, total, 0, numPeers, QStringLiteral("paused"));
    } else {
        job.status = state;
        job.detail = buildDetail(downloaded, total, downloadRate, numPeers, state);
    }

    updateJobInModel(job);
    persistJob(job);
}

void JobOrchestrator::onTorrentFinished(const QString& jobId, const QString& savePath)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    const JobKind kind = m_jobKinds.value(jobId, JobKind::Download);

    job.status = QStringLiteral("completed");
    job.progress = 100;
    job.detail = QStringLiteral("Загрузка завершена");
    job.completedAt = isoNow();
    updateJobInModel(job);
    persistJob(job);

    emit downloadCompleted(jobId, job.entryId, job.sourceId, savePath, kind);
    m_jobKinds.remove(jobId);
}

void JobOrchestrator::onTorrentFailed(const QString& jobId, const QString& error)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row >= 0) {
        JobEntry job = jobFromModelRow(row);
        job.status = QStringLiteral("failed");
        job.detail = error;
        job.completedAt = isoNow();
        updateJobInModel(job);
        persistJob(job);
    }

    emit downloadFailed(jobId, error);
    m_jobKinds.remove(jobId);
}

} // namespace arachnel::core
