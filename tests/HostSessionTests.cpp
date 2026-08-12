/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/HostSession.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace
{
	using namespace PvzMultiplayer;

	[[noreturn]] void Fail(const std::string& theMessage)
	{
		std::cerr << theMessage << '\n';
		std::exit(1);
	}
}

int main()
{
	using namespace PvzMultiplayer;

	HostSession aHost;
	HostLobbyConfig aConfig{0x8877665544332211ULL, 0x50563935, 100, 4, "Garden", "Host"};
	if (!aHost.Start(aConfig, 0))
		Fail("host failed to start: " + aHost.GetLastError());

	TcpSocket aSocket;
	if (!aSocket.StartConnect(Ipv4Endpoint::Loopback(aHost.GetLocalPort())))
		Fail("client failed to connect: " + aSocket.GetLastError());
	ReliableChannel aClient(std::move(aSocket));
	if (!aClient.Queue(Hello{12345, aConfig.mRulesetId, 0, "Guest"}))
		Fail("client failed to queue hello");

	Welcome aWelcome;
	bool aJoined = false;
	auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && !aJoined)
	{
		aClient.Poll();
		aHost.Poll();
		for (const auto& aMessage : aClient.TakeMessages())
		{
			if (const auto* aValue = std::get_if<Welcome>(&aMessage))
			{
				aWelcome = *aValue;
				aJoined = true;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (!aJoined || aWelcome.mPlayerId != 1 || aHost.GetLobby().GetPlayerCount() != 2)
		Fail("handshake did not assign the guest");

	auto anEvents = aHost.TakeEvents();
	if (anEvents.size() != 1 || !std::holds_alternative<PlayerJoined>(anEvents.front()))
		Fail("host did not report the join");

	CursorUpdate aCursor{42, 7, 11111, 22222, aWelcome.mPlayerId, 1, true};
	if (!aClient.Queue(aCursor))
		Fail("client failed to queue cursor");
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline)
	{
		aClient.Poll();
		aHost.Poll();
		anEvents = aHost.TakeEvents();
		if (!anEvents.empty())
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (anEvents.size() != 1 || !std::holds_alternative<CursorUpdate>(anEvents.front()) ||
		std::get<CursorUpdate>(anEvents.front()) != aCursor)
		Fail("host did not validate and deliver the cursor");

	StateHash aHash{100, 0x123456789ABCDEF0ULL};
	if (!aHost.Broadcast(aHash))
		Fail("host failed to broadcast state hash");
	bool aReceivedHash = false;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && !aReceivedHash)
	{
		aHost.Poll();
		aClient.Poll();
		for (const auto& aMessage : aClient.TakeMessages())
			aReceivedHash = aReceivedHash || aMessage == Message{aHash};
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (!aReceivedHash)
		Fail("client did not receive host broadcast");

	aClient.Close();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline)
	{
		aHost.Poll();
		anEvents = aHost.TakeEvents();
		if (!anEvents.empty())
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (anEvents.size() != 1 || !std::holds_alternative<PlayerLeft>(anEvents.front()) ||
		std::get<PlayerLeft>(anEvents.front()).mPlayerId != aWelcome.mPlayerId ||
		aHost.GetLobby().GetPlayerCount() != 1)
		Fail("host did not remove the disconnected guest");

	std::cout << "PvZ 95 host session test passed\n";
	return 0;
}
