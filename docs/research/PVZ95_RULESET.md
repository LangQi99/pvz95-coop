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

These values are read directly from the changed plant and projectile definition tables and are covered by `pvz95-rules-tests`. All reference-source locations in this document use `ruslan831/PlantsVsZombies-decompilation` commit `20b245acee018fe32804b271fd6400bca06618c1`.

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

Three-way traceability for all 22 direct values:

| Group | A: original/95 EXE data difference | B: reference C++ mapping | C: portable implementation |
|---|---|---|---|
| 20 plant-definition values | `gPlantDefs` starts at `0x69F2B0`, stride `0x24`; changed field offsets are `+0x10` cost, `+0x14` recharge, and `+0x1C` launch period. The exact changed VAs are `0x69F2F0`, `0x69F350`, `0x69F410`, `0x69F44C`, `0x69F470`, `0x69F5FC`, `0x69F600`, `0x69F624`, `0x69F668`, `0x69F68C`, `0x69F690`, `0x69F6D4`, `0x69F6E0`, `0x69F818`, `0x69F81C`, `0x69F824`, `0x69F860`, `0x69F8CC`, `0x69F8D8`, and `0x69F9A4`; their little-endian values match the table above. | `Lawn/Plant.cpp:24-77` defines `gPlantDefs`; `Lawn/Plant.h:301-311` names the fields and offsets. | `src/GameRules/Ruleset.cpp:22-52` stores the overrides; `ResolvePlantSeedCost`, `ResolvePlantRefreshTime`, and `ResolvePlantLaunchRate` at lines 105-118 feed `Plant::GetCost`, `Plant::GetRefreshTime`, and initialization. |
| 2 projectile damage values | In `gProjectileDefinition` at `0x69F1C0`, damage field `+0x8`: Star VA `0x69F21C` changes 20→40 and Spike VA `0x69F228` changes 20→1. | `Lawn/Projectile.cpp:14-29` defines the table and `Lawn/Projectile.cpp:1230-1234` indexes it by projectile type. | `ResolveProjectileDamage` at `src/GameRules/Ruleset.cpp:239-253` is used by Portable's projectile damage lookup; both values have focused tests. |

## Located behavior hooks

The remaining injected branches have been associated with functions in the reference decompilation. High-level groups include:

- `Board`: wave selection, spawn eligibility, level initialization, sun spawning, loot, cheat modes, typing, and the sun cap.
- `Challenge`: conveyor/raining seeds, challenge wave initialization and spawning, Beghouled Twist, Whack-a-Zombie, and Scary Potter.
- `Plant` and `Projectile`: initialization, shooters and production, special attacks, targeting rectangles, damage reactions, projectile update and death.
- `Zombie`: initialization variants, Gargantuar behavior, target selection, eating and squishing, animation/chill speed, mind control, butter/cold/fire, and damage.

These are **located**, not yet all behaviorally restored. A hook is marked complete only after its injected branch has been understood, reimplemented without copying proprietary machine code, and checked against an observed PvZ 95 outcome.

## Implemented behavior changes

The following changes have been translated into ruleset functions and focused tests. The virtual addresses identify evidence in the analyzed sample; no patched executable bytes are stored in this repository.

