/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "ConstEnums.h"

#include <cstdint>
#include <string_view>

enum MusicTune : int32_t;

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

	struct ZombiePreSwitchArmor
	{
		HelmType mHelmType;
		int mHelmHealth;
	};

	struct ZombieFlagArmor
	{
		bool mShowBucket;
		HelmType mHelmType;
		int mHelmHealth;
		int mHelmMaxHealth;
	};

	struct ZombieShieldDamagePolicy
	{
		bool mTakeShieldDamage;
		bool mRestoreOriginalDamage;
	};

	struct FutureModeMusic
	{
		bool mShouldChangeTune;
		MusicTune mTune;
	};

	struct DanceModeSeedPacket
	{
		SeedType mPacketType;
		SeedType mImitaterType;
	};

	enum class ChallengeWaveGraveAction : uint8_t
	{
		NONE,
		SPAWN_ZOMBIES_FROM_GRAVES,
		SPAWN_RANDOM_GRAVE
	};

	struct ChallengeStartSetup
	{
		bool mApply;
		int mZombieCountdown;
		SeedType mSeedType;
		int mConveyorBeltCounter;
		bool mSetShowBowlingLine;
		bool mShowBowlingLine;
		bool mAllowEmptyNonConveyorSeed;
	};

	struct BurnRowEffects
	{
		bool mUseSpecialSequence;
		int mChilled;
		int mIceTrapCounter;
		int mExtraDamage;
	};

	struct ProjectileDeathState
	{
		bool mDead;
		int mX;
	};

	enum class TorchwoodConversion : uint8_t
	{
		NONE,
		FIREBALL,
		PEA
	};

	struct PotatoMineExplosion
	{
		int mRadius;
		int mRowRange;
		bool mBurn;
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
	TorchwoodConversion ResolveTorchwoodConversion(ProjectileType theProjectileType);
	PotatoMineExplosion ResolvePotatoMineExplosion(
		int theOriginalRadius, int theOriginalRowRange, bool theOriginalBurn);
	int ResolveProjectileDamage(ProjectileType theProjectileType, int theOriginalValue);
	BurnRowEffects ResolveBurnRowEffects();
	unsigned int ResolveBurnRowDamageFlags(ZombiePhase theZombiePhase, unsigned int theOriginalValue);
	bool ShouldBloverBlowZombie(ZombiePhase theZombiePhase, bool theOriginalValue);
	int ResolveBloverDamage(bool theMindControlled, bool theBlowingAway, int theOriginalValue);
	unsigned int ResolveBloverDamageFlags(ZombiePhase theZombiePhase, unsigned int theOriginalValue);
	int ResolveBloverConeHelmHealth(HelmType theHelmType, int theOriginalValue);
	int ResolveFogBlownCountdown(int theOriginalValue);
	bool UsesHomingTargetOnlyCollision(ProjectileMotion theMotionType);
	ProjectileMotion ResolveProjectileMotionBeforeUpdate(ProjectileMotion theMotionType,
		BackgroundType theBackground, int theProjectileAge);
	ProjectileDeathState ResolveProjectileDeath(ProjectileType theProjectileType, int theProjectileX);
	ProjectileType ResolveTorchwoodSnowPeaType(ProjectileType theOriginalValue);
	bool ResolveZombieTargetPlantNotOnGround(bool theAirborneSquash, bool theScaredyShroomScared,
		bool theSquished, bool theDead, bool theOriginalValue);

	ZombieType ResolveZombieMemberType(ZombieType theRequestedType);
	ZombiePreSwitchArmor ResolveZombiePreSwitchArmor(
		ZombieType theMemberType, HelmType theOriginalHelmType, int theOriginalHelmHealth);
	ZombieFlagArmor ResolveZombieFlagArmor(HelmType theOriginalHelmType, int theOriginalHelmHealth);
	int ResolveZombieInitialBodyHealth(ZombieType theZombieType, int theOriginalValue);
	int ResolveZombieInitialHelmHealth(ZombieType theZombieType, int theOriginalValue);
	int ResolveZombieInitialShieldHealth(ZombieType theZombieType, int theOriginalValue);
	int ResolveBungeeStealDelay(int theOriginalValue);
	bool UsesYetiUpdate(ZombieType theZombieType);
	int ResolveZombieEatInterval(ZombieType theZombieType, ZombiePhase theZombiePhase,
		bool theIsChilled, int theOriginalBaseValue);
	int ResolveZombieEatDamage(int theOriginalValue);
	CoinType ResolveIZombieSunflowerReward(CoinType theOriginalValue);
	SeedType ResolveEatenPlantSeedType(SeedType theSeedType, int thePlantHealth);
	bool EatenPlantTransformTriggersSpecial(SeedType theSeedType, int thePlantHealth);
	ZombieMindControlStats ResolveMindControlStats(ZombieType theZombieType, int theBodyHealth,
		int theBodyMaxHealth, int theHelmHealth, int theHelmMaxHealth, int theShieldHealth,
		int theShieldMaxHealth, int theChilled, float theScale);
	int ResolveZombieBodyHealthAfterDamage(ZombiePhase theZombiePhase, int theOriginalValue);
	ZombieShieldDamagePolicy ResolveZombieShieldDamagePolicy(
		int theDamageRemaining, ShieldType theShieldType, unsigned int theDamageFlags);
	bool ShouldTakeBurnDamage(ZombieType theZombieType, ZombiePhase theZombiePhase,
		int theBodyHealth, int theHelmHealth, int theShieldHealth);
	int ResolveApplyBurnDamage(bool theFromPlantBurnRow, int theOriginalValue);
	ZombieStatusCounters ResolveButterStatus(int theChilled, int theButtered, int theIceTrapped);
	int ResolveChillAfterRemovingCold(int theOriginalValue);
	bool IsForcedChilledMovement(ZombiePhase theZombiePhase);
	float ResolveZombieAnimationRate(ZombiePhase theZombiePhase, int theChilledCounter,
		bool theIsMovingAtChilledSpeed, float theOriginalRate);
	FutureModeMusic ResolveFutureModeMusic(MusicTune theCurrentTune);
	DanceModeSeedPacket ResolveDanceModeSeedPacket(
		SeedType thePacketType, SeedType theImitaterType);
	LawnMowerState ResolveSuperMowerToggleState(LawnMowerState theOriginalState);
	bool CanUseRestrictedTypingCheat(bool theOriginalValue);
	bool ShouldProcessTypingCheats(bool theLanGameplayActive);
	bool ResolveSukhbirEasyPlanting(bool theEnableSukhbir, bool theOriginalValue);
	bool ShouldDieNoLootAtBoardEdge(bool theIsIZombieLevel, bool thePinataMode);
	bool ResolveChallengeZombieAllowed(GameMode theGameMode, bool theIsLittleTroubleLevel,
		bool theIsFirstWallnutBowlingLevel, ZombieType theZombieType, bool theOriginalValue);
	ChallengeWaveGraveAction ResolveChallengeWaveGraveAction(GameMode theGameMode,
		bool theDaisyMode, int theCurrentWave, int theNumWaves, bool theIsFlagWave);
	ChallengeStartSetup ResolveChallengeStartSetup(
		bool theBoardExists, bool theOriginalWallnutBowlingCondition);

	int ResolveInitialSunMoney(GameMode theGameMode, int theOriginalValue);
	int ResolveMaximumSunMoney(int theOriginalValue);
	int ResolveShortAdventureReplayWaveCount(int theOriginalValue);
	int ResolveNonAdventureWaveCount(GameMode theGameMode, int theOriginalValue);
	int ResolveZombieWavePointMultiplier(GameMode theGameMode, int theOriginalValue);
	bool UsesLegacyIceChallengeSpecialCase(GameMode theGameMode);
	BackgroundType ResolveChallengeBackground(GameMode theGameMode, BackgroundType theOriginalValue);
	bool ShouldIceChallengeLoseBeforeAward(GameMode theGameMode, int theSunMoney);
	bool ZombiePassesDefinitionSpawnGate(int theLevel, int theStartingLevel, int thePickWeight);
	bool ResolveZombieAllowedOnLevel(ZombieType theZombieType, int theLevel, bool theOriginalValue);
	bool ShouldEnforceZombieWaveBudgetGate(GameMode theGameMode, bool theOriginalValue);
	SeedType ResolveInitialSeedPacket(GameMode theGameMode, bool theIsScaryPotterLevel,
		int theSeedIndex, SeedType theOriginalValue);
	bool ShouldSuppressSkySunSpawning(GameMode theGameMode, bool theOriginalValue);
	CoinType ResolveFallingSunType(GameMode theGameMode, CoinType theOriginalValue);
	bool ShouldUseWhackSunDrop(bool theOriginalValue);
	CoinType ResolveWhackSunDropType(int theDropIndex, CoinType theOriginalValue);
	int ResolveBeghouledWinningScore(int theOriginalValue);
	int ResolveRainingSeedsCountdown(int theRandomValue);
	SeedType ResolveConveyorSeed(GameMode theGameMode, int theSeedIndex, SeedType theOriginalValue);
	int ResolveWhackZombieGroupSize(int theOriginalValue);
	int ResolveWhackZombieSpeedCurveStart(int theOriginalValue);
	SeedType ResolveScaryPotterSeed(GameMode theGameMode, int thePlacementIndex, SeedType theOriginalValue);
	ZombieType ResolveScaryPotterZombie(GameMode theGameMode, int thePlacementIndex, ZombieType theOriginalValue);
}
