/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "SharedInputState.h"

#include <algorithm>
#include <cstdint>

namespace PvzMultiplayer
{
	namespace
	{
		constexpr std::array<uint32_t, MAX_PLAYERS> CURSOR_COLORS = {
			0xEF5350, // red
			0x42A5F5, // blue
			0x66BB6A, // green
			0xAB47BC  // purple
		};
	}

	uint16_t NormalizeCoordinate(int theCoordinate, int theExtent)
	{
		if (theExtent <= 1)
			return 0;
		int aCoordinate = std::clamp(theCoordinate, 0, theExtent - 1);
		uint64_t aScaled = static_cast<uint64_t>(aCoordinate) * UINT16_MAX +
			static_cast<uint64_t>(theExtent - 1) / 2;
		return static_cast<uint16_t>(aScaled / static_cast<uint64_t>(theExtent - 1));
	}

	int DenormalizeCoordinate(uint16_t theCoordinate, int theExtent)
	{
		if (theExtent <= 1)
			return 0;
		uint64_t aScaled = static_cast<uint64_t>(theCoordinate) * static_cast<uint64_t>(theExtent - 1) +
			UINT16_MAX / 2;
		return static_cast<int>(aScaled / UINT16_MAX);
	}

	uint32_t GetPlayerCursorColor(PlayerId thePlayerId)
	{
		return thePlayerId < CURSOR_COLORS.size() ? CURSOR_COLORS[thePlayerId] : 0;
	}

	bool IsSequenceNewer(uint32_t theSequence, uint32_t thePreviousSequence)
	{
		return static_cast<int32_t>(theSequence - thePreviousSequence) > 0;
	}

	void SharedInputState::Reset(PlayerId theLocalPlayerId)
	{
		mCursors.fill(std::nullopt);
		mLocalPlayerId = theLocalPlayerId < MAX_PLAYERS ? theLocalPlayerId : 0;
	}

	bool SharedInputState::ApplyCursor(CursorUpdate theCursor, uint64_t theReceivedAtTick)
	{
		if (theCursor.mPlayerId >= MAX_PLAYERS)
			return false;
		auto& aSlot = mCursors[theCursor.mPlayerId];
		if (aSlot && !IsSequenceNewer(theCursor.mSequence, aSlot->mUpdate.mSequence))
			return false;

		aSlot = SharedCursor{theCursor, GetPlayerCursorColor(theCursor.mPlayerId), theReceivedAtTick};
		return true;
	}

	void SharedInputState::RemovePlayer(PlayerId thePlayerId)
	{
		if (thePlayerId < MAX_PLAYERS)
			mCursors[thePlayerId].reset();
	}

	PlayerId SharedInputState::GetLocalPlayerId() const
	{
		return mLocalPlayerId;
	}

	const std::array<std::optional<SharedCursor>, MAX_PLAYERS>& SharedInputState::GetCursors() const
	{
		return mCursors;
	}
}
