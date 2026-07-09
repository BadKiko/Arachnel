---
name: unicase-source-formatting
description: >-
  Checks and applies Unicase source formatting for C++, QML, and CMake using
  configs in src/unicase (.clang-format, .cmake-format, .qmlformat.ini). Use when
  the user asks to format or lint sources, run qmlformat/clang-format/cmake-format,
  verify CI formatting stage, or before committing Unicase/submodule changes.
---

# Unicase source formatting

Canonical configs (repo root = `src/unicase`):

| Tool | Config |
|------|--------|
| `clang-format` | `.clang-format` |
| `cmake-format` | `.cmake-format` |
| `qmlformat` | `.qmlformat.ini` |

Submodules (e.g. `unicase_launcher/`) keep copies of these files; prefer **explicit paths to the unicase root configs** when checking from a submodule.

## Prerequisites

```bash
# C++
command -v clang-format-20 || command -v clang-format

# QML (adjust if Qt path differs)
QMLFORMAT="${QMLFORMAT:-$HOME/Qt/6.10.0/gcc_64/bin/qmlformat}"
# CI: /opt/Qt6/6.10.0/gcc_64/bin/qmlformat

# CMake
pip3 install cmake-format --break-system-packages  # if missing
```

## Workflow

1. Set `UNICASE` to the unicase git root (`src/unicase`).
2. `cd` into the target repo or submodule to format.
3. Run the check script (preferred) or commands below.
4. If failures: apply in-place fixes, re-run until clean.
5. Report `git diff --stat`; do not commit unless the user asks.

```bash
UNICASE=/path/to/src/unicase
bash "$UNICASE/.cursor/skills/unicase-source-formatting/scripts/check-format.sh" .
```

With auto-fix:

```bash
FIX=1 bash "$UNICASE/.cursor/skills/unicase-source-formatting/scripts/check-format.sh" .
```

## Per-language rules

### C++

```bash
CF=clang-format-20  # or clang-format
git ls-files '*.cpp' '*.h' '*.hpp' '*.tpp' | xargs -r \
  $CF --dry-run --Werror -style="file:$UNICASE/.clang-format"
# Fix:
git ls-files '*.cpp' '*.h' '*.hpp' '*.tpp' | xargs -r \
  $CF -i -style="file:$UNICASE/.clang-format"
```

### QML

`qmlformat` reads `.qmlformat.ini` from the working directory.

```bash
cp -f "$UNICASE/.qmlformat.ini" .
git ls-files '*.qml' | xargs -r "$QMLFORMAT" -i
```

Check (no write): compare each file to `qmlformat` stdout via `diff`.

### CMake

**Use `-c`, not `--config-file`** — the latter can yield empty stdout on some cmake-format versions.

```bash
git ls-files | grep -E 'CMakeLists\.txt$|\.cmake$' | while read -r f; do
  cmake-format -c "$UNICASE/.cmake-format" -o /tmp/out "$f"
  diff -q "$f" /tmp/out || echo "FAIL: $f"
done
# Fix:
git ls-files | grep -E 'CMakeLists\.txt$|\.cmake$' | xargs -r \
  cmake-format -i -c "$UNICASE/.cmake-format"
```

## Scope

- **Single submodule**: run from `unicase_launcher/` (or pass path to script).
- **Whole unicase + submodules** (matches CI `formatting` job):

```bash
cd "$UNICASE"
git ls-files '*.cpp' '*.h' '*.hpp' '*.tpp' | xargs -r clang-format-20 -i
git ls-files '*.qml' | xargs -r "$QMLFORMAT" -i
git submodule foreach --recursive '
  git ls-files "*.cpp" "*.h" "*.hpp" "*.tpp" | xargs -r clang-format-20 -i
  git ls-files "*.qml" | xargs -r '"$QMLFORMAT"' -i
' 2>/dev/null || true
```

CMake in submodules: run `cmake-format -i -c "$UNICASE/.cmake-format"` per tracked `CMakeLists.txt` (CI does not auto-format CMake today).

## CI alignment

GitLab uses [`scripts/ci/check-formatting.sh`](../../../scripts/ci/check-formatting.sh) (`MODE=check`):

- C++: `clang-format-20 -i`, QML: `qmlformat -i`
- **`3rdparty/` is fully skipped** (format + diff check)
- Fails if `git diff` is non-empty outside `3rdparty/`

`3rdparty/COLCON_IGNORE` prevents colcon from treating vendored CMake projects (`KDDockWidgets`, `QmlMaterial`) as ROS packages.

`colcon lint` uses `--base-paths src unicase_launcher` (same as build) and requires `pip install colcon-lint` (often absent locally).

## Pitfalls

- **QML toggle / logic bugs** are not caught by formatters — only style.
- **cmake-format** with wrong flag (`--config-file`) silently breaks checks.
- After formatting, rebuild affected packages if unsure (`colcon build --packages-select …`).