| Area | PvZ 95 behavior | A: original/95 EXE instruction or injected flow | B: reference C++ mapping | C: portable implementation |
|---|---|---|---|---|
| Potato Mine | arms after 1000 ticks instead of 1500 | Immediate at `0x45E34E` changes `0x5DC`→`0x3E8`. | `Plant::PlantInitialize`, `Lawn/Plant.cpp:85-260`, initializes `mStateCountdown`; `Lawn/Plant.h:157` maps that field. | `ResolvePlantInitialStateCountdown`, `src/GameRules/Ruleset.cpp:128-142`, called by `src/Lawn/Plant.cpp:346`. |
| Sun-shroom | grows after 9000 ticks instead of 12000 | Immediate at `0x45E3F1` changes `0x2EE0`→`0x2328`. | Same `Plant::PlantInitialize` switch and `mStateCountdown` field. | Same resolver, called by `src/Lawn/Plant.cpp:388`. |
| Spikerock | starts at 16200 health, takes 1800 crush damage, and changes damage art at 10800/5400 | `0x45E5C3` changes health 450→16200; hook `0x45EC63 -> 0x4003B8` subtracts `0x708`; following threshold immediates are `0x2A30` and `0x1518`. | `Plant::PlantInitialize`, `Lawn/Plant.cpp:85-260`, and `Plant::SpikeRockTakeDamage`, lines 627-655; `Plant.h:152-153` maps health/max health. | `ResolvePlantInitialHealth` and `ResolveSpikeRock*`, `src/GameRules/Ruleset.cpp:120-126,210-225`; callsites `src/Lawn/Plant.cpp:461,652-658`. |
| Explode-o-nut | follows the Cherry Bomb special path when squished | Hooks `0x462BDE -> 0x40060A` and `0x46670A -> 0x400621` add seed type `0x31` beside Cherry Bomb type 2. | `Plant::Squish`, `Lawn/Plant.cpp:2335`, and `Plant::DoSpecial`, line 4334; `Plant.h:145` maps `+0x24` to seed type. | `UsesCherryBombSpecial`, `src/GameRules/Ruleset.cpp:227-231`; callsites `src/Lawn/Plant.cpp:2329,4333`. |
| Screen Door/Bucket hybrid | an incoming Screen Door stores Buckethead as its member type and receives Buckethead helmet type/1100 health, but still dispatches through the original Screen Door switch case, so it also receives the door shield | Hook `0x5225E6 -> 0x4002E0` maps incoming type 6 to 4 only for the store to `[Zombie+0x24]`; the switch later reloads the unchanged argument from `[EBP+0xC]`. Caves `0x400320`/`0x400350` test the stored type and write helmet type 2 plus health `0x44C` before the switch. | `Zombie::ZombieInitialize`, `Lawn/Zombie.cpp:85`; `Zombie.h:90,131,134,136-137` maps member type, helmet, helmet health, shield, and shield health. The Door case at `Lawn/Zombie.cpp:203-208` remains distinct from the Buckethead case. | `ResolveZombieMemberType` changes only `mZombieType`; `ResolveZombiePreSwitchArmor` applies the early Buckethead armor, while `src/Lawn/Zombie.cpp` deliberately switches on the unchanged requested type so the Door shield path still executes. |
| Flag Zombie | starts with 820 body health | Hook `0x5227B5 -> 0x400493` stores `0x334` for type 1, otherwise original `0x10E`. | `Zombie::ZombieInitialize`, `Lawn/Zombie.cpp:85`; `Zombie.h:90,132` maps type/body health. | `ResolveZombieInitialBodyHealth`, `src/GameRules/Ruleset.cpp:346-357`, called at `src/Lawn/Zombie.cpp:239`. |
| Tall-nut | receives the Spikerock-style Gargantuar smash path and is excluded from normal square squishing | Caves `0x40095E` and `0x400977` add plant type `0x17` beside Spikerock `0x2E` at hooks `0x526D72` and `0x52E96B`. | `Zombie::UpdateZombieGargantuar`, `Lawn/Zombie.cpp:2027`, and `Zombie::SquishAllInSquare`, line 6387; the compared plant type is `Plant+0x24`. | `TakesLayeredCrushDamage`, `src/GameRules/Ruleset.cpp:233-237`; callsites `src/Lawn/Zombie.cpp:2104,6491`. |
| Newspaper Zombie animation | `PHASE_NEWSPAPER_MAD` counts as chilled movement; its animation is 2.5× normally or 1.25× when chilled. | Hook `0x52EF13 -> 0x400990` returns special AL states for phase `0x1F`; hook `0x52F02F -> 0x4009B8` interprets AL 2/3 using the injected 2.5 constant and the original 0.5 multiplier. | `Zombie::IsMovingAtChilledSpeed`, `Lawn/Zombie.cpp:6503`, and `Zombie::ApplyAnimRate`, line 6549; `Zombie.h:91,119` maps phase/chill. | `IsForcedChilledMovement` and `ResolveZombieAnimationRate`, `src/GameRules/Ruleset.cpp:482-493`; callsites `src/Lawn/Zombie.cpp:6593,6643`. |
| Newspaper maddening body-health glitch | after any body-damage subtraction during `PHASE_NEWSPAPER_MADDENING`, body health is overwritten with 720 | Hook `0x531319 -> 0x4002F8` compares `[Zombie+0x28]` with `0x1E` and writes `[+0xC8]=0x2D0`. This is a **phase**, not zombie type. | `Zombie::TakeBodyDamage`, `Lawn/Zombie.cpp:7767`; `Zombie.h:91,132` maps `+0x28` to phase and `+0xC8` to body health; `ConstEnums.h:1216-1252` identifies phase `0x1E`. | `ResolveZombieBodyHealthAfterDamage`, `src/GameRules/Ruleset.cpp:447-453`, now receives `mZombiePhase` at `src/Lawn/Zombie.cpp:7855`. |
| Butter | applies 300 ticks of ice trap and 1000 ticks of chill instead of setting the butter timer | Hook `0x53273B -> 0x400AC4` writes `[Zombie+0xB4]=300`, `[+0xAC]=1000`, and skips the original `[+0xB0]=400`. | `Zombie::ApplyButter`, `Lawn/Zombie.cpp:8478`; `Zombie.h:119-121` identifies chill/butter/ice-trap fields. | `ResolveButterStatus`, `src/GameRules/Ruleset.cpp:469-475`, used at `src/Lawn/Zombie.cpp:8545-8549`. |
| Burn | the 1800-damage path considers body + helmet + shield health and also includes phases `PHASE_NEWSPAPER_READING`/`PHASE_NEWSPAPER_MADDENING` | Hook `0x532B96 -> 0x4005CE` sums `[+0xC8]+[+0xD0]+[+0xDC]`, compares with `0x708`, then compares **phase** `[+0x28]` with `0x1D` and `0x1E`; Boss still follows the unchanged original branch. | `Zombie::ApplyBurn`, `Lawn/Zombie.cpp:8628`; `Zombie.h:91,132,134,137` maps phase/body/helmet/shield. | `ShouldTakeBurnDamage`, `src/GameRules/Ruleset.cpp:455-467`, now receives both type and phase at `src/Lawn/Zombie.cpp:8691`. |

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
| Chew cadence | 4 ticks, doubled when chilled | non-Newspaper: 8, or 16 chilled; Newspaper: 1 outside reading / 2 while reading, with chill doubling either to 2 / 4 | `0x52F648`, hook `0x52F653`, injected `0x400528`, `0x4005B1` |
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

