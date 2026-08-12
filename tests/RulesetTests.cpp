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

	template <size_t N>
	void ExpectChallengeZombieSet(std::string_view theName, GameMode theGameMode,
		bool theIsLittleTroubleLevel, bool theIsFirstWallnutBowlingLevel,
		const std::array<ZombieType, N>& theExpectedTypes)
	{
		for (int aZombieIndex = 0; aZombieIndex < NUM_ZOMBIE_TYPES; aZombieIndex++)
		{
			const ZombieType aZombieType = static_cast<ZombieType>(aZombieIndex);
			bool anExpected = false;
			for (ZombieType anExpectedType : theExpectedTypes)
			{
				if (aZombieType == anExpectedType)
				{
					anExpected = true;
					break;
				}
			}

			for (bool anOriginalValue : {false, true})
			{
				const bool anActual = PvzRules::ResolveChallengeZombieAllowed(theGameMode,
					theIsLittleTroubleLevel, theIsFirstWallnutBowlingLevel,
					aZombieType, anOriginalValue);
				if (anActual != anExpected)
				{
					std::cerr << theName << ": zombie " << aZombieIndex << " with original " <<
						anOriginalValue << " expected " << anExpected << ", got " << anActual << '\n';
					std::exit(1);
				}
			}
		}
	}

	void ExpectTwistPermutation(std::string_view theName,
		const std::array<SeedType, 4>& theInput, const std::array<SeedType, 4>& theExpected)
	{
		for (int aCornerIndex = 0; aCornerIndex < 4; aCornerIndex++)
		{
			const auto aCorner = static_cast<PvzRules::BeghouledTwistCorner>(aCornerIndex);
			const PvzRules::BeghouledTwistCornerPlan aPlan =
				PvzRules::ResolveBeghouledTwistCornerPlan(aCorner);
			const SeedType anActual = theInput[static_cast<int>(aPlan.mTrialSourceCorner)];
			if (anActual != theExpected[aCornerIndex])
			{
				std::cerr << theName << ": corner " << aCornerIndex << " expected " <<
					theExpected[aCornerIndex] << ", got " << anActual << '\n';
				std::exit(1);
			}
		}
	}

	void ExpectTwistPlans(std::string_view theName,
		const std::array<PvzRules::BeghouledTwistCornerPlan, 4>& theExpected)
	{
		for (int aCornerIndex = 0; aCornerIndex < 4; aCornerIndex++)
		{
			const auto aCorner = static_cast<PvzRules::BeghouledTwistCorner>(aCornerIndex);
			const PvzRules::BeghouledTwistCornerPlan anActual =
				PvzRules::ResolveBeghouledTwistCornerPlan(aCorner);
			const PvzRules::BeghouledTwistCornerPlan& anExpected = theExpected[aCornerIndex];
			if (anActual.mTrialSourceCorner != anExpected.mTrialSourceCorner ||
				anActual.mSetInvalidX != anExpected.mSetInvalidX ||
				anActual.mSetInvalidY != anExpected.mSetInvalidY ||
				anActual.mInvalidOffsetX != anExpected.mInvalidOffsetX ||
				anActual.mInvalidOffsetY != anExpected.mInvalidOffsetY ||
				anActual.mValidColumnDelta != anExpected.mValidColumnDelta ||
				anActual.mValidRowDelta != anExpected.mValidRowDelta)
			{
				std::cerr << theName << ": mismatch at corner " << aCornerIndex << '\n';
				std::exit(1);
			}
		}
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
	struct ScaryPotterGateCase
	{
		GameMode mGameMode;
		bool mExpandedRange;
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
	const std::array<ScaryPotterGateCase, 8> aScaryPotterGateCases = {{
		{GAMEMODE_ADVENTURE, true},
		{GAMEMODE_TREE_OF_WISDOM, true},
		{GAMEMODE_SCARY_POTTER_1, true},
		{GAMEMODE_SCARY_POTTER_ENDLESS, true},
		{GAMEMODE_PUZZLE_I_ZOMBIE_1, false},
		{GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS, false},
		{GAMEMODE_UPSELL, false},
		{GAMEMODE_INTRO, false}
	}};
	const std::array<HelmType, 12> aMagnetHelmetCases = {{
		static_cast<HelmType>(-1),
		HELMTYPE_NONE,
		HELMTYPE_TRAFFIC_CONE,
		HELMTYPE_PAIL,
		HELMTYPE_FOOTBALL,
		HELMTYPE_DIGGER,
		HELMTYPE_REDEYES,
		HELMTYPE_HEADBAND,
		HELMTYPE_BOBSLED,
		HELMTYPE_WALLNUT,
		HELMTYPE_TALLNUT,
		static_cast<HelmType>(10)
	}};
	const std::array<SeedType, 9> anIZombieCursorSeedCases = {{
		SEED_NONE,
		SEED_PEASHOOTER,
		SEED_SPROUT,
		SEED_LEFTPEATER,
		static_cast<SeedType>(53),
		static_cast<SeedType>(54),
		static_cast<SeedType>(58),
		static_cast<SeedType>(69),
		SEED_ZOMBIE_IMP
	}};

	ExpectEqual("Adventure mode immediate", static_cast<int>(GAMEMODE_ADVENTURE), 0);
	ExpectEqual("Tree of Wisdom mode immediate", static_cast<int>(GAMEMODE_TREE_OF_WISDOM), 50);
	ExpectEqual("Scary Potter 1 mode immediate", static_cast<int>(GAMEMODE_SCARY_POTTER_1), 51);
	ExpectEqual("Scary Potter Endless mode immediate", static_cast<int>(GAMEMODE_SCARY_POTTER_ENDLESS), 60);
	ExpectEqual("I, Zombie 1 mode immediate", static_cast<int>(GAMEMODE_PUZZLE_I_ZOMBIE_1), 61);
	ExpectEqual("I, Zombie Endless mode immediate", static_cast<int>(GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS), 70);
	ExpectEqual("Upsell mode immediate", static_cast<int>(GAMEMODE_UPSELL), 71);
	ExpectEqual("Intro mode immediate", static_cast<int>(GAMEMODE_INTRO), 72);
	ExpectEqual("Football helmet immediate", static_cast<int>(HELMTYPE_FOOTBALL), 3);
	ExpectEqual("PvZ 95 magnet sentinel immediate", static_cast<int>(static_cast<HelmType>(-1)), -1);
	ExpectEqual("normal sun coin immediate", static_cast<int>(COIN_SUN), 4);
	ExpectEqual("small sun coin immediate", static_cast<int>(COIN_SMALLSUN), 5);
	ExpectEqual("large sun coin immediate", static_cast<int>(COIN_LARGESUN), 6);
	ExpectEqual("initial usable packet sentinel", static_cast<int>(SEED_NONE), -1);
	ExpectEqual("Blover seed immediate", static_cast<int>(SEED_BLOVER), 27);
	ExpectEqual("Leftpeater seed immediate", static_cast<int>(SEED_LEFTPEATER), 52);
	ExpectEqual("seed type count immediate", static_cast<int>(NUM_SEED_TYPES), 53);
	ExpectEqual("special zombie seed immediate", static_cast<int>(SEED_ZOMBIE_SCREEN_DOOR), 69);
	ExpectEqual("maximum special seed immediate", static_cast<int>(SEED_ZOMBIE_IMP), 74);

	SetActiveRuleset(RulesetId::ORIGINAL);
	ExpectEqual("original potato cost", ResolvePlantSeedCost(SeedType::SEED_POTATOMINE, 25), 25);
	ExpectEqual("original star damage", ResolveProjectileDamage(ProjectileType::PROJECTILE_STAR, 20), 20);
	ExpectEqual("original door member type", ResolveZombieMemberType(ZombieType::ZOMBIE_DOOR), ZombieType::ZOMBIE_DOOR);
	const ZombiePreSwitchArmor anOriginalDoorArmor = ResolveZombiePreSwitchArmor(
		ZombieType::ZOMBIE_DOOR, HelmType::HELMTYPE_NONE, 0);
	ExpectEqual("original door pre-switch helmet", anOriginalDoorArmor.mHelmType, HelmType::HELMTYPE_NONE);
	ExpectEqual("original door pre-switch helmet health", anOriginalDoorArmor.mHelmHealth, 0);
	const ZombieFlagArmor anOriginalFlagArmor = ResolveZombieFlagArmor(HelmType::HELMTYPE_NONE, 0);
	ExpectEqual("original flag does not show bucket", anOriginalFlagArmor.mShowBucket, false);
	ExpectEqual("original flag helmet", anOriginalFlagArmor.mHelmType, HelmType::HELMTYPE_NONE);
	ExpectEqual("original flag helmet health", anOriginalFlagArmor.mHelmHealth, 0);
	ExpectEqual("original flag does not override helmet max health", anOriginalFlagArmor.mHelmMaxHealth, 0);
	ExpectEqual("original flag health", ResolveZombieInitialBodyHealth(ZombieType::ZOMBIE_FLAG, 270), 270);
	ExpectEqual("original layered tall-nut crush", TakesLayeredCrushDamage(SeedType::SEED_TALLNUT), false);
	ExpectEqual("original torchwood pea becomes fireball", static_cast<int>(ResolveTorchwoodConversion(
		ProjectileType::PROJECTILE_PEA)), static_cast<int>(TorchwoodConversion::FIREBALL));
	ExpectEqual("original torchwood snow pea becomes pea", static_cast<int>(ResolveTorchwoodConversion(
		ProjectileType::PROJECTILE_SNOWPEA)), static_cast<int>(TorchwoodConversion::PEA));
	ExpectEqual("original torchwood ignores star", static_cast<int>(ResolveTorchwoodConversion(
		ProjectileType::PROJECTILE_STAR)), static_cast<int>(TorchwoodConversion::NONE));
	const PotatoMineExplosion anOriginalPotatoExplosion = ResolvePotatoMineExplosion(60, 0, false);
	ExpectEqual("original potato mine radius", anOriginalPotatoExplosion.mRadius, 60);
	ExpectEqual("original potato mine row range", anOriginalPotatoExplosion.mRowRange, 0);
	ExpectEqual("original potato mine burn", anOriginalPotatoExplosion.mBurn, false);
	ExpectEqual("original rising bungee plant stays untargetable", ResolveZombieTargetPlantNotOnGround(
		false, false, false, false, true), true);
	ExpectEqual("original scaredy-shroom target state unchanged", ResolveZombieTargetPlantNotOnGround(
		false, true, false, false, false), false);
	ExpectEqual("original cattail counter-fifty shot", ShootsAtCounterFifty(SeedType::SEED_CATTAIL), true);
	ExpectEqual("original gatling counter-fifty shot", ShootsAtCounterFifty(SeedType::SEED_GATLINGPEA), false);
	ExpectEqual("original chomper boss bite", ChomperOnlyDamagesZombie(ZombieType::ZOMBIE_BOSS), true);
	ExpectEqual("original chilled eat interval", ResolveZombieEatInterval(
		ZombieType::ZOMBIE_NEWSPAPER, ZombiePhase::PHASE_NEWSPAPER_READING, true, 4), 8);
	ExpectEqual("original cold removal", ResolveChillAfterRemovingCold(500), 0);
	ExpectEqual("original maximum sun", ResolveMaximumSunMoney(9990), 9990);
	ExpectEqual("original I, Zombie reward", ResolveIZombieSunflowerReward(COIN_SUN), COIN_SUN);
	ExpectEqual("original arbitrary I, Zombie reward passthrough",
		ResolveIZombieSunflowerReward(COIN_LARGESUN), COIN_LARGESUN);
	ExpectEqual("original usable packet initializes to sentinel",
		ResolveCoinInitialUsableSeedType(SEED_NONE), SEED_NONE);
	ExpectEqual("original usable packet preserves Blover",
		ResolveCoinInitialUsableSeedType(SEED_BLOVER), SEED_BLOVER);
	ExpectEqual("original normal sun score passthrough", ResolveScoredSunValue(COIN_SUN, 104), 104);
	ExpectEqual("original small sun score passthrough", ResolveScoredSunValue(COIN_SMALLSUN, 105), 105);
	ExpectEqual("original large sun score passthrough", ResolveScoredSunValue(COIN_LARGESUN, 106), 106);
	ExpectEqual("original non-sun score passthrough", ResolveScoredSunValue(COIN_DIAMOND, 103), 103);
	ExpectEqual("original short replay wave count", ResolveShortAdventureReplayWaveCount(20), 20);
	ExpectEqual("original non-adventure wave count", ResolveNonAdventureWaveCount(
		GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1, 10), 10);
	ExpectEqual("original zombie-point multiplier", ResolveZombieWavePointMultiplier(
		GameMode::GAMEMODE_ADVENTURE, 1), 1);
	ExpectEqual("original Ice uses legacy special-case chain", UsesLegacyIceChallengeSpecialCase(
		GameMode::GAMEMODE_CHALLENGE_ICE), true);
	ExpectEqual("original ordinary mode does not use Ice chain", UsesLegacyIceChallengeSpecialCase(
		GameMode::GAMEMODE_ADVENTURE), false);
	ExpectEqual("original Ice background", ResolveChallengeBackground(
		GameMode::GAMEMODE_CHALLENGE_ICE, BackgroundType::BACKGROUND_1_DAY), BackgroundType::BACKGROUND_1_DAY);
	ExpectEqual("original Air Raid background", ResolveChallengeBackground(
		GameMode::GAMEMODE_CHALLENGE_AIR_RAID, BackgroundType::BACKGROUND_4_FOG), BackgroundType::BACKGROUND_4_FOG);
	ExpectEqual("original Ice award never uses sun loss gate", ShouldIceChallengeLoseBeforeAward(
		GameMode::GAMEMODE_CHALLENGE_ICE, 27499), false);
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
	ExpectEqual("original Ice preserves a false sky-sun input", ShouldSuppressSkySunSpawning(
		GameMode::GAMEMODE_CHALLENGE_ICE, false), false);
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
	const unsigned int aBypassShieldFlag = 1U << static_cast<unsigned int>(DamageFlags::DAMAGE_BYPASSES_SHIELD);
	const unsigned int aHitShieldAndBodyFlag = 1U << static_cast<unsigned int>(DamageFlags::DAMAGE_HITS_SHIELD_AND_BODY);
	const ZombieShieldDamagePolicy anOriginalShieldHit = ResolveZombieShieldDamagePolicy(100, SHIELDTYPE_DOOR, 0U);
	ExpectEqual("original ordinary damage hits shield", anOriginalShieldHit.mTakeShieldDamage, true);
	ExpectEqual("original ordinary damage does not restore body damage", anOriginalShieldHit.mRestoreOriginalDamage, false);
	const ZombieShieldDamagePolicy anOriginalShieldBypass = ResolveZombieShieldDamagePolicy(
		100, SHIELDTYPE_DOOR, aBypassShieldFlag);
	ExpectEqual("original bypass flag skips shield", anOriginalShieldBypass.mTakeShieldDamage, false);
	ExpectEqual("original bypass flag does not restore body damage", anOriginalShieldBypass.mRestoreOriginalDamage, false);
	const ZombieShieldDamagePolicy anOriginalShieldAndBody = ResolveZombieShieldDamagePolicy(
		100, SHIELDTYPE_DOOR, aHitShieldAndBodyFlag);
	ExpectEqual("original dual-hit flag damages shield", anOriginalShieldAndBody.mTakeShieldDamage, true);
	ExpectEqual("original dual-hit flag restores body damage", anOriginalShieldAndBody.mRestoreOriginalDamage, true);
	ExpectEqual("original Plant BurnRow ApplyBurn damage", ResolveApplyBurnDamage(true, 1800), 1800);
	ExpectEqual("original generic ApplyBurn damage", ResolveApplyBurnDamage(false, 1800), 1800);
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
	const FutureModeMusic anOriginalFutureMusic = ResolveFutureModeMusic(static_cast<MusicTune>(1));
	ExpectEqual("original Future does not change music", anOriginalFutureMusic.mShouldChangeTune, false);
	ExpectEqual("original Future preserves music", static_cast<int>(anOriginalFutureMusic.mTune), 1);
	const DanceModeSeedPacket anOriginalDancePacket = ResolveDanceModeSeedPacket(SEED_WALLNUT, SEED_NONE);
	ExpectEqual("original Dance preserves packet type", anOriginalDancePacket.mPacketType, SEED_WALLNUT);
	ExpectEqual("original Dance preserves imitater type", anOriginalDancePacket.mImitaterType, SEED_NONE);
	ExpectEqual("original SuperMower preserves mower state", ResolveSuperMowerToggleState(MOWER_READY), MOWER_READY);
	ExpectEqual("original restricted typing cheat remains locked", CanUseRestrictedTypingCheat(false), false);
	ExpectEqual("offline typing cheats are processed", ShouldProcessTypingCheats(false), true);
	ExpectEqual("LAN typing cheats are blocked", ShouldProcessTypingCheats(true), false);
	ExpectEqual("original Sukhbir preserves easy planting", ResolveSukhbirEasyPlanting(true, false), false);
	ExpectEqual("original I, Zombie board edge dies without loot", ShouldDieNoLootAtBoardEdge(true, false), true);
	ExpectEqual("original Pinata board edge still loses", ShouldDieNoLootAtBoardEdge(false, true), false);
	ExpectEqual("original challenge zombie addition stays absent", ResolveChallengeZombieAllowed(
		GAMEMODE_CHALLENGE_SPEED, false, false, ZOMBIE_FOOTBALL, false), false);
	ExpectEqual("original challenge zombie remains allowed", ResolveChallengeZombieAllowed(
		GAMEMODE_CHALLENGE_SPEED, false, false, ZOMBIE_DOLPHIN_RIDER, true), true);
	ExpectEqual("original non-Grave mode skips grave action", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_ADVENTURE, false, 10, 20, true)), static_cast<int>(ChallengeWaveGraveAction::NONE));
	ExpectEqual("original Grave Danger flag wave spawns from graves", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_CHALLENGE_GRAVE_DANGER, true, 5, 20, true)),
		static_cast<int>(ChallengeWaveGraveAction::SPAWN_ZOMBIES_FROM_GRAVES));
	ExpectEqual("original Grave Danger threshold", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_CHALLENGE_GRAVE_DANGER, false, 6, 20, false)),
		static_cast<int>(ChallengeWaveGraveAction::SPAWN_RANDOM_GRAVE));
	ExpectEqual("original Grave Danger below threshold", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_CHALLENGE_GRAVE_DANGER, false, 5, 20, false)),
		static_cast<int>(ChallengeWaveGraveAction::NONE));
	const ChallengeStartSetup anOriginalNoStartSetup = ResolveChallengeStartSetup(true, false);
	ExpectEqual("original ordinary mode skips bowling setup", anOriginalNoStartSetup.mApply, false);
	const ChallengeStartSetup anOriginalStartSetup = ResolveChallengeStartSetup(true, true);
	ExpectEqual("original bowling setup applies", anOriginalStartSetup.mApply, true);
	ExpectEqual("original bowling setup countdown", anOriginalStartSetup.mZombieCountdown, 200);
	ExpectEqual("original bowling setup seed", anOriginalStartSetup.mSeedType, SEED_WALLNUT);
	ExpectEqual("original bowling setup conveyor", anOriginalStartSetup.mConveyorBeltCounter, 400);
	ExpectEqual("original bowling setup writes line", anOriginalStartSetup.mSetShowBowlingLine, true);
	ExpectEqual("original bowling setup line", anOriginalStartSetup.mShowBowlingLine, true);
	ExpectEqual("original bowling setup keeps AddSeed checks", anOriginalStartSetup.mAllowEmptyNonConveyorSeed, false);
	ExpectEqual("original missing Board skips bowling setup", ResolveChallengeStartSetup(false, true).mApply, false);
	ExpectEqual("original seed chooser preserves allowed result", ResolveSeedNotAllowedToPick(
		GAMEMODE_CHALLENGE_AIR_RAID, SEED_SUNFLOWER, false), false);
	ExpectEqual("original seed chooser preserves forbidden result", ResolveSeedNotAllowedToPick(
		GAMEMODE_ADVENTURE, SEED_PEASHOOTER, true), true);
	const std::array<SeedType, 4> aTwistMarkers{
		SEED_PEASHOOTER, SEED_SUNFLOWER, SEED_CHERRYBOMB, SEED_WALLNUT};
	ExpectTwistPermutation("original clockwise Twist trial", aTwistMarkers,
		std::array{SEED_CHERRYBOMB, SEED_PEASHOOTER, SEED_WALLNUT, SEED_SUNFLOWER});
	ExpectTwistPlans("original clockwise Twist plan", std::array<BeghouledTwistCornerPlan, 4>{
		BeghouledTwistCornerPlan{BeghouledTwistCorner::BOTTOM_LEFT, true, false, 20, 0, 1, 0},
		BeghouledTwistCornerPlan{BeghouledTwistCorner::TOP_LEFT, false, true, 0, 20, 0, 1},
		BeghouledTwistCornerPlan{BeghouledTwistCorner::BOTTOM_RIGHT, false, true, 0, -20, 0, -1},
		BeghouledTwistCornerPlan{BeghouledTwistCorner::TOP_RIGHT, true, false, -20, 0, -1, 0}});
	for (const SeedRuleCase& aCase : aScarySeedCases)
		ExpectEqual("original Scary Potter seed", ResolveScaryPotterSeed(aCase.mGameMode, aCase.mIndex, aCase.mOriginal), aCase.mOriginal);
	for (const ZombieRuleCase& aCase : aScaryZombieCases)
		ExpectEqual("original Scary Potter zombie", ResolveScaryPotterZombie(aCase.mGameMode, aCase.mIndex, aCase.mOriginal), aCase.mOriginal);
	for (const ScaryPotterGateCase& aCase : aScaryPotterGateCases)
	{
		for (bool anOriginalValue : {false, true})
		{
			ExpectEqual("original mouse-position gate passthrough", ShouldRunScaryPotterMousePositionBlock(
				aCase.mGameMode, anOriginalValue), anOriginalValue);
			ExpectEqual("original mouse-hit gate passthrough", ShouldEvaluateScaryPotterMouseHitBlock(
				aCase.mGameMode, anOriginalValue), anOriginalValue);
			ExpectEqual("original mouse-down gate passthrough", ShouldHandleScaryPotterMouseDown(
				aCase.mGameMode, anOriginalValue), anOriginalValue);
			ExpectEqual("original update gate passthrough", ShouldRunScaryPotterUpdate(
				aCase.mGameMode, anOriginalValue), anOriginalValue);
		}
	}
	ExpectEqual("original Adventure level 35 mouse-position gate", ShouldRunScaryPotterMousePositionBlock(
		GAMEMODE_ADVENTURE, true), true);
	ExpectEqual("original Adventure level 35 mouse-hit gate", ShouldEvaluateScaryPotterMouseHitBlock(
		GAMEMODE_ADVENTURE, true), true);
	ExpectEqual("original Adventure level 35 mouse-down gate", ShouldHandleScaryPotterMouseDown(
		GAMEMODE_ADVENTURE, true), true);
	ExpectEqual("original Adventure level 35 update gate", ShouldRunScaryPotterUpdate(
		GAMEMODE_ADVENTURE, true), true);
	for (GameMode aGameMode : {GAMEMODE_ADVENTURE, GAMEMODE_CHALLENGE_POGO_PARTY, GAMEMODE_INTRO})
	{
		for (bool anOriginalValue : {false, true})
		{
			ExpectEqual("original StageHasGraveStones Pogo gate passthrough",
				ShouldRejectStageHasGraveStonesAtPogoGate(aGameMode, anOriginalValue), anOriginalValue);
		}
	}
	for (bool aStageHasGraveStones : {false, true})
	{
		for (bool anOriginalValue : {false, true})
		{
			ExpectEqual("original Grave Buster warning passthrough",
				ShouldWarnGraveBusterForLevel(aStageHasGraveStones, anOriginalValue), anOriginalValue);
		}
	}
	ExpectEqual("original radius non-burn damage", ResolveKillAllZombiesInRadiusNonBurnDamage(1800), 1800);
	ExpectEqual("original radius arbitrary damage passthrough", ResolveKillAllZombiesInRadiusNonBurnDamage(17), 17);
	for (HelmType aHelmType : aMagnetHelmetCases)
	{
		for (bool anOriginalValue : {false, true})
		{
			ExpectEqual("original magnet Football slot passthrough",
				IsMagnetShroomFootballHelmetEligible(aHelmType, anOriginalValue), anOriginalValue);
		}
	}
	ExpectEqual("original bowling shield damage", ResolveBowlingShieldDamage(400), 400);
	ExpectEqual("original bowling arbitrary shield damage passthrough", ResolveBowlingShieldDamage(17), 17);
	for (bool aBoardExists : {false, true})
	{
		for (SeedType aSeedType : anIZombieCursorSeedCases)
		{
			for (bool anOriginalValue : {false, true})
			{
				ExpectEqual("original I, Zombie cursor policy passthrough",
					ShouldUseIZombieCursorBehavior(aBoardExists, aSeedType, anOriginalValue),
					anOriginalValue);
			}
		}
	}

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
	const FutureModeMusic aPvZ95FutureNext = ResolveFutureModeMusic(static_cast<MusicTune>(1));
	ExpectEqual("PvZ 95 Future changes music", aPvZ95FutureNext.mShouldChangeTune, true);
	ExpectEqual("PvZ 95 Future advances music", static_cast<int>(aPvZ95FutureNext.mTune), 2);
	const FutureModeMusic aPvZ95FutureWrap = ResolveFutureModeMusic(static_cast<MusicTune>(12));
	ExpectEqual("PvZ 95 Future wraps final-boss music", aPvZ95FutureWrap.mShouldChangeTune, true);
	ExpectEqual("PvZ 95 Future wraps to day music", static_cast<int>(aPvZ95FutureWrap.mTune), 1);
	const FutureModeMusic aPvZ95FutureInvalid = ResolveFutureModeMusic(static_cast<MusicTune>(13));
	ExpectEqual("PvZ 95 Future ignores non-gameplay music", aPvZ95FutureInvalid.mShouldChangeTune, false);
	ExpectEqual("PvZ 95 Future preserves non-gameplay music", static_cast<int>(aPvZ95FutureInvalid.mTune), 13);
	ExpectEqual("verified Explode-o-nut enum byte", static_cast<int>(SEED_EXPLODE_O_NUT), 0x31);
	ExpectEqual("verified Cob Cannon is not cave byte", static_cast<int>(SEED_COBCANNON), 0x2F);
	const DanceModeSeedPacket aPvZ95DancePacket = ResolveDanceModeSeedPacket(SEED_WALLNUT, SEED_NONE);
	ExpectEqual("PvZ 95 Dance converts Wall-nut packet", aPvZ95DancePacket.mPacketType, SEED_EXPLODE_O_NUT);
	ExpectEqual("PvZ 95 Dance preserves ordinary imitater type", aPvZ95DancePacket.mImitaterType, SEED_NONE);
	const DanceModeSeedPacket aPvZ95DanceImitater = ResolveDanceModeSeedPacket(SEED_IMITATER, SEED_WALLNUT);
	ExpectEqual("PvZ 95 Dance converts an imitated Wall-nut packet", aPvZ95DanceImitater.mPacketType, SEED_EXPLODE_O_NUT);
	ExpectEqual("PvZ 95 Dance does not rewrite imitater metadata", aPvZ95DanceImitater.mImitaterType, SEED_WALLNUT);
	const DanceModeSeedPacket aPvZ95DanceOther = ResolveDanceModeSeedPacket(SEED_PEASHOOTER, SEED_NONE);
	ExpectEqual("PvZ 95 Dance preserves unrelated packet", aPvZ95DanceOther.mPacketType, SEED_PEASHOOTER);
	ExpectEqual("PvZ 95 SuperMower triggers ready mower", ResolveSuperMowerToggleState(MOWER_READY), MOWER_TRIGGERED);
	ExpectEqual("PvZ 95 SuperMower also rewrites squished mower", ResolveSuperMowerToggleState(MOWER_SQUISHED), MOWER_TRIGGERED);
	ExpectEqual("PvZ 95 restricted typing cheat is unlocked", CanUseRestrictedTypingCheat(false), true);
	ExpectEqual("PvZ 95 Sukhbir enables easy planting", ResolveSukhbirEasyPlanting(true, false), true);
	ExpectEqual("PvZ 95 Sukhbir disables easy planting", ResolveSukhbirEasyPlanting(false, true), false);
	ExpectEqual("PvZ 95 Pinata board edge dies without loot", ShouldDieNoLootAtBoardEdge(false, true), true);
	ExpectEqual("PvZ 95 ordinary board edge still loses", ShouldDieNoLootAtBoardEdge(false, false), false);
	ExpectChallengeZombieSet("PvZ 95 Speed zombie set", GAMEMODE_CHALLENGE_SPEED, false, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE, ZOMBIE_POLEVAULTER, ZOMBIE_FOOTBALL,
			ZOMBIE_DOLPHIN_RIDER, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_LADDER});
	ExpectChallengeZombieSet("PvZ 95 Pogo Party zombie set", GAMEMODE_CHALLENGE_POGO_PARTY, false, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE, ZOMBIE_PAIL, ZOMBIE_NEWSPAPER,
			ZOMBIE_DOOR, ZOMBIE_FOOTBALL, ZOMBIE_ZAMBONI, ZOMBIE_GARGANTUAR});
	ExpectChallengeZombieSet("PvZ 95 Portal Combat zombie set", GAMEMODE_CHALLENGE_PORTAL_COMBAT, false, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_PAIL, ZOMBIE_NEWSPAPER, ZOMBIE_BALLOON});
	ExpectChallengeZombieSet("PvZ 95 Little Trouble zombie set", GAMEMODE_CHALLENGE_LITTLE_TROUBLE, true, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE, ZOMBIE_DOOR,
			ZOMBIE_FOOTBALL, ZOMBIE_SNORKEL, ZOMBIE_LADDER});
	ExpectChallengeZombieSet("PvZ 95 adventure 2-5 zombie set", GAMEMODE_ADVENTURE, true, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE, ZOMBIE_DOOR,
			ZOMBIE_FOOTBALL, ZOMBIE_SNORKEL, ZOMBIE_LADDER});
	ExpectChallengeZombieSet("PvZ 95 Big Time zombie set", GAMEMODE_CHALLENGE_BIG_TIME, false, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE, ZOMBIE_PAIL,
			ZOMBIE_DOOR, ZOMBIE_POGO, ZOMBIE_JACK_IN_THE_BOX});
	ExpectChallengeZombieSet("PvZ 95 Raining Seeds zombie set", GAMEMODE_CHALLENGE_RAINING_SEEDS, false, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_FLAG, ZOMBIE_TRAFFIC_CONE, ZOMBIE_POLEVAULTER,
			ZOMBIE_PAIL, ZOMBIE_DOOR, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_BUNGEE});
	ExpectChallengeZombieSet("PvZ 95 Air Raid zombie set", GAMEMODE_CHALLENGE_AIR_RAID, false, false,
		std::array{ZOMBIE_TRAFFIC_CONE, ZOMBIE_POLEVAULTER,
			ZOMBIE_PAIL, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_BALLOON});
	ExpectChallengeZombieSet("PvZ 95 Column zombie set", GAMEMODE_CHALLENGE_COLUMN, false, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE, ZOMBIE_POLEVAULTER, ZOMBIE_PAIL});
	ExpectChallengeZombieSet("PvZ 95 Invisighoul zombie set", GAMEMODE_CHALLENGE_INVISIGHOUL, false, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE, ZOMBIE_PAIL,
			ZOMBIE_SNORKEL, ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_BUNGEE});
	ExpectChallengeZombieSet("PvZ 95 War and Peas zombie set", GAMEMODE_CHALLENGE_WAR_AND_PEAS, false, false,
		std::array{ZOMBIE_POLEVAULTER, ZOMBIE_NEWSPAPER, ZOMBIE_JACK_IN_THE_BOX,
			ZOMBIE_BALLOON, ZOMBIE_POGO, ZOMBIE_GARGANTUAR});
	ExpectChallengeZombieSet("PvZ 95 Wall-nut Bowling zombie set", GAMEMODE_CHALLENGE_WALLNUT_BOWLING, false, true,
		std::array{ZOMBIE_NORMAL, ZOMBIE_FLAG, ZOMBIE_TRAFFIC_CONE, ZOMBIE_PAIL,
			ZOMBIE_POLEVAULTER, ZOMBIE_NEWSPAPER, ZOMBIE_LADDER});
	ExpectChallengeZombieSet("PvZ 95 adventure 1-5 zombie set", GAMEMODE_ADVENTURE, false, true,
		std::array{ZOMBIE_NORMAL, ZOMBIE_FLAG, ZOMBIE_TRAFFIC_CONE, ZOMBIE_PAIL,
			ZOMBIE_POLEVAULTER, ZOMBIE_NEWSPAPER, ZOMBIE_LADDER});
	ExpectChallengeZombieSet("PvZ 95 Ice zombie set", GAMEMODE_CHALLENGE_ICE, false, false,
		std::array{ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE, ZOMBIE_PAIL, ZOMBIE_NEWSPAPER,
			ZOMBIE_DANCER, ZOMBIE_SNORKEL, ZOMBIE_DOLPHIN_RIDER, ZOMBIE_DIGGER});
	ExpectEqual("PvZ 95 unaffected zombie permission stays true", ResolveChallengeZombieAllowed(
		GAMEMODE_CHALLENGE_SUNNY_DAY, false, false, ZOMBIE_NORMAL, true), true);
	ExpectEqual("PvZ 95 unaffected zombie permission stays false", ResolveChallengeZombieAllowed(
		GAMEMODE_CHALLENGE_SUNNY_DAY, false, false, ZOMBIE_POGO, false), false);
	ExpectEqual("PvZ 95 Daisy skips grave chain", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_ADVENTURE, true, 10, 20, true)), static_cast<int>(ChallengeWaveGraveAction::NONE));
	ExpectEqual("PvZ 95 all modes run flag grave chain", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_ADVENTURE, false, 5, 20, true)),
		static_cast<int>(ChallengeWaveGraveAction::SPAWN_ZOMBIES_FROM_GRAVES));
	ExpectEqual("PvZ 95 Seeing Stars compare has no semantic effect", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_CHALLENGE_SEEING_STARS, false, 5, 20, true)),
		static_cast<int>(ChallengeWaveGraveAction::SPAWN_ZOMBIES_FROM_GRAVES));
	ExpectEqual("PvZ 95 grave threshold excludes wave nine", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_ADVENTURE, false, 9, 20, false)), static_cast<int>(ChallengeWaveGraveAction::NONE));
	ExpectEqual("PvZ 95 grave threshold starts above nine", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_ADVENTURE, false, 10, 20, false)),
		static_cast<int>(ChallengeWaveGraveAction::SPAWN_RANDOM_GRAVE));
	ExpectEqual("PvZ 95 final wave skips grave chain", static_cast<int>(ResolveChallengeWaveGraveAction(
		GAMEMODE_ADVENTURE, false, 19, 20, true)), static_cast<int>(ChallengeWaveGraveAction::NONE));
	const ChallengeStartSetup aPvZ95StartSetup = ResolveChallengeStartSetup(true, false);
	ExpectEqual("PvZ 95 ordinary mode enters bowling setup", aPvZ95StartSetup.mApply, true);
	ExpectEqual("PvZ 95 universal setup countdown", aPvZ95StartSetup.mZombieCountdown, 200);
	ExpectEqual("PvZ 95 universal setup seed", aPvZ95StartSetup.mSeedType, SEED_NONE);
	ExpectEqual("PvZ 95 universal setup conveyor", aPvZ95StartSetup.mConveyorBeltCounter, 400);
	ExpectEqual("PvZ 95 ordinary mode does not write bowling line",
		aPvZ95StartSetup.mSetShowBowlingLine, false);
	ExpectEqual("PvZ 95 ordinary setup line value remains false", aPvZ95StartSetup.mShowBowlingLine, false);
	ExpectEqual("PvZ 95 universal setup permits empty non-conveyor seed",
		aPvZ95StartSetup.mAllowEmptyNonConveyorSeed, true);
	const ChallengeStartSetup aPvZ95BowlingStartSetup = ResolveChallengeStartSetup(true, true);
	ExpectEqual("PvZ 95 Wall-nut Bowling still writes line", aPvZ95BowlingStartSetup.mSetShowBowlingLine, true);
	ExpectEqual("PvZ 95 Wall-nut Bowling still shows line", aPvZ95BowlingStartSetup.mShowBowlingLine, true);
	ExpectEqual("PvZ 95 missing Board skips universal setup", ResolveChallengeStartSetup(false, true).mApply, false);
	ExpectEqual("verified Last Stand mode immediate", static_cast<int>(GAMEMODE_CHALLENGE_LAST_STAND), 0x1F);
	ExpectEqual("verified Air Raid mode immediate", static_cast<int>(GAMEMODE_CHALLENGE_AIR_RAID), 0x29);
	ExpectEqual("verified Marigold seed immediate", static_cast<int>(SEED_MARIGOLD), 0x26);
	constexpr std::array<SeedType, 6> aForbiddenChooserSeeds{
		SEED_SUNFLOWER, SEED_SUNSHROOM, SEED_TWINSUNFLOWER,
		SEED_SEASHROOM, SEED_MARIGOLD, SEED_PUFFSHROOM};
	for (GameMode aGameMode : {GAMEMODE_CHALLENGE_LAST_STAND, GAMEMODE_CHALLENGE_AIR_RAID})
	{
		for (SeedType aSeedType : aForbiddenChooserSeeds)
		{
			ExpectEqual("PvZ 95 seed chooser forbidden set", ResolveSeedNotAllowedToPick(
				aGameMode, aSeedType, false), true);
		}
	}
	ExpectEqual("PvZ 95 Last Stand allows adjacent seed", ResolveSeedNotAllowedToPick(
		GAMEMODE_CHALLENGE_LAST_STAND, SEED_PEASHOOTER, true), false);
	ExpectEqual("PvZ 95 Air Raid allows adjacent seed", ResolveSeedNotAllowedToPick(
		GAMEMODE_CHALLENGE_AIR_RAID, SEED_FUMESHROOM, true), false);
	ExpectEqual("PvZ 95 unrelated mode has no chooser restriction", ResolveSeedNotAllowedToPick(
		GAMEMODE_ADVENTURE, SEED_MARIGOLD, true), false);
	ExpectTwistPermutation("PvZ 95 diagonal Twist trial", aTwistMarkers,
		std::array{SEED_WALLNUT, SEED_CHERRYBOMB, SEED_SUNFLOWER, SEED_PEASHOOTER});
	ExpectTwistPlans("PvZ 95 diagonal Twist plan", std::array<BeghouledTwistCornerPlan, 4>{
		BeghouledTwistCornerPlan{BeghouledTwistCorner::BOTTOM_RIGHT, true, true, 20, 20, 1, 1},
		BeghouledTwistCornerPlan{BeghouledTwistCorner::BOTTOM_LEFT, true, true, -20, 20, -1, 1},
		BeghouledTwistCornerPlan{BeghouledTwistCorner::TOP_RIGHT, true, true, 20, -20, 1, -1},
		BeghouledTwistCornerPlan{BeghouledTwistCorner::TOP_LEFT, true, true, -20, -20, -1, -1}});
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
	ExpectEqual("PvZ 95 torchwood pea becomes snow pea", static_cast<int>(ResolveTorchwoodConversion(
		ProjectileType::PROJECTILE_PEA)), static_cast<int>(TorchwoodConversion::PEA));
	ExpectEqual("PvZ 95 torchwood snow pea becomes fireball", static_cast<int>(ResolveTorchwoodConversion(
		ProjectileType::PROJECTILE_SNOWPEA)), static_cast<int>(TorchwoodConversion::FIREBALL));
	ExpectEqual("PvZ 95 torchwood ignores star", static_cast<int>(ResolveTorchwoodConversion(
		ProjectileType::PROJECTILE_STAR)), static_cast<int>(TorchwoodConversion::NONE));
	const PotatoMineExplosion aPvZ95PotatoExplosion = ResolvePotatoMineExplosion(60, 0, false);
	ExpectEqual("PvZ 95 potato mine radius", aPvZ95PotatoExplosion.mRadius, 115);
	ExpectEqual("PvZ 95 potato mine row range", aPvZ95PotatoExplosion.mRowRange, 1);
	ExpectEqual("PvZ 95 potato mine burn", aPvZ95PotatoExplosion.mBurn, true);
	ExpectEqual("PvZ 95 rising bungee plant becomes targetable", ResolveZombieTargetPlantNotOnGround(
		false, false, false, false, true), false);
	ExpectEqual("PvZ 95 squished rising plant stays untargetable", ResolveZombieTargetPlantNotOnGround(
		false, false, true, false, true), true);
	ExpectEqual("PvZ 95 dead rising plant stays untargetable", ResolveZombieTargetPlantNotOnGround(
		false, false, false, true, true), true);
	ExpectEqual("PvZ 95 airborne squash stays untargetable", ResolveZombieTargetPlantNotOnGround(
		true, false, false, false, true), true);
	ExpectEqual("PvZ 95 scaredy-shroom scared state is untargetable", ResolveZombieTargetPlantNotOnGround(
		false, true, false, false, false), true);
	ExpectEqual("PvZ 95 ordinary grounded plant stays targetable", ResolveZombieTargetPlantNotOnGround(
		false, false, false, false, false), false);
	ExpectEqual("screen door stores bucket member type", ResolveZombieMemberType(
		ZombieType::ZOMBIE_DOOR), ZombieType::ZOMBIE_PAIL);
	const ZombiePreSwitchArmor aPvZ95DoorArmor = ResolveZombiePreSwitchArmor(
		ZombieType::ZOMBIE_PAIL, HelmType::HELMTYPE_NONE, 0);
	ExpectEqual("screen door receives bucket helmet", aPvZ95DoorArmor.mHelmType, HelmType::HELMTYPE_PAIL);
	ExpectEqual("screen door receives bucket helmet health", aPvZ95DoorArmor.mHelmHealth, 1100);
	const ZombieFlagArmor aPvZ95FlagArmor = ResolveZombieFlagArmor(HelmType::HELMTYPE_NONE, 0);
	ExpectEqual("flag zombie shows bucket", aPvZ95FlagArmor.mShowBucket, true);
	ExpectEqual("flag zombie receives bucket helmet", aPvZ95FlagArmor.mHelmType, HelmType::HELMTYPE_PAIL);
	ExpectEqual("flag zombie receives bucket helmet health", aPvZ95FlagArmor.mHelmHealth, 1100);
	ExpectEqual("flag zombie receives bucket helmet max health", aPvZ95FlagArmor.mHelmMaxHealth, 1100);
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
	ExpectEqual("I, Zombie reward fixed override",
		ResolveIZombieSunflowerReward(CoinType::COIN_LARGESUN), CoinType::COIN_SMALLSUN);
	ExpectEqual("PvZ 95 usable packet initializes to Blover",
		ResolveCoinInitialUsableSeedType(SEED_NONE), SEED_BLOVER);
	ExpectEqual("PvZ 95 usable packet keeps fixed Blover default",
		ResolveCoinInitialUsableSeedType(SEED_BLOVER), SEED_BLOVER);
	ExpectEqual("PvZ 95 normal sun scores 50", ResolveScoredSunValue(COIN_SUN, 25), 50);
	ExpectEqual("PvZ 95 small sun scores 25", ResolveScoredSunValue(COIN_SMALLSUN, 15), 25);
	ExpectEqual("PvZ 95 large sun scores 75", ResolveScoredSunValue(COIN_LARGESUN, 50), 75);
	ExpectEqual("PvZ 95 lower non-sun score passthrough", ResolveScoredSunValue(COIN_DIAMOND, 103), 103);
	ExpectEqual("PvZ 95 upper non-sun score passthrough", ResolveScoredSunValue(COIN_FINAL_SEED_PACKET, 107), 107);
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
	const ZombieShieldDamagePolicy aPvZ95ShieldBypass = ResolveZombieShieldDamagePolicy(
		100, SHIELDTYPE_DOOR, aBypassShieldFlag);
	ExpectEqual("PvZ 95 bypass flag still damages shield", aPvZ95ShieldBypass.mTakeShieldDamage, true);
	ExpectEqual("PvZ 95 bypass flag does not restore body damage", aPvZ95ShieldBypass.mRestoreOriginalDamage, false);
	const ZombieShieldDamagePolicy aPvZ95ShieldAndBody = ResolveZombieShieldDamagePolicy(
		100, SHIELDTYPE_DOOR, aHitShieldAndBodyFlag);
	ExpectEqual("PvZ 95 dual-hit flag damages shield", aPvZ95ShieldAndBody.mTakeShieldDamage, true);
	ExpectEqual("PvZ 95 dual-hit flag does not restore body damage", aPvZ95ShieldAndBody.mRestoreOriginalDamage, false);
	const ZombieShieldDamagePolicy aPvZ95BothShieldFlags = ResolveZombieShieldDamagePolicy(
		100, SHIELDTYPE_DOOR, aBypassShieldFlag | aHitShieldAndBodyFlag);
	ExpectEqual("PvZ 95 combined flags still damage shield", aPvZ95BothShieldFlags.mTakeShieldDamage, true);
	ExpectEqual("PvZ 95 combined flags do not restore body damage", aPvZ95BothShieldFlags.mRestoreOriginalDamage, false);
	ExpectEqual("PvZ 95 zero damage skips shield", ResolveZombieShieldDamagePolicy(
		0, SHIELDTYPE_DOOR, 0U).mTakeShieldDamage, false);
	ExpectEqual("PvZ 95 missing shield skips shield", ResolveZombieShieldDamagePolicy(
		100, SHIELDTYPE_NONE, 0U).mTakeShieldDamage, false);
	ExpectEqual("PvZ 95 Plant BurnRow suppresses ApplyBurn damage", ResolveApplyBurnDamage(true, 1800), 0);
	ExpectEqual("PvZ 95 generic ApplyBurn keeps damage", ResolveApplyBurnDamage(false, 1800), 1800);
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
	ExpectEqual("PvZ 95 Ice disables every legacy special-case compare", UsesLegacyIceChallengeSpecialCase(
		GameMode::GAMEMODE_CHALLENGE_ICE), false);
	ExpectEqual("PvZ 95 Ice no longer rewrites an intermediate fixed packet", ResolveInitialSeedPacket(
		GameMode::GAMEMODE_CHALLENGE_ICE, false, 1, SeedType::SEED_CHERRYBOMB), SeedType::SEED_CHERRYBOMB);
	ExpectEqual("PvZ 95 Ice pool background", ResolveChallengeBackground(
		GameMode::GAMEMODE_CHALLENGE_ICE, BackgroundType::BACKGROUND_1_DAY), BackgroundType::BACKGROUND_3_POOL);
	ExpectEqual("PvZ 95 Air Raid day background", ResolveChallengeBackground(
		GameMode::GAMEMODE_CHALLENGE_AIR_RAID, BackgroundType::BACKGROUND_4_FOG), BackgroundType::BACKGROUND_1_DAY);
	ExpectEqual("PvZ 95 ordinary background unchanged", ResolveChallengeBackground(
		GameMode::GAMEMODE_ADVENTURE, BackgroundType::BACKGROUND_2_NIGHT), BackgroundType::BACKGROUND_2_NIGHT);
	ExpectEqual("PvZ 95 Ice loses below sun threshold", ShouldIceChallengeLoseBeforeAward(
		GameMode::GAMEMODE_CHALLENGE_ICE, 27499), true);
	ExpectEqual("PvZ 95 Ice wins at sun threshold", ShouldIceChallengeLoseBeforeAward(
		GameMode::GAMEMODE_CHALLENGE_ICE, 27500), false);
	ExpectEqual("PvZ 95 non-Ice ignores sun threshold", ShouldIceChallengeLoseBeforeAward(
		GameMode::GAMEMODE_ADVENTURE, 0), false);
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
	for (const ScaryPotterGateCase& aCase : aScaryPotterGateCases)
	{
		for (bool anOriginalValue : {false, true})
		{
			ExpectEqual("PvZ 95 mouse-position expanded range", ShouldRunScaryPotterMousePositionBlock(
				aCase.mGameMode, anOriginalValue), aCase.mExpandedRange);
			ExpectEqual("PvZ 95 mouse-hit gate is unconditional", ShouldEvaluateScaryPotterMouseHitBlock(
				aCase.mGameMode, anOriginalValue), true);
			ExpectEqual("PvZ 95 mouse-down gate is unconditional", ShouldHandleScaryPotterMouseDown(
				aCase.mGameMode, anOriginalValue), true);
			ExpectEqual("PvZ 95 update expanded range", ShouldRunScaryPotterUpdate(
				aCase.mGameMode, anOriginalValue), aCase.mExpandedRange);
		}
	}
	for (GameMode aGameMode : {GAMEMODE_ADVENTURE, GAMEMODE_CHALLENGE_POGO_PARTY, GAMEMODE_INTRO})
	{
		for (bool anOriginalValue : {false, true})
		{
			ExpectEqual("PvZ 95 StageHasGraveStones forced rejection",
				ShouldRejectStageHasGraveStonesAtPogoGate(aGameMode, anOriginalValue), true);
		}
	}
	for (bool aStageHasGraveStones : {false, true})
	{
		for (bool anOriginalValue : {false, true})
		{
			ExpectEqual("PvZ 95 Grave Buster warning follows grave presence",
				ShouldWarnGraveBusterForLevel(aStageHasGraveStones, anOriginalValue), aStageHasGraveStones);
		}
	}
	ExpectEqual("PvZ 95 radius non-burn damage", ResolveKillAllZombiesInRadiusNonBurnDamage(1800), 2400);
	ExpectEqual("PvZ 95 radius non-burn fixed override", ResolveKillAllZombiesInRadiusNonBurnDamage(17), 2400);
	for (HelmType aHelmType : aMagnetHelmetCases)
	{
		const bool anExpected = static_cast<int>(aHelmType) == -1;
		for (bool anOriginalValue : {false, true})
		{
			ExpectEqual("PvZ 95 magnet Football slot compares against -1",
				IsMagnetShroomFootballHelmetEligible(aHelmType, anOriginalValue), anExpected);
		}
	}
	ExpectEqual("PvZ 95 bowling shield damage", ResolveBowlingShieldDamage(400), 800);
	ExpectEqual("PvZ 95 bowling shield fixed override", ResolveBowlingShieldDamage(17), 800);
	for (SeedType aSeedType : anIZombieCursorSeedCases)
	{
		for (bool anOriginalValue : {false, true})
		{
			ExpectEqual("PvZ 95 null Board disables I, Zombie cursor policy",
				ShouldUseIZombieCursorBehavior(false, aSeedType, anOriginalValue), false);
			ExpectEqual("PvZ 95 I, Zombie cursor seed threshold",
				ShouldUseIZombieCursorBehavior(true, aSeedType, anOriginalValue),
				static_cast<int>(aSeedType) > static_cast<int>(SEED_LEFTPEATER));
		}
	}
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
