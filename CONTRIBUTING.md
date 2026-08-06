# Contributing

PRs and issues welcome. Keep changes focused. If you're changing core layout / services, skim [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md).

UI strings: English in code (`qsTr` / `translate`), Russian in `translations/arachnel_ru.ts`. Translations also go through [Weblate](https://hosted.weblate.org/engage/arachnel/).

**You don't need to install Boost.** CMake pulls Boost headers + libtorrent on first configure when there's no system libtorrent. If you *do* have system `libtorrent`, install Boost headers too or the build dies late on `#include <boost/...>`.

<details>
<summary><strong>Build on Windows</strong></summary>

1. Git + LFS (fonts in QmlMaterial):

```powershell
winget install --id Git.Git -e
winget install --id GitHub.GitLFS -e
git lfs install
```

2. [Qt Online Installer](https://www.qt.io/download-qt-installer) - pick **Qt 6.8+** (CI uses 6.11). Under the kit tick:
   - **MinGW 64-bit** (easiest with `run.ps1`)
   - **Qt Multimedia**
   - **Qt Shader Tools**
   - under Tools: **MinGW**, **CMake**, **Ninja**

3. Clone and run:

```powershell
git clone https://github.com/BadKiko/Arachnel.git
cd Arachnel
.\run.ps1
```

First configure can take a while (libtorrent / QmlMaterial / Boost download). If Qt isn't found, set `QT_INSTALL_DIR` to your Qt root (the folder that contains `6.x.y` and `Tools`).

</details>

<details>
<summary><strong>Build on Linux</strong></summary>

Need a C++20 compiler, CMake 3.20+, Ninja (or Make), Git, **git-lfs**, and **Qt 6.8+** with Multimedia + Shader Tools.

**Arch / CachyOS**

```bash
sudo pacman -S --needed base-devel cmake ninja git git-lfs \
  qt6-base qt6-declarative qt6-multimedia qt6-shadertools qt6-tools
git lfs install
git clone https://github.com/BadKiko/Arachnel.git
cd Arachnel
./run.sh
```

**Ubuntu / Debian** - distro Qt is often too old. Easiest: install Qt 6.8+ via the [Online Installer](https://www.qt.io/download-qt-installer) (same modules as Windows: Multimedia, Shader Tools), then:

```bash
sudo apt install build-essential cmake ninja-build git git-lfs
git lfs install
export CMAKE_PREFIX_PATH=/path/to/Qt/6.x.y/gcc_64
./run.sh
```

Skip system `libtorrent` unless you also have Boost headers. Otherwise let CMake fetch it.

</details>
