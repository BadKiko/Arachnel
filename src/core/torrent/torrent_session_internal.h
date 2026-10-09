#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/write_resume_data.hpp>

namespace {

QString statusStateLabel(const lt::torrent_status& status)
{
    switch (status.state) {
    case lt::torrent_status::checking_files:
        return QStringLiteral("checking");
    case lt::torrent_status::downloading_metadata:
        return QStringLiteral("metadata");
    case lt::torrent_status::downloading:
        return QStringLiteral("downloading");
    case lt::torrent_status::finished:
        return QStringLiteral("finished");
    case lt::torrent_status::seeding:
        return QStringLiteral("seeding");
    default:
        // Magnets often sit in checking_resume_data / unknown before metadata arrives.
        if (!status.has_metadata)
            return QStringLiteral("metadata");
        return QStringLiteral("queued");
    }
}

QString libtorrentErrorMessage(const lt::error_code& ec)
{
    const std::string& msg = ec.message();
    if (msg.empty())
        return QCoreApplication::translate("Core", "Torrent error %1").arg(ec.value());

    QString text = QString::fromUtf8(msg.data(), static_cast<int>(msg.size()));
    if (text.contains(QChar::ReplacementCharacter))
        text = QString::fromLocal8Bit(msg.data(), static_cast<int>(msg.size()));
    if (text.isEmpty())
        return QCoreApplication::translate("Core", "Torrent error %1").arg(ec.value());
    return text;
}

} // namespace

namespace arachnel::core {

struct TorrentSession::Impl
{
    lt::session session{lt::session_params{}};
    QHash<QString, lt::torrent_handle> handles;
    QHash<QString, QString> savePaths;
    QHash<QString, QString> magnetUris;
    QSet<QString> pausedJobs;
    QHash<QString, qint64> metadataStallSinceMs;
    QHash<QString, qint64> lastPeerRefreshMs;

    // Last values sent through torrentProgress(): an unchanged tick (paused, stalled, idle
    // seeding) is not re-emitted.
    struct ProgressSnapshot
    {
        int progress = -1;
        qint64 downloaded = -1;
        qint64 total = -1;
        int downloadRate = -1;
        int peers = -1;
        QString state;

        bool operator==(const ProgressSnapshot& o) const
        {
            return progress == o.progress && downloaded == o.downloaded && total == o.total
                && downloadRate == o.downloadRate && peers == o.peers && state == o.state;
        }
    };
    QHash<QString, ProgressSnapshot> lastProgress;
};

namespace {

constexpr int kMetadataKickAfterMs = 3000;

// `status` is the tick's single handle.status() snapshot: every status() call is a blocking
// round trip to the libtorrent session thread.
void kickStalledMetadata(const QString& jobId, lt::torrent_handle handle,
                         const lt::torrent_status& status,
                         QHash<QString, qint64>& metadataStallSinceMs)
{
    if (!handle.is_valid())
        return;

    if (status.state != lt::torrent_status::downloading_metadata) {
        metadataStallSinceMs.remove(jobId);
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 since = metadataStallSinceMs.value(jobId, now);
    if (!metadataStallSinceMs.contains(jobId))
        metadataStallSinceMs.insert(jobId, now);

    if (status.num_peers > 0 || status.download_rate > 0) {
        metadataStallSinceMs.remove(jobId);
        return;
    }

    if (now - since < kMetadataKickAfterMs)
        return;

    handle.resume();
    handle.force_reannounce(0, -1, lt::torrent_handle::ignore_min_interval);
    handle.force_dht_announce();
    metadataStallSinceMs.insert(jobId, now);
}

} // namespace
