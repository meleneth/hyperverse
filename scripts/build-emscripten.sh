#!/usr/bin/env sh
set -eu

BUILD_DIR=${BUILD_DIR:-build-emscripten}
BUILD_TYPE=${BUILD_TYPE:-Release}
GENERATOR=${GENERATOR:-Ninja}
TARGET=${TARGET:-hyperverse}
EMSCRIPTEN_DEBUG=${EMSCRIPTEN_DEBUG:-OFF}
SERVE=0

usage() {
  cat <<'EOF'
Usage: scripts/build-emscripten.sh [--serve]

Environment overrides:
  BUILD_DIR    CMake build directory. Default: build-emscripten
  BUILD_TYPE   CMake build type. Default: Release
  EMSCRIPTEN_DEBUG
               Enable Emscripten assertions, safe heap checks, and source maps. Default: OFF
  GENERATOR    CMake generator. Default: Ninja
  TARGET       CMake target. Default: hyperverse
  JOBS         Parallel build jobs passed to cmake --build.

The Emscripten SDK environment must already be active, for example:
  . /path/to/emsdk/emsdk_env.sh

For a containerized build without a host SDK, use:
  scripts/build-emscripten-container.sh
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

if ! command -v emcmake >/dev/null 2>&1; then
  cat >&2 <<'EOF'
emcmake was not found.

Activate the Emscripten SDK environment first, for example:
  . /path/to/emsdk/emsdk_env.sh
EOF
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake was not found on PATH." >&2
  exit 1
fi

emcmake cmake \
  -S . \
  -B "$BUILD_DIR" \
  -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DHYPERVERSE_EMSCRIPTEN_DEBUG="$EMSCRIPTEN_DEBUG" \
  -DHYPERVERSE_BUILD_TESTS=OFF

if [ -n "${JOBS:-}" ]; then
  cmake --build "$BUILD_DIR" --target "$TARGET" --parallel "$JOBS"
else
  cmake --build "$BUILD_DIR" --target "$TARGET"
fi

cat <<EOF
Emscripten build complete:
  $BUILD_DIR/$TARGET.html

Run locally with:
  emrun --no_browser --port 8000 $BUILD_DIR/$TARGET.html
or:
  python3 -m http.server 8000 --directory $BUILD_DIR
EOF

if [ "$SERVE" -eq 1 ]; then
  if command -v emrun >/dev/null 2>&1; then
    emrun --no_browser --port 8000 "$BUILD_DIR/$TARGET.html"
  elif command -v python3 >/dev/null 2>&1; then
    python3 -m http.server 8000 --directory "$BUILD_DIR"
  else
    echo "Neither emrun nor python3 was found; cannot serve $BUILD_DIR/$TARGET.html." >&2
    exit 1
  fi
fi
