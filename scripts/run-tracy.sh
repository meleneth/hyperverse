#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR=${TRACY_BUILD_DIR:-"$PROJECT_DIR/build-tracy"}
PROFILER_BUILD_DIR="$BUILD_DIR/tracy-profiler"
BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo}
JOBS=${JOBS:-2}

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DHYPERVERSE_ENABLE_TRACY=ON \
  -DHYPERVERSE_BUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --parallel "$JOBS"

cmake -S "$BUILD_DIR/_deps/tracy-src/profiler" -B "$PROFILER_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLEGACY=ON
cmake --build "$PROFILER_BUILD_DIR" --parallel "$JOBS"

if [ "${1:-}" = "--build-only" ]; then
  exit 0
fi

game_pid=
profiler_pid=

stop_session() {
  trap - EXIT
  [ -z "$game_pid" ] || kill "$game_pid" 2>/dev/null || true
  [ -z "$profiler_pid" ] || kill "$profiler_pid" 2>/dev/null || true
  [ -z "$game_pid" ] || wait "$game_pid" 2>/dev/null || true
  [ -z "$profiler_pid" ] || wait "$profiler_pid" 2>/dev/null || true
}
trap stop_session EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo "Starting Tracy-instrumented Hyperverse and connecting the profiler."
(
  cd "$BUILD_DIR"
  exec ./hyperverse "$@"
) &
game_pid=$!

"$PROFILER_BUILD_DIR/tracy-profiler" -a 127.0.0.1 &
profiler_pid=$!

set +e
wait -n "$game_pid" "$profiler_pid"
session_status=$?
set -e
exit "$session_status"
