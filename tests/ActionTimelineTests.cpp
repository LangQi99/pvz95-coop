/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/ActionTimeline.h"

#include <cstdlib>
#include <iostream>

namespace
{
	void Require(bool theCondition, const char* theMessage)
	{
		if (!theCondition)
		{
			std::cerr << "FAILED: " << theMessage << '\n';
			std::exit(1);
		}
	}

	PvzMultiplayer::GameAction MakeAction(uint64_t theTick, uint32_t theSequence, PvzMultiplayer::PlayerId thePlayer)
	{
		return {theTick, theSequence, 1, 4, 2, thePlayer, PvzMultiplayer::ActionKind::PLANT_SEED};
	}
}

int main()
{
	using namespace PvzMultiplayer;

	ActionTimeline aTimeline(3);
	Require(aTimeline.Schedule(MakeAction(12, 1, 2), 10) == ScheduleResult::ACCEPTED, "future action was rejected");
	Require(aTimeline.Schedule(MakeAction(11, 1, 1), 10) == ScheduleResult::ACCEPTED, "earlier action was rejected");
	Require(aTimeline.Schedule(MakeAction(12, 2, 1), 10) == ScheduleResult::ACCEPTED, "same-tick action was rejected");
	Require(aTimeline.Schedule(MakeAction(13, 3, 1), 10) == ScheduleResult::FULL, "capacity limit was ignored");
	Require(aTimeline.Schedule(MakeAction(12, 1, 2), 10) == ScheduleResult::DUPLICATE, "duplicate action was accepted");

	auto anActions = aTimeline.TakeForTick(12);
	Require(anActions.size() == 2, "wrong same-tick action count");
	Require(anActions[0].mPlayerId == 2 && anActions[1].mPlayerId == 1, "host arrival order was not preserved");
	Require(aTimeline.GetSize() == 1, "other ticks were removed");
	Require(aTimeline.Schedule(MakeAction(9, 8, 1), 10) == ScheduleResult::PAST_TICK, "late action was accepted");
	Require(aTimeline.HasLateAction(), "late action was not recorded");

	aTimeline.Reset();
	Require(aTimeline.GetSize() == 0 && !aTimeline.HasLateAction(), "reset did not clear timeline state");

	std::cout << "Action timeline tests passed\n";
	return 0;
}