Batch 2 three-way audit index (the behavior table above remains the per-rule expected-value list):

| Audited group | A: original/95 EXE evidence | B: reference C++ mapping | C: portable implementation |
|---|---|---|---|
| Shooter/production/Chomper/attack rectangles (8 rows) | Direct instruction edits at `0x45F925`, `0x45FAFC`/`0x45FB06`/`0x45FB0B`, `0x45FB5C`, `0x461453`, `0x461551`, `0x468040`/`0x468043`, `0x46805E`, and `0x4680F2` change only the compared seed/zombie enums, coin enums, threshold, digest immediate, and rectangle components listed above. | `Plant::UpdateShooter`, `Lawn/Plant.cpp:932`; `UpdateProductionPlant`, line 993; `UpdateChomper`, line 1760; and `GetPlantAttackRect`, line 5200. `Plant.h:145,151-159` identifies seed/state/counters. | `ShootsAtCounterFifty`, `ResolveMarigoldCoinType`, `ResolveBigTimeMarigoldCoinType`, `ChomperOnlyDamagesZombie`, `ResolveChomperDigestTime`, and `ResolvePlantAttackRect*`, `src/GameRules/Ruleset.cpp:144-208`; callsites are in `src/Lawn/Plant.cpp:982,1061,1072,1776,1823,5202-5210`. |
| Zombie initialization and update dispatch (5 rows) | Immediates at `0x522BB0`, `0x52337D`, `0x523530`, and `0x525127` change 1400→2800, 150→1200, 500→1350, and 300→0. At `0x52B214`, the type compare before the unchanged `UpdateYeti` call changes `0x13` (Yeti)→1 (Flag). | `Zombie::ZombieInitialize`, `Lawn/Zombie.cpp:85`, contains the per-type health setup; `Zombie::Update`, lines 4195-4287, dispatches type `0x13` to `UpdateYeti`. `Zombie.h:90,132-138` identifies type and health layers. | `ResolveZombieInitialBodyHealth`, `ResolveZombieInitialHelmHealth`, `ResolveZombieInitialShieldHealth`, `ResolveBungeeStealDelay`, and `UsesYetiUpdate`, `src/GameRules/Ruleset.cpp:346-386`; callsites `src/Lawn/Zombie.cpp:239,346,615,662,1331,4505`. |
| Chew timer dispatch (1 row) | Base immediates at `0x52F648`/`0x52F64E` change 4/8→8/16. Hook `0x52F653 -> 0x400528` checks **type** `[Zombie+0x24]==5` (Newspaper); its `0x4005B1` branch starts ECX at 1, doubles for **phase** `[+0x28]==0x1D` (Reading), then doubles for chill `[+0xAC]>0`. | `Zombie::CheckIfPreyCaught`, `Lawn/Zombie.cpp:6738`; `Zombie.h:90-91,106,119` maps type/phase/age/chill; `ConstEnums.h` identifies type 5 and phase `0x1D`. | `ResolveZombieEatInterval`, `src/GameRules/Ruleset.cpp:388-403`, now accepts both type and phase; `src/Lawn/Zombie.cpp:6865` passes both. Tests cover 8/16 and every Newspaper 1/2/2/4 branch. |
| Mind control (1 row) | Hook `0x52FA7C -> 0x400372 -> 0x4002B0` clears `+0xAC`, writes body/helmet/shield max +200 from `+0xCC/+0xD4/+0xE0`, forces scale bits `0x3FA00000` (1.25), and stores mind control at `+0xB8`; type 5 instead gets body 920. | `Zombie::StartMindControlled`, `Lawn/Zombie.cpp:6910`; `Zombie.h:90,119,122,132-138,150` identifies every field. | `ResolveMindControlStats`, `src/GameRules/Ruleset.cpp:431-445`, applied at `src/Lawn/Zombie.cpp:6991-6998`. |
| Eating rewards/damage/transforms (4 rows) | `0x52FCE5` coin 4→5 gives Small Sun; damage immediates `0x52FCC2` and `0x52FE14` change 4→8. Cave `0x400563` transforms type `0x31` below 40 health into type 2 and sets `Plant+0x50/+0x54=1`; cave `0x40044F` changes Tall-nut type `0x17` below 300 health into Squash `0x11`. The unchanged I, Zombie bonus subtraction remains 4. | `Zombie::EatPlant`, `Lawn/Zombie.cpp:6953-7052`, and `EatZombie`, lines 7054-7063; `Plant.h:145,152,156-157` maps type/health/special/state countdown. | `ResolveZombieEatDamage`, `ResolveIZombieSunflowerReward`, `ResolveEatenPlantSeedType`, and `EatenPlantTransformTriggersSpecial`, `src/GameRules/Ruleset.cpp:405-429`; sequence is preserved at `src/Lawn/Zombie.cpp:7099-7125,7146-7149`. |
| Cold removal (1 row) | `0x532B51` replaces the conditional clear of `[Zombie+0xAC]` with an unconditional write of 1000. | `Zombie::RemoveColdEffects`, `Lawn/Zombie.cpp:8613-8625`; `Zombie.h:119` maps chill. | `ResolveChillAfterRemovingCold`, `src/GameRules/Ruleset.cpp:477-480`, used at `src/Lawn/Zombie.cpp:8677-8683`. |
| Board/Beghouled/Raining Seeds (4 rows) | Last Stand immediate `0x40B058` changes 5000→8000; both cap immediates at `0x41B96E` become 2,000,000,000; Beghouled constants at `0x417AA5`, `0x4211F5`, `0x421287`, `0x42131D` change 75/70→100/95; `0x4234E7` changes the random-range offset 500→200. | `Board::InitLevel`, `Lawn/Board.cpp:1362`, and `Board::AddSunMoney`, line 8641; `Challenge::BeghouledScore`, `Lawn/Challenge.cpp:836-887`, plus progress drawing at line 1541; `Challenge::UpdateRainingSeeds`, line 1925. | `ResolveInitialSunMoney`, `ResolveMaximumSunMoney`, `ResolveBeghouledWinningScore`, and `ResolveRainingSeedsCountdown`, `src/GameRules/Ruleset.cpp:496-517`; callsites are `src/Lawn/Board.cpp:1384,6647,8536` and `src/Lawn/Challenge.cpp:70,1945`. |

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

