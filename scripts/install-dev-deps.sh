#!/usr/bin/env sh
set -eu

if command -v sudo >/dev/null 2>&1 && [ "$(id -u)" -ne 0 ]; then
  SUDO=sudo
else
  SUDO=
fi

install_apt() {
  $SUDO apt-get update
  $SUDO apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    g++ \
    git \
    libgl1-mesa-dev \
    libpng-dev \
    libx11-xcb-dev \
    libwayland-dev \
    libvulkan-dev \
    libxkbcommon-dev \
    make \
    ninja-build \
    pkg-config \
    wayland-protocols \
    xorg-dev
}

install_dnf() {
  $SUDO dnf install -y \
    cmake \
    gcc-c++ \
    git \
    libX11-devel \
    libXcursor-devel \
    libXi-devel \
    libXinerama-devel \
    libXrandr-devel \
    libXScrnSaver-devel \
    libXext-devel \
    libpng-devel \
    libdrm-devel \
    libglvnd-devel \
    libxkbcommon-devel \
    make \
    mesa-libGL-devel \
    ninja-build \
    pkgconf-pkg-config \
    vulkan-headers \
    vulkan-loader-devel \
    wayland-devel \
    wayland-protocols-devel
}

install_pacman() {
  $SUDO pacman -Syu --needed \
    base-devel \
    cmake \
    git \
    libx11 \
    libxcursor \
    libxext \
    libxi \
    libxinerama \
    libpng \
    libxrandr \
    libxss \
    libxkbcommon \
    mesa \
    ninja \
    pkgconf \
    vulkan-headers \
    vulkan-icd-loader \
    wayland \
    wayland-protocols
}

install_zypper() {
  $SUDO zypper install -y \
    Mesa-libGL-devel \
    cmake \
    gcc-c++ \
    git \
    libX11-devel \
    libXcursor-devel \
    libXext-devel \
    libXi-devel \
    libXinerama-devel \
    libXrandr-devel \
    libXScrnSaver-devel \
    libxkbcommon-devel \
    libpng16-devel \
    make \
    ninja \
    pkgconf-pkg-config \
    vulkan-devel \
    wayland-devel \
    wayland-protocols-devel
}

if command -v apt-get >/dev/null 2>&1; then
  install_apt
elif command -v dnf >/dev/null 2>&1; then
  install_dnf
elif command -v pacman >/dev/null 2>&1; then
  install_pacman
elif command -v zypper >/dev/null 2>&1; then
  install_zypper
else
  cat >&2 <<'EOF'
Unsupported package manager.

Install these tool categories manually:
- CMake 3.28+
- C++23 compiler
- Git
- Make or Ninja
- pkg-config
- Vulkan headers and loader development package
- libpng development package
- OpenGL/Mesa development package
- X11, X11-XCB, and Wayland development packages used by SDL3 and Dawn
EOF
  exit 1
fi

cat <<'EOF'
Development dependencies installed.

Next commands:
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DHYPERVERSE_BUILD_TESTS=ON
  cmake --build build
  ctest --test-dir build --output-on-failure
EOF
