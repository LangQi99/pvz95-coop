/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/DeterministicHash.h"
#include "PvzpLib/DeterministicAnimationClock.h"

#include <bit>
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

	DeterministicHash64 aHash;
	aHash.AddU8('a');
	aHash.AddU8('b');
	aHash.AddU8('c');
	Require(aHash.Finish() == 0xE71FA2190541574BULL, "FNV-1a reference hash changed");

	DeterministicHash64 aTyped;
	aTyped.AddU32(0x04030201U);
	DeterministicHash64 aBytes;
	const uint8_t aLittleEndian[]{1, 2, 3, 4};
	aBytes.AddBytes(aLittleEndian);
	Require(aTyped.Finish() == aBytes.Finish(), "integer hashing is not canonical little-endian");

	DeterministicHash64 aFalse;
	aFalse.AddBool(false);
	DeterministicHash64 aTrue;
	aTrue.AddBool(true);
	Require(aFalse.Finish() != aTrue.Finish(), "boolean values collide");

	{
		DeterministicAnimationClock aClock;
		float aPhase = 0.0f;
		for (int i = 0; i < 250; ++i)
		{
			aClock.Advance(aPhase, 12.0f, 30);
			aPhase = aClock.GetPublishedPhase();
		}
		Require(aClock.GetPhase() == DeterministicAnimationClock::PHASE_ONE,
			"integer animation phase accumulated drift");
		Require(aPhase == 1.0f, "integer animation phase did not reach the loop boundary");
	}

	{
		DeterministicAnimationClock aFirst;
		DeterministicAnimationClock aSecond;
		float aFirstPhase = 0.1375f;
		float aSecondPhase = 0.1375f;
		for (int i = 0; i < 10000; ++i)
		{
			aFirst.Advance(aFirstPhase, 17.375f, 47);
			aSecond.Advance(aSecondPhase, 17.375f, 47);
			aFirstPhase = aFirst.GetPublishedPhase();
			aSecondPhase = aSecond.GetPublishedPhase();
			Require(aFirst.GetPhase() == aSecond.GetPhase(), "equal animation clocks diverged");
			Require(std::bit_cast<uint32_t>(aFirstPhase) == std::bit_cast<uint32_t>(aSecondPhase),
				"equal animation clocks published different float bits");
		}
	}

	{
		DeterministicAnimationClock aClock;
		aClock.Advance(0.25f, 10.0f, 20);
		const int64_t aBeforeOverride = aClock.GetPhase();
		aClock.Advance(0.75f, 10.0f, 20);
		Require(aClock.GetPhase() != aBeforeOverride, "external animation phase override was ignored");
		Require(aClock.GetPhase() > DeterministicAnimationClock::FloatToPhase(0.75f),
			"external animation phase override did not resynchronize the clock");
	}

	std::cout << "Deterministic hash tests passed\n";
	return 0;
}
