/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/LanCoordinator.h"

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

	LanCoordinator aHost;
	if (!aHost.StartHosting("Test Garden", "Host", 0x50563935))
		Fail("coordinator host failed: " + aHost.GetStatusText());

	LanCoordinator aClient;
	if (!aClient.StartJoining("Guest", 0x50563935, Ipv4Endpoint::Loopback(DEFAULT_DISCOVERY_PORT)))
		Fail("coordinator client failed: " + aClient.GetStatusText());

	auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (std::chrono::steady_clock::now() < aDeadline && aClient.GetMode() != LanMode::CONNECTED)
	{
		aHost.Poll();
		aClient.Poll();
		if (aHost.GetMode() == LanMode::FAILED || aClient.GetMode() == LanMode::FAILED)
			Fail("coordinator handshake error: " + aHost.GetStatusText() + "; " + aClient.GetStatusText());
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aClient.GetMode() != LanMode::CONNECTED || aHost.GetHostSession().GetLobby().GetPlayerCount() != 2)
		Fail("coordinator handshake timed out");
	aHost.TakeHostEvents();

	CursorUpdate aCursor{10, 1, 12345, 54321, 99, 1, true};
	if (!aClient.SendCursor(aCursor))
		Fail("coordinator failed to send cursor");
	std::vector<HostSessionEvent> anEvents;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && anEvents.empty())
	{
		aClient.Poll();
		aHost.Poll();
		anEvents = aHost.TakeHostEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (anEvents.size() != 1 || !std::holds_alternative<CursorUpdate>(anEvents.front()) ||
		std::get<CursorUpdate>(anEvents.front()).mPlayerId != 1)
		Fail("coordinator did not bind cursor to the assigned player");

	StateHash aHash{20, 0x0102030405060708ULL};
	if (!aHost.BroadcastFromHost(aHash))
		Fail("coordinator host broadcast failed");
	std::vector<Message> aMessages;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aMessages.empty())
	{
		aHost.Poll();
		aClient.Poll();
		aMessages = aClient.TakeClientMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aMessages.size() != 1 || aMessages.front() != Message{aHash})
		Fail("coordinator client did not receive host broadcast");

	std::cout << "PvZ 95 LAN coordinator test passed\n";
	return 0;
}
