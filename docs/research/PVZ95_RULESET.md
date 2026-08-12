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

## Implemented verified behavior batch 3

This batch restores 28 changed immediates in `Challenge`. Conveyor weights, Scary Potter pot counts, and all entries not listed below remain unchanged. Placement indices are zero-based call order within one Scary Potter level and are used only to express the executable's exact per-slot replacements.

| Area | PvZ 95 behavior | Evidence |
|---|---|---:|
| Portal Combat conveyor | slot 0 Peashooter → Threepeater; slot 4 Wall-nut → Explode-o-nut; slot 5 Cherry Bomb → Doom-shroom | `0x4231B4`, `0x4231E0`, `0x4231EC`, injected `0x400638` |
| Invisighoul conveyor | slot 0 Peashooter → Threepeater; slot 3 Squash → Cherry Bomb | `0x42325A`, `0x42327E`, injected `0x4004E8` |
| Whack-a-Zombie | ordinary spawn group starts at 2 instead of 1; maximum-speed curve starts at 3 instead of 1 | `0x426193`, `0x42630B` |
| Scary Potter 1 seeds | slot 0 Peashooter → Repeater; slot 2 Squash → Peashooter | `0x428B39`, `0x428B66` |
| Scary Potter 1 zombies | slot 5 Jack-in-the-box → Screen Door | `0x428BA3` |
| Scary Potter 2 seeds | slots 0–3: Leftpeater → Potato Mine, Snow Pea → Ice-shroom, Wall-nut → Explode-o-nut, Potato Mine → Cherry Bomb | `0x428C3A`, `0x428C53`, `0x428C67`, `0x428C7B` |
| Scary Potter 2 zombies | slots 4–6: Normal → Football, Buckethead → Newspaper, Jack-in-the-box → Red-eye Gargantuar | `0x428C90`, `0x428CA3`, `0x428CB6` |
| Scary Potter 3 seed | slot 4 Wall-nut → Hypno-shroom | `0x428D8B` |
| Scary Potter 3 zombies | slots 5–8: Normal → Buckethead, Buckethead → Newspaper, Dancing → Gargantuar, Jack-in-the-box → Flag | `0x428DA1`, `0x428DB4`, `0x428DC6`, `0x428DD9` |
| Scary Potter 4 seeds | slots 0–2: Puff-shroom → Blover, Hypno-shroom → Potato Mine, Leftpeater → Blover | `0x428E38`, `0x428E4E`, `0x428E61` |
| Scary Potter 4 zombies | slots 3–5: Jack-in-the-box → Conehead, Normal → Pole Vaulter, Football → Dancing | `0x428E76`, `0x428E8A`, `0x428E9E` |

The Portal Combat and Invisighoul slot-0 edits enter tiny injected stubs, but those stubs only replace the seed-type immediate and return to the unchanged weight assignment. The portable implementation therefore applies the replacement before the common conveyor weighting loop. Scary Potter 5 and later contain no direct type change in this contiguous patch region.

## Implemented verified behavior batch 4

This batch decodes the remaining self-contained `Plant` and `Projectile` branches below. Column B uses `ruslan831/PlantsVsZombies-decompilation` at commit `20b245acee018fe32804b271fd6400bca06618c1`. That repository is an address-annotated reconstruction of the original game, not 95 source: it identifies the C++ function, statement, and field behind column A's machine code. Field identities were verified before translation: a projectile has motion at `+0x58`, type at `+0x5C`, age at `+0x60`, and inherited integer x at `+0x18`; a zombie has chill at `+0xAC`, butter at `+0xB0`, and ice trap at `+0xB4`; `Board+0x554C` is `mBackground`. This distinction matters because several enum values happen to share the same integer.

