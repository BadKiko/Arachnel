# Core layout (`src/core/`)

Core is arranged by domain. `CoreController` in `facade/` is the QML boundary
(`Arachnel.Core`) and service composition point, not the owner of workflows.
Prefer **≤400 lines** per `.cpp`.

| Folder | Responsibility | Guide |
|--------|----------------|-------|
| `facade/` | Core façade, crash façade, service wiring | [README](facade/README.md) |
| `catalog/` | Feeds, parsing, filters, metadata, covers | [README](catalog/README.md) |
| `library/` | Installed games, stores, storage, updates | [README](library/README.md) |
| `jobs/` | Job state, models, orchestration, HTTP | [README](jobs/README.md) |
| `install/` | Install analysis and sessions | [README](install/README.md) |
| `plugins/` | Host, `.arach`, ABI and plugin catalog | [README](plugins/README.md) |
| `launch/` | Resolve, launch and track processes | [README](launch/README.md) |
| `runtime/` | Runtime dependencies, Proton, containers | [README](runtime/README.md) |
| `torrent/` | libtorrent session and metadata | [README](torrent/README.md) |
| `settings/` | Preferences, notifications, app updates | [README](settings/README.md) |
| `i18n/`, `util/` | Translation service and shared helpers | — |

App entry / crash reporting live in [`src/app/`](../app/).

## Adding code

1. Put a type in the domain owning its state and behaviour.
2. Register its `.cpp` in [`src/CMakeLists.txt`](../CMakeLists.txt).
3. Construct and connect it from `CoreController::initializeServices()` only
   when it is part of the application composition.
4. Expose QML operations through minimal controller forwards; keep
   `Arachnel.Core` stable.
5. Keep source-specific install, update and launch logic in an external plugin.

See [`docs/CONTRIBUTING.md`](../../docs/CONTRIBUTING.md) for the full workflow.

## Plugin SDK

Out-of-tree plugins include flat headers such as `plugin_interface.h` through
`cmake/ArachnelPluginSdk.cmake`. The current plugin API is v3; the host accepts
v2–v3 plugins.
