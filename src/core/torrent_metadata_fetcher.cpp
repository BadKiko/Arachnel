#include "torrent_metadata_fetcher.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

namespace arachnel::core {

namespace {

void applyProbeSessionDefaults(lt::session& session)
{
    lt::settings_pack pack;
    pack.set_int(lt::settings_pack::active_downloads, 4);
    pack.set_int(lt::settings_pack::active_seeds, 0);
    pack.set_bool(lt::settings_pack::enable_dht, true);
    pack.set_bool(lt::settings_pack::enable_lsd, true);
    pack.set_bool(lt::settings_pack::enable_upnp, true);
    pack.set_bool(lt::settings_pack::enable_natpmp, true);
    session.apply_settings(pack);
}

} // namespace

QString magnetInfoHashKey(const QString& magnetUri)
{
    static const QRegularExpression re(
        QStringLiteral(R"(btih:([a-fA-F0-9]{40}|[A-Z2-7]{32}))"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = re.match(magnetUri);
    if (!match.hasMatch())
        return {};
    return match.captured(1).toLower();
}

std::optional<QStringList> fetchMagnetFileNames(const QString& magnetUri, int timeoutMs)
{
    if (magnetUri.isEmpty())
        return std::nullopt;

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return std::nullopt;

    lt::error_code ec;
    lt::add_torrent_params params = lt::parse_magnet_uri(magnetUri.toStdString(), ec);
    if (ec)
        return std::nullopt;

    params.save_path = tempDir.path().toStdString();
    params.flags |= lt::torrent_flags::auto_managed;
    params.flags |= lt::torrent_flags::stop_when_ready;

    lt::session session{lt::session_params{}};
    applyProbeSessionDefaults(session);

    const lt::torrent_handle handle = session.add_torrent(params, ec);
    if (ec || !handle.is_valid())
        return std::nullopt;

    handle.resume();
    handle.force_reannounce(0, lt::torrent_handle::ignore_min_interval);

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    bool hasMetadata = false;
    while (QDateTime::currentMSecsSinceEpoch() < deadline) {
        std::vector<lt::alert*> alerts;
        session.pop_alerts(&alerts);
        for (lt::alert* alert : alerts) {
            if (lt::alert_cast<lt::metadata_received_alert>(alert))
                hasMetadata = true;
        }

        const lt::torrent_status status = handle.status();
        if (status.has_metadata)
            hasMetadata = true;

        if (hasMetadata)
            break;

        QThread::msleep(100);
    }

    QStringList names;
    if (hasMetadata) {
        if (const std::shared_ptr<const lt::torrent_info> info = handle.torrent_file()) {
            const lt::file_storage& files = info->files();
            names.reserve(static_cast<int>(files.num_files()));
            for (lt::file_index_t i : files.file_range()) {
                const std::string path = files.file_path(i);
                if (!path.empty())
                    names.append(QString::fromStdString(path));
            }
        }
    }

    session.remove_torrent(handle);
    if (names.isEmpty())
        return std::nullopt;
    return names;
}

} // namespace arachnel::core
