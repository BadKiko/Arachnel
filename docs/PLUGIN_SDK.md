# Plugin development (Arachnel)

Source plugins are **separate repositories**. Arachnel ships only the host (`PluginHost`) and a small **Plugin SDK** so third-party plugins can link against the same ABI as the app.

Reference implementations:

- [arachnel-plugin-freetp](https://github.com/PetWork/arachnel-plugin-freetp) — torrent download (API v4 JSON catalog; still loads on host as v2+ for older builds)
- [arachnel-plugin-steamidra](https://gitlab.com/BadKiko/arachnel-plugin-steamidra) — plugin-owned download (API v4, `owns_download`)

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
  "apiVersion": 4,
  "library": "my_source_plugin",
  "iconName": "storefront",
  "catalogUrl": "https://example.com/games-arachnel.json",
  "repository": "https://github.com/you/arachnel-plugin-mysource",
  "description": "Short description for Settings UI",
  "minArachnel": "0.1.34",
  "sdkRef": "v0.1.34a"
}
```

- **`id`** — folder name under `plugins/` and `sourceId` in the library.
- **`apiVersion`** — host accepts **2..4** (`ARACHNEL_PLUGIN_API_VERSION` / `ARACHNEL_PLUGIN_API_VERSION_MIN` in `src/core/plugins/plugin_api.h`). New plugins should ship **4**.
- **`library`** — base name of the shared library (`my_source_plugin` → `my_source_plugin.dll`).
- **`catalogUrl`** — where the launcher loads the game list JSON for this source (shown in Settings).
- **`repository`** — public git/source URL (shown in Plugin store / Plugins / Sources). Also accepted: `homepage`, `sourceUrl`, `git`.
- **`minArachnel` / `maxArachnel` / `sdkRef`** — store picker uses these so old launchers keep a compatible package. **`minArachnel` / `maxArachnel` are also enforced when loading `plugin.json` locally** (manual `.arach` / drop-in). If the running app is outside that range, the plugin is rejected and the UI tells the user to update Arachnel (or install a matching build).

### 3. `plugin_entry.cpp`

Export the C ABI (see `arachnel-plugin-freetp/src/plugin_entry.cpp`):

- `arachnel_plugin_api_version()`
- `arachnel_plugin_create(const char* plugin_root_utf8)`
- `arachnel_plugin_destroy(ISourcePlugin*)`
- **API 4:** `arachnel_plugin_catalog_json` / `arachnel_plugin_catalog_json_free` (UTF-8 JSON catalog; host parses into its own `CatalogEntry`)
- **API 2/3 only:** `arachnel_plugin_catalog_entry_size()` → `sizeof(CatalogEntry)` (legacy canary)

### 4. Implement `ISourcePlugin`

Header: `Arachnel/src/core/plugins/plugin_interface.h`

Required methods include `catalog()`, `analyzeDownload()`, `analyzeFileNames()`, `installFromDownload()`, `launchInfo()`, etc. Copy FreeTP or SteaMidra as a template. For API 4 the host loads catalog via JSON, not `CatalogEntry` layout across the DLL.

### Plugin-owned download (API v3+)

When the plugin must fetch content itself (Steam depots, HTTP, custom CDN) instead of receiving a finished torrent path:

1. Return capability **`owns_download`** from `capabilities()`.
2. Set `plugin.json` → `apiVersion`: **4** (or at least **3**).
3. Override `startOwnedDownload(InstallContext, progressCb)` and optionally `cancelOwnedDownload(jobId)`.
4. Catalog entries should carry **`steamAppId`** (or another id the plugin understands). The host skips the magnet gate and creates a `pluginDownload` job.
5. Report progress via the callback (`OwnedDownloadProgress`); on success return `InstallResult` with `installPath`.

Default implementations of the owned-download methods are no-ops so older plugins keep working.

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
| `src/core/plugins/plugin_api.h` | C ABI, `ARACHNEL_PLUGIN_API_VERSION` |
| `src/core/plugins/plugin_interface.h` | `ISourcePlugin`, install/launch structs |
| `src/core/plugins/plugin_catalog_json.h` | API 4 catalog JSON serialize/parse |
| `src/core/catalog/catalog_types.h` | `CatalogEntry` (host-side only for API 4) |

The SDK static library compiles shared helpers from Arachnel core (catalog parser, install heuristics, file utils, Windows runner, etc.) so plugins do not duplicate that code by hand.

---

## ABI rules (important)

1. **`plugin.json` → `apiVersion`** must be in the host’s allowed range (**2..4**). Prefer **4** for new plugins.
2. **`plugin.json` → `minArachnel` / `maxArachnel`** — enforced at load (not only in the store). Bump `minArachnel` whenever the plugin needs a newer core/SDK.
3. **API 4:** export `arachnel_plugin_catalog_json` / `_free`. `arachnel_plugin_catalog_entry_size()` is optional. If it matches the host, `entryById` / `detectUpdate` may cross the DLL; if it does not (or is missing), the plugin still loads and those calls are skipped.
4. **API 2/3:** `arachnel_plugin_catalog_entry_size()` is required and must match the app. Migrate to v4 when you can.
5. Build plugins with the **same MinGW + Qt kit** as Arachnel `release.yml` (`scripts/ci/launcher-toolchain.env`). Set `ARACHNEL_SDK_REF` to that release tag.
6. After cutting an Arachnel release that changes `CatalogEntry` / plugin ABI, bump plugin `ARACHNEL_SDK_REF` / `minArachnel` and publish sourcelist `builds[]` for both old and new hosts.

---

## Linux runtime policy (required)

Linux plugins must be self-contained. Do not rely on distro packages for plugin runtime deps.

### Must-haves

1. Build plugin `.so` with `RUNPATH=$ORIGIN` (or `$ORIGIN/lib` when using a subfolder).
2. Bundle every non-base shared library required by the plugin into the plugin folder.
3. Keep `libfreetp_plugin.so` / `libsteamidra_plugin.so` loadable on Ubuntu, Arch/Cachy, Fedora without extra host packages.
4. Fail CI when unresolved or forbidden dynamic dependencies are detected.

### Allowed host/runtime libs

The following are allowed from host runtime and should not be bundled:

- `linux-vdso.so.*`
- `ld-linux-*.so.*`
- `libc.so.*`
- `libm.so.*`
- `libpthread.so.*`
- `libdl.so.*`
- `librt.so.*`
- `libgcc_s.so.*`
- `libstdc++.so.*`
- `libresolv.so.*`
- `libnsl.so.*`
- `libutil.so.*`
- `libz.so.*`

For official Arachnel plugins, Qt 6 runtime libs (`libQt6*.so.*`) are also allowed to come
from the launcher runtime/AppImage and do not need to be bundled into the plugin package.

Everything else must either be bundled into the plugin package or explicitly justified in CI allowlist.

### Self-check command (run locally and in CI)

```bash
ldd plugin-bundle/lib<plugin>.so
readelf -d plugin-bundle/lib<plugin>.so | rg "NEEDED|RUNPATH|RPATH"
```

Expected:

- no `=> not found` lines in `ldd` output, except runtime-provided Qt libs when your CI
  intentionally tests against Arachnel's Qt runtime;
- no forbidden external deps outside allowlist;
- `RUNPATH` or `RPATH` contains `$ORIGIN`.

### CI gate (required for release)

Before publishing `.arach`, CI must:

1. inspect dynamic deps (`ldd` + `readelf`);
2. fail on unresolved deps;
3. fail on non-allowlisted external deps;
4. smoke-load plugin `.so` in a minimal runner image.

This gate is mandatory for official source plugins.

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
| `CatalogEntry size mismatch` | Legacy API 2/3 only - rebuild against matching SDK, or migrate to API 4 |
| Install does nothing | Plugin loaded? `InstallAnalyzer` picks a plugin with `canInstall` |
| Wrong Qt at configure | Set `CMAKE_PREFIX_PATH` to the same Qt kit as Arachnel |

---

## Related docs

- [ARCHITECTURE.md](ARCHITECTURE.md) — host vs plugin responsibilities  
- [CATALOG_FORMAT.md](CATALOG_FORMAT.md) — JSON feed format  
- [plugins/README.md](../plugins/README.md) — plugin index in this repo
