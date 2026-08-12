# PvZ 95 ruleset research

## Sample and scope

The behavior source used for this project is the user-supplied archive with SHA-256:

```text
bfc98d7e34a35c33ed7645d6b1385c2c1853255271f04a4b616a4e9cf508228a
```

No executable, modifier, DLL, game resource, or extracted proprietary asset from that archive is committed to this repository. Analysis is static; unrelated third-party executables in the archive were not run.

The archive contains both `PlantsVsZombies.exe` and `PlantsVsZombies(原版启动).exe`, plus the normal game data (`main.pak` and `LawnStrings.txt`). It contains no `patch.exe`. The two game executables are both 3,007,800-byte PE32/x86 programs with the same timestamp, entry point, section layout, imports, and resources:

| Sample | SHA-256 |
|---|---|
| PvZ 95 executable | `85f6b14d5e02eacc8ed661a2952151a5f151809aa41599db2b659d1486d0252c` |
| bundled original executable | `6f1729369ac9c5f859e8f3b55fe7d513fbc20b5c54127fd3a1c7e500237fde6f` |

The PvZ 95 game executable is therefore an already-patched 32-bit original executable rather than a runtime patcher. Compared with the bundled original executable, 2,754 bytes differ across 164 nearby patch clusters. Forty direct jumps enter about 1.8 KiB of injected code stored in unused PE-header space. The resource section is unchanged, and no new DLL import or trailing payload was added.

## Implemented direct table changes

These values are read directly from the changed plant and projectile definition tables and are covered by `pvz95-rules-tests`.

| Object | Field | Original | PvZ 95 |
|---|---|---:|---:|
| Sunflower | production period | 2500 | 3300 |
| Potato Mine | sun cost | 25 | 50 |
| Sun-shroom | production period | 2500 | 3300 |
| Grave Buster | sun cost | 75 | 50 |
| Hypno-shroom | sun cost | 75 | 100 |
| Tall-nut | sun cost | 125 | 175 |
| Tall-nut | recharge | 3000 | 2000 |
| Sea-shroom | recharge | 3000 | 1500 |
| Cactus | sun cost | 125 | 200 |
| Blover | sun cost | 100 | 200 |
| Blover | recharge | 750 | 2000 |
| Starfruit | sun cost | 125 | 250 |
| Starfruit | launch period | 150 | 200 |
| Marigold | sun cost | 50 | 75 |
| Marigold | recharge | 3000 | 1500 |
| Marigold | production period | 2500 | 3300 |
| Gatling Pea | sun cost | 250 | 450 |
| Cattail | sun cost | 225 | 275 |
| Cattail | launch period | 150 | 75 |
| Explode-o-nut | sun cost | 0 | 150 |
| Star projectile | damage | 20 | 40 |
| Spike projectile | table damage | 20 | 1 |

## Located behavior hooks

The remaining injected branches have been associated with functions in the reference decompilation. High-level groups include:

- `Board`: wave selection, spawn eligibility, level initialization, sun spawning, loot, cheat modes, typing, and the sun cap.
- `Challenge`: conveyor/raining seeds, challenge wave initialization and spawning, Beghouled Twist, Whack-a-Zombie, and Scary Potter.
- `Plant` and `Projectile`: initialization, shooters and production, special attacks, targeting rectangles, damage reactions, projectile update and death.
- `Zombie`: initialization variants, Gargantuar behavior, target selection, eating and squishing, animation/chill speed, mind control, butter/cold/fire, and damage.

These are **located**, not yet all behaviorally restored. A hook is marked complete only after its injected branch has been understood, reimplemented without copying proprietary machine code, and checked against an observed PvZ 95 outcome.

## Implemented behavior changes

The following changes have been translated into ruleset functions and focused tests. The virtual addresses identify evidence in the analyzed sample; no patched executable bytes are stored in this repository.

| Area | PvZ 95 behavior | Evidence |
|---|---|---:|
| Potato Mine | arms after 1000 ticks instead of 1500 | `0x45E34E` |
| Sun-shroom | grows after 9000 ticks instead of 12000 | `0x45E3F1` |
| Spikerock | starts at 16200 health, takes 1800 crush damage, and changes damage art at 10800/5400 | `0x45E5C3`, `0x45EC63` |
| Explode-o-nut | follows the Cherry Bomb special path when squished | `0x462BDE`, `0x46670A` |
| Screen Door Zombie | is initialized as a Buckethead Zombie | `0x5225E6` |
| Flag Zombie | starts with 820 body health | `0x5227B5` |
| Tall-nut | receives the Spikerock-style Gargantuar smash path and is excluded from normal square squishing | `0x526D72`, `0x52E96B` |
| Newspaper Zombie | mad phase counts as chilled movement while its animation runs at 2.5x, or 1.25x while chilled | `0x52EF13`, `0x52F02F` |
| Squash-head Zombie | body health is forced to 720 after body damage | `0x531319` |
| Butter | applies 300 ticks of ice trap and 1000 ticks of chill instead of setting the butter timer | `0x53273B` |
| Burn | the 1800-damage path considers body + helmet + shield health and always includes Gatling-head/Squash-head | `0x532B96` |

