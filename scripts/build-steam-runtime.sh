#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
IMAGE_NAME=${IMAGE_NAME:-hyperverse-steamrt-sniper}
IMAGE_TAG=${IMAGE_TAG:-local}
JOBS=${JOBS:-4}
BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_TESTS=${BUILD_TESTS:-ON}

if command -v docker >/dev/null 2>&1; then
  CONTAINER_ENGINE=docker
elif command -v podman >/dev/null 2>&1; then
  CONTAINER_ENGINE=podman
else
  echo "Docker or Podman is required to build the Steam Runtime bundle." >&2
  exit 1
fi

mkdir -p "$PROJECT_DIR/.cache/steam-runtime/home" "$PROJECT_DIR/.cache/steam-runtime/cpm"

"$CONTAINER_ENGINE" build \
  --network host \
  --tag "$IMAGE_NAME:$IMAGE_TAG" \
  --file "$PROJECT_DIR/packaging/steam-runtime/Dockerfile" \
  "$PROJECT_DIR"

"$CONTAINER_ENGINE" run --rm \
  --network host \
  --user "$(id -u):$(id -g)" \
  --env HOME=/container-home \
  --env CPM_SOURCE_CACHE=/cpm-cache \
  --env JOBS="$JOBS" \
  --env BUILD_TYPE="$BUILD_TYPE" \
  --env BUILD_TESTS="$BUILD_TESTS" \
  --volume "$PROJECT_DIR:/workspace" \
  --volume "$PROJECT_DIR/.cache/steam-runtime/home:/container-home" \
  --volume "$PROJECT_DIR/.cache/steam-runtime/cpm:/cpm-cache" \
  "$IMAGE_NAME:$IMAGE_TAG" \
  /workspace/scripts/build-steam-runtime-inner.sh
