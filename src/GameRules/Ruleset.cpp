/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Ruleset.h"

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
}
