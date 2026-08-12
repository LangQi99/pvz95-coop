# PvZ 95 Co-op

> **Work in progress:** a Windows/macOS, host-authoritative LAN co-op implementation of the community PvZ 95 ruleset. The engine and current tests build successfully; multiplayer gameplay is not release-ready yet.

[![Windows and macOS CI](https://github.com/LangQi99/pvz95-coop/actions/workflows/ci.yml/badge.svg)](https://github.com/LangQi99/pvz95-coop/actions/workflows/ci.yml)

PvZ 95 Co-op aims to let two to four people control one shared Plants vs. Zombies game. Each player gets a separately colored pointer; the host validates and orders game-affecting input so all machines remain in the same state.

## Project status

- [x] Windows x64 and macOS Apple Silicon CMake baseline
- [x] Runtime-selectable PvZ 95 ruleset and verified direct data changes
- [x] Versioned discovery, handshake, cursor, input, and state-hash packet codec
- [x] Non-blocking WinSock/BSD Socket UDP LAN discovery with loopback integration tests
- [ ] Restore the remaining injected PvZ 95 behavior hooks with regression tests
- [ ] Connect the implemented lobby/transport core to the host/join game UI
- [ ] Render colored remote pointers and dispatch host-authoritative input
- [ ] Add deterministic state hashing, resynchronization, reconnect, and soak tests
- [ ] Produce signed/notarized macOS and packaged Windows releases

See [architecture](docs/ARCHITECTURE.md) and [PvZ 95 research notes](docs/research/PVZ95_RULESET.md).

## Why this codebase

This project combines two complementary sources:

- [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) supplies the maintained SDL/OpenGL/CMake engine and the Windows/macOS platform layer.
- [PlantsVsZombies-decompilation](https://github.com/ruslan831/PlantsVsZombies-decompilation) is used only as a research reference to map PvZ 95 executable patches back to named game functions. Its Windows-only project, binaries, and game assets are not copied here.

Recovered behavior is implemented as maintainable C++ and tested against observed PvZ 95 outcomes. The project does not attempt to reproduce proprietary machine code byte-for-byte.

## Required game data

This repository does **not** contain PopCap/EA game assets or executables. You must supply compatible `main.pak` and `properties/` data from a copy of Plants vs. Zombies you are legally entitled to use.

Place the files next to the executable, or launch with an explicit resource directory:

```bash
./pvz95-coop -resdir /path/to/your/legal/game-data
```

Writable settings and saves use the separate product ID `io.github.langqi99.pvz95-coop`, so they do not overwrite an upstream PvZ-Portable profile. You can override that location with `-savedir <path>`.

## Build on macOS

Install dependencies with Homebrew:

```bash
brew install cmake ninja sdl2 libpng jpeg-turbo libogg libvorbis libopenmpt mpg123 dylibbundler
```

Configure, build, and test:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is `build/pvz95-coop`.

## Build on Windows

The CI-supported Windows build uses Visual Studio 2022, Ninja, CMake, and vcpkg manifest mode. In a Developer PowerShell with `VCPKG_ROOT` set:

```powershell
cmake -G Ninja -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCONSOLE=OFF `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is `build/pvz95-coop.exe`.

## Ruleset selection

PvZ 95 values are enabled by default. Original PvZ values remain available for A/B comparison:

```bash
./pvz95-coop -ruleset pvz95
./pvz95-coop -ruleset original
```

The ruleset protocol ID is part of the multiplayer handshake. Peers with incompatible gameplay rules are rejected before starting a session.

## Tests

The current native test suite covers:

- all directly verified plant and projectile table changes;
- round-trip encoding of every multiplayer packet type;
- malformed, oversized, mismatched-version, and invalid-field packets;
- real UDP discovery between a host and client over loopback;
- non-blocking TCP connection, framed two-way handshake traffic, and peer-close detection.

The protocol never sends native C++ struct layouts. It uses explicit fixed-width little-endian fields and validates packet length and enum/player ranges before use.

## Legal and attribution

PvZ 95 Co-op is an unofficial, non-commercial educational project. It is not affiliated with, authorized by, or endorsed by PopCap Games or Electronic Arts.

The source remains under [LGPL-3.0](LICENSE), following PvZ-Portable. No PopCap/EA resources are distributed. The PvZ 95 sample fingerprint is documented for reproducible research, but the sample itself is excluded from Git.
