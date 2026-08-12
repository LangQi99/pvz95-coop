/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Ruleset.h"
#include "GameConstants.h"

#include <array>
#include <cstddef>

namespace PvzRules
{
	namespace
	{
		constexpr uint32_t RULESET_PROTOCOL_ORIGINAL = 0x4F524947; // ORIG
		constexpr uint32_t RULESET_PROTOCOL_PVZ95 = 0x50563935;    // PV95

		RulesetId gActiveRuleset = RulesetId::PVZ95;

		const std::array<PlantTuning, static_cast<size_t>(SeedType::NUM_SEED_TYPES)> gPvZ95PlantTuning = []
		{
			std::array<PlantTuning, static_cast<size_t>(SeedType::NUM_SEED_TYPES)> aTuning{};

			aTuning[SeedType::SEED_SUNFLOWER].mLaunchRate = 3300;
			aTuning[SeedType::SEED_POTATOMINE].mSeedCost = 50;
			aTuning[SeedType::SEED_SUNSHROOM].mLaunchRate = 3300;
			aTuning[SeedType::SEED_GRAVEBUSTER].mSeedCost = 50;
			aTuning[SeedType::SEED_HYPNOSHROOM].mSeedCost = 100;

			aTuning[SeedType::SEED_TALLNUT].mSeedCost = 175;
			aTuning[SeedType::SEED_TALLNUT].mRefreshTime = 2000;
			aTuning[SeedType::SEED_SEASHROOM].mRefreshTime = 1500;
			aTuning[SeedType::SEED_CACTUS].mSeedCost = 200;

			aTuning[SeedType::SEED_BLOVER].mSeedCost = 200;
			aTuning[SeedType::SEED_BLOVER].mRefreshTime = 2000;
			aTuning[SeedType::SEED_STARFRUIT].mSeedCost = 250;
			aTuning[SeedType::SEED_STARFRUIT].mLaunchRate = 200;

			aTuning[SeedType::SEED_MARIGOLD].mSeedCost = 75;
			aTuning[SeedType::SEED_MARIGOLD].mRefreshTime = 1500;
			aTuning[SeedType::SEED_MARIGOLD].mLaunchRate = 3300;
			aTuning[SeedType::SEED_GATLINGPEA].mSeedCost = 450;

			aTuning[SeedType::SEED_CATTAIL].mSeedCost = 275;
			aTuning[SeedType::SEED_CATTAIL].mLaunchRate = 75;
			aTuning[SeedType::SEED_EXPLODE_O_NUT].mSeedCost = 150;

			return aTuning;
		}();

		int ResolveValue(int theOverride, int theOriginalValue)
		{
			return theOverride >= 0 ? theOverride : theOriginalValue;
		}

		template <typename... ZombieTypes>
		bool IsZombieInSet(ZombieType theZombieType, ZombieTypes... theAllowedTypes)
		{
			return ((theZombieType == theAllowedTypes) || ...);
		}

		bool IsPvZ95ScaryPotterExpandedMode(GameMode theGameMode)
		{
			const int aGameMode = static_cast<int>(theGameMode);
			return aGameMode >= static_cast<int>(GameMode::GAMEMODE_ADVENTURE) &&
				aGameMode <= static_cast<int>(GameMode::GAMEMODE_SCARY_POTTER_ENDLESS);
		}
	}

	RulesetId GetActiveRuleset()
	{
		return gActiveRuleset;
	}

	void SetActiveRuleset(RulesetId theRuleset)
	{
		gActiveRuleset = theRuleset;
	}

	bool SetActiveRuleset(std::string_view theName)
	{
		if (theName == "original")
		{
			SetActiveRuleset(RulesetId::ORIGINAL);
			return true;
		}
		if (theName == "pvz95" || theName == "95")
		{
			SetActiveRuleset(RulesetId::PVZ95);
			return true;
		}

		return false;
	}

	std::string_view GetActiveRulesetName()
	{
		return gActiveRuleset == RulesetId::PVZ95 ? "pvz95" : "original";
	}

	uint32_t GetActiveRulesetProtocolId()
	{
		return gActiveRuleset == RulesetId::PVZ95 ? RULESET_PROTOCOL_PVZ95 : RULESET_PROTOCOL_ORIGINAL;
	}

	const PlantTuning& GetPlantTuning(SeedType theSeedType)
	{
		static const PlantTuning aNoTuning;
		if (gActiveRuleset != RulesetId::PVZ95 || theSeedType < 0 || theSeedType >= SeedType::NUM_SEED_TYPES)
			return aNoTuning;

		return gPvZ95PlantTuning[static_cast<size_t>(theSeedType)];
	}

	int ResolvePlantSeedCost(SeedType theSeedType, int theOriginalValue)
	{
		return ResolveValue(GetPlantTuning(theSeedType).mSeedCost, theOriginalValue);
	}

	int ResolvePlantRefreshTime(SeedType theSeedType, int theOriginalValue)
	{
		return ResolveValue(GetPlantTuning(theSeedType).mRefreshTime, theOriginalValue);
	}

	int ResolvePlantLaunchRate(SeedType theSeedType, int theOriginalValue)
	{
		return ResolveValue(GetPlantTuning(theSeedType).mLaunchRate, theOriginalValue);
	}

