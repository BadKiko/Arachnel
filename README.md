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

<img src="docs/readme-carousel.svg" width="100%" alt="Arachnel UI previews">

</div>

<br>

## About

**Arachnel** is a desktop game launcher inspired by Hydra, built around **plugin-based sources** instead of one universal install pipeline.

Each source (FreeTP, Online-Fix, …) can define its own catalog, download, install, and launch flow — portable archives, installers, bundled fixes, or separate patches.

**Already in place**

- Material 3 UI: library, multi-source catalog, downloads, game details, settings
- Torrent downloads via libtorrent (magnet links from catalog JSON)
- Persistent settings, library, and download jobs
- Cover art and descriptions via Steam API
- Community translations on [Weblate](https://hosted.weblate.org/projects/arachnel/)

**Docs:** [Vision](docs/VISION.md) · [Architecture](docs/ARCHITECTURE.md) · [Roadmap](docs/ROADMAP.md) · [Translating](docs/TRANSLATING.md)

## Quick start

```bash
# Linux
./run.sh

# Windows
.\run.ps1
```

Manual build, dependencies, and environment variables are documented in the repo scripts and `cmake/`.

## Contact

**kirill.kif234@gmail.com** — only for important matters.

Everything else (bugs, ideas, questions): please use [GitHub Issues](https://github.com/BadKiko/Arachnel/issues).
