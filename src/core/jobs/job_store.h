#pragma once

#include "job_model.h"

#include <QObject>
#include <QVector>

namespace arachnel::core {

class JobStore : public QObject
{
    Q_OBJECT

public:
    explicit JobStore(QObject* parent = nullptr);

    QVector<JobEntry> jobs() const { return m_jobs; }
    void setJobs(QVector<JobEntry> jobs);

    const JobEntry* jobById(const QString& id) const;
    void upsertJob(const JobEntry& job);
    void removeJob(const QString& id);

    QVector<JobEntry> incompleteJobs() const;

    void load();
    void save();

signals:
    void jobsChanged();

private:
    QVector<JobEntry> m_jobs;
};

} // namespace arachnel::core
