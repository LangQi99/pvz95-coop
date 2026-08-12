/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/LanDiscovery.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace
{
	[[noreturn]] void Fail(const std::string& theMessage)
	{
		std::cerr << theMessage << '\n';
		std::exit(1);
	}
}

int main()
{
	using namespace PvzMultiplayer;

	auto aParsedLoopback = Ipv4Endpoint::Parse("127.0.0.1", 1234);
	if (!aParsedLoopback || *aParsedLoopback != Ipv4Endpoint::Loopback(1234) ||
		aParsedLoopback->AddressString() != "127.0.0.1" || Ipv4Endpoint::Parse("not-an-ip", 1234))
		Fail("IPv4 endpoint conversion failed");

	DiscoveryOffer anOffer{0x8877665544332211ULL, 43096, 1, 4, 0x50563935, "Loopback PvZ 95"};
	LanDiscoveryHost aHost;
	if (!aHost.Start(anOffer, 0))
		Fail("host failed to start: " + aHost.GetLastError());
	if (aHost.GetDiscoveryPort() == 0)
		Fail("host did not receive an ephemeral discovery port");

	LanDiscoveryClient aClient;
	if (!aClient.Start(0x1020304050607080ULL))
		Fail("client failed to start: " + aClient.GetLastError());
	if (!aClient.SendQueryTo(Ipv4Endpoint::Loopback(aHost.GetDiscoveryPort())))
		Fail("client failed to send query: " + aClient.GetLastError());

	auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline)
	{
		aHost.Poll();
		for (const auto& aSession : aClient.Poll())
		{
			if (aSession.mOffer == anOffer && aSession.mGameEndpoint == Ipv4Endpoint::Loopback(anOffer.mGamePort))
			{
				std::cout << "PvZ 95 LAN discovery test passed\n";
				return 0;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	Fail("timed out waiting for LAN discovery offer");
}
