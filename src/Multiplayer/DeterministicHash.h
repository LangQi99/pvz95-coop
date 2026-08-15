/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

class Board;

namespace PvzMultiplayer
{
	class DeterministicHash64
	{
	public:
		void AddBytes(std::span<const uint8_t> theBytes);
		void AddString(std::string_view theString);
		void AddBool(bool theValue);
		void AddU8(uint8_t theValue);
		void AddU32(uint32_t theValue);
		void AddU64(uint64_t theValue);
		void AddI32(int32_t theValue);
		void AddFloat(float theValue);

		uint64_t Finish() const;

	private:
		uint64_t mHash{14695981039346656037ULL};
	};

	struct BoardStateHashBreakdown
	{
		uint64_t mCore{};
		uint64_t mGrid{};
		uint64_t mFog{};
		uint64_t mRowsAndIce{};
		uint64_t mWaves{};
		uint64_t mSeedBank{};
		uint64_t mChallenge{};
		uint64_t mGameplayAnimations{};
		uint64_t mPlants{};
		uint64_t mZombies{};
		uint64_t mProjectiles{};
		uint64_t mCoins{};
		uint64_t mMowers{};
		uint64_t mGridItems{};
	};

	BoardStateHashBreakdown ComputeBoardStateHashBreakdown(const Board& theBoard);
	uint64_t ComputeBoardStateHash(const Board& theBoard);
}