Batch 3 three-way audit index for all 28 immediate replacements:

| Audited group | A: original/95 EXE evidence | B: reference C++ mapping | C: portable implementation |
|---|---|---|---|
| Conveyor seeds (5 replacements) | Portal Combat edits at `0x4231B4`, `0x4231E0`, and `0x4231EC`; Invisighoul edits at `0x42325A` and `0x42327E`. Slot-0 hooks enter `0x400638`/`0x4004E8`, replace only the seed immediate, then rejoin the original weight store. | `Challenge::UpdateConveyorBelt`, `Lawn/Challenge.cpp:1621-1922`; its local `SeedPick` array establishes slot order and weights. | `ResolveConveyorSeed`, `src/GameRules/Ruleset.cpp:519-548`, is called once per populated slot at `src/Lawn/Challenge.cpp:1875`, before unchanged weighting. |
| Whack-a-Zombie (2 replacements) | Immediates at `0x426193` and `0x42630B` change ordinary group start 1→2 and speed-curve start 1→3. | `Challenge::WhackAZombieSpawning`, `Lawn/Challenge.cpp:2791`, contains both group construction and the speed curve. | `ResolveWhackZombieGroupSize` and `ResolveWhackZombieSpeedCurveStart`, `src/GameRules/Ruleset.cpp:550-558`; callsites `src/Lawn/Challenge.cpp:2829,2876`. |
| Scary Potter levels 1-4 (21 replacements) | The 21 changed seed/zombie immediates are exactly the addresses in the table above: 3 in level 1, 7 in level 2, 5 in level 3, and 6 in level 4. Each changed operand remains an argument to the same `ScaryPotterPlacePot` call with unchanged count and placement selection. | `Challenge::ScaryPotterPopulate`, `Lawn/Challenge.cpp:3880`; sequential `ScaryPotterPlacePot` calls establish the zero-based placement indices. `ConstEnums.h` supplies the seed/zombie enum identities. | `ResolveScaryPotterSeed` and `ResolveScaryPotterZombie`, `src/GameRules/Ruleset.cpp:560-650`; every changed call is wrapped at `src/Lawn/Challenge.cpp:3868-3916`, while all unlisted calls remain unchanged. |

## Implemented verified behavior batch 4

This batch decodes the remaining self-contained `Plant` and `Projectile` branches below. Column B uses `ruslan831/PlantsVsZombies-decompilation` at commit `20b245acee018fe32804b271fd6400bca06618c1`. That repository is an address-annotated reconstruction of the original game, not 95 source: it identifies the C++ function, statement, and field behind column A's machine code. Field identities were verified before translation: a projectile has motion at `+0x58`, type at `+0x5C`, age at `+0x60`, and inherited integer x at `+0x18`; a zombie has chill at `+0xAC`, butter at `+0xB0`, and ice trap at `+0xB4`; `Board+0x554C` is `mBackground`. This distinction matters because several enum values happen to share the same integer.

