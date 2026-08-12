/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/InputTimeline.h"

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

	PvzMultiplayer::InputCommand MakeInput(uint64_t theTick, uint32_t theSequence, PvzMultiplayer::PlayerId thePlayer)
	{
		return {theTick, theSequence, 1, 100, 200, 0, thePlayer, PvzMultiplayer::InputKind::POINTER_DOWN};
	}
}

int main()
{
	using namespace PvzMultiplayer;

	InputTimeline aTimeline(3);
	Require(aTimeline.Schedule(MakeInput(12, 1, 2), 10) == ScheduleResult::ACCEPTED, "future input was rejected");
	Require(aTimeline.Schedule(MakeInput(11, 1, 1), 10) == ScheduleResult::ACCEPTED, "earlier input was rejected");
	Require(aTimeline.Schedule(MakeInput(12, 2, 1), 10) == ScheduleResult::ACCEPTED, "same-tick input was rejected");
	Require(aTimeline.Schedule(MakeInput(13, 3, 1), 10) == ScheduleResult::FULL, "capacity limit was ignored");
	Require(aTimeline.Schedule(MakeInput(12, 1, 2), 10) == ScheduleResult::DUPLICATE, "duplicate input was accepted");

	auto anInputs = aTimeline.TakeForTick(12);
	Require(anInputs.size() == 2, "wrong same-tick input count");
	Require(anInputs[0].mPlayerId == 2 && anInputs[1].mPlayerId == 1, "host arrival order was not preserved");
	Require(aTimeline.GetSize() == 1, "other ticks were removed");
	Require(aTimeline.Schedule(MakeInput(9, 8, 1), 10) == ScheduleResult::PAST_TICK, "late input was accepted");
	Require(aTimeline.HasLateInput(), "late input was not recorded");

	aTimeline.Reset();
	Require(aTimeline.GetSize() == 0 && !aTimeline.HasLateInput(), "reset did not clear timeline state");

	std::cout << "Input timeline tests passed\n";
	return 0;
}
