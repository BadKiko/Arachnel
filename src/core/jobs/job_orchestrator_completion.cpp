#include "job_orchestrator.h"

#include "http_download_session.h"
#include "job_status.h"
#include "torrent_session.h"

#include <QDateTime>

namespace arachnel::core {
namespace {
QString isoNow() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODate); }
} // namespace

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
    m_pluginEstimatedTotal.remove(jobId);
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
    m_pluginEstimatedTotal.remove(jobId);
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
