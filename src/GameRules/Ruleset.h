/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "ConstEnums.h"

#include <cstdint>
#include <string_view>

namespace PvzRules
{
	enum class RulesetId : uint8_t
	{
		ORIGINAL,
		PVZ95
	};

	struct PlantTuning
	{
		int mSeedCost{-1};
		int mRefreshTime{-1};
		int mLaunchRate{-1};
	};

	struct ZombieStatusCounters
	{
		int mChilled;
		int mButtered;
		int mIceTrapped;
	};

	RulesetId GetActiveRuleset();
	void SetActiveRuleset(RulesetId theRuleset);
	bool SetActiveRuleset(std::string_view theName);
	std::string_view GetActiveRulesetName();
	uint32_t GetActiveRulesetProtocolId();

	const PlantTuning& GetPlantTuning(SeedType theSeedType);
	int ResolvePlantSeedCost(SeedType theSeedType, int theOriginalValue);
	int ResolvePlantRefreshTime(SeedType theSeedType, int theOriginalValue);
	int ResolvePlantLaunchRate(SeedType theSeedType, int theOriginalValue);
	int ResolvePlantInitialHealth(SeedType theSeedType, int theOriginalValue);
	int ResolvePlantInitialStateCountdown(SeedType theSeedType, int theOriginalValue);
	int ResolveSpikeRockCrushDamage(int theOriginalValue);
	int ResolveSpikeRockDamageThreshold(int theDamageState, int theOriginalValue);
	bool UsesCherryBombSpecial(SeedType theSeedType);
	bool TakesLayeredCrushDamage(SeedType theSeedType);
	int ResolveProjectileDamage(ProjectileType theProjectileType, int theOriginalValue);

	ZombieType ResolveZombieType(ZombieType theZombieType);
	int ResolveZombieInitialBodyHealth(ZombieType theZombieType, int theOriginalValue);
	int ResolveZombieBodyHealthAfterDamage(ZombieType theZombieType, int theOriginalValue);
	bool ShouldTakeBurnDamage(ZombieType theZombieType, int theBodyHealth, int theHelmHealth, int theShieldHealth);
	ZombieStatusCounters ResolveButterStatus(int theChilled, int theButtered, int theIceTrapped);
	bool IsForcedChilledMovement(ZombiePhase theZombiePhase);
	float ResolveZombieAnimationRate(ZombiePhase theZombiePhase, int theChilledCounter,
		bool theIsMovingAtChilledSpeed, float theOriginalRate);
}
