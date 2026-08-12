/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ActionTimeline.h"

#include <algorithm>

namespace PvzMultiplayer
{
	ActionTimeline::ActionTimeline(size_t theCapacity) : mCapacity(theCapacity)
	{
	}

	ScheduleResult ActionTimeline::Schedule(const GameAction& theAction, uint64_t theCurrentTick)
	{
		if (theAction.mHostTick < theCurrentTick)
		{
			mHasLateAction = true;
			return ScheduleResult::PAST_TICK;
		}
		if (std::any_of(mActions.begin(), mActions.end(), [&](const ScheduledAction& theScheduled)
			{
				return theScheduled.mAction.mPlayerId == theAction.mPlayerId &&
					theScheduled.mAction.mSequence == theAction.mSequence;
			}))
			return ScheduleResult::DUPLICATE;
		if (mActions.size() >= mCapacity)
			return ScheduleResult::FULL;

		mActions.push_back({theAction, mNextArrivalOrder++});
		std::stable_sort(mActions.begin(), mActions.end(), [](const ScheduledAction& theLeft, const ScheduledAction& theRight)
		{
			if (theLeft.mAction.mHostTick != theRight.mAction.mHostTick)
				return theLeft.mAction.mHostTick < theRight.mAction.mHostTick;
			return theLeft.mArrivalOrder < theRight.mArrivalOrder;
		});
		return ScheduleResult::ACCEPTED;
	}

	std::vector<GameAction> ActionTimeline::TakeForTick(uint64_t theTick)
	{
		std::vector<GameAction> anActions;
		auto aFirst = std::lower_bound(mActions.begin(), mActions.end(), theTick,
			[](const ScheduledAction& theAction, uint64_t theValue)
			{
				return theAction.mAction.mHostTick < theValue;
			});
		auto anEnd = std::upper_bound(aFirst, mActions.end(), theTick,
			[](uint64_t theValue, const ScheduledAction& theAction)
			{
				return theValue < theAction.mAction.mHostTick;
			});
		anActions.reserve(static_cast<size_t>(anEnd - aFirst));
		for (auto anIterator = aFirst; anIterator != anEnd; ++anIterator)
			anActions.push_back(anIterator->mAction);
		mActions.erase(aFirst, anEnd);
		return anActions;
	}

	void ActionTimeline::Reset()
	{
		mActions.clear();
		mNextArrivalOrder = 0;
		mHasLateAction = false;
	}

	size_t ActionTimeline::GetSize() const
	{
		return mActions.size();
	}

	bool ActionTimeline::HasLateAction() const
	{
		return mHasLateAction;
	}
}
