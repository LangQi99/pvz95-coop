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
	ExpectEqual("squash-head post-damage health", ResolveZombieBodyHealthAfterDamage(ZombieType::ZOMBIE_SQUASH_HEAD, 13), 720);
	ExpectEqual("burn uses combined health", ShouldTakeBurnDamage(ZombieType::ZOMBIE_PAIL, 700, 1100, 0), true);
	ExpectEqual("gatling-head takes burn damage", ShouldTakeBurnDamage(ZombieType::ZOMBIE_GATLING_HEAD, 270, 0, 0), true);
	const ZombieStatusCounters aButterStatus = ResolveButterStatus(25, 0, 0);
	ExpectEqual("butter applies chill", aButterStatus.mChilled, 1000);
	ExpectEqual("butter does not immobilize as butter", aButterStatus.mButtered, 0);
	ExpectEqual("butter applies ice trap", aButterStatus.mIceTrapped, 300);
	ExpectEqual("newspaper mad movement flag", IsForcedChilledMovement(ZombiePhase::PHASE_NEWSPAPER_MAD), true);
	ExpectNear("newspaper mad animation", ResolveZombieAnimationRate(ZombiePhase::PHASE_NEWSPAPER_MAD, 0, true, 10.0f), 25.0f);
	ExpectNear("chilled newspaper mad animation", ResolveZombieAnimationRate(ZombiePhase::PHASE_NEWSPAPER_MAD, 1, true, 10.0f), 12.5f);

	if (SetActiveRuleset("not-a-ruleset"))
		return 1;

	std::cout << "PvZ 95 ruleset tests passed\n";
	return 0;
}
