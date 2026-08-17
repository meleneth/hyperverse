# Installation and Bootstrap

Hyperverse is a C++23/CMake playable prototype. The native build produces a `hyperverse`
executable that opens an SDL3 Vulkan window, initializes gamepad support, renders through Vulkan,
loads checked-in sprite assets, and runs the current mining/escort slice. The test build also
produces a Catch2 binary discovered by CTest.

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
  glslang-tools \
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

The release path is a relocatable x86-64 game bundle built in Valve's Steam Linux Runtime 3
(`sniper`) SDK. You do not need Arch or a Steam Deck build machine, and the Deck does not need
development packages. On the Debian development machine, install Docker or Podman and run:

```sh
./scripts/build-steam-runtime.sh
```

The first run downloads the pinned Valve SDK image. It then builds a Release executable with GCC
14, runs the full test suite, and creates both `dist/hyperverse-steamrt/` and
`dist/hyperverse-steamrt.tar.gz`. The directory contains the game, assets, compiled shaders,
SDL3, a relocatable launcher, and a build manifest. System Vulkan and graphics drivers are
deliberately not bundled.

### First-time Deck access

In Steam Deck Desktop Mode, open Konsole and set a password for the `deck` account, start SSH, and
find the Deck's LAN address:

```sh
passwd
sudo systemctl enable --now sshd
hostname -I
```

The development machine and Deck must be reachable on the same network. Test access from the
development machine, replacing the example address:

```sh
ssh deck@192.168.1.50
```

The deployment helper automatically uses `~/.ssh/hyperverse_deck_ed25519` when that dedicated key
exists. Set `SSH_IDENTITY` to use a different private key.

Deploy the tested bundle with:

```sh
./scripts/deploy-steam-deck.sh deck@192.168.1.50
```

This uses `rsync` to update `/home/deck/Games/hyperverse` incrementally. It deletes stale files
inside that one remote game directory so the Deck exactly matches the tested bundle. Override the
destination only when needed:

```sh
REMOTE_DIR=/home/deck/Games/hyperverse/test ./scripts/deploy-steam-deck.sh deck@192.168.1.50
```

### Add to Steam

In the Desktop Mode Steam client, choose **Games > Add a Non-Steam Game**, browse to
`/home/deck/Games/hyperverse/run-hyperverse.sh`, and add it. In the shortcut properties set:

- **Target:** `/home/deck/Games/hyperverse/run-hyperverse.sh`
- **Start In:** `/home/deck/Games/hyperverse`
- **Compatibility:** leave Proton disabled; this is a native Linux build

Under the shortcut's compatibility/runtime selection, use **Steam Linux Runtime 3.0 (sniper)**,
matching `steam-runtime.txt` in the bundle. Return to Gaming Mode and launch the shortcut.

For emergency on-device diagnosis only, `./scripts/steam-deck.sh --build-only` can still perform a
native source build on a configured Deck. It is not the release or routine deployment path.

The current input layer maps Steam Deck controls through SDL3 into semantic gameplay intent.

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
  mingw-w64-x86_64-glslang \
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

MSYS2/MinGW builds use Vulkan directly; SDL3 creates the Win32 Vulkan surface. The build stages runtime DLLs next to
`hyperverse.exe`, including `vulkan-1.dll` when the MSYS2 Vulkan loader package has put it
on `PATH`.

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

Hyperverse links directly against Vulkan 1.2. SDL3 supplies the platform-specific instance
extensions and creates the Vulkan surface on X11, Wayland, and Win32. GLSL shaders are compiled to
SPIR-V by `glslangValidator` during the build.
