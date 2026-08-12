# LAN deterministic-input audit

## Current coverage

The LAN protocol currently schedules four host-ordered semantic actions:

| Action | Covered gameplay |
|---|---|
| `PLANT_SEED` | Normal seed-bank planting, conveyor packets, Wall-nut Bowling and the existing I, Zombie placement branch |
| `COLLECT_COIN` | Sun, coins, diamonds and most ordinary collectible prizes, addressed by stable `CoinID` |
| `SHOVEL_PLANT` | Shovelling by stable `PlantID` |
| `FIRE_COB_CANNON` | Cob Cannon targeting by stable `PlantID` and board coordinates |

Lawn mowers need no input action because they are advanced by the deterministic board simulation. Host receive order provides a stable order when two valid actions target the same simulation tick. These covered paths still need real two-board contention tests.

## State-hash delivery invariant

State-hash delivery used to assume that a `StateHash` was available as soon as the corresponding `TickSync` let the client advance. TCP preserves byte order, but the two messages can be returned by different poll calls. A client could therefore report a false desync at tick 100 even when both simulations agreed.

`StateHashTimeline` replaces that assumption with a bounded two-sided timeline. Its protocol invariants are:

- A local hash and a host hash are keyed by the same simulation tick and stored independently.
- A tick is compared only after both sides have been observed; local-first, host-first and interleaved logical ticks are valid.
- A matching pair is removed immediately and cannot consume pending capacity.
- A real mismatch, a conflicting same-side duplicate, exhausted capacity, or a missing side beyond the 300-tick grace window is a desync.
- An observation that is already older than the grace window expires immediately instead of occupying a slot until another message arrives.
- `ResetLanGameState` clears the timeline so observations cannot pair across session IDs.

The integration points are `LawnApp::UpdateLanSession`, which records host hashes, and `LawnApp::PublishOrVerifyLanStateHash`, which records the local hash after that logical frame. `StateHashTimelineTests` covers split poll delivery, both arrival orders, interleaved ticks, the inclusive grace boundary, stale first arrival, mismatch, duplicates, expiry, capacity and session reset.

## P0 gaps for ordinary Adventure play

### Seed chooser and loadout

Seed selection, deselection, Imitater selection, Random and the Start button mutate only the local `SeedChooserScreen`. LAN mouse input is currently consumed outside `SCENE_PLAYING`, while `SessionStart` carries no canonical loadout.

Evidence: `LawnApp::LocalMouseButton`, `SeedChooserScreen::MouseDown`, `SeedChooserScreen::ButtonDepress`, `SeedChooserScreen::CloseSeedChooser`, and `Multiplayer/Protocol.h::SessionStart`.

Recommended protocol:

- Host-authoritative `LOADOUT_SET` containing the ordered seeds, Imitater type and survival stage.
- `LOADOUT_COMMIT` followed by the existing ready/begin barrier.
- Random is evaluated once by the host and broadcast as the resulting ordered loadout.

Required tests: Adventure seed chooser, survival repick, Imitater and Random; both peers must have identical seed-packet arrays and RNG state.

### Intro, Crazy Dave and tutorial progression

Mouse clicks that normally reach `CutScene::MouseDown` are consumed by the LAN input layer before play starts, while keyboard input can still advance a cutscene on only one peer. Some Crazy Dave branches also populate challenge state or place a rake.

Evidence: `LawnApp::LocalMouseButton`, `Board::MouseDown`, `Board::KeyDown`, `CutScene::MouseDown`, `CutScene::KeyDown`, and `Challenge::AdvanceCrazyDaveDialog`.

Recommended protocol: host-ordered `ADVANCE_CUTSCENE` and `ADVANCE_DAVE_DIALOG` actions, with a barrier before entering `SCENE_PLAYING`.

Required tests: first Adventure intro, multi-page Dave dialogue, tutorials, and Scary Potter dialogue paths 2702/2801.

### Session lifecycle

Main Menu, restart/retry, game-over and award/next-level transitions are local UI operations. They can leave one process connected with a board while the other has destroyed or replaced its board.

Evidence: `LawnApp::LocalMouseButton`, `NewOptionsDialog::ButtonDepress`, `LawnApp::PreNewGame`, `LawnApp::DoBackToMain`, `GameOverDialog::ButtonDepress`, and `AwardScreen::StartButtonPressed`.

