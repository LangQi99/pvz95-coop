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

	uint64_t ComputeBoardStateHash(const Board& theBoard);
}
