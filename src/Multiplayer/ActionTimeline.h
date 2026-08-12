/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "Protocol.h"

#include <cstddef>
#include <vector>

namespace PvzMultiplayer
{
	enum class ScheduleResult : uint8_t
	{
		ACCEPTED,
		PAST_TICK,
		DUPLICATE,
		FULL
	};

	class ActionTimeline
	{
	public:
		explicit ActionTimeline(size_t theCapacity = 512);

		ScheduleResult Schedule(const GameAction& theAction, uint64_t theCurrentTick);
		std::vector<GameAction> TakeForTick(uint64_t theTick);
		void Reset();

		size_t GetSize() const;
		bool HasLateAction() const;

	private:
		struct ScheduledAction
		{
			GameAction mAction;
			uint64_t mArrivalOrder{};
		};

		std::vector<ScheduledAction> mActions;
		size_t mCapacity;
		uint64_t mNextArrivalOrder{};
		bool mHasLateAction{};
	};
}