	int ResolvePlantInitialHealth(SeedType theSeedType, int theOriginalValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theSeedType == SeedType::SEED_SPIKEROCK)
			return 16200;

		return theOriginalValue;
	}

	int ResolvePlantInitialStateCountdown(SeedType theSeedType, int theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		switch (theSeedType)
		{
		case SeedType::SEED_POTATOMINE:
			return 1000;
		case SeedType::SEED_SUNSHROOM:
			return 9000;
		default:
			return theOriginalValue;
		}
	}

	bool ShootsAtCounterFifty(SeedType theSeedType)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
			return theSeedType == SeedType::SEED_GATLINGPEA;

		return theSeedType == SeedType::SEED_CATTAIL;
	}

	CoinType ResolveMarigoldCoinType(int theRandomPercent, CoinType theOriginalValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
			return theRandomPercent < 50 ? CoinType::COIN_LARGESUN : CoinType::COIN_SUN;

		return theOriginalValue;
	}

	CoinType ResolveBigTimeMarigoldCoinType(CoinType theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? CoinType::COIN_SUN : theOriginalValue;
	}

	bool ChomperOnlyDamagesZombie(ZombieType theZombieType)
	{
		if (theZombieType == ZombieType::ZOMBIE_GARGANTUAR || theZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
			return true;

		if (gActiveRuleset == RulesetId::PVZ95)
			return theZombieType == ZombieType::ZOMBIE_FOOTBALL;

		return theZombieType == ZombieType::ZOMBIE_BOSS;
	}

	int ResolveChomperDigestTime(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 2500 : theOriginalValue;
	}

	int ResolvePlantAttackRectX(SeedType theSeedType, int thePlantX, int theOriginalValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theSeedType == SeedType::SEED_SQUASH)
			return thePlantX - 16;

		return theOriginalValue;
	}

	int ResolvePlantAttackRectWidth(SeedType theSeedType, int theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		switch (theSeedType)
		{
		case SeedType::SEED_SQUASH:
			return theOriginalValue + 83;
		case SeedType::SEED_CHOMPER:
			return 150;
		case SeedType::SEED_FUMESHROOM:
			// The 95 executable writes INT_MAX here.  Its 32-bit rectangle addition then
			// overflows; a board-wide width preserves the intended infinite-range attack
			// without invoking signed-overflow undefined behavior on portable builds.
			return BOARD_WIDTH;
		default:
			return theOriginalValue;
		}
	}

	int ResolveSpikeRockCrushDamage(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 1800 : theOriginalValue;
	}

	int ResolveSpikeRockDamageThreshold(int theDamageState, int theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		if (theDamageState == 1)
			return 10800;
		if (theDamageState == 2)
			return 5400;
		return theOriginalValue;
	}

	bool UsesCherryBombSpecial(SeedType theSeedType)
	{
		return theSeedType == SeedType::SEED_CHERRYBOMB ||
			(gActiveRuleset == RulesetId::PVZ95 && theSeedType == SeedType::SEED_EXPLODE_O_NUT);
	}

	bool TakesLayeredCrushDamage(SeedType theSeedType)
	{
		return theSeedType == SeedType::SEED_SPIKEROCK ||
			(gActiveRuleset == RulesetId::PVZ95 && theSeedType == SeedType::SEED_TALLNUT);
	}

	TorchwoodConversion ResolveTorchwoodConversion(ProjectileType theProjectileType)
	{
		if (theProjectileType == ProjectileType::PROJECTILE_PEA)
		{
			return gActiveRuleset == RulesetId::PVZ95 ?
				TorchwoodConversion::PEA : TorchwoodConversion::FIREBALL;
		}
		if (theProjectileType == ProjectileType::PROJECTILE_SNOWPEA)
		{
			return gActiveRuleset == RulesetId::PVZ95 ?
				TorchwoodConversion::FIREBALL : TorchwoodConversion::PEA;
		}

		return TorchwoodConversion::NONE;
	}

	PotatoMineExplosion ResolvePotatoMineExplosion(
		int theOriginalRadius, int theOriginalRowRange, bool theOriginalBurn)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
			return {115, 1, true};

		return {theOriginalRadius, theOriginalRowRange, theOriginalBurn};
	}

	int ResolveProjectileDamage(ProjectileType theProjectileType, int theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		switch (theProjectileType)
		{
		case ProjectileType::PROJECTILE_STAR:
			return 40;
		case ProjectileType::PROJECTILE_SPIKE:
			return 1;
		default:
			return theOriginalValue;
		}
	}

	BurnRowEffects ResolveBurnRowEffects()
	{
		if (gActiveRuleset == RulesetId::PVZ95)
			return {true, 2500, 750, 1000};

		return {false, 0, 0, 0};
	}

	unsigned int ResolveBurnRowDamageFlags(ZombiePhase theZombiePhase, unsigned int theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		return theZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MAD ? 3U : 0xFFU;
	}

	bool ShouldBloverBlowZombie(ZombiePhase theZombiePhase, bool theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		switch (theZombiePhase)
		{
		case ZombiePhase::PHASE_POLEVAULTER_IN_VAULT:
		case ZombiePhase::PHASE_BOBSLED_BOARDING:
		case ZombiePhase::PHASE_POGO_BOUNCING:
		case ZombiePhase::PHASE_POGO_HIGH_BOUNCE_1:
		case ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_2:
		case ZombiePhase::PHASE_DOLPHIN_INTO_POOL:
		case ZombiePhase::PHASE_DOLPHIN_IN_JUMP:
		case ZombiePhase::PHASE_SNORKEL_INTO_POOL:
		case ZombiePhase::PHASE_IMP_GETTING_THROWN:
		case ZombiePhase::PHASE_BALLOON_FLYING:
			return true;
		default:
			return false;
		}
	}

	int ResolveBloverDamage(bool theMindControlled, bool theBlowingAway, int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 && !theMindControlled && !theBlowingAway ? 50 : theOriginalValue;
	}

	unsigned int ResolveBloverDamageFlags(ZombiePhase theZombiePhase, unsigned int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? static_cast<unsigned int>(theZombiePhase) : theOriginalValue;
	}

	int ResolveBloverConeHelmHealth(HelmType theHelmType, int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 && theHelmType == HelmType::HELMTYPE_TRAFFIC_CONE ? 50 : theOriginalValue;
	}

	int ResolveFogBlownCountdown(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 10000 : theOriginalValue;
	}

	bool UsesHomingTargetOnlyCollision(ProjectileMotion theMotionType)
	{
		return gActiveRuleset != RulesetId::PVZ95 && theMotionType == ProjectileMotion::MOTION_HOMING;
	}

	ProjectileMotion ResolveProjectileMotionBeforeUpdate(ProjectileMotion theMotionType,
		BackgroundType theBackground, int theProjectileAge)
	{
		if (gActiveRuleset != RulesetId::PVZ95 || theMotionType != ProjectileMotion::MOTION_STAR)
			return theMotionType;

		const int aStarMotionLifetime = theBackground == BackgroundType::BACKGROUND_3_POOL ||
			theBackground == BackgroundType::BACKGROUND_4_FOG ? 56 : 64;
		return theProjectileAge >= aStarMotionLifetime ? ProjectileMotion::MOTION_STRAIGHT : theMotionType;
	}

	ProjectileDeathState ResolveProjectileDeath(ProjectileType theProjectileType, int theProjectileX)
	{
		if (gActiveRuleset == RulesetId::PVZ95 &&
			theProjectileType == ProjectileType::PROJECTILE_SPIKE && theProjectileX < 64)
		{
			return {false, theProjectileX + 1};
		}

		return {true, theProjectileX};
	}

	ProjectileType ResolveTorchwoodSnowPeaType(ProjectileType theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? ProjectileType::PROJECTILE_SNOWPEA : theOriginalValue;
	}

	bool ResolveZombieTargetPlantNotOnGround(bool theAirborneSquash, bool theScaredyShroomScared,
		bool theSquished, bool theDead, bool theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		return theAirborneSquash || theSquished || theScaredyShroomScared || theDead;
	}

	ZombieType ResolveZombieMemberType(ZombieType theRequestedType)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theRequestedType == ZombieType::ZOMBIE_DOOR)
			return ZombieType::ZOMBIE_PAIL;

		return theRequestedType;
	}

	ZombiePreSwitchArmor ResolveZombiePreSwitchArmor(
		ZombieType theMemberType, HelmType theOriginalHelmType, int theOriginalHelmHealth)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theMemberType == ZombieType::ZOMBIE_PAIL)
			return {HelmType::HELMTYPE_PAIL, 1100};

		return {theOriginalHelmType, theOriginalHelmHealth};
	}

	ZombieFlagArmor ResolveZombieFlagArmor(HelmType theOriginalHelmType, int theOriginalHelmHealth)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
			return {true, HelmType::HELMTYPE_PAIL, 1100, 1100};

		return {false, theOriginalHelmType, theOriginalHelmHealth, 0};
	}

	int ResolveZombieInitialBodyHealth(ZombieType theZombieType, int theOriginalValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
		{
			if (theZombieType == ZombieType::ZOMBIE_FLAG)
				return 820;
			if (theZombieType == ZombieType::ZOMBIE_DANCER)
				return 1350;
		}

		return theOriginalValue;
	}

	int ResolveZombieInitialHelmHealth(ZombieType theZombieType, int theOriginalValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theZombieType == ZombieType::ZOMBIE_FOOTBALL)
			return 2800;

		return theOriginalValue;
	}

	int ResolveZombieInitialShieldHealth(ZombieType theZombieType, int theOriginalValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theZombieType == ZombieType::ZOMBIE_NEWSPAPER)
			return 1200;

		return theOriginalValue;
	}

	int ResolveBungeeStealDelay(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 0 : theOriginalValue;
	}

	bool UsesYetiUpdate(ZombieType theZombieType)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
			return theZombieType == ZombieType::ZOMBIE_FLAG;

		return theZombieType == ZombieType::ZOMBIE_YETI;
	}

	int ResolveZombieEatInterval(ZombieType theZombieType, ZombiePhase theZombiePhase,
		bool theIsChilled, int theOriginalBaseValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
		{
			if (theZombieType == ZombieType::ZOMBIE_NEWSPAPER)
			{
				int anInterval = theZombiePhase == ZombiePhase::PHASE_NEWSPAPER_READING ? 2 : 1;
				return theIsChilled ? anInterval * 2 : anInterval;
			}

			return theIsChilled ? theOriginalBaseValue * 4 : theOriginalBaseValue * 2;
		}

		return theIsChilled ? theOriginalBaseValue * 2 : theOriginalBaseValue;
	}

	int ResolveZombieEatDamage(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 8 : theOriginalValue;
	}

	CoinType ResolveIZombieSunflowerReward(CoinType theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? CoinType::COIN_SMALLSUN : theOriginalValue;
	}

	SeedType ResolveCoinInitialUsableSeedType(SeedType theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? SeedType::SEED_BLOVER : theOriginalValue;
	}

	int ResolveScoredSunValue(CoinType theCoinType, int theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		switch (theCoinType)
		{
		case CoinType::COIN_SUN:
			return 50;
		case CoinType::COIN_SMALLSUN:
			return 25;
		case CoinType::COIN_LARGESUN:
			return 75;
		default:
			return theOriginalValue;
		}
	}

	SeedType ResolveEatenPlantSeedType(SeedType theSeedType, int thePlantHealth)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theSeedType;

		if (theSeedType == SeedType::SEED_TALLNUT && thePlantHealth < 300)
			return SeedType::SEED_SQUASH;

		return theSeedType;
	}

	bool EatenPlantTransformTriggersSpecial(SeedType theSeedType, int thePlantHealth)
	{
		return gActiveRuleset == RulesetId::PVZ95 && theSeedType == SeedType::SEED_EXPLODE_O_NUT && thePlantHealth < 40;
	}

	ZombieMindControlStats ResolveMindControlStats(ZombieType theZombieType, int theBodyHealth,
		int theBodyMaxHealth, int theHelmHealth, int theHelmMaxHealth, int theShieldHealth,
		int theShieldMaxHealth, int theChilled, float theScale)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return {theBodyHealth, theHelmHealth, theShieldHealth, theChilled, theScale};

		return {
			theZombieType == ZombieType::ZOMBIE_NEWSPAPER ? 920 : theBodyMaxHealth + 200,
			theHelmMaxHealth + 200,
			theShieldMaxHealth + 200,
			0,
			1.25f
		};
	}

	int ResolveZombieBodyHealthAfterDamage(ZombiePhase theZombiePhase, int theOriginalValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING)
			return 720;

		return theOriginalValue;
	}

	ZombieShieldDamagePolicy ResolveZombieShieldDamagePolicy(
		int theDamageRemaining, ShieldType theShieldType, unsigned int theDamageFlags)
	{
		if (theDamageRemaining <= 0 || theShieldType == ShieldType::SHIELDTYPE_NONE)
			return {false, false};

		if (gActiveRuleset == RulesetId::PVZ95)
			return {true, false};

		const bool aBypassesShield = (theDamageFlags &
			(1U << static_cast<unsigned int>(DamageFlags::DAMAGE_BYPASSES_SHIELD))) != 0U;
		const bool aHitsShieldAndBody = (theDamageFlags &
			(1U << static_cast<unsigned int>(DamageFlags::DAMAGE_HITS_SHIELD_AND_BODY))) != 0U;
		return {!aBypassesShield, !aBypassesShield && aHitsShieldAndBody};
	}

	bool ShouldTakeBurnDamage(ZombieType theZombieType, ZombiePhase theZombiePhase,
		int theBodyHealth, int theHelmHealth, int theShieldHealth)
	{
		if (theZombieType == ZombieType::ZOMBIE_BOSS)
			return true;

		if (gActiveRuleset != RulesetId::PVZ95)
			return theBodyHealth >= 1800;

		const int64_t aTotalHealth = static_cast<int64_t>(theBodyHealth) + theHelmHealth + theShieldHealth;
		return aTotalHealth >= 1800 || theZombiePhase == ZombiePhase::PHASE_NEWSPAPER_READING ||
			theZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING;
	}

	int ResolveApplyBurnDamage(bool theFromPlantBurnRow, int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 && theFromPlantBurnRow ? 0 : theOriginalValue;
	}

	ZombieStatusCounters ResolveButterStatus(int theChilled, int theButtered, int theIceTrapped)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
			return {1000, theButtered, 300};

		return {theChilled, 400, theIceTrapped};
	}

	int ResolveChillAfterRemovingCold(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 1000 : 0;
	}

	bool IsForcedChilledMovement(ZombiePhase theZombiePhase)
	{
		return gActiveRuleset == RulesetId::PVZ95 && theZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MAD;
	}

	float ResolveZombieAnimationRate(ZombiePhase theZombiePhase, int theChilledCounter,
		bool theIsMovingAtChilledSpeed, float theOriginalRate)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MAD)
			return theOriginalRate * (theChilledCounter > 0 ? 1.25f : 2.5f);

		return theIsMovingAtChilledSpeed ? theOriginalRate * 0.5f : theOriginalRate;
	}

	FutureModeMusic ResolveFutureModeMusic(MusicTune theCurrentTune)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return {false, theCurrentTune};

		constexpr int FIRST_GAMEPLAY_TUNE = 1;
		constexpr int LAST_GAMEPLAY_TUNE = 12;
		const int aCurrentTune = static_cast<int>(theCurrentTune);
		if (aCurrentTune == LAST_GAMEPLAY_TUNE)
			return {true, static_cast<MusicTune>(FIRST_GAMEPLAY_TUNE)};

		if (aCurrentTune >= FIRST_GAMEPLAY_TUNE && aCurrentTune < LAST_GAMEPLAY_TUNE)
		{
			return {true, static_cast<MusicTune>(aCurrentTune + 1)};
		}

		// The injected loop is reached with a gameplay tune in the inclusive 1..12
		// domain. Keep invalid/non-gameplay values untouched in the portable build.
		return {false, theCurrentTune};
	}

	DanceModeSeedPacket ResolveDanceModeSeedPacket(
		SeedType thePacketType, SeedType theImitaterType)
	{
		if (gActiveRuleset == RulesetId::PVZ95 &&
			(thePacketType == SeedType::SEED_WALLNUT || theImitaterType == SeedType::SEED_WALLNUT))
		{
			return {SeedType::SEED_EXPLODE_O_NUT, theImitaterType};
		}

		return {thePacketType, theImitaterType};
	}

	LawnMowerState ResolveSuperMowerToggleState(LawnMowerState theOriginalState)
	{
		return gActiveRuleset == RulesetId::PVZ95 ?
			LawnMowerState::MOWER_TRIGGERED : theOriginalState;
	}

	bool CanUseRestrictedTypingCheat(bool theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? true : theOriginalValue;
	}

	bool ShouldProcessTypingCheats(bool theLanGameplayActive)
	{
		return !theLanGameplayActive;
	}

	bool ResolveSukhbirEasyPlanting(bool theEnableSukhbir, bool theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? theEnableSukhbir : theOriginalValue;
	}

	bool ShouldDieNoLootAtBoardEdge(bool theIsIZombieLevel, bool thePinataMode)
	{
		return theIsIZombieLevel || (gActiveRuleset == RulesetId::PVZ95 && thePinataMode);
	}

	bool ResolveChallengeZombieAllowed(GameMode theGameMode, bool theIsLittleTroubleLevel,
		bool theIsFirstWallnutBowlingLevel, ZombieType theZombieType, bool theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		if (theIsLittleTroubleLevel)
		{
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE,
				ZOMBIE_DOOR, ZOMBIE_FOOTBALL, ZOMBIE_SNORKEL, ZOMBIE_LADDER);
		}
		if (theIsFirstWallnutBowlingLevel)
		{
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_FLAG,
				ZOMBIE_TRAFFIC_CONE, ZOMBIE_PAIL, ZOMBIE_POLEVAULTER,
				ZOMBIE_NEWSPAPER, ZOMBIE_LADDER);
		}

		switch (theGameMode)
		{
		case GAMEMODE_CHALLENGE_SPEED:
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE,
				ZOMBIE_POLEVAULTER, ZOMBIE_FOOTBALL, ZOMBIE_DOLPHIN_RIDER,
				ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_LADDER);
		case GAMEMODE_CHALLENGE_POGO_PARTY:
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE,
				ZOMBIE_PAIL, ZOMBIE_NEWSPAPER, ZOMBIE_DOOR, ZOMBIE_FOOTBALL,
				ZOMBIE_ZAMBONI, ZOMBIE_GARGANTUAR);
		case GAMEMODE_CHALLENGE_PORTAL_COMBAT:
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_PAIL,
				ZOMBIE_NEWSPAPER, ZOMBIE_BALLOON);
		case GAMEMODE_CHALLENGE_BIG_TIME:
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE,
				ZOMBIE_PAIL, ZOMBIE_DOOR, ZOMBIE_POGO, ZOMBIE_JACK_IN_THE_BOX);
		case GAMEMODE_CHALLENGE_RAINING_SEEDS:
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_FLAG,
				ZOMBIE_TRAFFIC_CONE, ZOMBIE_POLEVAULTER, ZOMBIE_PAIL, ZOMBIE_DOOR,
				ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_BUNGEE);
		case GAMEMODE_CHALLENGE_AIR_RAID:
			return IsZombieInSet(theZombieType, ZOMBIE_TRAFFIC_CONE,
				ZOMBIE_POLEVAULTER, ZOMBIE_PAIL, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_BALLOON);
		case GAMEMODE_CHALLENGE_COLUMN:
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE,
				ZOMBIE_POLEVAULTER, ZOMBIE_PAIL);
		case GAMEMODE_CHALLENGE_INVISIGHOUL:
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE,
				ZOMBIE_PAIL, ZOMBIE_SNORKEL, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_BUNGEE);
		case GAMEMODE_CHALLENGE_WAR_AND_PEAS:
			return IsZombieInSet(theZombieType, ZOMBIE_POLEVAULTER, ZOMBIE_NEWSPAPER,
				ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_BALLOON, ZOMBIE_POGO, ZOMBIE_GARGANTUAR);
		case GAMEMODE_CHALLENGE_ICE:
			return IsZombieInSet(theZombieType, ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE,
				ZOMBIE_PAIL, ZOMBIE_NEWSPAPER, ZOMBIE_DANCER, ZOMBIE_SNORKEL,
				ZOMBIE_DOLPHIN_RIDER, ZOMBIE_DIGGER);
		default:
			return theOriginalValue;
		}
	}

	ChallengeWaveGraveAction ResolveChallengeWaveGraveAction(GameMode theGameMode,
		bool theDaisyMode, int theCurrentWave, int theNumWaves, bool theIsFlagWave)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
		{
			if (theDaisyMode || theCurrentWave == theNumWaves - 1)
				return ChallengeWaveGraveAction::NONE;
			if (theIsFlagWave)
				return ChallengeWaveGraveAction::SPAWN_ZOMBIES_FROM_GRAVES;
			return theCurrentWave > 9 ?
				ChallengeWaveGraveAction::SPAWN_RANDOM_GRAVE : ChallengeWaveGraveAction::NONE;
		}

		if (theGameMode != GAMEMODE_CHALLENGE_GRAVE_DANGER ||
			theCurrentWave == theNumWaves - 1)
		{
			return ChallengeWaveGraveAction::NONE;
		}
		if (theIsFlagWave)
			return ChallengeWaveGraveAction::SPAWN_ZOMBIES_FROM_GRAVES;
		return theCurrentWave > 5 ?
			ChallengeWaveGraveAction::SPAWN_RANDOM_GRAVE : ChallengeWaveGraveAction::NONE;
	}

	ChallengeStartSetup ResolveChallengeStartSetup(
		bool theBoardExists, bool theOriginalWallnutBowlingCondition)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
		{
			if (theBoardExists)
				return {true, 200, SEED_NONE, 400,
					theOriginalWallnutBowlingCondition, theOriginalWallnutBowlingCondition, true};
			return {false, 0, SEED_NONE, 0, false, false, false};
		}

		if (theBoardExists && theOriginalWallnutBowlingCondition)
			return {true, 200, SEED_WALLNUT, 400, true, true, false};
		return {false, 0, SEED_NONE, 0, false, false, false};
	}

	bool ResolveSeedNotAllowedToPick(
		GameMode theGameMode, SeedType theSeedType, bool theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		if (theGameMode != GAMEMODE_CHALLENGE_LAST_STAND &&
			theGameMode != GAMEMODE_CHALLENGE_AIR_RAID)
		{
			return false;
		}

		return theSeedType == SEED_SUNFLOWER || theSeedType == SEED_SUNSHROOM ||
			theSeedType == SEED_TWINSUNFLOWER || theSeedType == SEED_SEASHROOM ||
			theSeedType == SEED_MARIGOLD || theSeedType == SEED_PUFFSHROOM;
	}

	BeghouledTwistCornerPlan ResolveBeghouledTwistCornerPlan(
		BeghouledTwistCorner theCorner)
	{
		if (gActiveRuleset == RulesetId::PVZ95)
		{
			switch (theCorner)
			{
			case BeghouledTwistCorner::TOP_LEFT:
				return {BeghouledTwistCorner::BOTTOM_RIGHT, true, true, 20, 20, 1, 1};
			case BeghouledTwistCorner::TOP_RIGHT:
				return {BeghouledTwistCorner::BOTTOM_LEFT, true, true, -20, 20, -1, 1};
			case BeghouledTwistCorner::BOTTOM_LEFT:
				return {BeghouledTwistCorner::TOP_RIGHT, true, true, 20, -20, 1, -1};
			case BeghouledTwistCorner::BOTTOM_RIGHT:
				return {BeghouledTwistCorner::TOP_LEFT, true, true, -20, -20, -1, -1};
			default:
				break;
			}
		}
		else
		{
			switch (theCorner)
			{
			case BeghouledTwistCorner::TOP_LEFT:
				return {BeghouledTwistCorner::BOTTOM_LEFT, true, false, 20, 0, 1, 0};
			case BeghouledTwistCorner::TOP_RIGHT:
				return {BeghouledTwistCorner::TOP_LEFT, false, true, 0, 20, 0, 1};
			case BeghouledTwistCorner::BOTTOM_LEFT:
				return {BeghouledTwistCorner::BOTTOM_RIGHT, false, true, 0, -20, 0, -1};
			case BeghouledTwistCorner::BOTTOM_RIGHT:
				return {BeghouledTwistCorner::TOP_RIGHT, true, false, -20, 0, -1, 0};
			default:
				break;
			}
		}

		return {theCorner, false, false, 0, 0, 0, 0};
	}

	int ResolveInitialSunMoney(GameMode theGameMode, int theOriginalValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
			return 8000;

		return theOriginalValue;
	}

	int ResolveMaximumSunMoney(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 2000000000 : theOriginalValue;
	}

	int ResolveShortAdventureReplayWaveCount(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 40 : theOriginalValue;
	}

	int ResolveNonAdventureWaveCount(GameMode theGameMode, int theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95 ||
			theGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND || theOriginalValue == 0)
		{
			return theOriginalValue;
		}

		switch (theOriginalValue)
		{
		case 10:
		case 12:
			return 20;
		case 20:
		case 30:
			return 40;
		case 40:
			return 50;
		default:
			return theOriginalValue;
		}
	}

	int ResolveZombieWavePointMultiplier(GameMode theGameMode, int theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		return theGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN ? 6 : 4;
	}

	bool UsesLegacyIceChallengeSpecialCase(GameMode theGameMode)
	{
		return gActiveRuleset != RulesetId::PVZ95 &&
			theGameMode == GameMode::GAMEMODE_CHALLENGE_ICE;
	}

	BackgroundType ResolveChallengeBackground(GameMode theGameMode, BackgroundType theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		if (theGameMode == GameMode::GAMEMODE_CHALLENGE_ICE)
			return BackgroundType::BACKGROUND_3_POOL;
		if (theGameMode == GameMode::GAMEMODE_CHALLENGE_AIR_RAID)
			return BackgroundType::BACKGROUND_1_DAY;
		return theOriginalValue;
	}

	bool ShouldIceChallengeLoseBeforeAward(GameMode theGameMode, int theSunMoney)
	{
		return gActiveRuleset == RulesetId::PVZ95 &&
			theGameMode == GameMode::GAMEMODE_CHALLENGE_ICE && theSunMoney < 27500;
	}

	bool ZombiePassesDefinitionSpawnGate(int theLevel, int theStartingLevel, int thePickWeight)
	{
		return gActiveRuleset == RulesetId::PVZ95 ||
			(theLevel >= theStartingLevel && thePickWeight != 0);
	}

	bool ResolveZombieAllowedOnLevel(ZombieType theZombieType, int theLevel, bool theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		if (theZombieType == ZombieType::ZOMBIE_POLEVAULTER && theLevel == 32)
			return true;
		if (theZombieType == ZombieType::ZOMBIE_NEWSPAPER)
		{
			if (theLevel == 11 || theLevel == 12)
				return false;
			if (theLevel == 16)
				return true;
		}
		if (theZombieType == ZombieType::ZOMBIE_FOOTBALL && theLevel == 32)
			return false;

		return theOriginalValue;
	}

	bool ShouldEnforceZombieWaveBudgetGate(GameMode theGameMode, bool theOriginalValue)
	{
		(void)theGameMode;
		return gActiveRuleset == RulesetId::PVZ95 ? false : theOriginalValue;
	}

	SeedType ResolveInitialSeedPacket(GameMode theGameMode, bool theIsScaryPotterLevel,
		int theSeedIndex, SeedType theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		if (theIsScaryPotterLevel && theSeedIndex == 0)
			return SeedType::SEED_PLANTERN;

		return theOriginalValue;
	}

	bool ShouldSuppressSkySunSpawning(GameMode theGameMode, bool theOriginalValue)
	{
		if (gActiveRuleset == RulesetId::PVZ95 && theGameMode == GameMode::GAMEMODE_CHALLENGE_ICE)
			return false;

		return theOriginalValue;
	}

	CoinType ResolveFallingSunType(GameMode theGameMode, CoinType theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		if (theGameMode == GameMode::GAMEMODE_CHALLENGE_BIG_TIME)
			return CoinType::COIN_USABLE_SEED_PACKET;
		if (theGameMode == GameMode::GAMEMODE_CHALLENGE_SUNNY_DAY)
			return CoinType::COIN_SUN;

		return theOriginalValue;
	}

	bool ShouldUseWhackSunDrop(bool theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? true : theOriginalValue;
	}

	CoinType ResolveWhackSunDropType(int theDropIndex, CoinType theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		switch (theDropIndex)
		{
		case 0:
			return CoinType::COIN_SMALLSUN;
		case 1:
			return CoinType::COIN_LARGESUN;
		case 2:
			return CoinType::COIN_SUN;
		default:
			return theOriginalValue;
		}
	}

	int ResolveBeghouledWinningScore(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 100 : theOriginalValue;
	}

	int ResolveRainingSeedsCountdown(int theRandomValue)
	{
		return theRandomValue + (gActiveRuleset == RulesetId::PVZ95 ? 200 : 500);
	}

	SeedType ResolveConveyorSeed(GameMode theGameMode, int theSeedIndex, SeedType theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		if (theGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT)
		{
			switch (theSeedIndex)
			{
			case 0:
				return SeedType::SEED_THREEPEATER;
			case 4:
				return SeedType::SEED_EXPLODE_O_NUT;
			case 5:
				return SeedType::SEED_DOOMSHROOM;
			default:
				return theOriginalValue;
			}
		}

		if (theGameMode == GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL)
		{
			if (theSeedIndex == 0)
				return SeedType::SEED_THREEPEATER;
			if (theSeedIndex == 3)
				return SeedType::SEED_CHERRYBOMB;
		}

		return theOriginalValue;
	}

	int ResolveWhackZombieGroupSize(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 2 : theOriginalValue;
	}

	int ResolveWhackZombieSpeedCurveStart(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 3 : theOriginalValue;
	}

	SeedType ResolveScaryPotterSeed(GameMode theGameMode, int thePlacementIndex, SeedType theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		switch (theGameMode)
		{
		case GameMode::GAMEMODE_SCARY_POTTER_1:
			if (thePlacementIndex == 0)
				return SeedType::SEED_REPEATER;
			if (thePlacementIndex == 2)
				return SeedType::SEED_PEASHOOTER;
			break;
		case GameMode::GAMEMODE_SCARY_POTTER_2:
			switch (thePlacementIndex)
			{
			case 0:
				return SeedType::SEED_POTATOMINE;
			case 1:
				return SeedType::SEED_ICESHROOM;
			case 2:
				return SeedType::SEED_EXPLODE_O_NUT;
			case 3:
				return SeedType::SEED_CHERRYBOMB;
			default:
				break;
			}
			break;
		case GameMode::GAMEMODE_SCARY_POTTER_3:
			if (thePlacementIndex == 4)
				return SeedType::SEED_HYPNOSHROOM;
			break;
		case GameMode::GAMEMODE_SCARY_POTTER_4:
			if (thePlacementIndex == 0 || thePlacementIndex == 2)
				return SeedType::SEED_BLOVER;
			if (thePlacementIndex == 1)
				return SeedType::SEED_POTATOMINE;
			break;
		default:
			break;
		}

		return theOriginalValue;
	}

	ZombieType ResolveScaryPotterZombie(GameMode theGameMode, int thePlacementIndex, ZombieType theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		switch (theGameMode)
		{
		case GameMode::GAMEMODE_SCARY_POTTER_1:
			if (thePlacementIndex == 5)
				return ZombieType::ZOMBIE_DOOR;
			break;
		case GameMode::GAMEMODE_SCARY_POTTER_2:
			switch (thePlacementIndex)
			{
			case 4:
				return ZombieType::ZOMBIE_FOOTBALL;
			case 5:
				return ZombieType::ZOMBIE_NEWSPAPER;
			case 6:
				return ZombieType::ZOMBIE_REDEYE_GARGANTUAR;
			default:
				break;
			}
			break;
		case GameMode::GAMEMODE_SCARY_POTTER_3:
			switch (thePlacementIndex)
			{
			case 5:
				return ZombieType::ZOMBIE_PAIL;
			case 6:
				return ZombieType::ZOMBIE_NEWSPAPER;
			case 7:
				return ZombieType::ZOMBIE_GARGANTUAR;
			case 8:
				return ZombieType::ZOMBIE_FLAG;
			default:
				break;
			}
			break;
		case GameMode::GAMEMODE_SCARY_POTTER_4:
			switch (thePlacementIndex)
			{
			case 3:
				return ZombieType::ZOMBIE_TRAFFIC_CONE;
			case 4:
				return ZombieType::ZOMBIE_POLEVAULTER;
			case 5:
				return ZombieType::ZOMBIE_DANCER;
			default:
				break;
			}
			break;
		default:
			break;
		}

		return theOriginalValue;
	}

	bool ShouldRunScaryPotterMousePositionBlock(GameMode theGameMode, bool theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ?
			IsPvZ95ScaryPotterExpandedMode(theGameMode) : theOriginalValue;
	}

	bool ShouldEvaluateScaryPotterMouseHitBlock(GameMode theGameMode, bool theOriginalValue)
	{
		(void)theGameMode;
		return gActiveRuleset == RulesetId::PVZ95 ? true : theOriginalValue;
	}

	bool ShouldHandleScaryPotterMouseDown(GameMode theGameMode, bool theOriginalValue)
	{
		(void)theGameMode;
		return gActiveRuleset == RulesetId::PVZ95 ? true : theOriginalValue;
	}

	bool ShouldRunScaryPotterUpdate(GameMode theGameMode, bool theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ?
			IsPvZ95ScaryPotterExpandedMode(theGameMode) : theOriginalValue;
	}

	bool ShouldRejectStageHasGraveStonesAtPogoGate(GameMode theGameMode, bool theOriginalValue)
	{
		(void)theGameMode;
		return gActiveRuleset == RulesetId::PVZ95 ? true : theOriginalValue;
	}

	bool ShouldWarnGraveBusterForLevel(bool theStageHasGraveStones, bool theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? theStageHasGraveStones : theOriginalValue;
	}

	int ResolveKillAllZombiesInRadiusNonBurnDamage(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 2400 : theOriginalValue;
	}

	bool IsMagnetShroomFootballHelmetEligible(HelmType theHelmType, bool theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ?
			theHelmType == static_cast<HelmType>(-1) : theOriginalValue;
	}

	int ResolveBowlingShieldDamage(int theOriginalValue)
	{
		return gActiveRuleset == RulesetId::PVZ95 ? 800 : theOriginalValue;
	}

	bool ShouldUseIZombieCursorBehavior(
		bool theBoardExists, SeedType theCursorSeedType, bool theOriginalValue)
	{
		if (gActiveRuleset != RulesetId::PVZ95)
			return theOriginalValue;

		return theBoardExists &&
			static_cast<int>(theCursorSeedType) > static_cast<int>(SeedType::SEED_LEFTPEATER);
	}
}
