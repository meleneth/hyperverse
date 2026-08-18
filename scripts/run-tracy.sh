#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR=${TRACY_BUILD_DIR:-"$PROJECT_DIR/build-tracy"}
BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo}
JOBS=${JOBS:-2}

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DHYPERVERSE_ENABLE_TRACY=ON \
  -DHYPERVERSE_BUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --parallel "$JOBS"

if [ "${1:-}" = "--build-only" ]; then
  exit 0
fi

echo "Starting Tracy-instrumented Hyperverse; connect the Tracy profiler to capture."
exec "$BUILD_DIR/hyperverse" "$@"
