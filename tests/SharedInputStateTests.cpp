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
	Require(InterpolateCursorCoordinate(1000, 3000, 10, 10) == 1000,
		"interpolation did not start at the source");
	Require(InterpolateCursorCoordinate(1000, 3000, 10, 12) == 2000,
		"ascending interpolation midpoint was incorrect");
	Require(InterpolateCursorCoordinate(3000, 1000, 10, 12) == 2000,
		"descending interpolation midpoint was incorrect");
	Require(InterpolateCursorCoordinate(0, UINT16_MAX, 10, 14) == UINT16_MAX,
		"interpolation did not reach the target");
	Require(InterpolateCursorCoordinate(UINT16_MAX, 0, 10, 14) == 0,
		"descending interpolation overflowed");
	Require(InterpolateCursorCoordinate(100, 200, 20, 10) == 100,
		"pre-start interpolation underflowed");
	Require(InterpolateCursorCoordinate(100, 200, 10, 10, 0) == 200,
		"zero-duration interpolation did not snap");
	Require(InterpolateCursorCoordinate(10, 13, 0, 2) == 12 &&
		InterpolateCursorCoordinate(13, 10, 0, 2) == 11,
		"interpolation rounding was not directionally symmetric");

	SharedInputState aState;
	aState.Reset(1);
	CursorUpdate aCursor{50, UINT32_MAX, 1000, 2000, 2, true, 4};
	Require(aState.ApplyCursor(aCursor, 75), "first cursor update was rejected");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 75) == CursorPosition{1000, 2000},
		"first cursor update did not snap to its target");
	Require(!aState.ApplyCursor(aCursor, 76), "duplicate cursor update was accepted");
	aCursor.mSequence = 0;
	aCursor.mNormalizedX = 3000;
	Require(aState.ApplyCursor(aCursor, 77), "wrapped cursor update was rejected");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 77) == CursorPosition{1000, 2000},
		"new target caused an immediate visual jump");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 79) == CursorPosition{2000, 2000},
		"cursor did not interpolate toward the new target");
	// A packet arriving mid-transition starts at the currently displayed point.
	aCursor.mSequence = 1;
	aCursor.mNormalizedX = 5000;
	Require(aState.ApplyCursor(aCursor, 79), "mid-transition cursor update was rejected");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 79) == CursorPosition{2000, 2000},
		"mid-transition retargeting was discontinuous");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 81) == CursorPosition{3500, 2000},
		"retargeted cursor midpoint was incorrect");
	// Metadata-only/keepalive packets at the same target do not restart motion.
	aCursor.mSequence = 2;
	Require(aState.ApplyCursor(aCursor, 81), "same-target cursor update was rejected");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 81) == CursorPosition{3500, 2000},
		"same-target update moved the displayed cursor");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 83) == CursorPosition{5000, 2000},
		"same-target update delayed the existing tween");
	Require(aState.GetCursors()[2]->mRgb == GetPlayerCursorColor(2), "cursor color did not match player");
	Require(aState.GetCursors()[2]->mUpdate.mHeldSeedBankIndex == 4, "held seed presentation was not retained");
	Require(aState.GetCursors()[2]->mReceivedAtTick == 81, "cursor receive tick was not retained");
	// Host tick epochs are unrelated across processes; interpolation uses local receive time.
	aCursor.mHostTick = 1000000;
	aCursor.mSequence = 3;
	aCursor.mNormalizedX = 7000;
	Require(aState.ApplyCursor(aCursor, 100), "long-gap cursor update was rejected");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 100) == CursorPosition{7000, 2000},
		"long receive gap did not snap locally");
	// Hidden cursors disappear immediately and reappearance never sweeps in from stale coordinates.
	aCursor.mSequence = 4;
	aCursor.mVisible = false;
	aCursor.mNormalizedX = 9000;
	Require(aState.ApplyCursor(aCursor, 101), "hidden cursor update was rejected");
	aCursor.mSequence = 5;
	aCursor.mVisible = true;
	aCursor.mNormalizedX = 11000;
	Require(aState.ApplyCursor(aCursor, 102), "visible cursor update was rejected");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 102) == CursorPosition{11000, 2000},
		"reappearing cursor did not snap");
	// TCP may deliver multiple stale snapshots in one Poll; the latest one wins
	// immediately instead of animating a recovered backlog.
	aCursor.mSequence = 6;
	aCursor.mNormalizedX = 13000;
	Require(aState.ApplyCursor(aCursor, 102), "same-tick backlog update was rejected");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 102) == CursorPosition{13000, 2000},
		"same-tick backlog update did not snap");
	// Large discontinuities (window edge teleports) snap instead of sweeping across the board.
	aCursor.mSequence = 7;
	aCursor.mNormalizedX = UINT16_MAX;
	Require(aState.ApplyCursor(aCursor, 103), "large cursor jump was rejected");
	Require(SampleCursorPosition(*aState.GetCursors()[2], 103).mNormalizedX == UINT16_MAX,
		"large cursor jump did not snap");
	aState.RemovePlayer(2);
	Require(!aState.GetCursors()[2], "departed player cursor was retained");

	std::cout << "Shared input state tests passed\n";
	return 0;
}
