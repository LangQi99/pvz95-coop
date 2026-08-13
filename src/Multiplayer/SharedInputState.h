/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "Protocol.h"

#include <array>
#include <optional>

namespace PvzMultiplayer
{
	constexpr uint64_t CURSOR_INTERPOLATION_TICKS = 4;
	constexpr uint64_t CURSOR_INTERPOLATION_GAP_TICKS = 12;
	constexpr uint16_t CURSOR_INTERPOLATION_SNAP_DISTANCE = UINT16_MAX / 3;

	struct CursorPosition
	{
		uint16_t mNormalizedX{};
		uint16_t mNormalizedY{};

		bool operator==(const CursorPosition&) const = default;
	};

	struct SharedCursor
	{
		CursorUpdate mUpdate;
		uint32_t mRgb{};
		uint64_t mReceivedAtTick{};
		CursorPosition mInterpolationStart;
		uint64_t mInterpolationStartTick{};

		bool operator==(const SharedCursor&) const = default;
	};

	uint16_t NormalizeCoordinate(int theCoordinate, int theExtent);
	int DenormalizeCoordinate(uint16_t theCoordinate, int theExtent);
	uint32_t GetPlayerCursorColor(PlayerId thePlayerId);
	bool IsSequenceNewer(uint32_t theSequence, uint32_t thePreviousSequence);
	uint16_t InterpolateCursorCoordinate(uint16_t theStart, uint16_t theTarget,
		uint64_t theStartTick, uint64_t theCurrentTick,
		uint64_t theDuration = CURSOR_INTERPOLATION_TICKS);
	CursorPosition SampleCursorPosition(const SharedCursor& theCursor, uint64_t theCurrentTick);

	class SharedInputState
	{
	public:
		void Reset(PlayerId theLocalPlayerId);
		bool ApplyCursor(CursorUpdate theCursor, uint64_t theReceivedAtTick);
		void RemovePlayer(PlayerId thePlayerId);

		PlayerId GetLocalPlayerId() const;
		const std::array<std::optional<SharedCursor>, MAX_PLAYERS>& GetCursors() const;

	private:
		std::array<std::optional<SharedCursor>, MAX_PLAYERS> mCursors;
		PlayerId mLocalPlayerId{};
	};
}
