#!/usr/bin/env sh
set -eu

EMSDK_IMAGE=${EMSDK_IMAGE:-emscripten/emsdk:latest}
BUILD_DIR=${BUILD_DIR:-build-emscripten}
BUILD_TYPE=${BUILD_TYPE:-Release}
EMSCRIPTEN_DEBUG=${EMSCRIPTEN_DEBUG:-OFF}
GENERATOR=${GENERATOR:-Unix Makefiles}
TARGET=${TARGET:-hyperverse}
PORT=${PORT:-8000}
SERVE=0

usage() {
  cat <<'EOF'
Usage: scripts/build-emscripten-container.sh [--serve]

Environment overrides:
  EMSDK_IMAGE  Container image. Default: emscripten/emsdk:latest
  BUILD_DIR    CMake build directory. Default: build-emscripten
  BUILD_TYPE   CMake build type. Default: Release
  EMSCRIPTEN_DEBUG
               Enable Emscripten assertions, safe heap checks, and source maps. Default: OFF
  GENERATOR    CMake generator. Default: Unix Makefiles
  TARGET       CMake target. Default: hyperverse
  JOBS         Parallel build jobs passed to cmake --build.
  PORT         Host/container port used with --serve. Default: 8000
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --serve)
      SERVE=1
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if command -v docker >/dev/null 2>&1; then
  CONTAINER=docker
elif command -v podman >/dev/null 2>&1; then
  CONTAINER=podman
else
  cat >&2 <<'EOF'
Neither docker nor podman was found.

Install one of them, or use scripts/build-emscripten.sh from an activated
Emscripten SDK environment.
EOF
  exit 1
fi

if [ "$SERVE" -eq 1 ]; then
  if [ -n "${JOBS:-}" ]; then
    exec "$CONTAINER" run --rm \
      --user "$(id -u):$(id -g)" \
      -e HOME=/tmp \
      -e "EM_CACHE=/work/${BUILD_DIR}/.em-cache" \
      -e "BUILD_DIR=${BUILD_DIR}" \
      -e "BUILD_TYPE=${BUILD_TYPE}" \
      -e "EMSCRIPTEN_DEBUG=${EMSCRIPTEN_DEBUG}" \
      -e "GENERATOR=${GENERATOR}" \
      -e "TARGET=${TARGET}" \
      -e "PORT=${PORT}" \
      -e "JOBS=${JOBS}" \
      -p "${PORT}:${PORT}" \
      -v "${PWD}:/work" \
      -w /work \
      "$EMSDK_IMAGE" sh -lc 'scripts/build-emscripten.sh && python3 -m http.server "$PORT" --directory "$BUILD_DIR"'
  fi

  exec "$CONTAINER" run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e "EM_CACHE=/work/${BUILD_DIR}/.em-cache" \
    -e "BUILD_DIR=${BUILD_DIR}" \
    -e "BUILD_TYPE=${BUILD_TYPE}" \
    -e "EMSCRIPTEN_DEBUG=${EMSCRIPTEN_DEBUG}" \
    -e "GENERATOR=${GENERATOR}" \
    -e "TARGET=${TARGET}" \
    -e "PORT=${PORT}" \
    -p "${PORT}:${PORT}" \
    -v "${PWD}:/work" \
    -w /work \
    "$EMSDK_IMAGE" sh -lc 'scripts/build-emscripten.sh && python3 -m http.server "$PORT" --directory "$BUILD_DIR"'
fi

if [ -n "${JOBS:-}" ]; then
  exec "$CONTAINER" run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e "EM_CACHE=/work/${BUILD_DIR}/.em-cache" \
    -e "BUILD_DIR=${BUILD_DIR}" \
    -e "BUILD_TYPE=${BUILD_TYPE}" \
    -e "EMSCRIPTEN_DEBUG=${EMSCRIPTEN_DEBUG}" \
    -e "GENERATOR=${GENERATOR}" \
    -e "TARGET=${TARGET}" \
    -e "PORT=${PORT}" \
    -e "JOBS=${JOBS}" \
    -v "${PWD}:/work" \
    -w /work \
    "$EMSDK_IMAGE" sh -lc 'scripts/build-emscripten.sh'
fi

exec "$CONTAINER" run --rm \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -e "EM_CACHE=/work/${BUILD_DIR}/.em-cache" \
  -e "BUILD_DIR=${BUILD_DIR}" \
  -e "BUILD_TYPE=${BUILD_TYPE}" \
  -e "EMSCRIPTEN_DEBUG=${EMSCRIPTEN_DEBUG}" \
  -e "GENERATOR=${GENERATOR}" \
  -e "TARGET=${TARGET}" \
  -e "PORT=${PORT}" \
  -v "${PWD}:/work" \
  -w /work \
  "$EMSDK_IMAGE" sh -lc 'scripts/build-emscripten.sh'
