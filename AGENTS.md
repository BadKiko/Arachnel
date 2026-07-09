# AGENTS.md

## Cursor Cloud specific instructions

Arachnel is a **Qt 6 / QML (C++20) desktop game launcher**. There is a single
build artifact — the GUI app `arachnel_app`. There is currently no automated
test suite and no repo-level lint config (the `unicase-source-formatting` skill
targets a different project and does not apply here).

### Toolchain (already provisioned in the VM snapshot)

- **Qt 6.10.3** (official binaries via `aqtinstall`) at `~/Qt/6.10.3/gcc_64`.
  Qt **6.10+ is required**: `CMakeLists.txt` calls
  `find_package(Qt6 ... QuickPrivate)` unconditionally, and `QuickPrivate` only
  exists as a separate CMake component from 6.10 onward (Ubuntu's apt Qt is
  6.4 and will not work).
- The `qtshadertools` module is installed (needed by the bundled `QmlMaterial`
  dependency, which compiles shaders with `qsb`).
- System libs (Mesa/GL, xkbcommon, xcb, DBus, fontconfig), `ninja-build`,
  `pkg-config`, and `Xvfb` are installed.
- A login shell exports `QT_ROOT`, prepends `$QT_ROOT/bin` to `PATH`, sets
  `CMAKE_PREFIX_PATH`, and sets `QT_QML_MATERIAL_IMPORT_PATH` (see `~/.bashrc`).
  If you use a **non-interactive** shell, export these yourself (see below).

### Build

`QmlMaterial` is pulled at CMake **configure** time via `FetchContent` from
GitHub (network required, including its git submodules), and built into
`build/qml_modules`.

```bash
export CMAKE_PREFIX_PATH="$HOME/Qt/6.10.3/gcc_64"
export PATH="$HOME/Qt/6.10.3/gcc_64/bin:$PATH"
# /usr/bin/c++ points at clang (which can't find libstdc++); force GCC:
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build -j"$(nproc)"
```

`./run.sh` also works but does not pin the compiler, so a fresh `/usr/bin/c++`
(clang) checkout can fail with `cannot find -lstdc++`; prefer the explicit
`-DCMAKE_CXX_COMPILER=g++` above or keep `PATH`/`CMAKE_PREFIX_PATH` from `.bashrc`.

### Run

`arachnel_app` needs `QT_QML_MATERIAL_IMPORT_PATH` to point at the built
`QmlMaterial` QML modules, otherwise the QML fails to load.

```bash
export QT_QML_MATERIAL_IMPORT_PATH=/workspace/build/qml_modules
export DISPLAY=:1          # a real X display is at :1 for GUI/manual testing
./build/arachnel_app
```

Gotchas:
- **Do NOT set `QT_QPA_PLATFORM=offscreen` when you want a visible window.** With
  `offscreen`, the process runs fine and loads all QML but never creates an X11
  window (nothing appears on the desktop). Use `QT_QPA_PLATFORM=xcb` (or leave
  unset) for the display at `:1`. `offscreen` is only useful for a headless
  "does it load without QML errors" smoke check.
- The `QML FontLoader: Cannot load font: MaterialSymbolsRounded...woff2`
  warnings on startup are **non-fatal** and do not block rendering.
- The app uses mock data (`CoreController::loadMockData`); core actions
  (Play / Install / search / check-updates) push entries into the Active Tasks
  queue and a snackbar — no external services are required.
