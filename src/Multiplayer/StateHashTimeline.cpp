/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "StateHashTimeline.h"

#include <algorithm>
#include <set>

namespace PvzMultiplayer
{
	StateHashTimeline::StateHashTimeline(uint64_t theGraceTicks, size_t theCapacity) :
		mGraceTicks(theGraceTicks),
		mCapacity(theCapacity)
	{
	}

	StateHashResult StateHashTimeline::ObserveLocal(uint64_t theTick, uint64_t theHash,
		uint64_t theCurrentTick)
	{
		return Observe(mLocalHashes, mRemoteHashes, theTick, theHash, theCurrentTick, true);
	}

	StateHashResult StateHashTimeline::ObserveRemote(uint64_t theTick, uint64_t theHash,
		uint64_t theCurrentTick)
	{
		return Observe(mRemoteHashes, mLocalHashes, theTick, theHash, theCurrentTick, false);
	}

	StateHashResult StateHashTimeline::Observe(std::map<uint64_t, uint64_t>& theSide,
		std::map<uint64_t, uint64_t>& theOtherSide, uint64_t theTick,
		uint64_t theHash, uint64_t theCurrentTick, bool theIsLocal)
	{
		StateHashResult anExpired = AdvanceTo(theCurrentTick);
		if (anExpired.mKind != StateHashResultKind::WAITING)
			return anExpired;

		auto anExisting = theSide.find(theTick);
		if (anExisting != theSide.end() && anExisting->second != theHash)
		{
			return {StateHashResultKind::CONFLICT, theTick,
				theIsLocal ? anExisting->second : theHash,
				theIsLocal ? theHash : anExisting->second};
		}

		if (anExisting == theSide.end())
		{
			bool aCompletesPair = theOtherSide.contains(theTick);
			if (!aCompletesPair && GetPendingTickCount() >= mCapacity)
				return {StateHashResultKind::FULL, theTick};
			theSide.emplace(theTick, theHash);
		}

		return CompareAndErase(theTick);
	}

	StateHashResult StateHashTimeline::CompareAndErase(uint64_t theTick)
	{
		auto aLocal = mLocalHashes.find(theTick);
		auto aRemote = mRemoteHashes.find(theTick);
		if (aLocal == mLocalHashes.end() || aRemote == mRemoteHashes.end())
			return {StateHashResultKind::WAITING, theTick};

		StateHashResult aResult{
			aLocal->second == aRemote->second ? StateHashResultKind::MATCHED : StateHashResultKind::MISMATCH,
			theTick, aLocal->second, aRemote->second};
		mLocalHashes.erase(aLocal);
		mRemoteHashes.erase(aRemote);
		return aResult;
	}

	StateHashResult StateHashTimeline::AdvanceTo(uint64_t theCurrentTick)
	{
		uint64_t anOldestTick = UINT64_MAX;
		if (!mLocalHashes.empty())
			anOldestTick = std::min(anOldestTick, mLocalHashes.begin()->first);
		if (!mRemoteHashes.empty())
			anOldestTick = std::min(anOldestTick, mRemoteHashes.begin()->first);
		if (anOldestTick == UINT64_MAX || theCurrentTick <= anOldestTick ||
			theCurrentTick - anOldestTick <= mGraceTicks)
			return {StateHashResultKind::WAITING, anOldestTick == UINT64_MAX ? 0 : anOldestTick};

		StateHashResult aResult{StateHashResultKind::EXPIRED, anOldestTick};
		if (auto aLocal = mLocalHashes.find(anOldestTick); aLocal != mLocalHashes.end())
			aResult.mLocalHash = aLocal->second;
		if (auto aRemote = mRemoteHashes.find(anOldestTick); aRemote != mRemoteHashes.end())
			aResult.mRemoteHash = aRemote->second;
		mLocalHashes.erase(anOldestTick);
		mRemoteHashes.erase(anOldestTick);
		return aResult;
	}

	void StateHashTimeline::Reset()
	{
		mLocalHashes.clear();
		mRemoteHashes.clear();
	}

	size_t StateHashTimeline::GetPendingTickCount() const
	{
		std::set<uint64_t> aTicks;
		for (const auto& [aTick, aHash] : mLocalHashes)
			aTicks.insert(aTick);
		for (const auto& [aTick, aHash] : mRemoteHashes)
			aTicks.insert(aTick);
		return aTicks.size();
	}
}
