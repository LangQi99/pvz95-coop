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
	struct SharedCursor
	{
		CursorUpdate mUpdate;
		uint32_t mRgb{};
		uint64_t mReceivedAtTick{};

		bool operator==(const SharedCursor&) const = default;
	};

	uint16_t NormalizeCoordinate(int theCoordinate, int theExtent);
	int DenormalizeCoordinate(uint16_t theCoordinate, int theExtent);
	uint32_t GetPlayerCursorColor(PlayerId thePlayerId);
	bool IsSequenceNewer(uint32_t theSequence, uint32_t thePreviousSequence);

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
