/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "InputTimeline.h"

#include <algorithm>

namespace PvzMultiplayer
{
	InputTimeline::InputTimeline(size_t theCapacity) : mCapacity(theCapacity)
	{
	}

	ScheduleResult InputTimeline::Schedule(const InputCommand& theInput, uint64_t theCurrentTick)
	{
		if (theInput.mHostTick < theCurrentTick)
		{
			mHasLateInput = true;
			return ScheduleResult::PAST_TICK;
		}
		if (std::any_of(mInputs.begin(), mInputs.end(), [&](const ScheduledInput& theScheduled)
			{
				return theScheduled.mInput.mPlayerId == theInput.mPlayerId &&
					theScheduled.mInput.mSequence == theInput.mSequence;
			}))
			return ScheduleResult::DUPLICATE;
		if (mInputs.size() >= mCapacity)
			return ScheduleResult::FULL;

		mInputs.push_back({theInput, mNextArrivalOrder++});
		std::stable_sort(mInputs.begin(), mInputs.end(), [](const ScheduledInput& theLeft, const ScheduledInput& theRight)
		{
			if (theLeft.mInput.mHostTick != theRight.mInput.mHostTick)
				return theLeft.mInput.mHostTick < theRight.mInput.mHostTick;
			return theLeft.mArrivalOrder < theRight.mArrivalOrder;
		});
		return ScheduleResult::ACCEPTED;
	}

	std::vector<InputCommand> InputTimeline::TakeForTick(uint64_t theTick)
	{
		std::vector<InputCommand> anInputs;
		auto aFirst = std::lower_bound(mInputs.begin(), mInputs.end(), theTick,
			[](const ScheduledInput& theInput, uint64_t theValue)
			{
				return theInput.mInput.mHostTick < theValue;
			});
		auto anEnd = std::upper_bound(aFirst, mInputs.end(), theTick,
			[](uint64_t theValue, const ScheduledInput& theInput)
			{
				return theValue < theInput.mInput.mHostTick;
			});
		anInputs.reserve(static_cast<size_t>(anEnd - aFirst));
		for (auto anIterator = aFirst; anIterator != anEnd; ++anIterator)
			anInputs.push_back(anIterator->mInput);
		mInputs.erase(aFirst, anEnd);
		return anInputs;
	}

	void InputTimeline::Reset()
	{
		mInputs.clear();
		mNextArrivalOrder = 0;
		mHasLateInput = false;
	}

	size_t InputTimeline::GetSize() const
	{
		return mInputs.size();
	}

	bool InputTimeline::HasLateInput() const
	{
		return mHasLateInput;
	}
}
