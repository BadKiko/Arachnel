#include "job_store.h"

#include "job_status.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

namespace arachnel::core {

namespace {

QString jobsFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/jobs.json");
}

JobEntry jobFromJson(const QJsonObject& obj)
{
    JobEntry job;
    job.id = obj.value(QStringLiteral("id")).toString();
    job.title = obj.value(QStringLiteral("title")).toString();
    job.kind = static_cast<JobKind>(obj.value(QStringLiteral("kind")).toInt());
    job.status = obj.value(QStringLiteral("status")).toString();
    job.progress = obj.value(QStringLiteral("progress")).toInt();
    job.detail = obj.value(QStringLiteral("detail")).toString();
    job.bytesDownloaded = static_cast<qint64>(obj.value(QStringLiteral("bytesDownloaded")).toDouble());
    job.totalBytes = static_cast<qint64>(obj.value(QStringLiteral("totalBytes")).toDouble());
    job.entryId = obj.value(QStringLiteral("entryId")).toString();
    job.sourceId = obj.value(QStringLiteral("sourceId")).toString();
    job.magnetUri = obj.value(QStringLiteral("magnetUri")).toString();
    job.savePath = obj.value(QStringLiteral("savePath")).toString();
    job.coverUrl = obj.value(QStringLiteral("coverUrl")).toString();
    job.libraryId = obj.value(QStringLiteral("libraryId")).toString();
    job.createdAt = obj.value(QStringLiteral("createdAt")).toString();
    job.completedAt = obj.value(QStringLiteral("completedAt")).toString();
    return job;
}

QJsonObject jobToJson(const JobEntry& job)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), job.id);
    obj.insert(QStringLiteral("title"), job.title);
    obj.insert(QStringLiteral("kind"), static_cast<int>(job.kind));
    obj.insert(QStringLiteral("status"), job.status);
    obj.insert(QStringLiteral("progress"), job.progress);
    obj.insert(QStringLiteral("detail"), job.detail);
    obj.insert(QStringLiteral("bytesDownloaded"), static_cast<double>(job.bytesDownloaded));
    obj.insert(QStringLiteral("totalBytes"), static_cast<double>(job.totalBytes));
    obj.insert(QStringLiteral("entryId"), job.entryId);
    obj.insert(QStringLiteral("sourceId"), job.sourceId);
    obj.insert(QStringLiteral("magnetUri"), job.magnetUri);
    obj.insert(QStringLiteral("savePath"), job.savePath);
    obj.insert(QStringLiteral("coverUrl"), job.coverUrl);
    obj.insert(QStringLiteral("libraryId"), job.libraryId);
    obj.insert(QStringLiteral("createdAt"), job.createdAt);
    obj.insert(QStringLiteral("completedAt"), job.completedAt);
    return obj;
}

} // namespace

JobStore::JobStore(QObject* parent)
    : QObject(parent)
{
}

void JobStore::setJobs(QVector<JobEntry> jobs)
{
    m_jobs = std::move(jobs);
    emit jobsChanged();
    save();
}

const JobEntry* JobStore::jobById(const QString& id) const
{
    for (const auto& job : m_jobs) {
        if (job.id == id)
            return &job;
    }
    return nullptr;
}

void JobStore::upsertJob(const JobEntry& job)
{
    for (auto& existing : m_jobs) {
        if (existing.id == job.id) {
            existing = job;
            emit jobsChanged();
            save();
            return;
        }
    }
    m_jobs.append(job);
    emit jobsChanged();
    save();
}

void JobStore::removeJob(const QString& id)
{
    const auto it = std::find_if(m_jobs.begin(), m_jobs.end(),
                                 [&](const JobEntry& job) { return job.id == id; });
    if (it == m_jobs.end())
        return;
    m_jobs.erase(it);
    emit jobsChanged();
    save();
}

QVector<JobEntry> JobStore::incompleteJobs() const
{
    QVector<JobEntry> result;
    for (const auto& job : m_jobs) {
        if (!isJobTerminal(job.status))
            result.append(job);
    }
    return result;
}

void JobStore::load()
{
    QFile file(jobsFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    QVector<JobEntry> jobs;
    jobs.reserve(array.size());
    for (const QJsonValue& value : array)
        jobs.append(jobFromJson(value.toObject()));
    m_jobs = std::move(jobs);
    emit jobsChanged();
}

void JobStore::save()
{
    QJsonArray array;
    for (const auto& job : m_jobs)
        array.append(jobToJson(job));

    QFile file(jobsFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

} // namespace arachnel::core