| Area | PvZ 95 behavior | A: original/95 EXE instruction or injected flow | B: reference C++ mapping | C: portable implementation |
|---|---|---|---|---|
| Jalapeño `BurnRow` | Writes 2500 chill and 750 ice-trap ticks, invokes burn and animation-speed update, then applies an extra 1000 damage. The leaked damage-flags low bits are `3` if the post-burn phase is `PHASE_NEWSPAPER_MAD`, otherwise `0x3F` (represented as `0xFF`, since `TakeDamage` only consumes low flag bits). | At `0x4664FE`–`0x466529`, the 95 EXE writes `[Zombie+0xB4]=750` and `[Zombie+0xAC]=2500`, calls `0x532B70`, `0x52F050`, then `0x5317C0` without explicitly setting EAX. Register tracing through the `IsMovingAtChilledSpeed`/`ApplyAnimRate` hooks yields AL=3 for post-burn phase `0x1F`, otherwise AL=`0xFF`; unrelated high EAX bits are ABI garbage and are not portable behavior. | `Lawn/Plant.cpp:4269-4280` is `Plant::BurnRow`; `Lawn/Zombie.h:91,119-121` names phase/chill/butter/ice fields; `Lawn/Zombie.cpp:6503`, `6549`, `6558`, `7916`, and `8627` map the nested calls and `TakeDamage`. | `src/Lawn/Plant.cpp:4227-4250`; `ResolveBurnRowEffects` carries stable constants and `ResolveBurnRowDamageFlags` recreates the meaningful low-bit result from the post-`ApplyBurn` phase. Tests cover `0xFF` and 3. |
| Blover displacement | Blows away zombies in ten exact phases: pole-vaulting, bobsled boarding, three Pogo phases, two dolphin phases, snorkel entry, thrown Imp, and Balloon flying. | Hook `0x4665FE -> 0x400400`; comparisons at `0x400400`–`0x40051F` branch to `0x466608`, whose unchanged tail writes `[Zombie+0xB9]=1`. | `Lawn/Plant.cpp:4301-4317` is `Plant::BlowAwayFliers`; `Lawn/Zombie.h:91,123` maps `+0x28` to `mZombiePhase` and `+0xB9` to `mBlowingAway`; `ConstEnums.h:1216-1292` supplies the phase names. | `src/Lawn/Plant.cpp:4271-4293`; the exact phase set is in `src/GameRules/Ruleset.cpp:263-284`. |
| Blover damage | A live target that did not enter the blow-away branch takes 50 damage unless mind-controlled; Conehead helmet health is first forced to 50. Blow-away and damage are mutually exclusive. The target's phase integer is passed as damage flags. | The ten phase matches at `0x400400`–`0x40051F` jump directly to the unchanged blow-away write at `0x466608`. Only the non-match continuation reaches `0x400742`; EAX was loaded from `[Zombie+0x28]` at `0x4665FB`, comparisons preserve it, and `pushad`/`popad` restores it immediately before `TakeDamage(50)` at `0x400767`. | `Lawn/Zombie.h:91,122,131,134` maps phase, mind control, helmet type, and helmet health; `Lawn/Zombie.cpp:7916-7944` maps EAX to `theDamageFlags`; the original enclosing loop is `Lawn/Plant.cpp:4301-4314`. | `src/Lawn/Plant.cpp:4286-4295`; `ResolveBloverDamage` keeps blow-away/damage exclusive, `ResolveBloverConeHelmHealth` preserves the helmet write, and `ResolveBloverDamageFlags` explicitly passes `static_cast<unsigned int>(mZombiePhase)`. |
| Blover fog | Fog-clear countdown is 10000 instead of 4000. | The immediate stored to `[Board+0x5D4]` at `0x46663E` is `0x2710` instead of `0x0FA0`. | `Lawn/Plant.cpp:4316-4317` is the original `mFogBlownCountDown = 4000`; `Lawn/Board.h:133` maps `+0x5D4`. | `src/Lawn/Plant.cpp:4295-4296`; `ResolveFogBlownCountdown` is `src/GameRules/Ruleset.cpp:296-299`. |
| Homing projectile collision | The dedicated target-ID-only collision branch is bypassed, so homing projectiles continue through the common collision search. | At `0x46CECB`, original `jne 0x46CF6C` becomes `jno 0x46CF6C`. The preceding `cmp motion,9` cannot overflow for valid motion enum values, so the branch always reaches the common path. | `Lawn/Projectile.cpp:241-312` shows the original homing-only block at lines 255-268 and common `FindCollisionTarget` at lines 302-311; `Lawn/Projectile.h:40` maps `+0x58` to motion. | `src/Lawn/Projectile.cpp:266-329`; `UsesHomingTargetOnlyCollision` is `src/GameRules/Ruleset.cpp:301-304`. |
| Star projectile motion lifetime | A `MOTION_STAR` becomes `MOTION_STRAIGHT` once its pre-increment age reaches 56 on Pool/Fog or 64 on other backgrounds; age itself is not reset and is then incremented normally. | Hook `0x46E464 -> 0x400593` first tests `[Projectile+0x58]==7`. `0x400A76` reads `LawnApp+0x768 -> Board`, then `[Board+0x554C]`; values 2/3 select 56, otherwise 64. At threshold `0x40059F` writes `[Projectile+0x58]=0`, then `0x4005A6` increments `[+0x60]`. | `Lawn/Projectile.cpp:923-956` is `Projectile::Update`; `Lawn/Projectile.h:40-42` maps motion/type/age; `Lawn/Board.h:148` maps `+0x554C` to `mBackground`; `ConstEnums.h:88-99,762-774` names background 2/3 as Pool/Fog and motion 0/7 as Straight/Star. | `src/Lawn/Projectile.cpp:939-945`; `ResolveProjectileMotionBeforeUpdate` is `src/GameRules/Ruleset.cpp:306-315`, deliberately called before `mProjectileAge++`. |
| Spike projectile death | Type `PROJECTILE_SPIKE` with integer x below 64 increments x and skips only `mDead = true`; execution deliberately continues into unchanged attachment cleanup. | Hook `0x46EB2B -> 0x400A33` compares `[Projectile+0x5C]` with 8 and inherited `[+0x18]` with 64. The exceptional path increments `+0x18` and rejoins at `0x400A50`; all other paths write `[+0x50]=1`, and both resume the original type/attachment branch. | `Lawn/Projectile.cpp:1146-1160` is `Projectile::Die`; `Lawn/Projectile.h:38,41,49` maps dead/type/attachment; inherited `mX` is `Lawn/GameObject.h:11-22`; `ConstEnums.h:775-791` identifies type 8 as Spike. | `src/Lawn/Projectile.cpp:1153-1168`; `ResolveProjectileDeath` is `src/GameRules/Ruleset.cpp:317-326`. There is intentionally no early return before attachment cleanup. |
| Snow Pea through Torchwood | `ConvertToPea` clears the attachment but writes projectile **type** 1 (Snow Pea), rather than type 0 (Pea). | At `0x46EE12`, the immediate written to `[Projectile+0x5C]` is 1 rather than 0. | `Lawn/Projectile.cpp:1218-1228` is `ConvertToPea` and its original Pea assignment; `Lawn/Projectile.h:41` maps `+0x5C` to type; `ConstEnums.h:775-779` identifies type 1 as Snow Pea. | `src/Lawn/Projectile.cpp:1224-1232`; `ResolveTorchwoodSnowPeaType` is `src/GameRules/Ruleset.cpp:328-331`. |

