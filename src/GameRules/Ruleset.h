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

	struct ZombieMindControlStats
	{
		int mBodyHealth;
		int mHelmHealth;
		int mShieldHealth;
		int mChilled;
		float mScale;
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
	bool ShootsAtCounterFifty(SeedType theSeedType);
	CoinType ResolveMarigoldCoinType(int theRandomPercent, CoinType theOriginalValue);
	CoinType ResolveBigTimeMarigoldCoinType(CoinType theOriginalValue);
	bool ChomperOnlyDamagesZombie(ZombieType theZombieType);
	int ResolveChomperDigestTime(int theOriginalValue);
	int ResolvePlantAttackRectX(SeedType theSeedType, int thePlantX, int theOriginalValue);
	int ResolvePlantAttackRectWidth(SeedType theSeedType, int theOriginalValue);
	int ResolveSpikeRockCrushDamage(int theOriginalValue);
	int ResolveSpikeRockDamageThreshold(int theDamageState, int theOriginalValue);
	bool UsesCherryBombSpecial(SeedType theSeedType);
	bool TakesLayeredCrushDamage(SeedType theSeedType);
	int ResolveProjectileDamage(ProjectileType theProjectileType, int theOriginalValue);

	ZombieType ResolveZombieType(ZombieType theZombieType);
	int ResolveZombieInitialBodyHealth(ZombieType theZombieType, int theOriginalValue);
	int ResolveZombieInitialHelmHealth(ZombieType theZombieType, int theOriginalValue);
	int ResolveZombieInitialShieldHealth(ZombieType theZombieType, int theOriginalValue);
	int ResolveBungeeStealDelay(int theOriginalValue);
	bool UsesYetiUpdate(ZombieType theZombieType);
	int ResolveZombieEatInterval(ZombiePhase theZombiePhase, bool theIsChilled, int theOriginalBaseValue);
	int ResolveZombieEatDamage(int theOriginalValue);
	CoinType ResolveIZombieSunflowerReward(CoinType theOriginalValue);
	SeedType ResolveEatenPlantSeedType(SeedType theSeedType, int thePlantHealth);
	bool EatenPlantTransformTriggersSpecial(SeedType theSeedType, int thePlantHealth);
	ZombieMindControlStats ResolveMindControlStats(ZombieType theZombieType, int theBodyHealth,
		int theBodyMaxHealth, int theHelmHealth, int theHelmMaxHealth, int theShieldHealth,
		int theShieldMaxHealth, int theChilled, float theScale);
	int ResolveZombieBodyHealthAfterDamage(ZombieType theZombieType, int theOriginalValue);
	bool ShouldTakeBurnDamage(ZombieType theZombieType, int theBodyHealth, int theHelmHealth, int theShieldHealth);
	ZombieStatusCounters ResolveButterStatus(int theChilled, int theButtered, int theIceTrapped);
	int ResolveChillAfterRemovingCold(int theOriginalValue);
	bool IsForcedChilledMovement(ZombiePhase theZombiePhase);
	float ResolveZombieAnimationRate(ZombiePhase theZombiePhase, int theChilledCounter,
		bool theIsMovingAtChilledSpeed, float theOriginalRate);

	int ResolveInitialSunMoney(GameMode theGameMode, int theOriginalValue);
	int ResolveMaximumSunMoney(int theOriginalValue);
	int ResolveBeghouledWinningScore(int theOriginalValue);
	int ResolveRainingSeedsCountdown(int theRandomValue);
	SeedType ResolveConveyorSeed(GameMode theGameMode, int theSeedIndex, SeedType theOriginalValue);
	int ResolveWhackZombieGroupSize(int theOriginalValue);
	int ResolveWhackZombieSpeedCurveStart(int theOriginalValue);
	SeedType ResolveScaryPotterSeed(GameMode theGameMode, int thePlacementIndex, SeedType theOriginalValue);
	ZombieType ResolveScaryPotterZombie(GameMode theGameMode, int thePlacementIndex, ZombieType theOriginalValue);
}
