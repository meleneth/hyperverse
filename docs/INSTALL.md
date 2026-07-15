# Installation and Bootstrap

Hyperverse is currently a C++23/CMake prototype bootstrap. Milestone 0 builds a `hyperverse`
executable that opens an SDL3 Vulkan window, initializes gamepad support, clears the window
through Vulkan, and provides a Catch2 test binary.

## Linux

Ubuntu 24.04 packages:

```sh
./scripts/install-dev-deps.sh
```

Equivalent manual package install:

```sh
sudo apt-get update
sudo apt-get install -y \
  ca-certificates \
  cmake \
  g++ \
  git \
  libgl1-mesa-dev \
  libwayland-dev \
  libvulkan-dev \
  libxkbcommon-dev \
  ninja-build \
  pkg-config \
  wayland-protocols \
  xorg-dev
```

Configure, build, and test:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DHYPERVERSE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Run:

```sh
./build/hyperverse
```

Install to a local prefix:

```sh
cmake --install build --prefix "$PWD/install"
./install/bin/hyperverse
```

## Steam Deck

Use the Linux instructions in a SteamOS development environment. The primary runtime
requirements are a Vulkan-capable driver and SDL3 gamepad support. The first bootstrap logs
detected gamepads to stdout so controller detection can be verified before gameplay input
mapping exists.

## MSYS2

From an MSYS2 MinGW shell such as MINGW64, UCRT64, CLANG64, or CLANGARM64:

```sh
./scripts/install-msys2-dev-deps.sh
```

Equivalent manual package install from a MINGW64 shell:

```sh
pacman -S --needed \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-pkgconf \
  mingw-w64-x86_64-vulkan-headers \
  mingw-w64-x86_64-vulkan-loader \
  git
```

Then configure, build, and test:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DHYPERVERSE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Dependency Pins

Dependencies are fetched through CPM during CMake configure:

- SDL `release-3.4.12`
- EnTT `v3.16.0`
- Jolt `v5.6.0`
- Boost.SML `v1.2.0`
- EventPP `v0.1.3`
- Catch2 `v3.8.1`

Project code is built with warnings as errors. Third-party dependency warnings are not promoted
to errors.
