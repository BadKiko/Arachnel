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
    const qint64 estimatedTotal = parseSizeLabelBytes(entry.sizeLabel);
    if (estimatedTotal > 0) {
        m_pluginEstimatedTotal.insert(jobId, estimatedTotal);
        job.totalBytes = estimatedTotal;
    }
    m_jobKinds.insert(jobId, kind);
    m_jobs->addJob(job);
    m_jobStore->upsertJob(job);
    return jobId;
}


} // namespace arachnel::core
