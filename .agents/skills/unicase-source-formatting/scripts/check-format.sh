#!/usr/bin/env bash
# Check (and optionally fix) C++/QML/CMake formatting for a git repo.
# Usage: FIX=1 ./check-format.sh [repo_dir]
set -euo pipefail

REPO_DIR="${1:-.}"
REPO_DIR="$(cd "$REPO_DIR" && pwd)"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNICASE_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

CLANG_CFG="${UNICASE_ROOT}/.clang-format"
CMAKE_CFG="${UNICASE_ROOT}/.cmake-format"
QML_INI="${UNICASE_ROOT}/.qmlformat.ini"

QMLFORMAT="${QMLFORMAT:-$HOME/Qt/6.10.0/gcc_64/bin/qmlformat}"
if [[ ! -x "$QMLFORMAT" ]]; then
  for c in /opt/Qt6/6.10.0/gcc_64/bin/qmlformat /opt/Qt/*/gcc_64/bin/qmlformat; do
    [[ -x "$c" ]] && QMLFORMAT="$c" && break
  done
fi

CF="clang-format"
command -v clang-format-20 >/dev/null && CF="clang-format-20"

FIX="${FIX:-0}"
CPP_FAIL=0
QML_FAIL=0
CMAKE_FAIL=0

if [[ ! -f "$CLANG_CFG" || ! -f "$CMAKE_CFG" || ! -f "$QML_INI" ]]; then
  echo "error: unicase format configs not found under $UNICASE_ROOT" >&2
  exit 2
fi

if ! command -v cmake-format >/dev/null; then
  echo "error: cmake-format not installed (pip3 install cmake-format)" >&2
  exit 2
fi

if [[ ! -x "$QMLFORMAT" ]]; then
  echo "error: qmlformat not found (set QMLFORMAT=...)" >&2
  exit 2
fi

cd "$REPO_DIR"

mapfile -t CPP_FILES < <(git ls-files '*.cpp' '*.h' '*.hpp' '*.tpp' 2>/dev/null || true)
mapfile -t QML_FILES < <(git ls-files '*.qml' 2>/dev/null || true)
mapfile -t CMAKE_FILES < <(git ls-files | grep -E 'CMakeLists\.txt$|\.cmake$' || true)

echo "==> $REPO_DIR"
echo "    configs: $UNICASE_ROOT"

for f in "${CPP_FILES[@]}"; do
  if [[ "$FIX" == "1" ]]; then
    "$CF" -i -style="file:$CLANG_CFG" "$f"
  elif ! "$CF" --dry-run --Werror -style="file:$CLANG_CFG" "$f" 2>/dev/null; then
    echo "C++ FAIL: $f"
    CPP_FAIL=$((CPP_FAIL + 1))
  fi
done

cp -f "$QML_INI" .qmlformat.ini
for f in "${QML_FILES[@]}"; do
  if [[ "$FIX" == "1" ]]; then
    "$QMLFORMAT" -i "$f"
  else
    tmp="$(mktemp)"
    if ! "$QMLFORMAT" "$f" >"$tmp" 2>/dev/null; then
      echo "QML ERROR: $f"
      QML_FAIL=$((QML_FAIL + 1))
    elif ! diff -q "$f" "$tmp" >/dev/null 2>&1; then
      echo "QML FAIL: $f"
      QML_FAIL=$((QML_FAIL + 1))
    fi
    rm -f "$tmp"
  fi
done

for f in "${CMAKE_FILES[@]}"; do
  if [[ "$FIX" == "1" ]]; then
    cmake-format -i -c "$CMAKE_CFG" "$f"
  else
    out="$(mktemp)"
    if ! cmake-format -c "$CMAKE_CFG" -o "$out" "$f" 2>/dev/null; then
      echo "CMAKE ERROR: $f"
      CMAKE_FAIL=$((CMAKE_FAIL + 1))
    elif ! diff -q "$f" "$out" >/dev/null 2>&1; then
      echo "CMAKE FAIL: $f"
      CMAKE_FAIL=$((CMAKE_FAIL + 1))
    fi
    rm -f "$out"
  fi
done

if [[ "$FIX" == "1" ]]; then
  echo "Applied formatting (FIX=1)."
  git diff --stat 2>/dev/null || true
  exit 0
fi

TOTAL=$((CPP_FAIL + QML_FAIL + CMAKE_FAIL))
echo "Summary: C++=$CPP_FAIL QML=$QML_FAIL CMake=$CMAKE_FAIL (checked ${#CPP_FILES[@]}/${#QML_FILES[@]}/${#CMAKE_FILES[@]} files)"
exit "$TOTAL"