## Implemented verified behavior batch 2

This batch was decoded instruction-by-instruction against the bundled original and then mapped to the corresponding C++ function. The `original` ruleset retains the portable engine's original values.

| Area | Original | PvZ 95 | Evidence |
|---|---|---|---:|
| Gatling Pea / Cattail | Cattail fires the launch-counter-50 extra shot | Gatling Pea fires it; Cattail does not | `0x45F925` |
| Marigold | 10% gold coin, otherwise silver | 50% large sun, otherwise normal sun | `0x45FAFC`, `0x45FB06`, `0x45FB0B` |
| Big Time Marigold | extra silver coin | extra normal sun | `0x45FB5C` |
| Chomper tough target set | Gargantuar, Red-eye Gargantuar, Boss | Gargantuar, Red-eye Gargantuar, Football | `0x461453` |
| Chomper digestion | 4000 ticks | 2500 ticks | `0x461551` |
| Squash attack rectangle | `x + 20`, `width - 35` | `x - 16`, `width + 48` | `0x468040`, `0x468043` |
| Chomper attack width | 40 | 150 | `0x46805E` |
| Fume-shroom attack width | 340 | board-wide / intended unlimited | `0x4680F2` |
| Football helmet | 1400 health | 2800 health | `0x522BB0` |
| Newspaper shield | 150 health | 1200 health | `0x52337D` |
| Dancing Zombie body | 500 health | 1350 health | `0x523530` |
| Bungee at bottom | waits 300 ticks | delay is 0 | `0x525127` |
| Yeti update dispatch | Yeti calls `UpdateYeti` | Flag calls `UpdateYeti`; Yeti no longer does | `0x52B214` |
| Chew cadence | 4 ticks, doubled when chilled | base 8; Newspaper-reading phase doubles again; chill doubles again | `0x52F648`, injected `0x4005B1` |
| Mind control | only changes allegiance | clears chill, sets scale 1.25, and sets body/helmet/shield health to each max + 200; Newspaper body is fixed at 920 | `0x52FA7C`, injected `0x400372` |
| I, Zombie Sunflower reward | normal sun | small sun | `0x52FCE5` |
| Chew damage | 4 | 8 (the special I, Zombie bonus chew remains 4) | `0x52FCC2`, `0x52FE14`, injected `0x400563` |
| Low-health Explode-o-nut while eaten | no transform | below 40 health becomes a primed Cherry Bomb | injected `0x400563` |
| Low-health Tall-nut while eaten | no transform | after a bite leaves it below 300 health, it becomes Squash | injected `0x40044F` |
| Remove cold effects | clears chill | leaves/reapplies 1000 chill ticks | `0x532B51` |
| Last Stand starting sun | 5000 | 8000 | `0x40B058` |
| Sun bank cap | 9990 | 2,000,000,000 | `0x41B96E` |
| Beghouled goal | 75 matches; warning at 70 | 100 matches; warning at 95 | `0x417AA5`, `0x4211F5`, `0x421287`, `0x42131D` |
| Raining Seeds interval | random 500–999 ticks | random 200–699 ticks | `0x4234E7` |

Two unusual translations are intentional and evidence-backed:

- The modified executable compares zombie type `1` (Flag) instead of `19` (Yeti) immediately before the unchanged call to `UpdateYeti` at `0x52B214`. This looks like a gameplay glitch, but the ruleset preserves it because both bytes and call target are unambiguous.
- At `0x4680F2`, the executable literally stores `0x7fffffff` in the Fume-shroom rectangle's **width**, not its right edge. The original 32-bit overlap routine subsequently adds `x + width`, which wraps. Copying that literal into portable C++ would be signed-overflow undefined behavior and can make the attack miss everything. The source port uses `BOARD_WIDTH` instead: this safely preserves the apparent intended behavior of reaching every target to the right while remaining deterministic on Windows and macOS.

## Still pending translation

The following groups have been located but are not marked complete. They require decoding their injected branches and then adding behavior tests; merely finding a changed instruction is not treated as implementation.

- Board wave selection, zombie spawn eligibility, sun-spawn timing, loot, future/dance/super-mower modes, and typing hooks.
- Challenge conveyor contents, Whack-a-Zombie population, challenge-specific wave construction, and the full Scary Potter pot population tables.
- Plant bowling, Magnet-shroom, squish, burn-row and Blover target changes; remaining projectile update/death hooks.
- Gargantuar actions, board-edge behavior, plant targeting and square squishing; remaining zombie damage branches and special-head behavior.
- The changed `.data` zombie-allowed-level table and remaining `.rdata` presentation strings.

This list is the migration queue, not a claim that all 164 patch clusters are already understood.

These rules remain switchable with `-ruleset original`; that path preserves the portable engine's original behavior.

## Ruleset selection

PvZ 95 is the default. For A/B testing:

```bash
./pvz95-coop -ruleset pvz95
./pvz95-coop -ruleset original
```

Peers exchange the active ruleset protocol ID during discovery and handshake. Sessions reject a mismatch rather than risk a silent deterministic divergence.
