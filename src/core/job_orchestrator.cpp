#include "job_orchestrator.h"

#include "http_download_session.h"
#include "i18n.h"
#include "job_status.h"
#include "torrent_session.h"

#include <QDateTime>
#include <QFileInfo>
#include <QUuid>

namespace arachnel::core {

namespace {

QString isoNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

} // namespace

JobOrchestrator::JobOrchestrator(SettingsStore* settings, JobStore* jobStore,
                                 TorrentSession* torrent, HttpDownloadSession* http, JobModel* jobs,
                                 QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_jobStore(jobStore)
    , m_torrent(torrent)
    , m_http(http)
    , m_jobs(jobs)
{
    connect(m_torrent, &TorrentSession::torrentProgress, this, &JobOrchestrator::onTorrentProgress);
    connect(m_torrent, &TorrentSession::torrentFinished, this, &JobOrchestrator::onTorrentFinished);
    connect(m_torrent, &TorrentSession::torrentFailed, this, &JobOrchestrator::onTorrentFailed);

    connect(m_http, &HttpDownloadSession::httpProgress, this, &JobOrchestrator::onHttpProgress);
    connect(m_http, &HttpDownloadSession::httpFinished, this, &JobOrchestrator::onHttpFinished);
    connect(m_http, &HttpDownloadSession::httpFailed, this, &JobOrchestrator::onHttpFailed);

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
        if (job.httpDownload)
            startHttp(job);
        else
            startTorrent(job);
        if (!job.httpDownload && wasPaused)
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

QString JobOrchestrator::findActiveJobId(const QString& entryId,
                                         const QString& parentEntryId) const
{
    for (int i = 0; i < m_jobs->rowCount(); ++i) {
        const QString jobEntryId =
            m_jobs->data(m_jobs->index(i, 0), JobModel::EntryIdRole).toString();
        const QString jobParentId =
            m_jobs->data(m_jobs->index(i, 0), JobModel::ParentEntryIdRole).toString();
        const QString status =
            m_jobs->data(m_jobs->index(i, 0), JobModel::StatusRole).toString();
        if (jobEntryId != entryId || jobParentId != parentEntryId)
            continue;
        if (isJobInProgress(status))
            return m_jobs->data(m_jobs->index(i, 0), JobModel::JobIdRole).toString();
    }
    return {};
}

QString JobOrchestrator::findExistingJobId(const QString& entryId,
                                            const QString& parentEntryId) const
{
    QString latestId;
    QString latestCreated;
    for (int i = 0; i < m_jobs->rowCount(); ++i) {
        const QString jobEntryId =
            m_jobs->data(m_jobs->index(i, 0), JobModel::EntryIdRole).toString();
        const QString jobParentId =
            m_jobs->data(m_jobs->index(i, 0), JobModel::ParentEntryIdRole).toString();
        if (jobEntryId != entryId || jobParentId != parentEntryId)
            continue;

        const QString status =
            m_jobs->data(m_jobs->index(i, 0), JobModel::StatusRole).toString();
        if (isJobInProgress(status))
            return m_jobs->data(m_jobs->index(i, 0), JobModel::JobIdRole).toString();

        const QString created =
            m_jobs->data(m_jobs->index(i, 0), JobModel::CreatedAtRole).toString();
        if (latestId.isEmpty() || created > latestCreated) {
            latestId = m_jobs->data(m_jobs->index(i, 0), JobModel::JobIdRole).toString();
            latestCreated = created;
        }
    }
    return latestId;
}

QString JobOrchestrator::createJob(const QString& title, JobKind kind, const QString& entryId,
                                     const QString& sourceId, const QString& downloadUri,
                                     const QString& saveSubdir, const QString& coverUrl,
                                     const QString& libraryId, const QString& parentEntryId,
                                     bool httpDownload, const QString& referer)
{
    const QString jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString downloadsRoot = m_settings->resolvedDownloadsRoot(libraryId);
    const QString savePath = downloadsRoot + QLatin1Char('/') + saveSubdir;

    JobEntry job;
    job.id = jobId;
    job.title = title;
    job.kind = kind;
    job.status = QStringLiteral("starting");
    job.progress = 0;
    job.detail = httpDownload ? QStringLiteral("Downloading…") : QStringLiteral("Connecting…");
    job.entryId = entryId;
    job.sourceId = sourceId;
    job.magnetUri = downloadUri;
    job.savePath = savePath;
    job.coverUrl = coverUrl;
    job.libraryId = libraryId.isEmpty() ? m_settings->defaultLibraryId() : libraryId;
    job.parentEntryId = parentEntryId;
    job.referer = referer;
    job.httpDownload = httpDownload;
    job.createdAt = isoNow();
    m_jobKinds.insert(jobId, kind);

    m_jobs->addJob(job);
    m_jobStore->upsertJob(job);
    if (httpDownload)
        startHttp(job);
    else
        startTorrent(job);
    return jobId;
}

void JobOrchestrator::startTorrent(const JobEntry& job)
{
    if (!m_torrent->addJob(job.id, job.magnetUri, job.savePath)) {
        JobEntry failed = job;
        failed.status = QStringLiteral("failed");
        failed.detail = QStringLiteral("Failed to start torrent");
        failed.completedAt = isoNow();
        updateJobInModel(failed);
        persistJob(failed);
        emit downloadFailed(job.id, failed.detail);
        m_jobKinds.remove(job.id);
    }
}

void JobOrchestrator::startHttp(const JobEntry& job)
{
    if (!m_http->addJob(job.id, job.magnetUri, job.referer, job.savePath)) {
        JobEntry failed = job;
        failed.status = QStringLiteral("failed");
        failed.detail = QStringLiteral("Failed to start HTTP download");
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
    job.libraryId = m_jobs->data(idx, JobModel::LibraryIdRole).toString();
    job.parentEntryId = m_jobs->data(idx, JobModel::ParentEntryIdRole).toString();
    job.referer = m_jobs->data(idx, JobModel::RefererRole).toString();
    job.httpDownload = m_jobs->data(idx, JobModel::HttpDownloadRole).toBool();
    job.pluginDownload = m_jobs->data(idx, JobModel::PluginDownloadRole).toBool();
    job.artifactPath = m_jobs->data(idx, JobModel::ArtifactPathRole).toString();
    job.createdAt = m_jobs->data(idx, JobModel::CreatedAtRole).toString();
    job.completedAt = m_jobs->data(idx, JobModel::CompletedAtRole).toString();
    return job;
}

void JobOrchestrator::updateJobInModel(const JobEntry& job)
{
    m_jobs->updateJob(job);
}

QString JobOrchestrator::startCatalogDownload(const CatalogEntry& entry, JobKind kind,
                                              const QString& libraryId)
{
    const QString magnet = pickMagnet(entry.magnetUris);
    if (magnet.isEmpty())
        return {};

    const QString existing = findActiveJobId(entry.id, {});
    if (!existing.isEmpty())
        return existing;

    const QString prefix =
        kind == JobKind::Update ? QStringLiteral("update") : QStringLiteral("install");
    const QString saveSubdir = QStringLiteral("%1/%2").arg(prefix, entry.id);
    const QString title = kind == JobKind::Update
                              ? QStringLiteral("Updating %1").arg(entry.title)
                              : QStringLiteral("Downloading %1").arg(entry.title);

    const QString libId = libraryId.isEmpty() ? m_settings->defaultLibraryId() : libraryId;
    return createJob(title, kind, entry.id, entry.sourceId, magnet, saveSubdir, entry.coverUrl,
                     libId);
}

QString JobOrchestrator::startPluginOwnedDownload(const CatalogEntry& entry, JobKind kind,
                                                    const QString& libraryId)
{
    const QString existing = findActiveJobId(entry.id, {});
    if (!existing.isEmpty())
        return existing;

    const QString prefix =
        kind == JobKind::Update ? QStringLiteral("update") : QStringLiteral("install");
    const QString saveSubdir = QStringLiteral("%1/%2").arg(prefix, entry.id);
    const QString title = kind == JobKind::Update
                              ? QStringLiteral("Updating %1").arg(entry.title)
                              : QStringLiteral("Downloading %1").arg(entry.title);
    const QString libId = libraryId.isEmpty() ? m_settings->defaultLibraryId() : libraryId;
    const QString downloadsRoot = m_settings->resolvedDownloadsRoot(libId);
    const QString savePath = downloadsRoot + QLatin1Char('/') + saveSubdir;

    const QString jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    JobEntry job;
    job.id = jobId;
    job.title = title;
    job.kind = kind;
    job.status = QStringLiteral("starting");
    job.progress = 0;
    job.detail = QStringLiteral("Preparing…");
    job.entryId = entry.id;
    job.sourceId = entry.sourceId;
    job.magnetUri = entry.steamAppId.isEmpty()
                        ? QString()
                        : QStringLiteral("steam://app/%1").arg(entry.steamAppId);
    job.savePath = savePath;
    job.coverUrl = entry.coverUrl;
    job.libraryId = libId;
    job.pluginDownload = true;
    job.createdAt = isoNow();
    m_jobKinds.insert(jobId, kind);
    m_jobs->addJob(job);
    m_jobStore->upsertJob(job);
    return jobId;
}

void JobOrchestrator::reportPluginProgress(const QString& jobId,
                                           const OwnedDownloadProgress& progress)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;
    JobEntry job = jobFromModelRow(row);
    if (isJobTerminal(job.status))
        return;
    if (!progress.status.isEmpty())
        job.status = progress.status;
    else if (job.status == QStringLiteral("starting"))
        job.status = QStringLiteral("downloading");
    job.progress = qBound(0, progress.percent, 100);
    job.bytesDownloaded = progress.bytesDownloaded;
    // Ignore absurd totals from bad percent-based estimates (e.g. 70 GB for a 3 GB game).
    qint64 total = progress.totalBytes;
    if (total > 0 && progress.bytesDownloaded > 0 && total > progress.bytesDownloaded * 8)
        total = 0;
    job.totalBytes = total;

    int rate = progress.downloadRateBps;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (progress.bytesDownloaded > 0) {
        const auto it = m_pluginSpeed.constFind(jobId);
        if (it != m_pluginSpeed.cend() && nowMs > it->ms) {
            const qint64 dBytes = progress.bytesDownloaded - it->bytes;
            const qint64 dt = nowMs - it->ms;
            if (rate <= 0 && dBytes > 0 && dBytes < 80LL * 1024 * 1024 && dt >= 200)
                rate = static_cast<int>(dBytes * 1000 / dt);
            if (rate <= 0 && dBytes == 0 && it->rate > 0 && dt < 5000)
                rate = it->rate;
        }
        SpeedSample sample;
        sample.bytes = progress.bytesDownloaded;
        sample.ms = nowMs;
        sample.rate = rate > 0 ? rate : m_pluginSpeed.value(jobId).rate;
        m_pluginSpeed.insert(jobId, sample);
    }

    if (progress.bytesDownloaded > 0 || total > 0)
        job.detail = buildTransferDetail(progress.bytesDownloaded, total, rate);
    else if (!progress.detail.isEmpty())
        job.detail = progress.detail;

    updateJobInModel(job);
    persistJob(job);
}

void JobOrchestrator::completePluginDownload(const QString& jobId, const QString& installPath)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;
    JobEntry job = jobFromModelRow(row);
    job.status = QStringLiteral("completed");
    job.progress = 100;
    job.detail = QStringLiteral("Installed");
    job.artifactPath = installPath;
    job.completedAt = isoNow();
    updateJobInModel(job);
    persistJob(job);
    const JobKind kind = m_jobKinds.value(jobId, job.kind);
    m_jobKinds.remove(jobId);
    m_pluginSpeed.remove(jobId);
    emit downloadCompleted(jobId, job.entryId, job.sourceId, installPath, kind, job.libraryId);
}

void JobOrchestrator::failPluginDownload(const QString& jobId, const QString& error)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;
    JobEntry job = jobFromModelRow(row);
    job.status = QStringLiteral("failed");
    job.detail = error.isEmpty() ? QStringLiteral("Failed") : error;
    job.completedAt = isoNow();
    updateJobInModel(job);
    persistJob(job);
    m_jobKinds.remove(jobId);
    m_pluginSpeed.remove(jobId);
    emit downloadFailed(jobId, job.detail);
}

QString JobOrchestrator::startAddonDownload(const CatalogEntry& parent,
                                            const CatalogComponent& addon)
{
    const QString existing = findExistingJobId(addon.id, parent.id);
    if (!existing.isEmpty()) {
        const int row = m_jobs->indexOfJob(existing);
        if (row >= 0) {
            const QString status = m_jobs->data(m_jobs->index(row, 0), JobModel::StatusRole).toString();
            if (status != QStringLiteral("failed") && status != QStringLiteral("cancelled"))
                return existing;
        }
    }

    const QString saveSubdir = QStringLiteral("addons/%1/%2").arg(parent.id, addon.id);
    const QString title = QStringLiteral("Add-on %1 — %2").arg(parent.title, addon.title);
    const QString libId = m_settings->defaultLibraryId();

    if (addon.delivery == ComponentDelivery::Direct) {
        QString url = addon.downloadUrl;
        if (url.isEmpty())
            url = addon.magnetUris.value(0);
        if (url.isEmpty() || !url.startsWith(QStringLiteral("http"), Qt::CaseInsensitive))
            return {};

        QString referer = addon.referer;
        if (referer.isEmpty() && !addon.getfileUrl.isEmpty())
            referer = addon.getfileUrl;

        return createJob(title, JobKind::Download, addon.id, parent.sourceId, url, saveSubdir,
                         parent.coverUrl, libId, parent.id, true, referer);
    }

    const QString magnet = pickMagnet(addon.magnetUris);
    if (magnet.isEmpty())
        return {};

    return createJob(title, JobKind::Download, addon.id, parent.sourceId, magnet, saveSubdir,
                     parent.coverUrl, libId, parent.id);
}

void JobOrchestrator::cancelJob(const QString& jobId)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    if (isJobTerminal(job.status))
        return;

