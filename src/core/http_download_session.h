#pragma once

#include <QObject>
#include <QString>

namespace arachnel::core {

class HttpDownloadSession : public QObject
{
    Q_OBJECT

public:
    explicit HttpDownloadSession(QObject* parent = nullptr);
    ~HttpDownloadSession() override;

    bool addJob(const QString& jobId, const QString& url, const QString& referer,
                const QString& saveDirectory);
    void cancel(const QString& jobId);

signals:
    void httpProgress(const QString& jobId, int progress, qint64 downloaded, qint64 total);
    void httpFinished(const QString& jobId, const QString& filePath);
    void httpFailed(const QString& jobId, const QString& error);

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace arachnel::core
