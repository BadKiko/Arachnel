#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${ROOT}/build"
APP="${BUILD}/arachnel_app"
export QT_QML_MATERIAL_IMPORT_PATH="${BUILD}/qml_modules"

build() {
  cmake -S "${ROOT}" -B "${BUILD}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Debug}"
  cmake --build "${BUILD}" -j"$(nproc)"
}

case "${1:-}" in
  -b|--build)
    shift
    build
    ;;
  -r|--rebuild)
    shift
    rm -rf "${BUILD}"
    build
    ;;
  -h|--help)
    cat <<'EOF'
Usage: ./run.sh [options] [app args...]

  -b, --build     Собрать перед запуском
  -r, --rebuild   Чистая пересборка
  -h, --help      Справка
EOF
    exit 0
    ;;
esac

if [[ ! -x "${APP}" ]]; then
  build
fi

exec "${APP}" "$@"
