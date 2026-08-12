/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/SharedInputState.h"

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

	Require(NormalizeCoordinate(-10, 800) == 0, "negative coordinate was not clamped");
	Require(NormalizeCoordinate(799, 800) == UINT16_MAX, "right edge was not normalized");
	Require(DenormalizeCoordinate(0, 600) == 0, "zero did not denormalize");
	Require(DenormalizeCoordinate(UINT16_MAX, 600) == 599, "bottom edge did not denormalize");
	for (int aCoordinate : {0, 1, 123, 400, 798, 799})
	{
		int aRoundTrip = DenormalizeCoordinate(NormalizeCoordinate(aCoordinate, 800), 800);
		Require(aRoundTrip == aCoordinate, "coordinate round trip drifted");
	}

	Require(IsSequenceNewer(1, 0), "ascending sequence was rejected");
	Require(!IsSequenceNewer(5, 5), "duplicate sequence was accepted");
	Require(IsSequenceNewer(0, UINT32_MAX), "wrapped sequence was rejected");

	SharedInputState aState;
	aState.Reset(1);
	CursorUpdate aCursor{50, UINT32_MAX, 1000, 2000, 2, true, 4};
	Require(aState.ApplyCursor(aCursor, 75), "first cursor update was rejected");
	Require(!aState.ApplyCursor(aCursor, 76), "duplicate cursor update was accepted");
	aCursor.mSequence = 0;
	aCursor.mNormalizedX = 3000;
	Require(aState.ApplyCursor(aCursor, 77), "wrapped cursor update was rejected");
	Require(aState.GetCursors()[2]->mRgb == GetPlayerCursorColor(2), "cursor color did not match player");
	Require(aState.GetCursors()[2]->mUpdate.mHeldSeedBankIndex == 4, "held seed presentation was not retained");
	Require(aState.GetCursors()[2]->mReceivedAtTick == 77, "cursor receive tick was not retained");
	aState.RemovePlayer(2);
	Require(!aState.GetCursors()[2], "departed player cursor was retained");

	std::cout << "Shared input state tests passed\n";
	return 0;
}
