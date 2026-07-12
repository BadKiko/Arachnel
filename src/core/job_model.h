#pragma once

#include "job_kind.h"

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>

namespace arachnel::core {

struct JobEntry {
    QString id;
    QString title;
    JobKind kind = JobKind::Download;
    QString status;
    int progress = 0;
    QString detail;
    qint64 bytesDownloaded = 0;
    qint64 totalBytes = 0;
    QString entryId;
    QString sourceId;
    QString magnetUri;
    QString savePath;
    QString coverUrl;
    QString libraryId;
    QString parentEntryId;
    QString referer;
    bool httpDownload = false;
    QString artifactPath;
    QString createdAt;
    QString completedAt;
};

class JobModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY countChanged)

public:
    enum Role {
        JobIdRole = Qt::UserRole + 1,
        TitleRole,
        StatusRole,
        ProgressRole,
        KindRole,
        KindLabelRole,
        DetailRole,
        BytesDownloadedRole,
        TotalBytesRole,
        EntryIdRole,
        SourceIdRole,
        StatusLabelRole,
        MagnetUriRole,
        SavePathRole,
        CoverUrlRole,
        LibraryIdRole,
        ParentEntryIdRole,
        RefererRole,
        HttpDownloadRole,
        ArtifactPathRole,
        CreatedAtRole,
        CompletedAtRole,
    };
    Q_ENUM(Role)

    explicit JobModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_jobs.size(); }
    int activeCount() const;

    Q_INVOKABLE QVariantMap jobForEntry(const QString& entryId) const;
    Q_INVOKABLE QVariantMap jobForAddon(const QString& parentEntryId,
                                        const QString& addonId) const;
    Q_INVOKABLE QVariantMap primaryActiveJob() const;
    Q_INVOKABLE QVariantList downloadGroups() const;

    void setJobs(QVector<JobEntry> jobs);
    void addJob(JobEntry job);
    void updateJob(const JobEntry& job);
    void updateJob(const QString& jobId, const QString& status, int progress);
    void removeJob(const QString& jobId);
    int indexOfJob(const QString& jobId) const;

    Q_INVOKABLE void refreshLocalizedText();

signals:
    void countChanged();
    void jobsChanged();

private:
    QVector<JobEntry> m_jobs;
};

} // namespace arachnel::core
