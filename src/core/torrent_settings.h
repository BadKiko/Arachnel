#pragma once

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>

namespace arachnel::core::torrent_settings {

constexpr int kMaxConnectionsPerTorrent = 100;

void applySessionDefaults(lt::session& session);
void prepareDownloadParams(lt::add_torrent_params& params, bool fromResume = false);
void tuneActiveDownloadHandle(lt::torrent_handle handle);
void maintainDownloadHandle(lt::torrent_handle handle, int connectedPeers, int downloadRate);

} // namespace arachnel::core::torrent_settings