Earlier live progress notes made two offset errors. First, they mislabeled the Spike death condition as motion `MOTION_FLOAT_OVER` and the Torchwood write as motion `MOTION_LOBBED`, because both pairs use the integers 8 and 1; `+0x5C` proves both are projectile-type operations. Second, they described the star hook as resetting age and briefly misidentified `Board+0x554C`; the complete injected flow proves it changes `Projectile+0x58` from Star to Straight based on `mBackground`, while `Projectile+0x60` age still increments. The same notes called zombie `+0xB4` butter; it is the ice-trap counter, while butter is `+0xB0`. The implementation and typed tests use the corrected interpretations.

## Implemented verified behavior batch 5

This batch restores the self-contained `Board` wave-selection, spawn-eligibility, level-initialization, sky-drop, and loot changes. As in the prior batches, column B uses `ruslan831/PlantsVsZombies-decompilation` commit `20b245acee018fe32804b271fd6400bca06618c1` only to identify the original C++ statement and field behind column A; the PvZ 95 behavior comes from the original/95 executable difference itself.

| Area | PvZ 95 behavior | A: original/95 EXE instruction or data difference | B: reference C++ mapping | C: portable implementation |
|---|---|---|---|---|
| Wave counts | A replayed Adventure level whose original wave count is below 10 expands to 40 rather than 20. Outside Adventure: Survival Normal becomes 20; Survival Hard/Endless becomes 40; Whack-a-Zombie becomes 20; the fixed 20-wave group becomes 40; the fixed 30-wave group becomes 40; and the default 40 becomes 50. Last Stand and zero-wave modes remain unchanged. | `Board::PickZombieWaves` changes the shared 20 constant at `0x4092FC` to 40, Whack's store at `0x4093EC` from 12 to 20, the default store at `0x409460` from 40 to 50, the 30-wave store at `0x40946C` to 40, and the Survival Normal value at `0x409498` from 10 to 20. The shared constant is used by the short-Adventure-replay branch at `0x409394`, the fixed 20-wave group, and Survival Hard/Endless at `0x4094A5`. | `Lawn/Board.cpp:573-794` is `Board::PickZombieWaves`; its first block selects `mNumWaves`, while `GetNumWavesPerSurvivalStage` at line 9748 explains the original Survival 10/20 values. | `ResolveShortAdventureReplayWaveCount` and `ResolveNonAdventureWaveCount` in `src/GameRules/Ruleset.cpp`; callsites are the two wave-count branches in `src/Lawn/Board.cpp:620-649`. Typed tests cover every changed category and the unchanged Last Stand/zero-wave cases. |
| Wave zombie-point multiplier | Column keeps its ×6 multiplier; every other mode uses ×4, so the later original ×3, ×2, and ×1 branches are unreachable in PvZ 95. | At `0x40988A`, the conditional jump taken only for Wall-nut Bowling becomes an unconditional jump to `0x409960`, which performs two consecutive doublings. Column has already taken its separate ×3 then ×2 path at `0x409833`–`0x40983F`; Little Trouble has already selected the same ×4 destination. | The multiplier chain is in `Board::PickZombieWaves`, `Lawn/Board.cpp:680-708`. | Portable first computes the original multiplier, then applies `ResolveZombieWavePointMultiplier`; `src/Lawn/Board.cpp:704-730`. Tests cover Column, ordinary, and an original ×3 mode. |
| Definition spawn gate and allowed-level table | Non-Yeti zombies ignore `mStartingLevel` and zero `mPickWeight`, but still use the per-level table. The table additionally allows Pole-vaulter on level 32, removes Newspaper from levels 11/12, allows Newspaper on level 16, and removes Football from level 32. Yeti retains its separate `CanSpawnYetis` path. | In `CanZombieSpawnOnLevel`, `0x40D6A3` changes `jge 0x40D6A8` to unconditional `jmp`, and `0x40D6AC` replaces the zero-weight rejection jump with NOPs. The five table dwords change at `0x6A3894`, `0x6A39D8`, `0x6A39DC`, `0x6A39EC`, and `0x6A3BC4`; the table has base `0x6A35B0`, stride `0xCC`, with 50 four-byte level flags after the type field. | `Board::CanZombieSpawnOnLevel`, `Lawn/Board.cpp:2369-2384`, names `mStartingLevel`, `mPickWeight`, and `gZombieAllowedLevels`; the table definition is `Lawn/Challenge.cpp:41`, and `Challenge.h:242-249` defines its typed layout. | `ZombiePassesDefinitionSpawnGate` and `ResolveZombieAllowedOnLevel` are called from `src/Lawn/Board.cpp:2374-2393`. Tests cover both bypassed definition checks, all five changed cells, an unchanged cell, and original-ruleset passthrough. |
| Per-pick wave/budget gate | Except for the earlier Survival-Endless Bungee/flag-wave special case, candidate zombies no longer check `mFirstAllowedWave` or whether their value exceeds the wave's remaining zombie points. Later Survival caps and weight adjustments remain active. | At `0x40D917`, the original code compares game mode with Pogo Party and conditionally jumps at `0x40D91A` over the common restriction block. PvZ 95 changes that conditional jump to an unconditional jump to `0x40D984`, for every candidate that reached this branch. | `Board::PickZombieType`, `Lawn/Board.cpp:2435-2523`, contains the Bungee special case followed by the Pogo/Bobsled/Air Raid exception and the shared first-wave/value gate. | `ShouldEnforceZombieWaveBudgetGate` controls only the shared `else if` block at `src/Lawn/Board.cpp:2443-2484`, leaving the prior Bungee and later weight logic intact. |
| Initial seed packets | Ice Challenge's six fixed packets become Peashooter, Sunflower, Cherry Bomb, Wall-nut, Potato Mine, and Snow Pea. Scary Potter's sole initial Cherry Bomb becomes Plantern, including Adventure level 35. | `0x40B283` changes the Ice-mode compare from `0x2A` to `-1`, disabling its special six-packet branch; the unchanged generic fill later assigns seed enum indices 0–5. At `0x40B7E1`, `lea edi,[edx+3]` becomes `[edx+0x1A]`; with `edx=-1`, Scary Potter seed 2 (Cherry Bomb) becomes 25 (Plantern). | Both branches are in `Board::InitLevel`, `Lawn/Board.cpp:1362-1574`; `ConstEnums.h` identifies game mode `0x2A`, Cherry Bomb 2, and Plantern 25. | `ResolveInitialSeedPacket` is used at the Ice and Scary Potter initialization callsites in `src/Lawn/Board.cpp:1431-1540`. The resolver reproduces the observable packet sequence while retaining the portable control flow; tests cover all six Ice slots, Scary Potter, and passthrough. |
| Sky spawning | Ice Challenge no longer suppresses sky drops. Sunny Day now receives normal sun instead of large sun; Big Time receives usable seed packets from the sky instead of normal sun. | `0x413AE1` changes the Ice-mode compare from `0x2A` to `-1`, so it no longer reaches the early return. At `0x413BD1`, the special-mode compare changes from Sunny Day `0x25` to Big Time `0x29`; at `0x413BDF`, the special coin value changes from Large Sun 6 to Usable Seed Packet 16. The default coin remains Sun 4. | `Board::UpdateSunSpawning`, `Lawn/Board.cpp:5301-5331`, contains both the Ice early-return condition and the final Sunny-Day coin selection; `ConstEnums.h:155-176,357-429` supplies the coin/game-mode identities. | `ShouldSuppressSkySunSpawning` and `ResolveFallingSunType` are called at `src/Lawn/Board.cpp:5270-5317`. Tests cover Ice, Sunny Day, Big Time, unaffected modes, and original passthrough. |
| Loot triple-sun branch | Every mode is eligible for the Whack-a-Zombie adaptive sun-drop probability before ordinary loot. A successful drop creates Small Sun, Large Sun, then normal Sun at the original three positions. | In `DropLootPiece`, `0x41CF10` changes the Whack-mode `je 0x41CF2A` to unconditional `jmp`, bypassing the Adventure-level-15 fallback check as well. The first two `AddCoin` type pushes at `0x41CFE5` and `0x41CFF5` change Sun 4 to Small Sun 5 and Large Sun 6; the third remains Sun 4. | `Board::DropLootPiece`, `Lawn/Board.cpp:9443-9587`, contains the `IsWhackAZombieLevel` guard, adaptive thresholds, and three `AddCoin` calls. | `ShouldUseWhackSunDrop` widens the guard and `ResolveWhackSunDropType` supplies the ordered types at `src/Lawn/Board.cpp:9136-9190`. Tests cover the global guard, all three typed results, and original passthrough. |