| Area | PvZ 95 behavior | A: original/95 EXE instruction or injected flow | B: reference C++ mapping | C: portable implementation |
|---|---|---|---|---|
| Jalapeño `BurnRow` | Before/around burn, writes 2500 chill and 750 ice-trap ticks, invokes burn and animation-speed update, then applies an extra 1000 damage with flags 1. | At `0x4664FE`–`0x466529`, the 95 EXE writes `[Zombie+0xB4]=750` and `[Zombie+0xAC]=2500`, calls `0x532B70`, `0x52F050`, then `0x5317C0`; the replaced original path called `RemoveColdEffects` then `ApplyBurn`. | `Lawn/Plant.cpp:4269-4280` is `Plant::BurnRow`; `Lawn/Zombie.h:119-121` names the three status fields; `Lawn/Zombie.cpp:6558`, `7916`, and `8627` map the call targets to `UpdateAnimSpeed`, `TakeDamage`, and `ApplyBurn`. | `src/Lawn/Plant.cpp:4227-4250`; constants are isolated by `ResolveBurnRowEffects` in `src/GameRules/Ruleset.cpp:255-261`. |
| Blover displacement | Blows away zombies in ten exact phases: pole-vaulting, bobsled boarding, three Pogo phases, two dolphin phases, snorkel entry, thrown Imp, and Balloon flying. | Hook `0x4665FE -> 0x400400`; comparisons at `0x400400`–`0x40051F` branch to `0x466608`, whose unchanged tail writes `[Zombie+0xB9]=1`. | `Lawn/Plant.cpp:4301-4317` is `Plant::BlowAwayFliers`; `Lawn/Zombie.h:91,123` maps `+0x28` to `mZombiePhase` and `+0xB9` to `mBlowingAway`; `ConstEnums.h:1216-1292` supplies the phase names. | `src/Lawn/Plant.cpp:4271-4293`; the exact phase set is in `src/GameRules/Ruleset.cpp:263-284`. |
| Blover damage | A live target that did not enter the blow-away branch takes 50 damage unless mind-controlled; Conehead helmet health is first forced to 50. Blow-away and damage are mutually exclusive. | The ten phase matches at `0x400400`–`0x40051F` jump directly to the unchanged blow-away write at `0x466608`. Only the non-match continuation reaches `0x400742`, which tests `[Zombie+0xB8]`, tests low byte `[+0xC4]==1`, optionally writes `[+0xD0]=50`, then calls `0x5317C0` with damage 50 before returning to the original flow. | `Lawn/Zombie.h:122,131,134` maps mind control, helmet type, and helmet health; `Lawn/Zombie.cpp:7916-7944` is `TakeDamage`; the original enclosing loop is `Lawn/Plant.cpp:4301-4314`. | `src/Lawn/Plant.cpp:4286-4294`; `ResolveBloverDamage` accepts the already-decoded blow-away result so the two branches remain exclusive; `ResolveBloverConeHelmHealth` preserves the conditional helmet write. |
| Blover fog | Fog-clear countdown is 10000 instead of 4000. | The immediate stored to `[Board+0x5D4]` at `0x46663E` is `0x2710` instead of `0x0FA0`. | `Lawn/Plant.cpp:4316-4317` is the original `mFogBlownCountDown = 4000`; `Lawn/Board.h:133` maps `+0x5D4`. | `src/Lawn/Plant.cpp:4295-4296`; `ResolveFogBlownCountdown` is `src/GameRules/Ruleset.cpp:296-299`. |
| Homing projectile collision | The dedicated target-ID-only collision branch is bypassed, so homing projectiles continue through the common collision search. | At `0x46CECB`, original `jne 0x46CF6C` becomes `jno 0x46CF6C`. The preceding `cmp motion,9` cannot overflow for valid motion enum values, so the branch always reaches the common path. | `Lawn/Projectile.cpp:241-312` shows the original homing-only block at lines 255-268 and common `FindCollisionTarget` at lines 302-311; `Lawn/Projectile.h:40` maps `+0x58` to motion. | `src/Lawn/Projectile.cpp:266-329`; `UsesHomingTargetOnlyCollision` is `src/GameRules/Ruleset.cpp:301-304`. |
| Star projectile motion lifetime | A `MOTION_STAR` becomes `MOTION_STRAIGHT` once its pre-increment age reaches 56 on Pool/Fog or 64 on other backgrounds; age itself is not reset and is then incremented normally. | Hook `0x46E464 -> 0x400593` first tests `[Projectile+0x58]==7`. `0x400A76` reads `LawnApp+0x768 -> Board`, then `[Board+0x554C]`; values 2/3 select 56, otherwise 64. At threshold `0x40059F` writes `[Projectile+0x58]=0`, then `0x4005A6` increments `[+0x60]`. | `Lawn/Projectile.cpp:923-956` is `Projectile::Update`; `Lawn/Projectile.h:40-42` maps motion/type/age; `Lawn/Board.h:148` maps `+0x554C` to `mBackground`; `ConstEnums.h:88-99,762-774` names background 2/3 as Pool/Fog and motion 0/7 as Straight/Star. | `src/Lawn/Projectile.cpp:939-945`; `ResolveProjectileMotionBeforeUpdate` is `src/GameRules/Ruleset.cpp:306-315`, deliberately called before `mProjectileAge++`. |
| Spike projectile death | Type `PROJECTILE_SPIKE` with integer x below 64 increments x and skips only `mDead = true`; execution deliberately continues into unchanged attachment cleanup. | Hook `0x46EB2B -> 0x400A33` compares `[Projectile+0x5C]` with 8 and inherited `[+0x18]` with 64. The exceptional path increments `+0x18` and rejoins at `0x400A50`; all other paths write `[+0x50]=1`, and both resume the original type/attachment branch. | `Lawn/Projectile.cpp:1146-1160` is `Projectile::Die`; `Lawn/Projectile.h:38,41,49` maps dead/type/attachment; inherited `mX` is `Lawn/GameObject.h:11-22`; `ConstEnums.h:775-791` identifies type 8 as Spike. | `src/Lawn/Projectile.cpp:1153-1168`; `ResolveProjectileDeath` is `src/GameRules/Ruleset.cpp:317-326`. There is intentionally no early return before attachment cleanup. |
| Snow Pea through Torchwood | `ConvertToPea` clears the attachment but writes projectile **type** 1 (Snow Pea), rather than type 0 (Pea). | At `0x46EE12`, the immediate written to `[Projectile+0x5C]` is 1 rather than 0. | `Lawn/Projectile.cpp:1218-1228` is `ConvertToPea` and its original Pea assignment; `Lawn/Projectile.h:41` maps `+0x5C` to type; `ConstEnums.h:775-779` identifies type 1 as Snow Pea. | `src/Lawn/Projectile.cpp:1224-1232`; `ResolveTorchwoodSnowPeaType` is `src/GameRules/Ruleset.cpp:328-331`. |

