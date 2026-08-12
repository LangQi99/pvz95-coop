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

	RulesetId GetActiveRuleset();
	void SetActiveRuleset(RulesetId theRuleset);
	bool SetActiveRuleset(std::string_view theName);
	std::string_view GetActiveRulesetName();
	uint32_t GetActiveRulesetProtocolId();

	const PlantTuning& GetPlantTuning(SeedType theSeedType);
	int ResolvePlantSeedCost(SeedType theSeedType, int theOriginalValue);
	int ResolvePlantRefreshTime(SeedType theSeedType, int theOriginalValue);
	int ResolvePlantLaunchRate(SeedType theSeedType, int theOriginalValue);
	int ResolveProjectileDamage(ProjectileType theProjectileType, int theOriginalValue);
}
