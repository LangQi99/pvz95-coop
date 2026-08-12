/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/SessionBarrier.h"

#include <array>
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
}

int main()
{
	using namespace PvzMultiplayer;

	SessionBarrier aBarrier;
	std::array<PlayerId, 3> aPlayers{0, 1, 3};
	Require(aBarrier.Start(44, aPlayers), "valid barrier did not start");
	Require(!aBarrier.AllReady(), "barrier opened before guests were ready");
	Require(!aBarrier.MarkReady({45, 1}), "wrong start ID was accepted");
	Require(!aBarrier.MarkReady({44, 2}), "non-member was accepted");
	Require(aBarrier.MarkReady({44, 1}), "first guest was not marked ready");
	Require(!aBarrier.AllReady(), "barrier opened with one guest missing");
	aBarrier.RemovePlayer(3);
	Require(aBarrier.AllReady(), "departed guest kept the barrier closed");

	std::array<PlayerId, 2> aDuplicatePlayers{0, 0};
	Require(!aBarrier.Start(55, aDuplicatePlayers), "duplicate player list was accepted");
	Require(!aBarrier.IsActive(), "failed start left the barrier active");

	std::cout << "Session start barrier tests passed\n";
	return 0;
}
