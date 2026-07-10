#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

// Downloads Steam CDN covers once and serves them from AppData/cover-cache/.
class CoverImageCache : public QObject
{
    Q_OBJECT

public:
    explicit CoverImageCache(QObject* parent = nullptr);

    // file:///… if already on disk, otherwise empty.
    QString localUrlFor(const QString& remoteUrl) const;
    bool has(const QString& remoteUrl) const;

    // If cached → emit ready immediately (queued). Else download then emit.
    void ensure(const QString& remoteUrl);
    void remove(const QString& remoteUrl);

signals:
    void ready(const QString& remoteUrl, const QString& localUrl);
    void failed(const QString& remoteUrl);

private:
    void handleFinished(QNetworkReply* reply);
    void startNext();
    QString cacheDir() const;
    QString filePathFor(const QString& remoteUrl) const;
    static QString extensionFor(const QString& remoteUrl);

    QNetworkAccessManager* m_network = nullptr;
    QSet<QString> m_inFlight;
    QList<QString> m_pending;
    int m_active = 0;

    static constexpr int kMaxConcurrent = 4;
};

} // namespace arachnel::core
