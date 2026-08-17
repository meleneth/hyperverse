#!/bin/sh
set -eu

PROJECT_DIR=${PROJECT_DIR:-/workspace}
BUILD_DIR=${BUILD_DIR:-$PROJECT_DIR/build-steam-runtime}
DIST_ROOT=${DIST_ROOT:-$PROJECT_DIR/dist}
BUNDLE_DIR=$DIST_ROOT/hyperverse-steamrt
BUILD_TYPE=${BUILD_TYPE:-Release}
JOBS=${JOBS:-4}
BUILD_TESTS=${BUILD_TESTS:-ON}

mkdir -p "${HOME:-/tmp/hyperverse-home}" "$BUILD_DIR" "$DIST_ROOT"

cmake \
  -S "$PROJECT_DIR" \
  -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DHYPERVERSE_BUILD_TESTS="$BUILD_TESTS" \
  -DHYPERVERSE_STEAM_BUNDLE=ON

cmake --build "$BUILD_DIR" --parallel "$JOBS"

if [ "$BUILD_TESTS" = ON ]; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

cmake -E rm -rf "$BUNDLE_DIR"
cmake --install "$BUILD_DIR" --prefix "$BUNDLE_DIR" --component HyperverseRuntime

SDL_LIBRARY=$(find "$BUNDLE_DIR/lib" -maxdepth 1 -type f -name 'libSDL3.so.*' -print -quit)
if [ -z "$SDL_LIBRARY" ]; then
  echo "Steam Runtime bundle does not contain the SDL3 shared library." >&2
  exit 1
fi
SDL_SONAME=$(readelf -d "$SDL_LIBRARY" | sed -n 's/.*SONAME.*\[\(.*\)\].*/\1/p')
if [ -z "$SDL_SONAME" ]; then
  echo "Bundled SDL3 library does not declare an ELF SONAME." >&2
  exit 1
fi
ln -sfn "$(basename "$SDL_LIBRARY")" "$BUNDLE_DIR/lib/$SDL_SONAME"

if [ ! -x "$BUNDLE_DIR/hyperverse" ] || [ ! -x "$BUNDLE_DIR/run-hyperverse.sh" ]; then
  echo "Steam Runtime bundle is missing its executable or launcher." >&2
  exit 1
fi

if ! find "$BUNDLE_DIR/lib" -maxdepth 1 -name 'libSDL3.so.0*' -print -quit | grep -q .; then
  echo "Steam Runtime bundle does not contain SDL3." >&2
  exit 1
fi

if ldd "$BUNDLE_DIR/hyperverse" | grep -q 'not found'; then
  echo "Steam Runtime bundle has unresolved shared-library dependencies:" >&2
  ldd "$BUNDLE_DIR/hyperverse" >&2
  exit 1
fi

{
  printf 'Built with:\n'
  "${CXX:-c++}" --version | sed -n '1p'
  cmake --version | sed -n '1p'
  printf '\nExecutable dependencies:\n'
  ldd "$BUNDLE_DIR/hyperverse"
  printf '\nELF dynamic section:\n'
  readelf -d "$BUNDLE_DIR/hyperverse"
} > "$BUNDLE_DIR/build-manifest.txt"

cmake -E chdir "$DIST_ROOT" cmake -E tar czf hyperverse-steamrt.tar.gz --format=gnutar hyperverse-steamrt

printf 'Steam Runtime bundle ready:\n  %s\n  %s\n' \
  "$BUNDLE_DIR" \
  "$DIST_ROOT/hyperverse-steamrt.tar.gz"
