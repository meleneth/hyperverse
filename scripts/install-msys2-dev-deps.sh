#!/usr/bin/env sh
set -eu

if ! command -v pacman >/dev/null 2>&1; then
  cat >&2 <<'EOF'
This script must be run from an MSYS2 shell with pacman available.
EOF
  exit 1
fi

if [ -z "${MSYSTEM:-}" ] || [ -z "${MINGW_PACKAGE_PREFIX:-}" ]; then
  cat >&2 <<'EOF'
Run this script from an MSYS2 MinGW shell, such as MINGW64, UCRT64, CLANG64,
or CLANGARM64. The plain MSYS shell does not provide the right compiler/runtime
environment for this project.
EOF
  exit 1
fi

case "$MSYSTEM" in
  MINGW32)
    cat >&2 <<'EOF'
32-bit MSYS2 builds are not a supported Hyperverse development target.
Use MINGW64, UCRT64, CLANG64, or CLANGARM64 instead.
EOF
    exit 1
    ;;
  CLANG64 | CLANGARM64)
    COMPILER_PACKAGES="
      ${MINGW_PACKAGE_PREFIX}-clang
      ${MINGW_PACKAGE_PREFIX}-lld
    "
    ;;
  MINGW64 | UCRT64)
    COMPILER_PACKAGES="
      ${MINGW_PACKAGE_PREFIX}-gcc
    "
    ;;
  *)
    cat >&2 <<EOF
Unsupported MSYS2 environment: $MSYSTEM

Use MINGW64, UCRT64, CLANG64, or CLANGARM64.
EOF
    exit 1
    ;;
esac

pacman -Syu --needed \
  git \
  ${MINGW_PACKAGE_PREFIX}-cmake \
  ${MINGW_PACKAGE_PREFIX}-ninja \
  ${MINGW_PACKAGE_PREFIX}-pkgconf \
  ${MINGW_PACKAGE_PREFIX}-vulkan-headers \
  ${MINGW_PACKAGE_PREFIX}-vulkan-loader \
  $COMPILER_PACKAGES

cat <<EOF
MSYS2 development dependencies installed for $MSYSTEM.

Next commands:
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DHYPERVERSE_BUILD_TESTS=ON
  cmake --build build
  ctest --test-dir build --output-on-failure
EOF
