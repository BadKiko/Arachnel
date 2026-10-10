#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

/**
 * Keeps the short Steam store trailers (a few MB) on disk. A game page asks for its trailer as
 * soon as it opens; by the time the player is opened the clip is a local file, so playback does
 * not wait for FFmpeg to pull it over HTTP (3-8 s for the first frame before).
 */
class TrailerCache : public QObject
{
    Q_OBJECT

public:
    explicit TrailerCache(QObject* parent = nullptr);

    /** file: URL when the clip is on disk, otherwise empty. */
    QString localUrlFor(const QString& remoteUrl) const;
    /** Starts the download unless it is cached or already running. */
    void prefetch(const QString& remoteUrl);

signals:
    void ready(const QString& remoteUrl, const QString& localUrl);

private:
    QString cacheDir() const;
    QString filePathFor(const QString& remoteUrl) const;
    void trimToBudget();
    void handleFinished(QNetworkReply* reply);

    QNetworkAccessManager* m_network = nullptr;
    QSet<QString> m_inFlight;
};

} // namespace arachnel::core
