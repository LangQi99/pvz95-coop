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

	class InputTimeline
	{
	public:
		explicit InputTimeline(size_t theCapacity = 512);

		ScheduleResult Schedule(const InputCommand& theInput, uint64_t theCurrentTick);
		std::vector<InputCommand> TakeForTick(uint64_t theTick);
		void Reset();

		size_t GetSize() const;
		bool HasLateInput() const;

	private:
		struct ScheduledInput
		{
			InputCommand mInput;
			uint64_t mArrivalOrder{};
		};

		std::vector<ScheduledInput> mInputs;
		size_t mCapacity;
		uint64_t mNextArrivalOrder{};
		bool mHasLateInput{};
	};
}