Earlier live progress notes made two offset errors. First, they mislabeled the Spike death condition as motion `MOTION_FLOAT_OVER` and the Torchwood write as motion `MOTION_LOBBED`, because both pairs use the integers 8 and 1; `+0x5C` proves both are projectile-type operations. Second, they described the star hook as resetting age and briefly misidentified `Board+0x554C`; the complete injected flow proves it changes `Projectile+0x58` from Star to Straight based on `mBackground`, while `Projectile+0x60` age still increments. The same notes called zombie `+0xB4` butter; it is the ice-trap counter, while butter is `+0xB0`. The implementation and typed tests use the corrected interpretations.

## Still pending translation

The following groups have been located but are not marked complete. They require decoding their injected branches and then adding behavior tests; merely finding a changed instruction is not treated as implementation.

- Board wave selection, zombie spawn eligibility, sun-spawn timing, loot, future/dance/super-mower modes, and typing hooks.
- Challenge-specific wave construction and `SpawnZombieWave`'s injected board-state gate. Its nearby current-wave threshold change from 5 to 9 is intentionally pending with that gate so the two dependent edits are not translated separately.
- Remaining Scary Potter 5+ behavior hooks, if later analysis finds any outside the decoded direct type-immediate region.
- Plant bowling and Magnet-shroom changes, plus any remaining non-self-contained Plant hooks.
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
