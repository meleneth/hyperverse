#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build-steam-deck"}
BUILD_TYPE=${BUILD_TYPE:-Release}
GENERATOR=${GENERATOR:-Ninja}
JOBS=${JOBS:-2}
BUILD_ONLY=0

usage() {
  cat <<'EOF'
Usage: ./scripts/steam-deck.sh [--build-only] [--debug]

Configures and incrementally builds the native Linux game, then launches it.
The script may be run from any working directory, including as a Steam shortcut.

Options:
  --build-only  Build without launching the game.
  --debug       Build with debug information and assertions.
  -h, --help    Show this help.

Environment overrides:
  BUILD_DIR     CMake build directory. Default: build-steam-deck
  BUILD_TYPE    CMake build type. Default: Release
  GENERATOR     CMake generator. Default: Ninja
  JOBS          Parallel build jobs passed to CMake. Default: 2.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --build-only)
      BUILD_ONLY=1
      ;;
    --debug)
      BUILD_TYPE=Debug
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

if [ "$(uname -s)" != Linux ]; then
  echo "The Steam Deck build requires Linux." >&2
  exit 1
fi

for command_name in cmake ninja; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "$command_name was not found. Install the Linux development dependencies first:" >&2
    echo "  ./scripts/install-dev-deps.sh" >&2
    exit 1
  fi
done

cmake \
  -S "$PROJECT_DIR" \
  -B "$BUILD_DIR" \
  -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DHYPERVERSE_BUILD_TESTS=OFF

# Keep the default conservative for the Steam Deck's 16 GiB shared memory;
# users with more headroom can opt into additional parallelism with JOBS.
cmake --build "$BUILD_DIR" --target hyperverse --parallel "$JOBS"

if [ "$BUILD_ONLY" -eq 1 ]; then
  echo "Steam Deck build ready: $BUILD_DIR/hyperverse"
  exit 0
fi

# Runtime assets are staged beside the executable and are currently loaded
# relative to the process working directory.
cd "$BUILD_DIR"
exec ./hyperverse
