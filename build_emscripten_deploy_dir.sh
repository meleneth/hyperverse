#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build-wasm}"
DIST_DIR="${DIST_DIR:-dist/hyperverse}"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

BUILD_DIR="$BUILD_DIR" ./scripts/build-emscripten-container.sh

find "$BUILD_DIR" -type f | grep -E 'hyperverse\.(html|js|wasm|worker\.js|data|symbols|wasm\.map|js\.map)$' || true

cp webserve/Dockerfile "$DIST_DIR/"
cp webserve/nginx.conf "$DIST_DIR/"
cp webserve/index.html "$DIST_DIR/"
cp webserve/app.js "$DIST_DIR/"

cp "$BUILD_DIR/hyperverse.js" "$DIST_DIR/"
cp "$BUILD_DIR/hyperverse.wasm" "$DIST_DIR/"
cp "$BUILD_DIR/"*.data "$DIST_DIR/" 2>/dev/null || true
cp "$BUILD_DIR/"*.worker.js "$DIST_DIR/" 2>/dev/null || true
cp "$BUILD_DIR/"*.map "$DIST_DIR/" 2>/dev/null || true

sha256sum "$DIST_DIR"/* >"$DIST_DIR/SHA256SUMS"

echo "Built deploy bundle:"
cat "$DIST_DIR/SHA256SUMS"
