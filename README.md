<div align="center">

<img src="resources/icons/arachnel-github.svg" width="128" alt="Arachnel logo" />

<h1>Arachnel</h1>

<p>
  <a href="README.md"><img src="https://img.shields.io/badge/README-English-8E8E93?style=for-the-badge&labelColor=161618" alt="English README"></a>
  <a href="README.ru.md"><img src="https://img.shields.io/badge/README-Русский-8E8E93?style=for-the-badge&labelColor=161618" alt="Russian README"></a>
</p>

<p>
  <a href="https://hosted.weblate.org/engage/arachnel/">
    <img src="https://hosted.weblate.org/widget/arachnel/application/svg-badge.svg" alt="Translation status">
  </a>
</p>

<br>

<img src="docs/readme-carousel.svg" width="960" alt="Arachnel UI previews">

</div>

<br>

<img src="images/demo.gif" width="800" alt="Arachnel demo — catalog, downloads, library">

## What is Arachnel?

Arachnel is a Material 3 desktop launcher: browse catalogs, download via torrent, install through **source plugins**, and launch everything from one library.

The closest open-source project by feature set is **[Hydra Launcher](https://github.com/hydralauncher/hydra)** — excellent software; use whichever fits you better. Arachnel takes a different route under the hood: each source is its own plugin (portable, installer, bundled fix, or separate patch), and the native Qt stack stays light on RAM.

| | [Hydra](https://github.com/hydralauncher/hydra) | Arachnel |
|---|--------|----------|
| Sources | Shared download / install model | **One plugin per source** |
| Install | Mostly one pipeline | Portable / installer / fix — decided by the plugin |
| Catalogs | Built-in + community feeds | Plugins **and** Hydra-compatible `games.json` URLs |
| Stack | Electron | Native Qt / QML |
| RAM (Windows, idle-ish) | ~700&nbsp;MB | ~200&nbsp;MB |

## What works today

### For players

- **Library** — installed games, “Playing now”, update badges, remove / move between drives
- **Catalog** — multi-source chips, search, sort, grid/list, Steam covers & descriptions (cached on disk)
- **Downloads** — magnet torrents (libtorrent): progress, speed, peers, ETA; pause / resume / cancel; HTTP add-ons when the feed provides them
- **Install & Play** — with a source plugin (e.g. FreeTP): download → auto-install → **Play**; optional DLC/add-on picker
- **Updates** — detect newer catalog builds; check / install game updates; optional auto-update on launch
- **App updates** — check GitHub Releases from Settings (download Setup.exe / AppImage in-app)
- **Storage** — several library folders/drives; pick where to install; separate downloads folder
- **Appearance** — dark/light, accent colors; **English / Russian** UI ([Weblate](https://hosted.weblate.org/projects/arachnel/))
- **Linux** — Proton-GE download & selection in Settings → Launch; Windows builds run through Proton
- **Windows** — custom title bar; Setup installer from [Releases](https://github.com/BadKiko/Arachnel/releases)

### Sources (important)

| Mode | What you get |
|------|----------------|
| **Source plugin** (`.arach`) | Full cycle: catalog → download → **install** → **Play** |
| **Hydra catalog** (JSON URL only) | Browse + torrent, then **manual install** — same as in Hydra (open folder / run installer / point to the game) |

Official plugin today: **FreeTP** ([arachnel-plugin-freetp](https://github.com/PetWork/arachnel-plugin-freetp)) — auto install & Play. Online-Fix as a plugin is not ready yet; JSON feeds under Settings → Hydra catalogs still work with manual install like Hydra.

## Quick start (users)

1. Download the latest build from **[Releases](https://github.com/BadKiko/Arachnel/releases)**:
   - Windows: `Arachnel-<version>-Setup.exe`
   - Linux: `Arachnel-<version>-x86_64.AppImage`
2. Install the **FreeTP** plugin: get `freetp.arach` from the [plugin repo](https://github.com/PetWork/arachnel-plugin-freetp), then in Arachnel open **Settings → Plugins → Install .arach…**
3. On Linux, open **Settings → Launch** and install Proton-GE if prompted.
4. Open **Catalog** → pick a game → Install → wait for Downloads → **Play** from Library.

Optional: add extra Hydra-compatible `games.json` URLs under **Settings → Hydra catalogs**.

<details>
<summary>Build from source</summary>

```bash
# Linux
./run.sh

# Windows
.\run.ps1
```

Needs Qt 6.8+, CMake 3.20+, C++20, and libtorrent (system package or fetched by CMake).  
AppImage packaging: `scripts/ci/package-appimage.sh` — see [docs/RELEASE.md](docs/RELEASE.md).

</details>

## Screens

| Catalog | Library | Downloads |
|:---:|:---:|:---:|
| <img src="images/1.png" width="280" alt="Catalog"> | <img src="images/4.png" width="280" alt="Library"> | <img src="images/3.png" width="280" alt="Downloads"> |

## Coming next

- Online-Fix as a first-class source plugin
- Shipping popular plugins inside official releases
- More polish (notification center, portable integrity checks)

Roadmap for contributors: [docs/ROADMAP.md](docs/ROADMAP.md)

## Docs & plugins

| Doc | For |
|-----|-----|
| [VISION.md](docs/VISION.md) | Why the project exists |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Layers & plugin contract |
| [PLUGIN_SDK.md](docs/PLUGIN_SDK.md) | Writing a source plugin |
| [CATALOG_FORMAT.md](docs/CATALOG_FORMAT.md) | JSON catalog feed format |
| [TRANSLATING.md](docs/TRANSLATING.md) | i18n / Weblate |
| [RELEASE.md](docs/RELEASE.md) | GitHub Actions releases |
| [plugins/README.md](plugins/README.md) | Plugin status & FreeTP workflow |

Plugins live in **separate repositories**. This repo is the host + SDK only.

## Contact

**kirill.kif234@gmail.com** — only for important matters.

Bugs, ideas, questions: [GitHub Issues](https://github.com/BadKiko/Arachnel/issues).
