#include "job_model.h"

#include "job_kind.h"
#include "job_status.h"

namespace arachnel::core {

namespace {

QVariantMap jobToMap(const JobEntry& job)
{
    return {
        {QStringLiteral("jobId"), job.id},
        {QStringLiteral("title"), job.title},
        {QStringLiteral("status"), job.status},
        {QStringLiteral("statusLabel"), jobStatusLabel(job.status)},
        {QStringLiteral("progress"), job.progress},
        {QStringLiteral("kind"), static_cast<int>(job.kind)},
        {QStringLiteral("kindLabel"), jobKindLabel(job.kind)},
        {QStringLiteral("detail"), job.detail},
        {QStringLiteral("bytesDownloaded"), job.bytesDownloaded},
        {QStringLiteral("totalBytes"), job.totalBytes},
        {QStringLiteral("entryId"), job.entryId},
        {QStringLiteral("sourceId"), job.sourceId},
        {QStringLiteral("coverUrl"), job.coverUrl},
        {QStringLiteral("libraryId"), job.libraryId},
        {QStringLiteral("savePath"), job.savePath},
        {QStringLiteral("createdAt"), job.createdAt},
        {QStringLiteral("completedAt"), job.completedAt},
        {QStringLiteral("inProgress"), isJobInProgress(job.status)},
        {QStringLiteral("paused"), isJobPaused(job.status)},
    };
}

} // namespace

JobModel::JobModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int JobModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_jobs.size();
}

QVariant JobModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_jobs.size())
        return {};

    const auto& job = m_jobs.at(index.row());
    switch (role) {
    case JobIdRole:
        return job.id;
    case TitleRole:
        return job.title;
    case StatusRole:
        return job.status;
    case ProgressRole:
        return job.progress;
    case KindRole:
        return static_cast<int>(job.kind);
    case KindLabelRole:
        return jobKindLabel(job.kind);
    case DetailRole:
        return job.detail;
    case BytesDownloadedRole:
        return job.bytesDownloaded;
    case TotalBytesRole:
        return job.totalBytes;
    case EntryIdRole:
        return job.entryId;
    case SourceIdRole:
        return job.sourceId;
    case StatusLabelRole:
        return jobStatusLabel(job.status);
    case MagnetUriRole:
        return job.magnetUri;
    case SavePathRole:
        return job.savePath;
    case CoverUrlRole:
        return job.coverUrl;
    case LibraryIdRole:
        return job.libraryId;
    case CreatedAtRole:
        return job.createdAt;
    case CompletedAtRole:
        return job.completedAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> JobModel::roleNames() const
{
    return {
        {JobIdRole, "jobId"},
        {TitleRole, "title"},
        {StatusRole, "status"},
        {ProgressRole, "progress"},
        {KindRole, "kind"},
        {KindLabelRole, "kindLabel"},
        {DetailRole, "detail"},
        {BytesDownloadedRole, "bytesDownloaded"},
        {TotalBytesRole, "totalBytes"},
        {EntryIdRole, "entryId"},
        {SourceIdRole, "sourceId"},
        {StatusLabelRole, "statusLabel"},
        {MagnetUriRole, "magnetUri"},
        {SavePathRole, "savePath"},
        {CoverUrlRole, "coverUrl"},
        {LibraryIdRole, "libraryId"},
        {CreatedAtRole, "createdAt"},
        {CompletedAtRole, "completedAt"},
    };
}

int JobModel::activeCount() const
{
    int count = 0;
    for (const auto& job : m_jobs) {
        if (isJobRunning(job.status))
            ++count;
    }
    return count;
}

QVariantMap JobModel::jobForEntry(const QString& entryId) const
{
    if (entryId.isEmpty())
        return {};

    const JobEntry* activeMatch = nullptr;
    const JobEntry* anyMatch = nullptr;
    for (const auto& job : m_jobs) {
        if (job.entryId != entryId)
            continue;
        anyMatch = &job;
        if (isJobInProgress(job.status)) {
            activeMatch = &job;
            break;
        }
    }

    if (activeMatch)
        return jobToMap(*activeMatch);
    if (anyMatch)
        return jobToMap(*anyMatch);
    return {};
}

QVariantMap JobModel::primaryActiveJob() const
{
    for (const auto& job : m_jobs) {
        if (isJobInProgress(job.status))
            return jobToMap(job);
    }
    return {};
}

void JobModel::setJobs(QVector<JobEntry> jobs)
{
    beginResetModel();
    m_jobs = std::move(jobs);
    endResetModel();
    emit countChanged();
    emit jobsChanged();
}

void JobModel::addJob(JobEntry job)
{
    const int row = m_jobs.size();
    beginInsertRows({}, row, row);
    m_jobs.append(std::move(job));
    endInsertRows();
    emit countChanged();
    emit jobsChanged();
}

void JobModel::updateJob(const JobEntry& job)
{
    const int row = indexOfJob(job.id);
    if (row < 0)
        return;

    const int prevActive = activeCount();
    m_jobs[row] = job;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
    if (activeCount() != prevActive)
        emit countChanged();
    emit jobsChanged();
}

void JobModel::updateJob(const QString& jobId, const QString& status, int progress)
{
    const int row = indexOfJob(jobId);
    if (row < 0)
        return;
    m_jobs[row].status = status;
    m_jobs[row].progress = progress;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {StatusRole, ProgressRole});
    emit jobsChanged();
}

void JobModel::removeJob(const QString& jobId)
{
    const int row = indexOfJob(jobId);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_jobs.removeAt(row);
    endRemoveRows();
    emit countChanged();
    emit jobsChanged();
}

int JobModel::indexOfJob(const QString& jobId) const
{
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs.at(i).id == jobId)
            return i;
    }
    return -1;
}

} // namespace arachnel::core
