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
   | reliable actions | throttled cursor presentation
Versioned wire protocol
        |
LAN discovery + reliable game transport
        |
SDL/CMake cross-platform engine
```

`GameRules` owns balance values and restored PvZ 95 behavior. Original values remain available through `-ruleset original`, both for comparison and for regression tests. A ruleset protocol identifier is exchanged during connection setup so incompatible peers cannot silently start a session.

`Multiplayer` is split into a transport-independent protocol and the eventual socket/session code. Packet encoding uses explicit fixed-width little-endian fields and strict length checks; no native struct layout is sent over the network.

LAN discovery uses non-blocking IPv4 UDP on port `43095` by default. A client broadcasts a versioned query; a host replies directly with its session ID, player count, ruleset ID, and reliable game port. Discovery is kept separate from the reliable gameplay connection so cursor/action traffic never depends on broadcast delivery.

The reliable path is a non-blocking TCP channel with bounded outgoing queues and incremental frame decoding. `HostSession` accepts and validates `Hello`, binds each connection to the assigned player ID, rejects stale cursor/action sequences, and exposes validated messages as events. `ClientSession` validates the selected room and ruleset against `Welcome`, stamps outbound messages with its assigned player ID, and accepts only gameplay messages after the handshake.

`LanCoordinator` owns discovery and reliable-session lifetimes. The main-menu Host LAN and Join Room controls call it directly; the regular application update loop polls it without blocking rendering or input. A host continually refreshes its discovery offer as players join, while a client discovers a compatible non-full room and advances through search, handshake, and connected states.

The resource loader supports both native compiled definitions and the original retail 1.0 32-bit cache layout used by the analyzed PvZ 95 data. Legacy cache records are decoded field-by-field into native objects instead of reinterpreting pointer-sized structures. Chinese GBK text is normalized to UTF-8 before it enters the platform-independent UI.

## Session model

The host is authoritative. It sends a versioned `SessionStart` containing the game mode, simulation seed, and a gameplay-only copy of the host profile. Clients initialize an in-memory profile shadow, construct the same board, and answer with `SessionReady`; no machine advances the simulation until every current lobby member is ready and the host broadcasts `SessionBegin`. The shadow is never written over the guest's local profile.

Local pointer input is interpreted on the originating machine and reduced to semantic `GameAction` values: plant a seed-bank packet in a grid cell, collect a coin by deterministic object ID, shovel a plant by ID, or fire a cob cannon at a normalized target. Raw mouse presses and releases never enter the protocol. The host validates each action, assigns it a future simulation tick, preserves a single arrival order for actions sharing a tick, then broadcasts the accepted action. All peers apply it immediately before updating that tick. Clients are paced by the host's `TickSync` stream and can run a small bounded catch-up burst without allowing wall-clock timing to enter the simulation.

Cursor movement is presentation state. Each update also carries the selected seed-bank slot, allowing every machine to draw independent plant-in-hand and translucent grid previews without modifying deterministic board state. `SessionStart` carries the host-validated player-name snapshot, so each remote pointer can show a UTF-8 name without repeating identity data in the 25 Hz cursor stream. The plant art is left untinted; ownership is conveyed by the colored pointer that follows it. Cursor positions are interpolated over a short presentation-only window, include a periodic keepalive, and are discarded when their per-player sequence is stale. Presentation data shares the bounded TCP channel with actions but remains excluded from state hashes.

The initial implementation targets up to four players on a trusted LAN. Network input is still treated as untrusted: packet sizes, enum ranges, player IDs, sequence numbers, and current UI state must be validated by the host.

## Consistency and recovery

Clients run the same deterministic simulation after receiving the host's random seed and ordered action stream. The host periodically broadcasts a canonical state hash. A missing or mismatched hash currently freezes the client to prevent silent divergence. Snapshot transfer, automatic resynchronization, and reconnect are still pending; they must reuse one bounded, validated snapshot format.

The canonical hash explicitly serializes fixed-width fields and excludes pointers, wall-clock time, audio state, renderer state, cursor presentation, and native padding. Its first schema includes the board tick and counters, random-generator state, sun, seed packets, plants, zombies, projectiles, coins, mowers, grid items, and mode-specific challenge state in stable container order.

## Compatibility policy

The packet protocol and ruleset protocol are separate version axes. A codec change increments `PROTOCOL_VERSION`; a behavior change that affects determinism changes the ruleset protocol ID. Unknown versions are rejected before gameplay starts.

Game assets and save files remain local. The network does not transfer `main.pak`, profiles, or copyrighted resources.