    if (job.pluginDownload) {
        // CoreController/PluginHost cancel the plugin work; mark job here.
    } else if (job.httpDownload) {
        if (isJobRunning(job.status) || job.status == QStringLiteral("starting"))
            m_http->cancel(jobId);
    } else if (isJobRunning(job.status) || job.status == QStringLiteral("starting")) {
        m_torrent->cancel(jobId, true);
    } else {
        m_torrent->removeResumeFile(jobId);
    }

    job.status = QStringLiteral("cancelled");
    job.detail = QStringLiteral("Cancelled");
    job.completedAt = isoNow();
    updateJobInModel(job);
    persistJob(job);
    m_jobKinds.remove(jobId);
    m_pluginSpeed.remove(jobId);
}

void JobOrchestrator::toggleJobPause(const QString& jobId)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    if (isJobTerminal(job.status) || job.httpDownload || job.pluginDownload)
        return;

    if (job.status == QStringLiteral("paused")) {
        m_torrent->setPaused(jobId, false);
        job.status = QStringLiteral("starting");
        job.detail = QStringLiteral("Resuming…");
    } else if (isJobRunning(job.status)) {
        m_torrent->setPaused(jobId, true);
        job.status = QStringLiteral("paused");
        job.detail = QStringLiteral("Paused");
    } else {
        return;
    }

    updateJobInModel(job);
    persistJob(job);
}

