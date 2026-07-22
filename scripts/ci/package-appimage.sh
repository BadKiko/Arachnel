#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD_DIR:-${ROOT}/build-linux}"
DIST="${DIST_DIR:-${ROOT}/dist-linux}"
APPDIR="${DIST}/AppDir"
VERSION="${ARACHNEL_VERSION:-dev}"

export BUILD_DIR="${BUILD}"
export QT_QML_MATERIAL_IMPORT_PATH="${BUILD}/qml_modules"
export QML2_IMPORT_PATH="${BUILD}/qml_modules"

echo "==> Configure (Release)"
FETCH_DIR="${FETCHCONTENT_BASE_DIR:-${ROOT}/.cache/fetchcontent}"
mkdir -p "${FETCH_DIR}"

CMAKE_ARGS=(
  -S "${ROOT}"
  -B "${BUILD}"
  -G Ninja
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-${QT_ROOT_DIR:-}}"
  -DFETCHCONTENT_BASE_DIR="${FETCH_DIR}"
  -DARACHNEL_VERSION="${VERSION}"
  -DARACHNEL_FAST_BUILD=OFF
)
if [[ -n "${CMAKE_C_COMPILER_LAUNCHER:-}" ]]; then
  CMAKE_ARGS+=(-DCMAKE_C_COMPILER_LAUNCHER="${CMAKE_C_COMPILER_LAUNCHER}")
fi
if [[ -n "${CMAKE_CXX_COMPILER_LAUNCHER:-}" ]]; then
  CMAKE_ARGS+=(-DCMAKE_CXX_COMPILER_LAUNCHER="${CMAKE_CXX_COMPILER_LAUNCHER}")
fi

cmake "${CMAKE_ARGS[@]}"

echo "==> Build arachnel_app"
cmake --build "${BUILD}" --target arachnel_app -j"$(nproc)"

if [[ -x "${ROOT}/scripts/setup-material-fonts.sh" ]]; then
  bash "${ROOT}/scripts/setup-material-fonts.sh"
fi

APP_BIN="${BUILD}/arachnel_app"
if [[ ! -x "${APP_BIN}" ]]; then
  echo "arachnel_app not found at ${APP_BIN}" >&2
  exit 1
fi

rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin" "${DIST}"

cp "${APP_BIN}" "${APPDIR}/usr/bin/arachnel_app"
cp "${ROOT}/packaging/linux/arachnel.desktop" "${APPDIR}/arachnel.desktop"
cp "${ROOT}/resources/icons/png/256.png" "${APPDIR}/arachnel.png"

TOOLS="${DIST}/linuxdeploy"
mkdir -p "${TOOLS}"
LINUXDEPLOY="${TOOLS}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${TOOLS}/linuxdeploy-plugin-qt-x86_64.AppImage"

if [[ ! -x "${LINUXDEPLOY}" ]]; then
  curl -fsSL -o "${LINUXDEPLOY}" \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
  chmod +x "${LINUXDEPLOY}"
fi

if [[ ! -x "${LINUXDEPLOY_QT}" ]]; then
  curl -fsSL -o "${LINUXDEPLOY_QT}" \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
  chmod +x "${LINUXDEPLOY_QT}"
fi

export QML_SOURCES_PATHS="${ROOT}/qml:${BUILD}/qml_modules"
# Prefer Qt6 qmake — /usr/bin/qmake may be Qt5 and makes plugin-qt fail.
if [[ -z "${QMAKE:-}" ]]; then
  if command -v qmake6 >/dev/null 2>&1; then
    export QMAKE="$(command -v qmake6)"
  elif [[ -n "${QT_ROOT_DIR:-}" && -x "${QT_ROOT_DIR}/bin/qmake" ]]; then
    export QMAKE="${QT_ROOT_DIR}/bin/qmake"
  fi
fi
echo "==> Using QMAKE=${QMAKE:-<default>}"

install_apprun_hooks() {
  # linuxdeploy sources every script in apprun-hooks/ from its generated AppRun.
  # Never sed the AppRun binary — that corrupts ELF and causes SIGSEGV in ld.so.
  mkdir -p "${APPDIR}/apprun-hooks"
  cat >"${APPDIR}/apprun-hooks/90-arachnel-platform.sh" <<'EOF'
# Prefer xcb when a Wayland QPA plugin is not bundled / breaks on some hosts.
if [ -z "${QT_QPA_PLATFORM:-}" ]; then
  export QT_QPA_PLATFORM=xcb
fi
EOF
}

