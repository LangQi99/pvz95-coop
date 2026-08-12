/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "GameRules/Ruleset.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
	void ExpectEqual(std::string_view theName, int theActual, int theExpected)
	{
		if (theActual == theExpected)
			return;

		std::cerr << theName << ": expected " << theExpected << ", got " << theActual << '\n';
		std::exit(1);
	}

	void ExpectNear(std::string_view theName, float theActual, float theExpected)
	{
		if (theActual > theExpected - 0.001f && theActual < theExpected + 0.001f)
			return;

		std::cerr << theName << ": expected " << theExpected << ", got " << theActual << '\n';
		std::exit(1);
	}
}

int main()
{
	using namespace PvzRules;
	struct SeedRuleCase
	{
		GameMode mGameMode;
		int mIndex;
		SeedType mOriginal;
		SeedType mPvZ95;
	};
	struct ZombieRuleCase
	{
		GameMode mGameMode;
		int mIndex;
		ZombieType mOriginal;
		ZombieType mPvZ95;
	};

	const std::array<SeedRuleCase, 10> aScarySeedCases = {{
		{GAMEMODE_SCARY_POTTER_1, 0, SEED_PEASHOOTER, SEED_REPEATER},
		{GAMEMODE_SCARY_POTTER_1, 2, SEED_SQUASH, SEED_PEASHOOTER},
		{GAMEMODE_SCARY_POTTER_2, 0, SEED_LEFTPEATER, SEED_POTATOMINE},
		{GAMEMODE_SCARY_POTTER_2, 1, SEED_SNOWPEA, SEED_ICESHROOM},
		{GAMEMODE_SCARY_POTTER_2, 2, SEED_WALLNUT, SEED_EXPLODE_O_NUT},
		{GAMEMODE_SCARY_POTTER_2, 3, SEED_POTATOMINE, SEED_CHERRYBOMB},
		{GAMEMODE_SCARY_POTTER_3, 4, SEED_WALLNUT, SEED_HYPNOSHROOM},
		{GAMEMODE_SCARY_POTTER_4, 0, SEED_PUFFSHROOM, SEED_BLOVER},
		{GAMEMODE_SCARY_POTTER_4, 1, SEED_HYPNOSHROOM, SEED_POTATOMINE},
		{GAMEMODE_SCARY_POTTER_4, 2, SEED_LEFTPEATER, SEED_BLOVER}
	}};
	const std::array<ZombieRuleCase, 11> aScaryZombieCases = {{
		{GAMEMODE_SCARY_POTTER_1, 5, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_DOOR},
		{GAMEMODE_SCARY_POTTER_2, 4, ZOMBIE_NORMAL, ZOMBIE_FOOTBALL},
		{GAMEMODE_SCARY_POTTER_2, 5, ZOMBIE_PAIL, ZOMBIE_NEWSPAPER},
		{GAMEMODE_SCARY_POTTER_2, 6, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_REDEYE_GARGANTUAR},
		{GAMEMODE_SCARY_POTTER_3, 5, ZOMBIE_NORMAL, ZOMBIE_PAIL},
		{GAMEMODE_SCARY_POTTER_3, 6, ZOMBIE_PAIL, ZOMBIE_NEWSPAPER},
		{GAMEMODE_SCARY_POTTER_3, 7, ZOMBIE_DANCER, ZOMBIE_GARGANTUAR},
		{GAMEMODE_SCARY_POTTER_3, 8, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_FLAG},
		{GAMEMODE_SCARY_POTTER_4, 3, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_TRAFFIC_CONE},
		{GAMEMODE_SCARY_POTTER_4, 4, ZOMBIE_NORMAL, ZOMBIE_POLEVAULTER},
		{GAMEMODE_SCARY_POTTER_4, 5, ZOMBIE_FOOTBALL, ZOMBIE_DANCER}
	}};

	SetActiveRuleset(RulesetId::ORIGINAL);
	ExpectEqual("original potato cost", ResolvePlantSeedCost(SeedType::SEED_POTATOMINE, 25), 25);
	ExpectEqual("original star damage", ResolveProjectileDamage(ProjectileType::PROJECTILE_STAR, 20), 20);
	ExpectEqual("original door member type", ResolveZombieMemberType(ZombieType::ZOMBIE_DOOR), ZombieType::ZOMBIE_DOOR);
	const ZombiePreSwitchArmor anOriginalDoorArmor = ResolveZombiePreSwitchArmor(
		ZombieType::ZOMBIE_DOOR, HelmType::HELMTYPE_NONE, 0);
	ExpectEqual("original door pre-switch helmet", anOriginalDoorArmor.mHelmType, HelmType::HELMTYPE_NONE);
	ExpectEqual("original door pre-switch helmet health", anOriginalDoorArmor.mHelmHealth, 0);
	ExpectEqual("original flag health", ResolveZombieInitialBodyHealth(ZombieType::ZOMBIE_FLAG, 270), 270);
	ExpectEqual("original layered tall-nut crush", TakesLayeredCrushDamage(SeedType::SEED_TALLNUT), false);
	ExpectEqual("original cattail counter-fifty shot", ShootsAtCounterFifty(SeedType::SEED_CATTAIL), true);
	ExpectEqual("original gatling counter-fifty shot", ShootsAtCounterFifty(SeedType::SEED_GATLINGPEA), false);
	ExpectEqual("original chomper boss bite", ChomperOnlyDamagesZombie(ZombieType::ZOMBIE_BOSS), true);
	ExpectEqual("original chilled eat interval", ResolveZombieEatInterval(
		ZombieType::ZOMBIE_NEWSPAPER, ZombiePhase::PHASE_NEWSPAPER_READING, true, 4), 8);
	ExpectEqual("original cold removal", ResolveChillAfterRemovingCold(500), 0);
	ExpectEqual("original maximum sun", ResolveMaximumSunMoney(9990), 9990);
	ExpectEqual("original short replay wave count", ResolveShortAdventureReplayWaveCount(20), 20);
	ExpectEqual("original non-adventure wave count", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1, 10), 10);
	ExpectEqual("original zombie-point multiplier", ResolveZombieWavePointMultiplier(
		GameMode::GAMEMODE_ADVENTURE, 1), 1);
	ExpectEqual("original early zombie definition gate", ZombiePassesDefinitionSpawnGate(3, 5, 10), false);
	ExpectEqual("original zero-weight zombie definition gate", ZombiePassesDefinitionSpawnGate(5, 5, 0), false);
	ExpectEqual("original zombie allowed table value", ResolveZombieAllowedOnLevel(
		ZombieType::ZOMBIE_POLEVAULTER, 32, false), false);
	ExpectEqual("original zombie wave-budget gate", ShouldEnforceZombieWaveBudgetGate(
		GameMode::GAMEMODE_ADVENTURE, true), true);
	ExpectEqual("original Ice initial seed", ResolveInitialSeedPacket(
		GameMode::GAMEMODE_CHALLENGE_ICE, false, 1, SeedType::SEED_CHERRYBOMB), SeedType::SEED_CHERRYBOMB);
	ExpectEqual("original Ice suppresses sky sun", ShouldSuppressSkySunSpawning(
		GameMode::GAMEMODE_CHALLENGE_ICE, true), true);
	ExpectEqual("original Sunny Day falling sun", ResolveFallingSunType(
		GameMode::GAMEMODE_CHALLENGE_SUNNY_DAY, CoinType::COIN_LARGESUN), CoinType::COIN_LARGESUN);
	ExpectEqual("original non-Whack sun drop gate", ShouldUseWhackSunDrop(false), false);
	ExpectEqual("original Whack sun type", ResolveWhackSunDropType(0, CoinType::COIN_SUN), CoinType::COIN_SUN);
	ExpectEqual("original raining seeds countdown", ResolveRainingSeedsCountdown(123), 623);
	ExpectEqual("original portal conveyor seed", ResolveConveyorSeed(GAMEMODE_CHALLENGE_PORTAL_COMBAT, 0, SEED_PEASHOOTER), SEED_PEASHOOTER);
	ExpectEqual("original whack group size", ResolveWhackZombieGroupSize(1), 1);
	ExpectEqual("original whack speed curve", ResolveWhackZombieSpeedCurveStart(1), 1);
	const BurnRowEffects anOriginalBurnRow = ResolveBurnRowEffects();
	ExpectEqual("original burn row sequence", anOriginalBurnRow.mUseSpecialSequence, false);
	ExpectEqual("original burn row damage flags", ResolveBurnRowDamageFlags(PHASE_ZOMBIE_NORMAL, 7U), 7);
	ExpectEqual("original Blover normal phase", ShouldBloverBlowZombie(PHASE_ZOMBIE_NORMAL, false), false);
	ExpectEqual("original Blover flying phase", ShouldBloverBlowZombie(PHASE_BALLOON_FLYING, true), true);
	ExpectEqual("original Blover damage", ResolveBloverDamage(false, false, 0), 0);
	ExpectEqual("original Blover damage flags", ResolveBloverDamageFlags(PHASE_NEWSPAPER_MAD, 0U), 0);
	ExpectEqual("original Blover cone health", ResolveBloverConeHelmHealth(HELMTYPE_TRAFFIC_CONE, 370), 370);
	ExpectEqual("original fog countdown", ResolveFogBlownCountdown(4000), 4000);
	ExpectEqual("original homing collision", UsesHomingTargetOnlyCollision(MOTION_HOMING), true);
	ExpectEqual("original non-homing collision", UsesHomingTargetOnlyCollision(MOTION_FLOAT_OVER), false);
	ExpectEqual("original star motion", ResolveProjectileMotionBeforeUpdate(
		MOTION_STAR, BACKGROUND_3_POOL, 56), MOTION_STAR);
	const ProjectileDeathState anOriginalSpikeDeath = ResolveProjectileDeath(PROJECTILE_SPIKE, 63);
	ExpectEqual("original spike dies", anOriginalSpikeDeath.mDead, true);
	ExpectEqual("original spike death x", anOriginalSpikeDeath.mX, 63);
	ExpectEqual("original torchwood snow pea", ResolveTorchwoodSnowPeaType(PROJECTILE_PEA), PROJECTILE_PEA);
	for (const SeedRuleCase& aCase : aScarySeedCases)
		ExpectEqual("original Scary Potter seed", ResolveScaryPotterSeed(aCase.mGameMode, aCase.mIndex, aCase.mOriginal), aCase.mOriginal);
	for (const ZombieRuleCase& aCase : aScaryZombieCases)
		ExpectEqual("original Scary Potter zombie", ResolveScaryPotterZombie(aCase.mGameMode, aCase.mIndex, aCase.mOriginal), aCase.mOriginal);

	if (!SetActiveRuleset("pvz95"))
		return 1;

	ExpectEqual("sunflower production", ResolvePlantLaunchRate(SeedType::SEED_SUNFLOWER, 2500), 3300);
	ExpectEqual("potato mine cost", ResolvePlantSeedCost(SeedType::SEED_POTATOMINE, 25), 50);
	ExpectEqual("sun-shroom production", ResolvePlantLaunchRate(SeedType::SEED_SUNSHROOM, 2500), 3300);
	ExpectEqual("grave buster cost", ResolvePlantSeedCost(SeedType::SEED_GRAVEBUSTER, 75), 50);
	ExpectEqual("hypno-shroom cost", ResolvePlantSeedCost(SeedType::SEED_HYPNOSHROOM, 75), 100);
	ExpectEqual("tall-nut cost", ResolvePlantSeedCost(SeedType::SEED_TALLNUT, 125), 175);
	ExpectEqual("tall-nut refresh", ResolvePlantRefreshTime(SeedType::SEED_TALLNUT, 3000), 2000);
	ExpectEqual("sea-shroom refresh", ResolvePlantRefreshTime(SeedType::SEED_SEASHROOM, 3000), 1500);
	ExpectEqual("cactus cost", ResolvePlantSeedCost(SeedType::SEED_CACTUS, 125), 200);
	ExpectEqual("blover cost", ResolvePlantSeedCost(SeedType::SEED_BLOVER, 100), 200);
	ExpectEqual("blover refresh", ResolvePlantRefreshTime(SeedType::SEED_BLOVER, 750), 2000);
	ExpectEqual("starfruit cost", ResolvePlantSeedCost(SeedType::SEED_STARFRUIT, 125), 250);
	ExpectEqual("starfruit launch rate", ResolvePlantLaunchRate(SeedType::SEED_STARFRUIT, 150), 200);
	ExpectEqual("marigold cost", ResolvePlantSeedCost(SeedType::SEED_MARIGOLD, 50), 75);
	ExpectEqual("marigold refresh", ResolvePlantRefreshTime(SeedType::SEED_MARIGOLD, 3000), 1500);
	ExpectEqual("marigold production", ResolvePlantLaunchRate(SeedType::SEED_MARIGOLD, 2500), 3300);
	ExpectEqual("gatling pea cost", ResolvePlantSeedCost(SeedType::SEED_GATLINGPEA, 250), 450);
	ExpectEqual("cattail cost", ResolvePlantSeedCost(SeedType::SEED_CATTAIL, 225), 275);
	ExpectEqual("cattail launch rate", ResolvePlantLaunchRate(SeedType::SEED_CATTAIL, 150), 75);
	ExpectEqual("explodo-nut cost", ResolvePlantSeedCost(SeedType::SEED_EXPLODE_O_NUT, 0), 150);
	ExpectEqual("gatling counter-fifty shot", ShootsAtCounterFifty(SeedType::SEED_GATLINGPEA), true);
	ExpectEqual("cattail counter-fifty shot removed", ShootsAtCounterFifty(SeedType::SEED_CATTAIL), false);
	ExpectEqual("marigold large sun", ResolveMarigoldCoinType(49, CoinType::COIN_GOLD), CoinType::COIN_LARGESUN);
	ExpectEqual("marigold normal sun", ResolveMarigoldCoinType(50, CoinType::COIN_SILVER), CoinType::COIN_SUN);
	ExpectEqual("big time marigold sun", ResolveBigTimeMarigoldCoinType(CoinType::COIN_SILVER), CoinType::COIN_SUN);
	ExpectEqual("chomper football bite", ChomperOnlyDamagesZombie(ZombieType::ZOMBIE_FOOTBALL), true);
	ExpectEqual("chomper boss swallow", ChomperOnlyDamagesZombie(ZombieType::ZOMBIE_BOSS), false);
	ExpectEqual("chomper digest time", ResolveChomperDigestTime(4000), 2500);
	ExpectEqual("squash attack x", ResolvePlantAttackRectX(SeedType::SEED_SQUASH, 100, 120), 84);
	ExpectEqual("squash attack width", ResolvePlantAttackRectWidth(SeedType::SEED_SQUASH, 45), 128);
	ExpectEqual("chomper attack width", ResolvePlantAttackRectWidth(SeedType::SEED_CHOMPER, 40), 150);
	ExpectEqual("fume-shroom board-wide attack", ResolvePlantAttackRectWidth(SeedType::SEED_FUMESHROOM, 340), 800);
	ExpectEqual("star damage", ResolveProjectileDamage(ProjectileType::PROJECTILE_STAR, 20), 40);
	ExpectEqual("spike damage", ResolveProjectileDamage(ProjectileType::PROJECTILE_SPIKE, 20), 1);
	ExpectEqual("potato mine arming time", ResolvePlantInitialStateCountdown(SeedType::SEED_POTATOMINE, 1500), 1000);
	ExpectEqual("sun-shroom growth time", ResolvePlantInitialStateCountdown(SeedType::SEED_SUNSHROOM, 12000), 9000);
	ExpectEqual("spikerock health", ResolvePlantInitialHealth(SeedType::SEED_SPIKEROCK, 450), 16200);
	ExpectEqual("spikerock crush damage", ResolveSpikeRockCrushDamage(50), 1800);
	ExpectEqual("spikerock damage state one", ResolveSpikeRockDamageThreshold(1, 300), 10800);
	ExpectEqual("spikerock damage state two", ResolveSpikeRockDamageThreshold(2, 150), 5400);
	ExpectEqual("explodo-nut cherry special", UsesCherryBombSpecial(SeedType::SEED_EXPLODE_O_NUT), true);
	ExpectEqual("layered tall-nut crush", TakesLayeredCrushDamage(SeedType::SEED_TALLNUT), true);
	ExpectEqual("screen door stores bucket member type", ResolveZombieMemberType(
		ZombieType::ZOMBIE_DOOR), ZombieType::ZOMBIE_PAIL);
	const ZombiePreSwitchArmor aPvZ95DoorArmor = ResolveZombiePreSwitchArmor(
		ZombieType::ZOMBIE_PAIL, HelmType::HELMTYPE_NONE, 0);
	ExpectEqual("screen door receives bucket helmet", aPvZ95DoorArmor.mHelmType, HelmType::HELMTYPE_PAIL);
	ExpectEqual("screen door receives bucket helmet health", aPvZ95DoorArmor.mHelmHealth, 1100);
	ExpectEqual("flag zombie health", ResolveZombieInitialBodyHealth(ZombieType::ZOMBIE_FLAG, 270), 820);
	ExpectEqual("dancer zombie health", ResolveZombieInitialBodyHealth(ZombieType::ZOMBIE_DANCER, 500), 1350);
	ExpectEqual("football helmet health", ResolveZombieInitialHelmHealth(ZombieType::ZOMBIE_FOOTBALL, 1400), 2800);
	ExpectEqual("newspaper shield health", ResolveZombieInitialShieldHealth(ZombieType::ZOMBIE_NEWSPAPER, 150), 1200);
	ExpectEqual("bungee steal delay", ResolveBungeeStealDelay(300), 0);
	ExpectEqual("flag uses yeti update", UsesYetiUpdate(ZombieType::ZOMBIE_FLAG), true);
	ExpectEqual("yeti update replaced", UsesYetiUpdate(ZombieType::ZOMBIE_YETI), false);
	ExpectEqual("normal eat interval", ResolveZombieEatInterval(
		ZombieType::ZOMBIE_NORMAL, ZombiePhase::PHASE_ZOMBIE_NORMAL, false, 4), 8);
	ExpectEqual("chilled eat interval", ResolveZombieEatInterval(
		ZombieType::ZOMBIE_NORMAL, ZombiePhase::PHASE_ZOMBIE_NORMAL, true, 4), 16);
	ExpectEqual("newspaper non-reading eat interval", ResolveZombieEatInterval(
		ZombieType::ZOMBIE_NEWSPAPER, ZombiePhase::PHASE_NEWSPAPER_MAD, false, 4), 1);
	ExpectEqual("chilled newspaper non-reading eat interval", ResolveZombieEatInterval(
		ZombieType::ZOMBIE_NEWSPAPER, ZombiePhase::PHASE_NEWSPAPER_MAD, true, 4), 2);
	ExpectEqual("newspaper reading eat interval", ResolveZombieEatInterval(
		ZombieType::ZOMBIE_NEWSPAPER, ZombiePhase::PHASE_NEWSPAPER_READING, false, 4), 2);
	ExpectEqual("chilled newspaper reading eat interval", ResolveZombieEatInterval(
		ZombieType::ZOMBIE_NEWSPAPER, ZombiePhase::PHASE_NEWSPAPER_READING, true, 4), 4);
	ExpectEqual("zombie eat damage", ResolveZombieEatDamage(4), 8);
	ExpectEqual("I, Zombie sunflower reward", ResolveIZombieSunflowerReward(CoinType::COIN_SUN), CoinType::COIN_SMALLSUN);
	ExpectEqual("eaten tall-nut transforms", ResolveEatenPlantSeedType(SeedType::SEED_TALLNUT, 299), SeedType::SEED_SQUASH);
	ExpectEqual("healthy tall-nut stays", ResolveEatenPlantSeedType(SeedType::SEED_TALLNUT, 300), SeedType::SEED_TALLNUT);
	ExpectEqual("low explodo-nut triggers", EatenPlantTransformTriggersSpecial(SeedType::SEED_EXPLODE_O_NUT, 39), true);
	ExpectEqual("40-health explodo-nut waits", EatenPlantTransformTriggersSpecial(SeedType::SEED_EXPLODE_O_NUT, 40), false);
	const ZombieMindControlStats aMindControlStats = ResolveMindControlStats(ZombieType::ZOMBIE_PAIL,
		50, 270, 20, 1100, 0, 0, 500, 1.0f);
	ExpectEqual("mind control body health", aMindControlStats.mBodyHealth, 470);
	ExpectEqual("mind control helmet health", aMindControlStats.mHelmHealth, 1300);
	ExpectEqual("mind control shield health", aMindControlStats.mShieldHealth, 200);
	ExpectEqual("mind control removes chill", aMindControlStats.mChilled, 0);
	ExpectNear("mind control scale", aMindControlStats.mScale, 1.25f);
	const ZombieMindControlStats aNewspaperMindControlStats = ResolveMindControlStats(ZombieType::ZOMBIE_NEWSPAPER,
		270, 270, 0, 0, 100, 1200, 0, 1.0f);
	ExpectEqual("mind controlled newspaper body", aNewspaperMindControlStats.mBodyHealth, 920);
	ExpectEqual("newspaper maddening post-damage health", ResolveZombieBodyHealthAfterDamage(
		ZombiePhase::PHASE_NEWSPAPER_MADDENING, 13), 720);
	ExpectEqual("newspaper mad post-damage health unchanged", ResolveZombieBodyHealthAfterDamage(
		ZombiePhase::PHASE_NEWSPAPER_MAD, 13), 13);
	ExpectEqual("burn uses combined health", ShouldTakeBurnDamage(
		ZombieType::ZOMBIE_PAIL, ZombiePhase::PHASE_ZOMBIE_NORMAL, 700, 1100, 0), true);
	ExpectEqual("newspaper reading takes burn damage", ShouldTakeBurnDamage(
		ZombieType::ZOMBIE_NEWSPAPER, ZombiePhase::PHASE_NEWSPAPER_READING, 270, 0, 0), true);
	ExpectEqual("newspaper maddening takes burn damage", ShouldTakeBurnDamage(
		ZombieType::ZOMBIE_NEWSPAPER, ZombiePhase::PHASE_NEWSPAPER_MADDENING, 270, 0, 0), true);
	ExpectEqual("newspaper mad is not an injected burn phase", ShouldTakeBurnDamage(
		ZombieType::ZOMBIE_NEWSPAPER, ZombiePhase::PHASE_NEWSPAPER_MAD, 270, 0, 0), false);
	ExpectEqual("gatling-head is not a phase alias", ShouldTakeBurnDamage(
		ZombieType::ZOMBIE_GATLING_HEAD, ZombiePhase::PHASE_ZOMBIE_NORMAL, 270, 0, 0), false);
	const ZombieStatusCounters aButterStatus = ResolveButterStatus(25, 0, 0);
	ExpectEqual("butter applies chill", aButterStatus.mChilled, 1000);
	ExpectEqual("butter does not immobilize as butter", aButterStatus.mButtered, 0);
	ExpectEqual("butter applies ice trap", aButterStatus.mIceTrapped, 300);
	ExpectEqual("cold removal reapplies chill", ResolveChillAfterRemovingCold(500), 1000);
	ExpectEqual("newspaper mad movement flag", IsForcedChilledMovement(ZombiePhase::PHASE_NEWSPAPER_MAD), true);
	ExpectNear("newspaper mad animation", ResolveZombieAnimationRate(ZombiePhase::PHASE_NEWSPAPER_MAD, 0, true, 10.0f), 25.0f);
	ExpectNear("chilled newspaper mad animation", ResolveZombieAnimationRate(ZombiePhase::PHASE_NEWSPAPER_MAD, 1, true, 10.0f), 12.5f);
	ExpectEqual("last stand initial sun", ResolveInitialSunMoney(GameMode::GAMEMODE_CHALLENGE_LAST_STAND, 5000), 8000);
	ExpectEqual("maximum sun", ResolveMaximumSunMoney(9990), 2000000000);
	ExpectEqual("short replay wave count", ResolveShortAdventureReplayWaveCount(20), 40);
	ExpectEqual("normal survival wave count", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1, 10), 20);
	ExpectEqual("hard survival wave count", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_1, 20), 40);
	ExpectEqual("Whack-a-Zombie wave count", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_CHALLENGE_WHACK_A_ZOMBIE, 12), 20);
	ExpectEqual("fixed twenty-wave challenge count", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT, 20), 40);
	ExpectEqual("fixed thirty-wave challenge count", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_CHALLENGE_COLUMN, 30), 40);
	ExpectEqual("default challenge wave count", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_CHALLENGE_SPEED, 40), 50);
	ExpectEqual("Last Stand wave count unchanged", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_CHALLENGE_LAST_STAND, 10), 10);
	ExpectEqual("zero-wave challenge count unchanged", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN, 0), 0);
	ExpectEqual("Column zombie-point multiplier", ResolveZombieWavePointMultiplier(
		GameMode::GAMEMODE_CHALLENGE_COLUMN, 6), 6);
	ExpectEqual("ordinary zombie-point multiplier", ResolveZombieWavePointMultiplier(
		GameMode::GAMEMODE_ADVENTURE, 1), 4);
	ExpectEqual("mini-boss zombie-point multiplier", ResolveZombieWavePointMultiplier(
		GameMode::GAMEMODE_CHALLENGE_FINAL_BOSS, 3), 4);
	ExpectEqual("early zombie passes definition gate", ZombiePassesDefinitionSpawnGate(3, 5, 10), true);
	ExpectEqual("zero-weight zombie passes definition gate", ZombiePassesDefinitionSpawnGate(5, 5, 0), true);
	ExpectEqual("Pole-vaulter allowed on level 32", ResolveZombieAllowedOnLevel(
		ZombieType::ZOMBIE_POLEVAULTER, 32, false), true);
	ExpectEqual("Newspaper removed from level 11", ResolveZombieAllowedOnLevel(
		ZombieType::ZOMBIE_NEWSPAPER, 11, true), false);
	ExpectEqual("Newspaper removed from level 12", ResolveZombieAllowedOnLevel(
		ZombieType::ZOMBIE_NEWSPAPER, 12, true), false);
	ExpectEqual("Newspaper allowed on level 16", ResolveZombieAllowedOnLevel(
		ZombieType::ZOMBIE_NEWSPAPER, 16, false), true);
	ExpectEqual("Football removed from level 32", ResolveZombieAllowedOnLevel(
		ZombieType::ZOMBIE_FOOTBALL, 32, true), false);
	ExpectEqual("unmodified zombie allowed table value", ResolveZombieAllowedOnLevel(
		ZombieType::ZOMBIE_NORMAL, 32, true), true);
	ExpectEqual("ordinary zombie wave-budget gate bypassed", ShouldEnforceZombieWaveBudgetGate(
		GameMode::GAMEMODE_ADVENTURE, true), false);
	ExpectEqual("already-exempt zombie wave-budget gate", ShouldEnforceZombieWaveBudgetGate(
		GameMode::GAMEMODE_CHALLENGE_POGO_PARTY, false), false);
	const std::array<SeedType, 6> aPvZ95IceSeeds = {{
		SeedType::SEED_PEASHOOTER, SeedType::SEED_SUNFLOWER, SeedType::SEED_CHERRYBOMB,
		SeedType::SEED_WALLNUT, SeedType::SEED_POTATOMINE, SeedType::SEED_SNOWPEA
	}};
	const std::array<SeedType, 6> anOriginalIceSeeds = {{
		SeedType::SEED_PEASHOOTER, SeedType::SEED_CHERRYBOMB, SeedType::SEED_WALLNUT,
		SeedType::SEED_REPEATER, SeedType::SEED_SNOWPEA, SeedType::SEED_CHOMPER
	}};
	for (int i = 0; i < static_cast<int>(aPvZ95IceSeeds.size()); ++i)
	{
		ExpectEqual("PvZ 95 Ice initial seed", ResolveInitialSeedPacket(
			GameMode::GAMEMODE_CHALLENGE_ICE, false, i, anOriginalIceSeeds[i]), aPvZ95IceSeeds[i]);
	}
	ExpectEqual("Scary Potter initial Plantern", ResolveInitialSeedPacket(
		GameMode::GAMEMODE_ADVENTURE, true, 0, SeedType::SEED_CHERRYBOMB), SeedType::SEED_PLANTERN);
	ExpectEqual("unmodified initial seed", ResolveInitialSeedPacket(
		GameMode::GAMEMODE_ADVENTURE, false, 0, SeedType::SEED_CHERRYBOMB), SeedType::SEED_CHERRYBOMB);
	ExpectEqual("Ice permits sky sun", ShouldSuppressSkySunSpawning(
		GameMode::GAMEMODE_CHALLENGE_ICE, true), false);
	ExpectEqual("unmodified sky-sun suppression", ShouldSuppressSkySunSpawning(
		GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN, true), true);
	ExpectEqual("Big Time falling seed packet", ResolveFallingSunType(
		GameMode::GAMEMODE_CHALLENGE_BIG_TIME, CoinType::COIN_SUN), CoinType::COIN_USABLE_SEED_PACKET);
	ExpectEqual("Sunny Day falling normal sun", ResolveFallingSunType(
		GameMode::GAMEMODE_CHALLENGE_SUNNY_DAY, CoinType::COIN_LARGESUN), CoinType::COIN_SUN);
	ExpectEqual("ordinary falling sun unchanged", ResolveFallingSunType(
		GameMode::GAMEMODE_ADVENTURE, CoinType::COIN_SUN), CoinType::COIN_SUN);
	ExpectEqual("all modes use Whack sun-drop branch", ShouldUseWhackSunDrop(false), true);
	ExpectEqual("first Whack drop is small sun", ResolveWhackSunDropType(
		0, CoinType::COIN_SUN), CoinType::COIN_SMALLSUN);
	ExpectEqual("second Whack drop is large sun", ResolveWhackSunDropType(
		1, CoinType::COIN_SUN), CoinType::COIN_LARGESUN);
	ExpectEqual("third Whack drop is normal sun", ResolveWhackSunDropType(
		2, CoinType::COIN_SUN), CoinType::COIN_SUN);
	ExpectEqual("Beghouled winning score", ResolveBeghouledWinningScore(75), 100);
	ExpectEqual("raining seeds countdown", ResolveRainingSeedsCountdown(123), 323);
	ExpectEqual("Portal Combat threepeater", ResolveConveyorSeed(GAMEMODE_CHALLENGE_PORTAL_COMBAT, 0, SEED_PEASHOOTER), SEED_THREEPEATER);
	ExpectEqual("Portal Combat explodo-nut", ResolveConveyorSeed(GAMEMODE_CHALLENGE_PORTAL_COMBAT, 4, SEED_WALLNUT), SEED_EXPLODE_O_NUT);
	ExpectEqual("Portal Combat doom-shroom", ResolveConveyorSeed(GAMEMODE_CHALLENGE_PORTAL_COMBAT, 5, SEED_CHERRYBOMB), SEED_DOOMSHROOM);
	ExpectEqual("Invisighoul threepeater", ResolveConveyorSeed(GAMEMODE_CHALLENGE_INVISIGHOUL, 0, SEED_PEASHOOTER), SEED_THREEPEATER);
	ExpectEqual("Invisighoul cherry bomb", ResolveConveyorSeed(GAMEMODE_CHALLENGE_INVISIGHOUL, 3, SEED_SQUASH), SEED_CHERRYBOMB);
	ExpectEqual("unmodified conveyor seed", ResolveConveyorSeed(GAMEMODE_CHALLENGE_PORTAL_COMBAT, 1, SEED_REPEATER), SEED_REPEATER);
	ExpectEqual("Whack-a-Zombie group size", ResolveWhackZombieGroupSize(1), 2);
	ExpectEqual("Whack-a-Zombie speed curve", ResolveWhackZombieSpeedCurveStart(1), 3);
	for (const SeedRuleCase& aCase : aScarySeedCases)
		ExpectEqual("PvZ 95 Scary Potter seed", ResolveScaryPotterSeed(aCase.mGameMode, aCase.mIndex, aCase.mOriginal), aCase.mPvZ95);
	for (const ZombieRuleCase& aCase : aScaryZombieCases)
		ExpectEqual("PvZ 95 Scary Potter zombie", ResolveScaryPotterZombie(aCase.mGameMode, aCase.mIndex, aCase.mOriginal), aCase.mPvZ95);
	ExpectEqual("unmodified Scary Potter seed", ResolveScaryPotterSeed(GAMEMODE_SCARY_POTTER_3, 0, SEED_LEFTPEATER), SEED_LEFTPEATER);
	ExpectEqual("unmodified Scary Potter zombie", ResolveScaryPotterZombie(GAMEMODE_SCARY_POTTER_1, 3, ZOMBIE_NORMAL), ZOMBIE_NORMAL);
	const BurnRowEffects aPvZ95BurnRow = ResolveBurnRowEffects();
	ExpectEqual("PvZ 95 burn row sequence", aPvZ95BurnRow.mUseSpecialSequence, true);
	ExpectEqual("PvZ 95 burn row chill", aPvZ95BurnRow.mChilled, 2500);
	ExpectEqual("PvZ 95 burn row ice trap", aPvZ95BurnRow.mIceTrapCounter, 750);
	ExpectEqual("PvZ 95 burn row extra damage", aPvZ95BurnRow.mExtraDamage, 1000);
	ExpectEqual("PvZ 95 burn row normal-phase damage flags", ResolveBurnRowDamageFlags(
		PHASE_ZOMBIE_NORMAL, 0U), 0xFF);
	ExpectEqual("PvZ 95 burn row newspaper-mad damage flags", ResolveBurnRowDamageFlags(
		PHASE_NEWSPAPER_MAD, 0U), 3);
	const std::array<ZombiePhase, 10> aBloverTargets = {{
		PHASE_POLEVAULTER_IN_VAULT, PHASE_BOBSLED_BOARDING, PHASE_POGO_BOUNCING,
		PHASE_POGO_HIGH_BOUNCE_1, PHASE_POGO_FORWARD_BOUNCE_2, PHASE_DOLPHIN_INTO_POOL,
		PHASE_DOLPHIN_IN_JUMP, PHASE_SNORKEL_INTO_POOL, PHASE_IMP_GETTING_THROWN,
		PHASE_BALLOON_FLYING
	}};
	for (ZombiePhase aZombiePhase : aBloverTargets)
		ExpectEqual("PvZ 95 Blover target phase", ShouldBloverBlowZombie(aZombiePhase, false), true);
	ExpectEqual("PvZ 95 Blover excludes normal phase", ShouldBloverBlowZombie(PHASE_ZOMBIE_NORMAL, true), false);
	ExpectEqual("PvZ 95 Blover damages non-blown enemy", ResolveBloverDamage(false, false, 0), 50);
	ExpectEqual("PvZ 95 Blover preserves phase as damage flags", ResolveBloverDamageFlags(
		PHASE_NEWSPAPER_MAD, 0U), static_cast<unsigned int>(PHASE_NEWSPAPER_MAD));
	ExpectEqual("PvZ 95 Blover does not damage blown enemy", ResolveBloverDamage(false, true, 0), 0);
	ExpectEqual("PvZ 95 Blover spares mind-controlled", ResolveBloverDamage(true, false, 0), 0);
	ExpectEqual("PvZ 95 Blover normalizes cone health", ResolveBloverConeHelmHealth(HELMTYPE_TRAFFIC_CONE, 370), 50);
	ExpectEqual("PvZ 95 Blover preserves bucket health", ResolveBloverConeHelmHealth(HELMTYPE_PAIL, 1100), 1100);
	ExpectEqual("PvZ 95 fog countdown", ResolveFogBlownCountdown(4000), 10000);
	ExpectEqual("PvZ 95 homing uses generic collision", UsesHomingTargetOnlyCollision(MOTION_HOMING), false);
	ExpectEqual("typed motion/type value overlap", static_cast<int>(PROJECTILE_BASKETBALL), static_cast<int>(MOTION_HOMING));
	ExpectEqual("typed spike/float value overlap", static_cast<int>(PROJECTILE_SPIKE), static_cast<int>(MOTION_FLOAT_OVER));
	ExpectEqual("typed snow/lobbed value overlap", static_cast<int>(PROJECTILE_SNOWPEA), static_cast<int>(MOTION_LOBBED));
	ExpectEqual("PvZ 95 pool star remains before threshold", ResolveProjectileMotionBeforeUpdate(
		MOTION_STAR, BACKGROUND_3_POOL, 55), MOTION_STAR);
	ExpectEqual("PvZ 95 pool star becomes straight", ResolveProjectileMotionBeforeUpdate(
		MOTION_STAR, BACKGROUND_3_POOL, 56), MOTION_STRAIGHT);
	ExpectEqual("PvZ 95 fog star becomes straight", ResolveProjectileMotionBeforeUpdate(
		MOTION_STAR, BACKGROUND_4_FOG, 56), MOTION_STRAIGHT);
	ExpectEqual("PvZ 95 day star remains before threshold", ResolveProjectileMotionBeforeUpdate(
		MOTION_STAR, BACKGROUND_1_DAY, 63), MOTION_STAR);
	ExpectEqual("PvZ 95 day star becomes straight", ResolveProjectileMotionBeforeUpdate(
		MOTION_STAR, BACKGROUND_1_DAY, 64), MOTION_STRAIGHT);
	ExpectEqual("PvZ 95 non-star motion unchanged", ResolveProjectileMotionBeforeUpdate(
		MOTION_HOMING, BACKGROUND_3_POOL, 99), MOTION_HOMING);
	const ProjectileDeathState aPvZ95YoungSpikeDeath = ResolveProjectileDeath(PROJECTILE_SPIKE, 63);
	ExpectEqual("PvZ 95 young spike survives", aPvZ95YoungSpikeDeath.mDead, false);
	ExpectEqual("PvZ 95 young spike x increment", aPvZ95YoungSpikeDeath.mX, 64);
	const ProjectileDeathState aPvZ95OldSpikeDeath = ResolveProjectileDeath(PROJECTILE_SPIKE, 64);
	ExpectEqual("PvZ 95 spike at boundary dies", aPvZ95OldSpikeDeath.mDead, true);
	const ProjectileDeathState aPvZ95PeaDeath = ResolveProjectileDeath(PROJECTILE_PEA, 10);
	ExpectEqual("PvZ 95 pea still dies", aPvZ95PeaDeath.mDead, true);
	ExpectEqual("PvZ 95 torchwood snow pea remains snow", ResolveTorchwoodSnowPeaType(PROJECTILE_PEA), PROJECTILE_SNOWPEA);

	if (SetActiveRuleset("not-a-ruleset"))
		return 1;

	std::cout << "PvZ 95 ruleset tests passed\n";
	return 0;
}
