#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${ROOT}/build"
APP="${BUILD}/arachnel_app"
export QT_QML_MATERIAL_IMPORT_PATH="${BUILD}/qml_modules"

ensure_material_fonts() {
  if [[ -x "${ROOT}/scripts/setup-material-fonts.sh" ]]; then
    bash "${ROOT}/scripts/setup-material-fonts.sh"
  fi
}

needs_configure() {
  local cache="${BUILD}/CMakeCache.txt"
  [[ ! -f "${cache}" ]] && return 0
  for f in "${ROOT}/CMakeLists.txt" "${ROOT}"/cmake/*.cmake; do
    [[ "${f}" -nt "${cache}" ]] && return 0
  done
  return 1
}

ensure_dev_build() {
  ensure_material_fonts
  if needs_configure; then
    echo "CMake configure ..."
    cmake -S "${ROOT}" -B "${BUILD}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Debug}" \
      -DCMAKE_CXX_COMPILER=g++ \
      -DARACHNEL_FAST_BUILD="${ARACHNEL_FAST_BUILD:-ON}"
  fi
  echo "Build ..."
  cmake --build "${BUILD}" --target arachnel_app -j"$(nproc)"
}

RUN_ONLY=0
case "${1:-}" in
  -r|--rebuild)
    shift
    rm -rf "${BUILD}"
    ;;
  --run)
    shift
    RUN_ONLY=1
    ;;
  -h|--help)
    cat <<'EOF'
Arachnel dev launcher

  ./run.sh              configure (if needed) + build + run
  ./run.sh --rebuild    clean build/, then build + run
  ./run.sh --run        run without build

Env: BUILD_TYPE, ARACHNEL_FAST_BUILD=0 for release QML cache
EOF
    exit 0
    ;;
esac

if [[ "${RUN_ONLY}" -eq 0 ]]; then
  ensure_dev_build
fi

if [[ ! -x "${APP}" ]]; then
  echo "arachnel_app not found. Run without --run first." >&2
  exit 1
fi

exec "${APP}" "$@"