void JobOrchestrator::removeJob(const QString& jobId)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    if (!isJobTerminal(job.status)) {
        if (job.pluginDownload) {
            // cancelled via cancelJob path when user cancels first
        } else if (job.httpDownload) {
            if (isJobRunning(job.status) || job.status == QStringLiteral("starting"))
                m_http->cancel(jobId);
        } else if (isJobRunning(job.status) || job.status == QStringLiteral("starting")) {
            m_torrent->cancel(jobId, false);
        } else {
            m_torrent->removeResumeFile(jobId);
        }
    } else if (!job.httpDownload && !job.pluginDownload) {
        m_torrent->removeResumeFile(jobId);
    }

    m_jobs->removeJob(jobId);
    m_jobStore->removeJob(jobId);
    m_jobKinds.remove(jobId);
}

void JobOrchestrator::retryJob(const QString& jobId)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    if (job.status != QStringLiteral("failed") && job.status != QStringLiteral("cancelled"))
        return;
    if (job.pluginDownload)
        return; // Plugin-owned retry is started again from UI install
    if (job.magnetUri.isEmpty())
        return;

    if (!job.httpDownload)
        m_torrent->removeResumeFile(jobId);

    job.status = QStringLiteral("starting");
    job.progress = 0;
    job.detail = job.httpDownload ? QStringLiteral("Downloading…") : QStringLiteral("Connecting…");
    job.bytesDownloaded = 0;
    job.totalBytes = 0;
    job.artifactPath.clear();
    job.completedAt.clear();
    m_jobKinds.insert(jobId, job.kind);
    updateJobInModel(job);
    persistJob(job);
    if (job.httpDownload)
        startHttp(job);
    else
        startTorrent(job);
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

