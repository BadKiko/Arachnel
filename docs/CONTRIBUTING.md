# Contributing to Arachnel

## Where code belongs

Put core code in the domain that owns its state and behaviour:

- `catalog/` — feeds, parsing, filters, metadata and covers;
- `library/` — installed-game state, storage and update maintenance;
- `jobs/` and `torrent/` — job lifecycle and download transports;
- `install/`, `launch/`, `runtime/` — install sessions, processes and runtime setup;
- `plugins/` — host, package handling and plugin ABI;
- `settings/` — persisted preferences, notifications and app update support.

`facade/` contains the QML boundary. Do not put source-specific work in core:
catalog scraping, unpacking rules and launch conventions belong in an external
source plugin, not in `CoreController` or a new `if (sourceId == ...)`.

## Size and structure

Keep implementation files at **400 lines or fewer**. Split a class by cohesive
responsibility before it grows past that boundary; name the new service after
the operation or state it owns. Keep public headers focused and use forward
declarations where appropriate.

## Adding a core service

1. Choose the owning `src/core/<domain>/` directory and add its header/source.
2. Add the `.cpp` to `ARACHNEL_APP_SOURCES` in `src/CMakeLists.txt`.
3. Give the service its dependencies explicitly through its constructor.
4. Create and connect it in `CoreController::initializeServices()` in
   `src/core/facade/core_wiring_services.cpp`.
5. Add only the required forwarding property or invokable to `CoreController`;
   QML must not construct or reach into domain services directly.
6. Document the ownership boundary in the domain README when it changes.

## Plugins and user-facing text

The host ABI is defined by `src/core/plugins/plugin_interface.h` and
`plugin_api.h`; preserve ABI compatibility deliberately. New QML/C++ user text
must use the project i18n rules and update both translation catalogs.

Build and test the smallest relevant target after changes. Do not edit planning
documents as incidental cleanup; keep documentation aligned only with code that
exists.
