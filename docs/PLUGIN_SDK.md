# Plugin development (Arachnel)

Source plugins are **separate repositories**. Arachnel ships only the host (`PluginHost`) and a small **Plugin SDK** so third-party plugins can link against the same ABI as the app.

Reference implementations:

- [arachnel-plugin-freetp](https://github.com/PetWork/arachnel-plugin-freetp) — torrent download (API v2+)
- [arachnel-plugin-steamidra](https://github.com/PetWork/arachnel-plugin-steamidra) — plugin-owned download (API v3, `owns_download`)

---

## What you need

| Tool | Version |
|------|---------|
| CMake | 3.20+ |
| C++ compiler | C++20 (MinGW on Windows, GCC/Clang on Linux) |
| Qt | 6.8+ (**Core**, **Network**) |
| Ninja | recommended on Windows |

You also need a checkout of **Arachnel** (for headers + `cmake/ArachnelPluginSdk.cmake`). The app itself is optional for plugin-only work, but useful for end-to-end testing.

**Nothing from `build-*` folders is committed to git** — each developer configures locally.

---

## Repository layout (recommended)

```
~/src/
  Arachnel/                  # launcher + SDK
  arachnel-plugin-freetp/    # example plugin
  arachnel-plugin-mysource/  # your plugin
```

---

## Quick start (existing plugin — FreeTP)

### Windows

```powershell
git clone https://github.com/PetWork/Arachnel.git
git clone https://github.com/PetWork/arachnel-plugin-freetp.git

cd arachnel-plugin-freetp
$env:ARACHNEL_SDK_DIR = "C:\path\to\Arachnel"
# Qt: set CMAKE_PREFIX_PATH if not under D:\Qt or C:\Qt, e.g.:
# $env:CMAKE_PREFIX_PATH = "D:\Qt\6.11.1\mingw_64"

.\run.ps1
```

`run.ps1` configures `build-win/`, builds `freetp_plugin`, and copies the bundle to the launcher plugins folder.

### Linux

```bash
git clone https://github.com/PetWork/Arachnel.git
git clone https://github.com/PetWork/arachnel-plugin-freetp.git

cd arachnel-plugin-freetp
export ARACHNEL_SDK_DIR=~/src/Arachnel
export CMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target freetp_plugin
```

---

## Where to install the plugin

Arachnel loads plugins from **`plugins/<id>/`** under the app data directory (`PluginHost::pluginSearchRoots()`).

| OS | Plugins directory |
|----|-------------------|
| **Windows** | `%APPDATA%\Arachnel\plugins\<id>\` |
| **Linux** | `~/.local/share/Arachnel/plugins/<id>/` |

Each folder must contain at least:

```
plugins/freetp/
  plugin.json
  freetp_plugin.dll    # Windows
  freetp_plugin.so     # Linux
  games-arachnel.json  # optional bundled catalog
  linux/               # optional extra assets
```

### Two ways to install

1. **Development** — copy everything from `build-win/plugin-bundle/` (or `build/plugin-bundle/`) into `plugins/<id>/`.
2. **Release** — build `dist/<id>.arach`, then in Arachnel: **Settings → Plugins → Install .arach**.

`.arach` is a ZIP archive (`.zip` renamed) with the same files as the folder above (often wrapped in one subfolder — the host finds `plugin.json` automatically).

---

## Run Arachnel for testing

### Windows

```powershell
cd Arachnel
.\run.ps1
```

`run.ps1` builds only the launcher. If `arachnel-plugin-freetp/build-win/plugin-bundle/` exists (or `ARACHNEL_FREETP_PLUGIN_BUILD_DIR` is set), it deploys FreeTP automatically before launch.

### Linux

```bash
cd Arachnel
./run.sh
```

Install the plugin manually into `~/.local/share/Arachnel/plugins/<id>/` (no auto-deploy yet).

Check **`run.log`** in the app data folder for lines like `Plugin loaded: freetp`.

---

## Creating a new plugin

### 1. New git repository

```
arachnel-plugin-<source-id>/
  CMakeLists.txt
  plugin.json
  README.md
  run.ps1              # optional, copy from freetp
  src/
    plugin_entry.cpp   # C ABI exports
    <source>_plugin.h
    <source>_plugin.cpp
```

### 2. `plugin.json`

```json
{
  "id": "my-source",
  "name": "My Source",
  "version": "1.0.0",
  "apiVersion": 3,
  "library": "my_source_plugin",
  "iconName": "storefront",
  "catalogUrl": "https://example.com/games-arachnel.json",
  "description": "Short description for Settings UI"
}
```

- **`id`** — folder name under `plugins/` and `sourceId` in the library.
- **`apiVersion`** — host accepts **2..3** (`ARACHNEL_PLUGIN_API_VERSION` / `ARACHNEL_PLUGIN_API_VERSION_MIN` in `src/core/plugin_api.h`). New plugins should ship **3**.
- **`library`** — base name of the shared library (`my_source_plugin` → `my_source_plugin.dll`).

### 3. `plugin_entry.cpp`

Export the C ABI (see `arachnel-plugin-freetp/src/plugin_entry.cpp`):

- `arachnel_plugin_api_version()`
- `arachnel_plugin_catalog_entry_size()` → `sizeof(arachnel::core::CatalogEntry)`
- `arachnel_plugin_create(const char* plugin_root_utf8)`
- `arachnel_plugin_destroy(ISourcePlugin*)`

### 4. Implement `ISourcePlugin`

Header: `Arachnel/src/core/plugin_interface.h`

Required methods include `catalog()`, `analyzeDownload()`, `analyzeFileNames()`, `installFromDownload()`, `launchInfo()`, etc. Copy FreeTP or SteaMidra as a template.

### Plugin-owned download (API v3)

When the plugin must fetch content itself (Steam depots, HTTP, custom CDN) instead of receiving a finished torrent path:

1. Return capability **`owns_download`** from `capabilities()`.
2. Set `plugin.json` → `apiVersion`: **3**.
3. Override `startOwnedDownload(InstallContext, progressCb)` and optionally `cancelOwnedDownload(jobId)`.
4. Catalog entries should carry **`steamAppId`** (or another id the plugin understands). The host skips the magnet gate and creates a `pluginDownload` job.
5. Report progress via the callback (`OwnedDownloadProgress`); on success return `InstallResult` with `installPath`.

Default implementations of the new methods are no-ops so API v2 plugins keep working.

### 5. `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
project(arachnel_plugin_mysource LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

if(NOT DEFINED ARACHNEL_SDK_DIR)
    set(ARACHNEL_SDK_DIR "$ENV{ARACHNEL_SDK_DIR}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Network)
include(${ARACHNEL_SDK_DIR}/cmake/ArachnelPluginSdk.cmake)
arachnel_plugin_sdk_init(${ARACHNEL_SDK_DIR})

add_library(my_source_plugin SHARED
    src/plugin_entry.cpp
    src/my_source_plugin.cpp
)
target_include_directories(my_source_plugin PRIVATE src)
target_link_libraries(my_source_plugin PRIVATE arachnel_plugin_sdk Qt6::Core Qt6::Network)

set_target_properties(my_source_plugin PROPERTIES
    OUTPUT_NAME "my_source_plugin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin-bundle"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin-bundle"
)
if(MINGW)
    set_target_properties(my_source_plugin PROPERTIES PREFIX "")
endif()

arachnel_stage_plugin_bundle(my_source_plugin my-source "${CMAKE_BINARY_DIR}/plugin-bundle")
# Optional: arachnel_package_plugin_arach(...) — see freetp CMakeLists.txt
```

Environment variables:

| Variable | Purpose |
|----------|---------|
| `ARACHNEL_SDK_DIR` | Path to Arachnel checkout |
| `CMAKE_PREFIX_PATH` | Qt 6 kit (e.g. `.../6.11.1/mingw_64`) |
| `ARACHNEL_SKIP_FREETP_CATALOG_FETCH` | FreeTP only: skip catalog download at configure |

---

## SDK contents (inside Arachnel)

| Path | Purpose |
|------|---------|
| `cmake/ArachnelPluginSdk.cmake` | `arachnel_plugin_sdk` static lib + bundle helpers |
| `src/core/plugin_api.h` | C ABI, `ARACHNEL_PLUGIN_API_VERSION` |
| `src/core/plugin_interface.h` | `ISourcePlugin`, install/launch structs |
| `src/core/catalog_types.h` | `CatalogEntry` (size must match app) |

The SDK static library compiles shared helpers from Arachnel core (catalog parser, install heuristics, file utils, Windows runner, etc.) so plugins do not duplicate that code by hand.

---

## ABI rules (important)

1. **`plugin.json` → `apiVersion`** must be in the host’s allowed range (**2..3**). Prefer **3** for new plugins; use **3** if you implement `owns_download`.
2. **`arachnel_plugin_catalog_entry_size()`** must return the same `sizeof(CatalogEntry)` as the app — otherwise the host logs `Plugin rejected (CatalogEntry size mismatch)` and skips the plugin.
3. After updating Arachnel, **rebuild all plugins** against the same SDK revision.
4. Match **Qt major version** and **compiler** (MinGW vs MSVC) with the Arachnel build you test against.

---

## Catalog JSON

Plugins may bundle `games-arachnel.json` next to `plugin.json` and/or point to a remote URL in `catalogUrl`. Format: [docs/CATALOG_FORMAT.md](CATALOG_FORMAT.md).

Downloaded catalogs (`games-arachnel.json`) are **gitignored** in plugin repos — fetched at CMake configure time or shipped only inside `.arach` releases.

---

## Troubleshooting

| Symptom | Check |
|---------|--------|
| Plugin not listed | `plugin.json` `apiVersion`, files in `plugins/<id>/`, `run.log` |
| `Plugin rejected (apiVersion …)` | Rebuild plugin; bump `apiVersion` in manifest |
| `CatalogEntry size mismatch` | Rebuild plugin against current Arachnel SDK |
| Install does nothing | Plugin loaded? `InstallAnalyzer` picks a plugin with `canInstall` |
| Wrong Qt at configure | Set `CMAKE_PREFIX_PATH` to the same Qt kit as Arachnel |

---

## Related docs

- [ARCHITECTURE.md](ARCHITECTURE.md) — host vs plugin responsibilities  
- [CATALOG_FORMAT.md](CATALOG_FORMAT.md) — JSON feed format  
- [plugins/README.md](../plugins/README.md) — plugin index in this repo