void JobOrchestrator::setJobPhase(const QString& jobId, const QString& status,
                                  const QString& detail)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    job.status = status;
    job.detail = detail;
    updateJobInModel(job);
    persistJob(job);
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
        return QStringLiteral("%1 s").arg(seconds);
    if (seconds < 3600)
        return QStringLiteral("%1 min").arg(seconds / 60);
    return QStringLiteral("%1 h").arg(seconds / 3600);
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

QString JobOrchestrator::buildHttpDetail(qint64 downloaded, qint64 total) const
{
    return buildTransferDetail(downloaded, total, 0);
}

QString JobOrchestrator::buildTransferDetail(qint64 downloaded, qint64 total, int downloadRate) const
{
    const qint64 remaining = total > 0 ? qMax<qint64>(0, total - downloaded) : 0;
    if (total > 0) {
        return QStringLiteral("%1 / %2 · %3 · ETA %4")
            .arg(formatBytes(downloaded), formatBytes(total), formatSpeed(downloadRate),
                 formatEta(remaining, downloadRate));
    }
    if (downloaded > 0) {
        if (downloadRate > 0)
            return QStringLiteral("%1 · %2").arg(formatBytes(downloaded), formatSpeed(downloadRate));
        return formatBytes(downloaded);
    }
    return QStringLiteral("Downloading…");
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
    job.detail = QStringLiteral("Download complete");
    job.artifactPath = savePath;
    job.completedAt = isoNow();
    updateJobInModel(job);
    persistJob(job);

    emit downloadCompleted(jobId, job.entryId, job.sourceId, savePath, kind, job.libraryId);
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

void JobOrchestrator::onHttpProgress(const QString& jobId, int progress, qint64 downloaded,
                                     qint64 total)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    job.progress = progress;
    job.bytesDownloaded = downloaded;
    job.totalBytes = total;
    job.status = QStringLiteral("downloading");

    int rate = 0;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const auto it = m_pluginSpeed.constFind(jobId);
    if (it != m_pluginSpeed.cend() && nowMs > it->ms && downloaded >= it->bytes) {
        const qint64 dt = nowMs - it->ms;
        if (dt >= 200)
            rate = static_cast<int>((downloaded - it->bytes) * 1000 / dt);
    }
    m_pluginSpeed.insert(jobId, {downloaded, nowMs});

    job.detail = buildTransferDetail(downloaded, total, rate);
    updateJobInModel(job);
    persistJob(job);
}

void JobOrchestrator::onHttpFinished(const QString& jobId, const QString& filePath)
{
    const int row = m_jobs->indexOfJob(jobId);
    if (row < 0)
        return;

    JobEntry job = jobFromModelRow(row);
    const JobKind kind = m_jobKinds.value(jobId, JobKind::Download);

    job.status = QStringLiteral("completed");
    job.progress = 100;
    job.detail = QStringLiteral("Download complete");
    job.artifactPath = filePath;
    job.completedAt = isoNow();
    updateJobInModel(job);
    persistJob(job);

    emit downloadCompleted(jobId, job.entryId, job.sourceId, filePath, kind, job.libraryId);
    m_jobKinds.remove(jobId);
}

void JobOrchestrator::onHttpFailed(const QString& jobId, const QString& error)
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
