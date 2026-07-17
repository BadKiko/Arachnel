#include "job_model.h"

#include "job_kind.h"
#include "job_display.h"
#include "job_status.h"

namespace arachnel::core {

namespace {

QVariantMap jobToMap(const JobEntry& job)
{
    return {
        {QStringLiteral("jobId"), job.id},
        {QStringLiteral("title"), displayJobTitle(job.title)},
        {QStringLiteral("status"), job.status},
        {QStringLiteral("statusLabel"), jobDisplayStatusLabel(job.status, job.detail)},
        {QStringLiteral("progress"), job.progress},
        {QStringLiteral("kind"), static_cast<int>(job.kind)},
        {QStringLiteral("kindLabel"), jobKindLabel(job.kind)},
        {QStringLiteral("detail"), displayJobDetail(job.detail)},
        {QStringLiteral("bytesDownloaded"), job.bytesDownloaded},
        {QStringLiteral("totalBytes"), job.totalBytes},
        {QStringLiteral("entryId"), job.entryId},
        {QStringLiteral("sourceId"), job.sourceId},
        {QStringLiteral("coverUrl"), job.coverUrl},
        {QStringLiteral("libraryId"), job.libraryId},
        {QStringLiteral("parentEntryId"), job.parentEntryId},
        {QStringLiteral("referer"), job.referer},
        {QStringLiteral("httpDownload"), job.httpDownload},
        {QStringLiteral("pluginDownload"), job.pluginDownload},
        {QStringLiteral("artifactPath"), job.artifactPath},
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
        return displayJobTitle(job.title);
    case StatusRole:
        return job.status;
    case ProgressRole:
        return job.progress;
    case KindRole:
        return static_cast<int>(job.kind);
    case KindLabelRole:
        return jobKindLabel(job.kind);
    case DetailRole:
        return displayJobDetail(job.detail);
    case BytesDownloadedRole:
        return job.bytesDownloaded;
    case TotalBytesRole:
        return job.totalBytes;
    case EntryIdRole:
        return job.entryId;
    case SourceIdRole:
        return job.sourceId;
    case StatusLabelRole:
        return jobDisplayStatusLabel(job.status, job.detail);
    case MagnetUriRole:
        return job.magnetUri;
    case SavePathRole:
        return job.savePath;
    case CoverUrlRole:
        return job.coverUrl;
    case LibraryIdRole:
        return job.libraryId;
    case ParentEntryIdRole:
        return job.parentEntryId;
    case RefererRole:
        return job.referer;
    case HttpDownloadRole:
        return job.httpDownload;
    case PluginDownloadRole:
        return job.pluginDownload;
    case ArtifactPathRole:
        return job.artifactPath;
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
        {ParentEntryIdRole, "parentEntryId"},
        {RefererRole, "referer"},
        {HttpDownloadRole, "httpDownload"},
        {PluginDownloadRole, "pluginDownload"},
        {ArtifactPathRole, "artifactPath"},
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
    const JobEntry* completedMatch = nullptr;
    for (const auto& job : m_jobs) {
        if (job.entryId != entryId)
            continue;
        if (isJobInProgress(job.status)) {
            activeMatch = &job;
            break;
        }
        if (job.status == QStringLiteral("completed"))
            completedMatch = &job;
    }

    if (activeMatch)
        return jobToMap(*activeMatch);
    if (completedMatch)
        return jobToMap(*completedMatch);
    return {};
}

QVariantMap JobModel::jobForAddon(const QString& parentEntryId, const QString& addonId) const
{
    if (parentEntryId.isEmpty() || addonId.isEmpty())
        return {};

    const JobEntry* activeMatch = nullptr;
    const JobEntry* completedMatch = nullptr;
    for (const auto& job : m_jobs) {
        if (job.entryId != addonId || job.parentEntryId != parentEntryId)
            continue;
        if (isJobInProgress(job.status)) {
            activeMatch = &job;
            break;
        }
        if (job.status == QStringLiteral("completed"))
            completedMatch = &job;
    }

    if (activeMatch)
        return jobToMap(*activeMatch);
    if (completedMatch)
        return jobToMap(*completedMatch);
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

void JobModel::refreshLocalizedText()
{
    if (m_jobs.isEmpty())
        return;
    emit dataChanged(index(0), index(m_jobs.size() - 1));
    emit jobsChanged();
}

QVariantList JobModel::downloadGroups() const
{
    QHash<QString, QHash<QString, JobEntry>> addonsByParent;
    for (const auto& job : m_jobs) {
        if (job.parentEntryId.isEmpty())
            continue;

        QHash<QString, JobEntry>& parentAddons = addonsByParent[job.parentEntryId];
        const auto it = parentAddons.constFind(job.entryId);
        if (it == parentAddons.constEnd() || job.createdAt >= it->createdAt)
            parentAddons.insert(job.entryId, job);
    }

    QVariantList groups;
    groups.reserve(m_jobs.size());
    for (const auto& job : m_jobs) {
        if (!job.parentEntryId.isEmpty())
            continue;

        QVariantMap group = jobToMap(job);
        const QHash<QString, JobEntry> parentAddons = addonsByParent.value(job.entryId);

        QVariantList addonMaps;
        addonMaps.reserve(parentAddons.size());
        for (const auto& addon : parentAddons) {
            if (addon.status == QStringLiteral("cancelled"))
                continue;
            addonMaps.append(jobToMap(addon));
        }

        group.insert(QStringLiteral("addons"), addonMaps);
        group.insert(QStringLiteral("addonCount"), addonMaps.size());
        group.insert(QStringLiteral("hasAddons"), !addonMaps.isEmpty());
        groups.append(group);
    }
    return groups;
}

} // namespace arachnel::core
