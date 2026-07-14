#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QTimer;

namespace arachnel::core {

class TorrentSession : public QObject
{
    Q_OBJECT

public:
    explicit TorrentSession(QObject* parent = nullptr);
    ~TorrentSession() override;

    bool isAvailable() const { return m_available; }

    bool addJob(const QString& jobId, const QString& magnetUri, const QString& savePath);
    void cancel(const QString& jobId, bool deleteFiles = true);
    void setPaused(const QString& jobId, bool paused);
    bool isPaused(const QString& jobId) const;
    void saveAllResumeData();
    void flushResumeData();
    void shutdown();
    void removeResumeFile(const QString& jobId);

    static QString resumeDirectory();

signals:
    void torrentProgress(const QString& jobId, int progress, qint64 downloaded, qint64 total,
                         int downloadRate, int numPeers, const QString& state);
    void torrentFinished(const QString& jobId, const QString& savePath);
    void torrentFailed(const QString& jobId, const QString& error);

private:
    void pollAlerts();
    QString resumeFilePath(const QString& jobId) const;
    void requestResumeSave(const QString& jobId);
    void updateIdleTimers();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    QTimer* m_pollTimer = nullptr;
    QTimer* m_resumeTimer = nullptr;
    bool m_available = true;
};

} // namespace arachnel::core
