#pragma once

#include <QObject>
#include <QString>

#include <memory>

namespace arachnel::core {

class TorrentSession : public QObject
{
    Q_OBJECT

public:
    explicit TorrentSession(QObject* parent = nullptr);
    ~TorrentSession() override;

    bool isAvailable() const { return m_available; }

    void addMagnet(const QString& jobId, const QString& magnetUri, const QString& savePath);
    void cancel(const QString& jobId);

signals:
    void torrentProgress(const QString& jobId, int progress, qint64 downloaded, qint64 total,
                         int downloadRate, int numPeers, const QString& state);
    void torrentFinished(const QString& jobId, const QString& savePath);
    void torrentFailed(const QString& jobId, const QString& error);

private:
    void pollAlerts();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_available = true;
};

} // namespace arachnel::core
