/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "GameRules/Ruleset.h"

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

	SetActiveRuleset(RulesetId::ORIGINAL);
	ExpectEqual("original potato cost", ResolvePlantSeedCost(SeedType::SEED_POTATOMINE, 25), 25);
	ExpectEqual("original star damage", ResolveProjectileDamage(ProjectileType::PROJECTILE_STAR, 20), 20);
	ExpectEqual("original door type", ResolveZombieType(ZombieType::ZOMBIE_DOOR), ZombieType::ZOMBIE_DOOR);
	ExpectEqual("original flag health", ResolveZombieInitialBodyHealth(ZombieType::ZOMBIE_FLAG, 270), 270);
	ExpectEqual("original layered tall-nut crush", TakesLayeredCrushDamage(SeedType::SEED_TALLNUT), false);
	ExpectEqual("original cattail counter-fifty shot", ShootsAtCounterFifty(SeedType::SEED_CATTAIL), true);
	ExpectEqual("original gatling counter-fifty shot", ShootsAtCounterFifty(SeedType::SEED_GATLINGPEA), false);
	ExpectEqual("original chomper boss bite", ChomperOnlyDamagesZombie(ZombieType::ZOMBIE_BOSS), true);
	ExpectEqual("original chilled eat interval", ResolveZombieEatInterval(ZombiePhase::PHASE_NEWSPAPER_READING, true, 4), 8);
	ExpectEqual("original cold removal", ResolveChillAfterRemovingCold(500), 0);
	ExpectEqual("original maximum sun", ResolveMaximumSunMoney(9990), 9990);
	ExpectEqual("original raining seeds countdown", ResolveRainingSeedsCountdown(123), 623);

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
	ExpectEqual("screen door becomes bucket", ResolveZombieType(ZombieType::ZOMBIE_DOOR), ZombieType::ZOMBIE_PAIL);
	ExpectEqual("flag zombie health", ResolveZombieInitialBodyHealth(ZombieType::ZOMBIE_FLAG, 270), 820);
	ExpectEqual("dancer zombie health", ResolveZombieInitialBodyHealth(ZombieType::ZOMBIE_DANCER, 500), 1350);
	ExpectEqual("football helmet health", ResolveZombieInitialHelmHealth(ZombieType::ZOMBIE_FOOTBALL, 1400), 2800);
	ExpectEqual("newspaper shield health", ResolveZombieInitialShieldHealth(ZombieType::ZOMBIE_NEWSPAPER, 150), 1200);
	ExpectEqual("bungee steal delay", ResolveBungeeStealDelay(300), 0);
	ExpectEqual("flag uses yeti update", UsesYetiUpdate(ZombieType::ZOMBIE_FLAG), true);
	ExpectEqual("yeti update replaced", UsesYetiUpdate(ZombieType::ZOMBIE_YETI), false);
	ExpectEqual("normal eat interval", ResolveZombieEatInterval(ZombiePhase::PHASE_ZOMBIE_NORMAL, false, 4), 8);
	ExpectEqual("chilled eat interval", ResolveZombieEatInterval(ZombiePhase::PHASE_ZOMBIE_NORMAL, true, 4), 16);
	ExpectEqual("newspaper reading eat interval", ResolveZombieEatInterval(ZombiePhase::PHASE_NEWSPAPER_READING, false, 4), 16);
	ExpectEqual("chilled newspaper reading eat interval", ResolveZombieEatInterval(ZombiePhase::PHASE_NEWSPAPER_READING, true, 4), 32);
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
	ExpectEqual("squash-head post-damage health", ResolveZombieBodyHealthAfterDamage(ZombieType::ZOMBIE_SQUASH_HEAD, 13), 720);
	ExpectEqual("burn uses combined health", ShouldTakeBurnDamage(ZombieType::ZOMBIE_PAIL, 700, 1100, 0), true);
	ExpectEqual("gatling-head takes burn damage", ShouldTakeBurnDamage(ZombieType::ZOMBIE_GATLING_HEAD, 270, 0, 0), true);
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
	ExpectEqual("Beghouled winning score", ResolveBeghouledWinningScore(75), 100);
	ExpectEqual("raining seeds countdown", ResolveRainingSeedsCountdown(123), 323);

	if (SetActiveRuleset("not-a-ruleset"))
		return 1;

	std::cout << "PvZ 95 ruleset tests passed\n";
	return 0;
}
