# Library domain

This domain owns the local installed-game library and configured storage roots.
It is the source of truth for entries persisted across application restarts.

## Main types

- `LibraryStore` persists `LibraryGame` records.
- `LibraryModel` exposes games and components to QML.
- `StorageLibrary` and `StorageLibraryModel` represent library locations.
- `LibraryController` performs user-facing library operations.
- `LibraryMaintenanceService` restores and reconciles persisted state.
- `GameUpdateService` compares local entries with current catalog entries.

An install service commits an installed result here only after the plugin has
provided an install path. Controllers notify the façade to synchronize the QML
model rather than allowing QML to mutate stores directly.

## Boundaries

Remote records and feed loading belong to `catalog/`; job lifecycle belongs to
`jobs/`; install policy belongs to `install/`. A source plugin determines
source-specific update rules, while `GameUpdateService` coordinates the common
library-facing flow.

Keep filesystem persistence and QML model synchronization separate.