### Retrospective audit corrections

The full-reference audit now covers 100 documented changes at the document's existing granularity: 22 direct table fields, 11 initial behavior rows, 24 batch-2 behavior rows, 28 batch-3 immediate replacements, 8 batch-4 behavior rows, and 7 batch-5 Board behavior groups. Every group has an A/B/C path from executable evidence, through reference C++ semantics, to the Portable implementation. The audit found five translation-error groups in already-implemented rules and corrected them in code and tests:

- The Screen Door hook changes only the stored member type and adds pre-switch Buckethead armor. The switch still dispatches on the original Screen Door argument, producing a deliberate Buckethead/door-shield hybrid rather than a plain Buckethead.
- The Newspaper chew cave dispatches on zombie **type** 5, then builds intervals 1/2/4 from phase and chill. It does not multiply the ordinary interval for every Newspaper.
- The write of body health 720 at `0x4002F8` tests **phase** `0x1E` (`PHASE_NEWSPAPER_MADDENING`), not `ZOMBIE_SQUASH_HEAD` or a Digger phase.
- The two forced-burn comparisons at `0x4005F1`/`0x4005FB` test phases `0x1D`/`0x1E` (`PHASE_NEWSPAPER_READING`/`PHASE_NEWSPAPER_MADDENING`), not Gatling-head/Squash-head zombie types.
- Two calls to `TakeDamage` depend on live-register values. Blover deliberately carries `mZombiePhase` in EAX, so Portable passes the phase as flags. BurnRow receives AL from the patched movement/animation chain: 3 for post-burn `PHASE_NEWSPAPER_MAD`, otherwise `0xFF`. Portable reproduces these meaningful low bits but intentionally does not reproduce unrelated high EAX pointer garbage; `TakeDamage` only tests the low damage-flag bits.

## Still pending translation

The following groups have been located but are not marked complete. They require decoding their injected branches and then adding behavior tests; merely finding a changed instruction is not treated as implementation.

- Board future/dance/super-mower modes and typing hooks.
- Challenge-specific wave construction and `SpawnZombieWave`'s injected board-state gate. Its nearby current-wave threshold change from 5 to 9 is intentionally pending with that gate so the two dependent edits are not translated separately.
- Remaining Scary Potter 5+ behavior hooks, if later analysis finds any outside the decoded direct type-immediate region.
- Plant bowling and Magnet-shroom changes, plus any remaining non-self-contained Plant hooks.
- Gargantuar actions, board-edge behavior, plant targeting and square squishing; remaining zombie damage branches and special-head behavior.
- Remaining `.rdata` presentation strings.

This list is the migration queue, not a claim that all 164 patch clusters are already understood.

These rules remain switchable with `-ruleset original`; that path preserves the portable engine's original behavior.

## Ruleset selection

PvZ 95 is the default. For A/B testing:

```bash
./pvz95-coop -ruleset pvz95
./pvz95-coop -ruleset original
```

Peers exchange the active ruleset protocol ID during discovery and handshake. Sessions reject a mismatch rather than risk a silent deterministic divergence.
