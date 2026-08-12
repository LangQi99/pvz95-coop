/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "SessionBarrier.h"

#include <algorithm>

namespace PvzMultiplayer
{
	bool SessionBarrier::Start(uint64_t theStartId, std::span<const PlayerId> thePlayers)
	{
		Reset();
		if (theStartId == 0 || thePlayers.empty())
			return false;
		for (PlayerId aPlayerId : thePlayers)
		{
			if (aPlayerId >= MAX_PLAYERS || mMembers[aPlayerId])
			{
				Reset();
				return false;
			}
			mMembers[aPlayerId] = true;
		}
		if (!mMembers[0])
		{
			Reset();
			return false;
		}

		mStartId = theStartId;
		mReadyPlayers[0] = true;
		return true;
	}

	bool SessionBarrier::MarkReady(const SessionReady& theReady)
	{
		if (!IsActive() || theReady.mStartId != mStartId || theReady.mPlayerId == 0 ||
			theReady.mPlayerId >= MAX_PLAYERS || !mMembers[theReady.mPlayerId])
			return false;
		mReadyPlayers[theReady.mPlayerId] = true;
		return true;
	}

	void SessionBarrier::RemovePlayer(PlayerId thePlayerId)
	{
		if (thePlayerId == 0 || thePlayerId >= MAX_PLAYERS)
			return;
		mMembers[thePlayerId] = false;
		mReadyPlayers[thePlayerId] = false;
	}

	void SessionBarrier::Reset()
	{
		mMembers.fill(false);
		mReadyPlayers.fill(false);
		mStartId = 0;
	}

	bool SessionBarrier::IsActive() const
	{
		return mStartId != 0;
	}

	bool SessionBarrier::AllReady() const
	{
		return IsActive() && std::equal(mMembers.begin(), mMembers.end(), mReadyPlayers.begin());
	}

	uint64_t SessionBarrier::GetStartId() const
	{
		return mStartId;
	}

	const std::array<bool, MAX_PLAYERS>& SessionBarrier::GetMembers() const
	{
		return mMembers;
	}

	const std::array<bool, MAX_PLAYERS>& SessionBarrier::GetReadyPlayers() const
	{
		return mReadyPlayers;
	}
}
