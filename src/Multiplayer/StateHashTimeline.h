/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

namespace PvzMultiplayer
{
	enum class StateHashResultKind : uint8_t
	{
		WAITING,
		MATCHED,
		MISMATCH,
		CONFLICT,
		EXPIRED,
		FULL
	};

	struct StateHashResult
	{
		StateHashResultKind mKind{StateHashResultKind::WAITING};
		uint64_t mTick{};
		uint64_t mLocalHash{};
		uint64_t mRemoteHash{};
	};

	// TCP preserves message order, but a TickSync and its StateHash can still be
	// returned by different Poll calls. Keep both observations until they can be
	// compared instead of treating a temporarily missing hash as a desync.
	class StateHashTimeline
	{
	public:
		explicit StateHashTimeline(uint64_t theGraceTicks = 300, size_t theCapacity = 16);

		StateHashResult ObserveLocal(uint64_t theTick, uint64_t theHash, uint64_t theCurrentTick);
		StateHashResult ObserveRemote(uint64_t theTick, uint64_t theHash, uint64_t theCurrentTick);
		StateHashResult AdvanceTo(uint64_t theCurrentTick);
		void Reset();

		size_t GetPendingTickCount() const;

	private:
		StateHashResult Observe(std::map<uint64_t, uint64_t>& theSide,
			std::map<uint64_t, uint64_t>& theOtherSide, uint64_t theTick,
			uint64_t theHash, uint64_t theCurrentTick, bool theIsLocal);
		StateHashResult CompareAndErase(uint64_t theTick);

		std::map<uint64_t, uint64_t> mLocalHashes;
		std::map<uint64_t, uint64_t> mRemoteHashes;
		uint64_t mGraceTicks;
		size_t mCapacity;
	};
}
