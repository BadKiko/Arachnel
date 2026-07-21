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
};

namespace {

constexpr int kMetadataKickAfterMs = 3000;

void kickStalledMetadata(const QString& jobId, lt::torrent_handle handle,
                         QHash<QString, qint64>& metadataStallSinceMs)
{
    if (!handle.is_valid())
        return;

    const lt::torrent_status status = handle.status();
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
    handle.force_reannounce(0, lt::torrent_handle::ignore_min_interval);
    handle.force_dht_announce();
    metadataStallSinceMs.insert(jobId, now);
}

} // namespace
