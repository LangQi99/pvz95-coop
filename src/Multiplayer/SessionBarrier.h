/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "Protocol.h"

#include <array>
#include <span>

namespace PvzMultiplayer
{
	class SessionBarrier
	{
	public:
		bool Start(uint64_t theStartId, std::span<const PlayerId> thePlayers);
		bool MarkReady(const SessionReady& theReady);
		void RemovePlayer(PlayerId thePlayerId);
		void Reset();

		bool IsActive() const;
		bool AllReady() const;
		uint64_t GetStartId() const;
		const std::array<bool, MAX_PLAYERS>& GetMembers() const;
		const std::array<bool, MAX_PLAYERS>& GetReadyPlayers() const;

	private:
		std::array<bool, MAX_PLAYERS> mMembers{};
		std::array<bool, MAX_PLAYERS> mReadyPlayers{};
		uint64_t mStartId{};
	};
}