Recommended protocol: host-authoritative `SESSION_ABORT`, `SESSION_RESTART` and `SESSION_ADVANCE` transitions with acknowledgement/barrier. The client may request a transition but must not apply one independently.

Required tests: host and client attempts for ESC -> menu, retry and next; disconnect/reconnect and survival repick.

## P0 gaps for special modes

Until these actions exist, unsupported special modes should be rejected when creating a LAN session instead of silently swallowing their input.

| Mode | Required semantic actions |
|---|---|
| Whack-a-Zombie | `WHACK_ZOMBIE` |
| Vasebreaker / Scary Potter | `BREAK_VASE` |
| Slot Machine | `PULL_SLOT_MACHINE` |
| Beghouled / Twist | `BEGHOULED_SWAP`, `BEGHOULED_TWIST`, `BEGHOULED_BUY` |
| Zombiquarium | `DROP_BRAIN`, `BUY_ZOMBIQUARIUM_ITEM` |
| Last Stand | `START_LAST_STAND_WAVE` |

Evidence: the unsynchronised mutations are rooted in `Challenge::MouseDownWhackAZombie`, `Challenge::ScaryPotterMalletPot`, `Challenge::MouseDown` (Slot Machine), the Beghouled drag/twist functions, `Challenge::ZombiquariumMouseDown`, and `Board::MouseUp` (Last Stand button).

Raining Seeds, Slot Machine and Vasebreaker use `COIN_USABLE_SEED_PACKET`. The existing `COLLECT_COIN` path writes the shared board cursor on both peers, but independent LAN held state does not retain the source `CoinID`. This must become per-player local held state and an atomic `PLANT_USABLE_SEED { coinId, cell }` action. It must not use the shared board cursor.

Evidence: `Coin::Collect`, `Board::MouseDownWithPlant`, `LawnApp::LocalMouseButton`, and `LawnApp::ApplyLanAction`.

Zen Garden and Tree of Wisdom are also outside the current protocol. Full support needs host-authoritative actions for tools, moving/selling potted plants, the wheelbarrow, Stinky, tree food and garden changes, plus a shared gameplay profile. Until then these modes should be filtered out of LAN play.

Evidence: `Board::PickUpTool`, `Board::MouseDownWithTool`, `ZenGarden::MouseDownZenGarden`, `ZenGarden::MouseDownWithTool`, and the `Challenge::TreeOfWisdom*` input paths.

## P1 deterministic hardening

- Disable gameplay cheat/debug typing while connected. It can currently change mowers, visuals, waves, zombies or RNG on one peer only. ESC should clear only the local held item or open local options; it must never mutate a shared cursor.
- Extend the gameplay-profile snapshot (or broadcast authoritative reward results). Potted inventory and related purchase progress can currently make present/plant rewards branch differently on peers.
- Extend the board hash to future-affecting fields including tutorial state/timer, mower-row history, shared cheat modes and collected reward counters. Do not hash cursors, tooltips or other intentionally local presentation state.
- Replace the boolean action-apply result with `APPLIED`, `STALE_NOOP` and `INVALID`. A second valid claim on an already-collected `CoinID` is stale; a fabricated ID is invalid and should desync.
- Give conveyor packets a stable identity/version. Two same-tick actions addressed only by packet index can refer to different packets after the first action consumes and shifts the belt.

Evidence for these items is in `Board::DoTypingCheck`/`Board::KeyChar`, `LawnApp::InstallLanGameplayProfile`, `Coin::Collect`, `Multiplayer/DeterministicHash.cpp`, `LawnApp::ApplyLanAction`, and the conveyor branches of `Board::PlantSeedFromBank`/`SeedBank::Update`.

## Minimum regression matrix

1. Ordinary contention: same coin, same seed packet, same cell, plant plus shovel, and same Cob Cannon.
2. Network timing: hash split across poll calls, delayed action delivery and minimized-client catch-up.
3. Lifecycle: chooser -> intro -> play -> award/game-over -> next/retry/quit.
4. Each new special-mode action: valid, stale and invalid cases.
5. Usable seed packet: each player independently picks one up and plants it on a different cell.

The current unit tests cover serialization, transport, timeline ordering and hash primitives. They do not yet instantiate two real boards and drive the lifecycle or contention matrix above.
