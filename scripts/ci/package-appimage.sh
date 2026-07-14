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
cmake -S "${ROOT}" -B "${BUILD}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-${QT_ROOT_DIR:-}}" \
  -DARACHNEL_FAST_BUILD=OFF

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
export LINUXDEPLOY_PLUGIN_QT="${LINUXDEPLOY_QT}"

deploy_qml_modules() {
  local bin_dir="${APPDIR}/usr/bin"
  local lib_dir="${APPDIR}/usr/lib"
  local plugins_dir="${APPDIR}/usr/plugins"

  mkdir -p "${bin_dir}" "${lib_dir}" "${plugins_dir}"

  if [[ -d "${BUILD}/qml_modules" ]]; then
    echo "==> Copy Qcm.Material (qml_modules)"
    rm -rf "${bin_dir}/qml_modules"
    cp -a "${BUILD}/qml_modules" "${bin_dir}/"
  else
    echo "ERROR: ${BUILD}/qml_modules not found — Qcm.Material will be missing" >&2
    exit 1
  fi

  shopt -s nullglob
  for lib in "${BUILD}"/libqml_material.so*; do
    echo "==> Copy ${lib##*/}"
    cp -a "${lib}" "${lib_dir}/"
  done

  while IFS= read -r -d '' plugin; do
    echo "==> Copy QML plugin ${plugin##*/}"
    cp -a "${plugin}" "${plugins_dir}/"
  done < <(find "${BUILD}" -name 'libqml_materialplugin.so' -print0 2>/dev/null)

  cat >"${bin_dir}/qt.conf" <<'EOF'
[Paths]
Prefix = ..
Plugins = plugins
Qml2Imports = qml_modules:../qml
Imports = qml_modules
EOF
}

patch_apprun() {
  local apprun="${APPDIR}/AppRun"
  [[ -f "${apprun}" ]] || return 0
  if ! grep -q 'QT_QPA_PLATFORM' "${apprun}"; then
    echo "==> Patch AppRun (default to xcb when wayland plugin is absent)"
    sed -i '2i export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"' "${apprun}"
  fi
}

echo "==> linuxdeploy (Qt plugin)"
cd "${ROOT}"
"${LINUXDEPLOY}" --appdir "${APPDIR}" \
  --plugin qt \
  --executable "${APPDIR}/usr/bin/arachnel_app" \
  --desktop-file "${APPDIR}/arachnel.desktop" \
  --icon-file "${APPDIR}/arachnel.png"

deploy_qml_modules
patch_apprun

echo "==> AppImage"
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
