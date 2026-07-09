#include "job_model.h"

#include "job_kind.h"

namespace arachnel::core {

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
    };
}

void JobModel::setJobs(QVector<JobEntry> jobs)
{
    beginResetModel();
    m_jobs = std::move(jobs);
    endResetModel();
}

void JobModel::addJob(JobEntry job)
{
    const int row = m_jobs.size();
    beginInsertRows({}, row, row);
    m_jobs.append(std::move(job));
    endInsertRows();
}

void JobModel::updateJob(const JobEntry& job)
{
    const int row = indexOfJob(job.id);
    if (row < 0)
        return;
    m_jobs[row] = job;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
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
}

void JobModel::removeJob(const QString& jobId)
{
    const int row = indexOfJob(jobId);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_jobs.removeAt(row);
    endRemoveRows();
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
