# Torrent domain

This domain provides the shared libtorrent transport used for magnet-based
downloads. It does not know source installation rules or QML presentation.

## Main types

- `TorrentSession` owns the libtorrent session and public transfer operations.
- `torrent_session_poll.cpp` translates engine state into periodic updates.
- `TorrentSettings` holds transport configuration.
- `TorrentMetadataFetcher` obtains metadata needed before a torrent can run.
- `torrent_session_internal.h` contains implementation-only libtorrent details.

`JobOrchestrator` creates and controls torrent jobs through `TorrentSession`.
Progress and terminal results return to `jobs/`, which then lets `install/`
continue the selected source's install session.

## Boundaries

Keep libtorrent headers and protocol-specific implementation here. Job
persistence, pause/retry policy and QML model roles belong to `jobs/`.
Archive extraction, installer execution and source-specific logic belong in a
plugin, never in this domain.

Changes to transport settings should flow through `SettingsStore` rather than
introducing a second persisted configuration path.