deploy_qml_modules() {
  local bin_dir="${APPDIR}/usr/bin"
  local lib_dir="${APPDIR}/usr/lib"
  local material_dir="${bin_dir}/qml_modules/Qcm/Material"

  mkdir -p "${bin_dir}" "${lib_dir}"

  if [[ -d "${BUILD}/qml_modules" ]]; then
    echo "==> Copy Qcm.Material (qml_modules)"
    rm -rf "${bin_dir}/qml_modules"
    cp -a "${BUILD}/qml_modules" "${bin_dir}/"
  else
    echo "ERROR: ${BUILD}/qml_modules not found — Qcm.Material will be missing" >&2
    exit 1
  fi

  local material_lib=""
  local search_roots=("${BUILD}")
  if [[ -n "${FETCHCONTENT_BASE_DIR:-}" ]]; then
    search_roots+=("${FETCHCONTENT_BASE_DIR}")
  fi
  search_roots+=("${ROOT}/.cache/fetchcontent")

  for root in "${search_roots[@]}"; do
    [[ -d "${root}" ]] || continue
    if [[ -f "${root}/_deps/qml_material-build/libqml_material.so" ]]; then
      material_lib="${root}/_deps/qml_material-build/libqml_material.so"
      break
    fi
    if [[ -f "${root}/qml_material-build/libqml_material.so" ]]; then
      material_lib="${root}/qml_material-build/libqml_material.so"
      break
    fi
  done
  if [[ -z "${material_lib}" ]]; then
    for root in "${search_roots[@]}"; do
      [[ -d "${root}" ]] || continue
      material_lib="$(find "${root}" -name 'libqml_material.so' -type f 2>/dev/null | head -n1 || true)"
      [[ -n "${material_lib}" ]] && break
    done
  fi
  if [[ -z "${material_lib}" || ! -f "${material_lib}" ]]; then
    echo "ERROR: libqml_material.so not found under ${search_roots[*]}" >&2
    exit 1
  fi
  echo "==> Found libqml_material.so at ${material_lib}"

  echo "==> Copy libqml_material.so → usr/lib and beside QML plugin"
  cp -a "${material_lib}" "${lib_dir}/"
  # Plugin RUNPATH in the build tree points at an absolute path; put a copy next
  # to the plugin and rewrite RPATH to $ORIGIN so dlopen works in the AppImage.
  cp -a "${material_lib}" "${material_dir}/"

  local plugin="${material_dir}/libqml_materialplugin.so"
  if [[ -f "${plugin}" ]]; then
    local patchelf_bin=""
    if command -v patchelf >/dev/null 2>&1; then
      patchelf_bin="$(command -v patchelf)"
    elif [[ -x /tmp/patchelf-bin/bin/patchelf ]]; then
      patchelf_bin=/tmp/patchelf-bin/bin/patchelf
    else
      echo "==> Fetch portable patchelf"
      mkdir -p /tmp/patchelf-bin
      curl -fsSL -o /tmp/patchelf-bin/patchelf.tar.gz \
        https://github.com/NixOS/patchelf/releases/download/0.18.0/patchelf-0.18.0-x86_64.tar.gz
      tar -xzf /tmp/patchelf-bin/patchelf.tar.gz -C /tmp/patchelf-bin
      patchelf_bin=/tmp/patchelf-bin/bin/patchelf
    fi
    echo "==> patchelf QML plugin RPATH → \$ORIGIN"
    "${patchelf_bin}" --set-rpath '$ORIGIN:$ORIGIN/../../../../lib' "${plugin}"
  fi

  # Belt-and-suspenders: help dlopen find material + Qt libs even if RPATH is wrong.
  mkdir -p "${APPDIR}/apprun-hooks"
  cat >"${APPDIR}/apprun-hooks/80-arachnel-libs.sh" <<'EOF'
# Ensure bundled Qcm.Material resolves when the QML plugin is dlopened.
# When sourced from AppRun, $0 is AppRun → AppDir root.
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$HERE/usr/bin/qml_modules/Qcm/Material${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QML2_IMPORT_PATH="$HERE/usr/qml:$HERE/usr/bin/qml_modules${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
EOF

  cat >"${bin_dir}/qt.conf" <<'EOF'
[Paths]
Prefix = ..
Plugins = plugins
# Qt built-in modules (QtQuick, Layouts, Shapes, …) live in usr/qml.
# App QML modules (Qcm.Material) are loaded via engine.addImportPath(…/qml_modules).
Qml2Imports = qml
Imports = qml
EOF
}

ensure_apprun_not_sed_patched() {
  local apprun="${APPDIR}/AppRun"
  [[ -f "${apprun}" ]] || return 0

  # If someone (or an old CI step) corrupted an ELF AppRun with a text insert,
  # refuse to package it.
  if file "${apprun}" | grep -q 'ELF'; then
    if grep -aq 'QT_QPA_PLATFORM' "${apprun}" 2>/dev/null; then
      echo "ERROR: AppRun ELF looks sed-patched (contains QT_QPA_PLATFORM text)." >&2
      echo "Use apprun-hooks instead of sed -i on AppRun." >&2
      exit 1
    fi
  fi
}

echo "==> Install AppRun hooks"
install_apprun_hooks

echo "==> linuxdeploy (Qt plugin)"
cd "${ROOT}"
# Explicit --plugin path; do not leave LINUXDEPLOY_PLUGIN_QT exported for the
# second pass — it would re-run qt deploy and may pick the wrong qmake.
"${LINUXDEPLOY}" --appdir "${APPDIR}" \
  --plugin qt \
  --executable "${APPDIR}/usr/bin/arachnel_app" \
  --desktop-file "${APPDIR}/arachnel.desktop" \
  --icon-file "${APPDIR}/arachnel.png"

deploy_qml_modules
ensure_apprun_not_sed_patched

echo "==> AppImage"
# Package only — Qt libs/QML already deployed above.
env -u LINUXDEPLOY_PLUGIN_QT \
  "${LINUXDEPLOY}" --appdir "${APPDIR}" --output appimage

APPIMAGE="$(find "${DIST}" -maxdepth 1 -name '*.AppImage' -type f | head -n1)"
if [[ -z "${APPIMAGE}" ]]; then
  APPIMAGE="$(find "${ROOT}" -maxdepth 1 -name '*.AppImage' -type f | head -n1)"
fi
if [[ -z "${APPIMAGE}" ]]; then
  echo "AppImage was not produced" >&2
  exit 1
fi

OUT="${DIST}/Arachnel-${VERSION}-x86_64.AppImage"
mv -f "${APPIMAGE}" "${OUT}"
chmod +x "${OUT}"

echo "==> Done: ${OUT}"
