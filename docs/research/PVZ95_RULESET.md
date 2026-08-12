# PvZ 95 ruleset research

## Sample and scope

The behavior source used for this project is the user-supplied archive with SHA-256:

```text
bfc98d7e34a35c33ed7645d6b1385c2c1853255271f04a4b616a4e9cf508228a
```

No executable, modifier, DLL, game resource, or extracted proprietary asset from that archive is committed to this repository. Analysis is static; unrelated third-party executables in the archive were not run.

The PvZ 95 game executable is a patched 32-bit original executable rather than a runtime patcher. Compared with the bundled original executable, 2,754 bytes differ across 164 nearby patch clusters. Forty branches jump into about 1.8 KiB of injected code stored in unused PE-header space. The resource section is unchanged.

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

These rules remain switchable with `-ruleset original`; that path preserves the portable engine's original behavior.

## Ruleset selection

PvZ 95 is the default. For A/B testing:

```bash
./pvz95-coop -ruleset pvz95
./pvz95-coop -ruleset original
```

Peers exchange the active ruleset protocol ID during discovery and handshake. Sessions reject a mismatch rather than risk a silent deterministic divergence.
