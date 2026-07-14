#include "torrent_settings.h"

#include <libtorrent/announce_entry.hpp>

#include <string>
#include <unordered_set>
#include <vector>

namespace arachnel::core::torrent_settings {

namespace {

// Small curated set — qBittorrent-style, avoids flooding the peer list with duplicates.
std::vector<std::string> publicTrackerUrls()
{
    return {
        "udp://zer0day.ch:1337/announce",
        "udp://tracker.opentrackr.org:1337/announce",
        "http://tracker.opentrackr.org:1337/announce",
        "udp://open.demonii.com:1337/announce",
        "udp://open.tracker.cl:1337/announce",
        "udp://exodus.desync.com:6969/announce",
        "udp://tracker.torrent.eu.org:451/announce",
        "udp://tracker.openbittorrent.com:6969/announce",
    };
}

void mergeTrackersIntoParams(lt::add_torrent_params& params)
{
    std::unordered_set<std::string> existing;
    existing.reserve(params.trackers.size());
    for (const std::string& tracker : params.trackers)
        existing.insert(tracker);

    if (params.tracker_tiers.size() != params.trackers.size())
        params.tracker_tiers.assign(params.trackers.size(), 0);

    for (const std::string& tracker : publicTrackerUrls()) {
        if (existing.contains(tracker))
            continue;
        params.trackers.push_back(tracker);
        params.tracker_tiers.push_back(1);
        existing.insert(tracker);
    }
}

void addMissingTrackersToHandle(lt::torrent_handle handle)
{
    if (!handle.is_valid())
        return;

    std::unordered_set<std::string> existing;
    for (const lt::announce_entry& entry : handle.trackers())
        existing.insert(entry.url);

    for (const std::string& tracker : publicTrackerUrls()) {
        if (existing.contains(tracker))
            continue;
        lt::announce_entry entry(tracker);
        entry.tier = 1;
        entry.source = lt::announce_entry::source_client;
        handle.add_tracker(entry);
    }
}

constexpr int kLowPeerThreshold = 15;
constexpr int kLowRateThresholdBytes = 18 * 1024 * 1024;

} // namespace

void applySessionDefaults(lt::session& session)
{
    lt::settings_pack pack;
    pack.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:0,[::]:0");
    pack.set_int(lt::settings_pack::active_downloads, 100);
    pack.set_int(lt::settings_pack::active_seeds, 100);
    pack.set_int(lt::settings_pack::active_tracker_limit, 200);
    pack.set_int(lt::settings_pack::active_dht_limit, 300);
    pack.set_int(lt::settings_pack::active_lsd_limit, 200);
    pack.set_int(lt::settings_pack::connections_limit, 500);
    pack.set_int(lt::settings_pack::connection_speed, 30);
    pack.set_int(lt::settings_pack::min_reconnect_time, 10);
    pack.set_int(lt::settings_pack::peer_timeout, 120);
    pack.set_int(lt::settings_pack::inactivity_timeout, 600);
    pack.set_int(lt::settings_pack::unchoke_slots_limit, -1);
    pack.set_int(lt::settings_pack::choking_algorithm, lt::settings_pack::rate_based_choker);
    pack.set_int(lt::settings_pack::mixed_mode_algorithm, lt::settings_pack::peer_proportional);
    pack.set_int(lt::settings_pack::aio_threads, 10);
    pack.set_int(lt::settings_pack::file_pool_size, 500);
    pack.set_int(lt::settings_pack::max_out_request_queue, 1000);
    pack.set_int(lt::settings_pack::send_buffer_low_watermark, 512 * 1024);
    pack.set_int(lt::settings_pack::send_buffer_watermark, 4 * 1024 * 1024);
    pack.set_int(lt::settings_pack::send_buffer_watermark_factor, 200);
    pack.set_int(lt::settings_pack::disk_io_write_mode, lt::settings_pack::enable_os_cache);
    pack.set_int(lt::settings_pack::disk_io_read_mode, lt::settings_pack::enable_os_cache);
    pack.set_bool(lt::settings_pack::allow_multiple_connections_per_ip, false);
    pack.set_bool(lt::settings_pack::announce_to_all_trackers, false);
    pack.set_bool(lt::settings_pack::announce_to_all_tiers, true);
    pack.set_bool(lt::settings_pack::incoming_starts_queued_torrents, true);
    pack.set_bool(lt::settings_pack::enable_dht, true);
    pack.set_bool(lt::settings_pack::enable_lsd, true);
    pack.set_bool(lt::settings_pack::enable_upnp, true);
    pack.set_bool(lt::settings_pack::enable_natpmp, true);
    pack.set_int(lt::settings_pack::download_rate_limit, 0);
    pack.set_int(lt::settings_pack::upload_rate_limit, 0);
    pack.set_bool(lt::settings_pack::enable_incoming_utp, true);
    pack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
    pack.set_bool(lt::settings_pack::enable_incoming_tcp, true);
    pack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
    session.apply_settings(pack);
}

void prepareDownloadParams(lt::add_torrent_params& params, bool fromResume)
{
    mergeTrackersIntoParams(params);
    if (fromResume)
        params.peers.clear();
    params.max_connections = kMaxConnectionsPerTorrent;
    params.max_uploads = -1;
    params.upload_limit = 0;
    params.download_limit = 0;
    params.flags |= lt::torrent_flags::stop_when_ready;
    params.flags &= ~lt::torrent_flags::paused;
    params.flags &= ~lt::torrent_flags::auto_managed;
}

void tuneActiveDownloadHandle(lt::torrent_handle handle)
{
    if (!handle.is_valid())
        return;

    handle.unset_flags(lt::torrent_flags::auto_managed);
    handle.set_max_connections(kMaxConnectionsPerTorrent);
    handle.set_max_uploads(-1);
    handle.set_upload_limit(-1);
    handle.set_download_limit(-1);
    addMissingTrackersToHandle(handle);
    handle.resume();
    handle.force_reannounce(0, lt::torrent_handle::ignore_min_interval);
    handle.force_dht_announce();
}

void maintainDownloadHandle(lt::torrent_handle handle, int connectedPeers, int downloadRate)
{
    if (!handle.is_valid())
        return;
    if (connectedPeers >= kLowPeerThreshold && downloadRate >= kLowRateThresholdBytes)
        return;

    handle.unset_flags(lt::torrent_flags::auto_managed);
    handle.force_dht_announce();
}

} // namespace arachnel::core::torrent_settings
