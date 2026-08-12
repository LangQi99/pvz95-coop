# Architecture

## Goals

PvZ 95 Co-op must preserve the observable PvZ 95 gameplay, allow two to four people on one LAN to control the same game, and produce native Windows and macOS builds from one C++ codebase. Windows x64 and macOS Apple Silicon are release-blocking targets; Intel macOS remains source-compatible where the upstream engine supports it. The repository must remain buildable without proprietary game data.

## Source strategy

The shipping engine is based on PvZ-Portable. It already supplies portable rendering, audio, input, storage, CMake builds, and deterministic recording support.

The `ruslan831/PlantsVsZombies-decompilation` repository is a research oracle, not a vendored dependency. Addresses modified by the PvZ 95 executable are mapped to named functions there, then translated into maintainable C++ in this repository. This keeps platform-specific DirectX 8 code, bundled binaries, and proprietary assets outside the project.

Every restored behavior should have four pieces of evidence:

1. PvZ 95 sample address or data-table offset.
2. Corresponding named function in the reference decompilation.
3. Functional C++ implementation in the ruleset layer or engine.
4. A focused test, replay, or state comparison.

## Runtime layers

```text
Game UI and simulation
        |
PvZ 95 ruleset/behavior layer
        |
Host-authoritative session controller
   | reliable commands | lossy cursors
Versioned wire protocol
        |
LAN discovery + reliable game transport
        |
SDL/CMake cross-platform engine
```

`GameRules` owns balance values and restored PvZ 95 behavior. Original values remain available through `-ruleset original`, both for comparison and for regression tests. A ruleset protocol identifier is exchanged during connection setup so incompatible peers cannot silently start a session.

`Multiplayer` is split into a transport-independent protocol and the eventual socket/session code. Packet encoding uses explicit fixed-width little-endian fields and strict length checks; no native struct layout is sent over the network.

LAN discovery uses non-blocking IPv4 UDP on port `43095` by default. A client broadcasts a versioned query; a host replies directly with its session ID, player count, ruleset ID, and reliable game port. Discovery is kept separate from the reliable gameplay connection so cursor/input traffic never depends on broadcast delivery.

## Session model

The host is authoritative. All local and remote input becomes an `InputCommand`; the host validates and orders commands at a simulation tick, then broadcasts the accepted order. This prevents two clients from independently spending the same sun or acting on different UI states.

Cursor movement is presentation state. It can be sent at 20–30 Hz, may be dropped, and is interpolated by recipients. Clicks, key events, pause requests, and game-affecting actions are reliable and ordered. Each player receives a stable ID and cursor color for the lifetime of the session.

The initial implementation targets up to four players on a trusted LAN. Network input is still treated as untrusted: packet sizes, enum ranges, player IDs, sequence numbers, and current UI state must be validated by the host.

## Consistency and recovery

Clients run the same deterministic simulation after receiving the host's random seed and ordered command stream. The host periodically broadcasts a canonical state hash. A mismatch pauses command application and requests a host snapshot; reconnect follows the same snapshot path.

The canonical hash must exclude pointers, wall-clock time, audio state, renderer state, and native padding. It should include the board tick, random-generator state, sun, selected seeds, plants, zombies, projectiles, coins, mowers, and mode-specific challenge state in stable ID order.

## Compatibility policy

The packet protocol and ruleset protocol are separate version axes. A codec change increments `PROTOCOL_VERSION`; a behavior change that affects determinism changes the ruleset protocol ID. Unknown versions are rejected before gameplay starts.

Game assets and save files remain local. The network does not transfer `main.pak`, profiles, or copyrighted resources.
