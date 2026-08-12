/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/StateHashTimeline.h"

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

	StateHashTimeline aTimeline(200, 2);
	Require(aTimeline.ObserveLocal(100, 0xA1, 100).mKind == StateHashResultKind::WAITING,
		"local hash should wait when the network hash is split across Poll calls");
	Require(aTimeline.AdvanceTo(101).mKind == StateHashResultKind::WAITING,
		"one tick of network delay caused a false desync");
	Require(aTimeline.ObserveRemote(100, 0xA1, 101).mKind == StateHashResultKind::MATCHED,
		"late matching network hash was rejected");

	Require(aTimeline.ObserveRemote(200, 0xB2, 150).mKind == StateHashResultKind::WAITING,
		"future network hash should wait for local simulation");
	Require(aTimeline.ObserveLocal(200, 0xB2, 200).mKind == StateHashResultKind::MATCHED,
		"network-first matching hash was rejected");

	Require(aTimeline.ObserveLocal(300, 0xC3, 300).mKind == StateHashResultKind::WAITING,
		"mismatch setup failed");
	StateHashResult aMismatch = aTimeline.ObserveRemote(300, 0xC4, 301);
	Require(aMismatch.mKind == StateHashResultKind::MISMATCH &&
		aMismatch.mLocalHash == 0xC3 && aMismatch.mRemoteHash == 0xC4,
		"different hashes did not produce a detailed mismatch");

	aTimeline.Reset();
	Require(aTimeline.ObserveLocal(400, 0xD4, 400).mKind == StateHashResultKind::WAITING,
		"expiry setup failed");
	Require(aTimeline.AdvanceTo(600).mKind == StateHashResultKind::WAITING,
		"hash expired at the inclusive grace boundary");
	Require(aTimeline.AdvanceTo(601).mKind == StateHashResultKind::EXPIRED,
		"missing hash did not expire after the grace window");

	aTimeline.Reset();
	Require(aTimeline.ObserveLocal(700, 0xE5, 700).mKind == StateHashResultKind::WAITING,
		"capacity setup tick one failed");
	Require(aTimeline.ObserveLocal(800, 0xF6, 800).mKind == StateHashResultKind::WAITING,
		"capacity setup tick two failed");
	Require(aTimeline.ObserveLocal(900, 0x17, 900).mKind == StateHashResultKind::FULL,
		"pending hash capacity limit was ignored");
	Require(aTimeline.ObserveRemote(700, 0xE5, 900).mKind == StateHashResultKind::MATCHED,
		"a completing pair was rejected at capacity");

	std::cout << "State hash timeline tests passed\n";
	return 0;
}
