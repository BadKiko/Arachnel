#pragma once

#include <QAbstractListModel>
#include <QString>

namespace arachnel::core {

struct JobEntry {
    QString id;
    QString title;
    QString status;
    int progress = 0;
};

class JobModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        JobIdRole = Qt::UserRole + 1,
        TitleRole,
        StatusRole,
        ProgressRole,
    };
    Q_ENUM(Role)

    explicit JobModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setJobs(QVector<JobEntry> jobs);
    void addJob(JobEntry job);
    void updateJob(const QString& jobId, const QString& status, int progress);

private:
    int indexOfJob(const QString& jobId) const;

    QVector<JobEntry> m_jobs;
};

} // namespace arachnel::core
