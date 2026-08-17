#!/bin/sh
set -eu

BUNDLE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$BUNDLE_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd "$BUNDLE_DIR"
exec "$BUNDLE_DIR/hyperverse" "$@"
