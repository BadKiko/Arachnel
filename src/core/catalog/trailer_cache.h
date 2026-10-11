#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

/**
 * Keeps Steam store trailers on disk so the player only ever opens local files.
 *
 * A short clip (mp4 / webm) is saved as one file. A full trailer is an HLS stream: one 720p
 * variant and the audio track are downloaded in parallel next to their playlists, and a local
 * master playlist points at them. Played from the network, FFmpeg opens every variant one by one
 * (seconds before the first frame) and blocks the window when the player is closed mid-load.
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

    static bool isHls(const QString& url);

signals:
    void ready(const QString& remoteUrl, const QString& localUrl);
    /** 0..1 while an HLS trailer downloads. */
    void progress(const QString& remoteUrl, double fraction);
    void failed(const QString& remoteUrl);

private:
    struct HlsJob {
        QString remoteUrl;
        QUrl masterUrl;
        QString dir;
        QString variantInf;
        QString variantUri;
        QString audioUri;
        QStringList files;      // paths relative to the master playlist
        int next = 0;
        int done = 0;
        int running = 0;
        int playlistsPending = 0;
        qint64 bytes = 0;
        bool failed = false;
    };

    QString cacheDir() const;
    QString filePathFor(const QString& remoteUrl) const;
    QString hlsDirFor(const QString& remoteUrl) const;
    void trimToBudget();
    void handleFinished(QNetworkReply* reply);

    QNetworkReply* get(const QUrl& url);
    void startHls(const QString& remoteUrl);
    void handleHlsMaster(QNetworkReply* reply);
    void handleHlsPlaylist(QNetworkReply* reply, const QString& localName, const QString& relUri);
    void pumpHls(const QString& remoteUrl);
    void handleHlsFile(QNetworkReply* reply, const QString& relPath);
    void finishHls(const QString& remoteUrl);
    void failHls(const QString& remoteUrl);

    QNetworkAccessManager* m_network = nullptr;
    QSet<QString> m_inFlight;
    QHash<QString, HlsJob> m_hls;
};

} // namespace arachnel::core
