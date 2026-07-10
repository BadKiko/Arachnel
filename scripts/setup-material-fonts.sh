#!/usr/bin/env bash
# QmlMaterial ships icon fonts via Git LFS. Shallow FetchContent clones get pointer files.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build"
QM_DIR="${BUILD}/_deps/qml_material-src"

if [[ ! -d "${QM_DIR}/.git" ]]; then
  echo "setup-material-fonts: QmlMaterial not fetched yet, skipping"
  exit 0
fi

FONT_FILE="${QM_DIR}/assets/MaterialSymbolsRounded.wght_400.opsz_24.fill_0.woff2"
if [[ -f "${FONT_FILE}" ]] && head -c 4 "${FONT_FILE}" | grep -q "wOF2"; then
  echo "setup-material-fonts: fonts already present"
  exit 0
fi

if ! command -v git-lfs >/dev/null 2>&1; then
  echo "setup-material-fonts: installing git-lfs…"
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq git-lfs
fi

echo "setup-material-fonts: pulling Material Symbols fonts via git lfs…"
git -C "${QM_DIR}" lfs pull
echo "setup-material-fonts: done"
