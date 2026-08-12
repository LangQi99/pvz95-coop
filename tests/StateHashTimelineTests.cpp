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

	StateHashTimeline aTimeline(200, 4);
	Require(aTimeline.ObserveLocal(100, 0xA1, 100).mKind == StateHashResultKind::WAITING,
		"local hash should wait when the network hash is split across Poll calls");
	Require(aTimeline.AdvanceTo(101).mKind == StateHashResultKind::WAITING,
		"one tick of network delay caused a false desync");
	Require(aTimeline.ObserveRemote(100, 0xA1, 101).mKind == StateHashResultKind::MATCHED,
		"late matching network hash was rejected");
	Require(aTimeline.GetPendingTickCount() == 0,
		"matched hash pair was not removed");

	Require(aTimeline.ObserveRemote(200, 0xB2, 150).mKind == StateHashResultKind::WAITING,
		"future network hash should wait for local simulation");
	Require(aTimeline.ObserveLocal(200, 0xB2, 200).mKind == StateHashResultKind::MATCHED,
		"network-first matching hash was rejected");

	Require(aTimeline.ObserveLocal(400, 0xD4, 400).mKind == StateHashResultKind::WAITING,
		"out-of-order setup tick one failed");
	Require(aTimeline.ObserveLocal(500, 0xE5, 500).mKind == StateHashResultKind::WAITING,
		"out-of-order setup tick two failed");
	Require(aTimeline.ObserveRemote(500, 0xE5, 500).mKind == StateHashResultKind::MATCHED,
		"newer logical frame did not pair independently");
	Require(aTimeline.ObserveRemote(400, 0xD4, 500).mKind == StateHashResultKind::MATCHED,
		"older logical frame was rejected after a newer frame matched first");

	Require(aTimeline.ObserveLocal(300, 0xC3, 300).mKind == StateHashResultKind::WAITING,
		"mismatch setup failed");
	StateHashResult aMismatch = aTimeline.ObserveRemote(300, 0xC4, 301);
	Require(aMismatch.mKind == StateHashResultKind::MISMATCH &&
		aMismatch.mLocalHash == 0xC3 && aMismatch.mRemoteHash == 0xC4,
		"different hashes did not produce a detailed mismatch");

	aTimeline.Reset();
	Require(aTimeline.ObserveLocal(600, 0xF6, 600).mKind == StateHashResultKind::WAITING,
		"expiry setup failed");
	Require(aTimeline.AdvanceTo(800).mKind == StateHashResultKind::WAITING,
		"hash expired at the inclusive grace boundary");
	Require(aTimeline.ObserveRemote(600, 0xF6, 800).mKind == StateHashResultKind::MATCHED,
		"matching hash at the inclusive grace boundary was rejected");

	aTimeline.Reset();
	Require(aTimeline.ObserveLocal(900, 0x17, 900).mKind == StateHashResultKind::WAITING,
		"post-boundary expiry setup failed");
	StateHashResult anExpired = aTimeline.AdvanceTo(1101);
	Require(anExpired.mKind == StateHashResultKind::EXPIRED &&
		anExpired.mTick == 900 && anExpired.mLocalHash == 0x17,
		"missing hash did not expire after the grace window");
	Require(aTimeline.GetPendingTickCount() == 0,
		"expired hash was not removed");

	aTimeline.Reset();
	Require(aTimeline.ObserveRemote(1200, 0x28, 1401).mKind == StateHashResultKind::EXPIRED,
		"an already stale first observation was retained");
	Require(aTimeline.GetPendingTickCount() == 0,
		"stale first observation consumed pending capacity");

	aTimeline.Reset();
	Require(aTimeline.ObserveLocal(1500, 0x39, 1500).mKind == StateHashResultKind::WAITING,
		"duplicate setup failed");
	Require(aTimeline.ObserveLocal(1500, 0x39, 1501).mKind == StateHashResultKind::WAITING &&
		aTimeline.GetPendingTickCount() == 1,
		"an identical duplicate created another pending tick");
	StateHashResult aConflict = aTimeline.ObserveLocal(1500, 0x40, 1501);
	Require(aConflict.mKind == StateHashResultKind::CONFLICT &&
		aConflict.mTick == 1500 && aConflict.mLocalHash == 0x39,
		"a conflicting local duplicate was accepted");

	aTimeline.Reset();
	StateHashTimeline aSmallTimeline(200, 2);
	Require(aSmallTimeline.ObserveLocal(1700, 0x51, 1700).mKind == StateHashResultKind::WAITING,
		"capacity setup tick one failed");
	Require(aSmallTimeline.ObserveLocal(1800, 0x62, 1800).mKind == StateHashResultKind::WAITING,
		"capacity setup tick two failed");
	Require(aSmallTimeline.ObserveLocal(1900, 0x73, 1900).mKind == StateHashResultKind::FULL,
		"pending hash capacity limit was ignored");
	Require(aSmallTimeline.ObserveRemote(1700, 0x51, 1900).mKind == StateHashResultKind::MATCHED,
		"a completing pair was rejected at capacity");

	aTimeline.Reset();
	Require(aTimeline.ObserveLocal(2000, 0x84, 2000).mKind == StateHashResultKind::WAITING,
		"reset setup failed");
	aTimeline.Reset();
	Require(aTimeline.GetPendingTickCount() == 0,
		"reset did not isolate hash observations between sessions");
	Require(aTimeline.ObserveRemote(2000, 0x84, 2000).mKind == StateHashResultKind::WAITING,
		"a remote hash incorrectly matched state from a reset session");

	std::cout << "State hash timeline tests passed\n";
	return 0;
}
