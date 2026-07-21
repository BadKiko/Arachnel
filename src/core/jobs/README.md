# Jobs domain

This domain owns durable download and install job state and its QML projection.
It provides the common lifecycle regardless of whether work uses HTTP, torrent
or a plugin-owned download.

## Main types

- `JobStore` persists `JobEntry` records.
- `JobModel` exposes jobs and roles to QML.
- `JobOrchestrator` creates, restores, pauses, retries and cancels work.
- `HttpDownloadSession` performs direct HTTP transfers.
- `job_kind`, `job_status` and `job_display` define shared job vocabulary.

The orchestrator depends on `TorrentSession` for magnet downloads and emits
completion/progress signals for the façade and install service to consume.
Plugin-owned downloads still have jobs, but the plugin reports their progress.

## Boundaries

This domain does not parse catalogs, decide a source install pipeline or commit
library entries. Those responsibilities are in `catalog/`, `install/` and
`library/`. Torrent protocol details live in `torrent/`.

Keep job transitions explicit and persisted through `JobStore`; do not mirror
job lifecycle in QML-only state.
