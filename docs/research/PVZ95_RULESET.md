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

## Ruleset selection

PvZ 95 is the default. For A/B testing:

```bash
./pvz95-coop -ruleset pvz95
./pvz95-coop -ruleset original
```

Peers exchange the active ruleset protocol ID during discovery and handshake. Sessions reject a mismatch rather than risk a silent deterministic divergence.
