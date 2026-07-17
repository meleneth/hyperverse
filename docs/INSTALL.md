# Installation and Bootstrap

Hyperverse is currently a C++23/CMake prototype bootstrap. Milestone 0 builds a `hyperverse`
executable that opens an SDL3 window, initializes gamepad support, renders through Dawn/WebGPU,
and provides a Catch2 test binary.

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
  libpng-dev \
  libx11-xcb-dev \
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
requirements are a Vulkan-capable driver for Dawn's Linux backend and SDL3 gamepad support. The first bootstrap logs
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
  mingw-w64-x86_64-libpng \
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

## Emscripten

Dawn is wired to use `emdawnwebgpu_cpp` through Dawn's `webgpu_cpp` target when configured with
the Emscripten toolchain. Use an activated Emscripten SDK environment:

```sh
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release -DHYPERVERSE_BUILD_TESTS=OFF
cmake --build build-wasm --target hyperverse
```

## Dependency Pins

Dependencies are fetched through CPM during CMake configure:

- Dawn `2a49ad12f8a9bf7451cb71983647d662fbe70224`
- SDL `release-3.4.12`
- EnTT `v3.16.0`
- Jolt `v5.6.0`
- Boost.SML `v1.2.0`
- EventPP `v0.1.3`
- Catch2 `v3.8.1`

Project code is built with warnings as errors. Third-party dependency warnings are not promoted
to errors.

Hyperverse code no longer links directly against Vulkan. The current native Dawn integration
supports X11 and Wayland surfaces on Linux, Win32 surfaces on Windows, and uses Dawn's generated
emdawn WebGPU bindings under Emscripten. Linux native runtime still relies on Dawn's Vulkan backend.
