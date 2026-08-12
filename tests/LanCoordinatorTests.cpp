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
	if (IsLanClientWaitingForHost(LanMode::OFFLINE) ||
		IsLanClientWaitingForHost(LanMode::HOSTING) ||
		!IsLanClientWaitingForHost(LanMode::SEARCHING) ||
		!IsLanClientWaitingForHost(LanMode::JOINING) ||
		!IsLanClientWaitingForHost(LanMode::CONNECTED) ||
		IsLanClientWaitingForHost(LanMode::FAILED))
		Fail("client waiting-mode classification is incorrect");

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

	GameplayProfile aProfile;
	aProfile.mProfileId = 1;
	aProfile.mAdventureLevel = 8;
	SessionStart aStart{0, 77, 1234, 0, aProfile};
	if (!aHost.BroadcastFromHost(aStart))
		Fail("coordinator failed to broadcast session start");
	std::vector<Message> aMessages;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aMessages.empty())
	{
		aHost.Poll();
		aClient.Poll();
		aMessages = aClient.TakeClientMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aMessages.size() != 1 || aMessages.front() != Message{aStart})
		Fail("coordinator client did not receive session start");
	if (!aClient.SendReady({aStart.mStartId, 99}))
		Fail("coordinator client failed to send ready response");
	std::vector<HostSessionEvent> anEvents;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && anEvents.empty())
	{
		aClient.Poll();
		aHost.Poll();
		anEvents = aHost.TakeHostEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (anEvents.size() != 1 || !std::holds_alternative<SessionReady>(anEvents.front()) ||
		std::get<SessionReady>(anEvents.front()).mPlayerId != 1)
		Fail("coordinator did not bind ready response to the assigned player");
	SessionBegin aBegin{0, aStart.mStartId};
	if (!aHost.BroadcastFromHost(aBegin))
		Fail("coordinator failed to broadcast session begin");
	aMessages.clear();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aMessages.empty())
	{
		aHost.Poll();
		aClient.Poll();
		aMessages = aClient.TakeClientMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aMessages.size() != 1 || aMessages.front() != Message{aBegin})
		Fail("coordinator client did not receive session begin");

	CursorUpdate aCursor{10, 1, 12345, 54321, 99, true};
	if (!aClient.SendCursor(aCursor))
		Fail("coordinator failed to send cursor");
	anEvents.clear();
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

	GameAction anAction{12, 1, 1, 4, 2, 99, ActionKind::PLANT_SEED};
	if (!aClient.SendAction(anAction))
		Fail("coordinator failed to send action");
	anEvents.clear();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && anEvents.empty())
	{
		aClient.Poll();
		aHost.Poll();
		anEvents = aHost.TakeHostEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (anEvents.size() != 1 || !std::holds_alternative<GameAction>(anEvents.front()) ||
		std::get<GameAction>(anEvents.front()).mPlayerId != 1)
		Fail("coordinator did not bind action to the assigned player");
	GameAction anAcceptedInput = std::get<GameAction>(anEvents.front());
	anAcceptedInput.mHostTick = 20;
	if (!aHost.BroadcastFromHost(anAcceptedInput))
		Fail("coordinator failed to broadcast accepted action");
	aMessages.clear();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aMessages.empty())
	{
		aHost.Poll();
		aClient.Poll();
		aMessages = aClient.TakeClientMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aMessages.size() != 1 || aMessages.front() != Message{anAcceptedInput})
		Fail("coordinator client did not receive accepted action");

	StateHash aHash{20, 77, 0x0102030405060708ULL};
	if (!aHost.BroadcastFromHost(aHash))
		Fail("coordinator host broadcast failed");
	aMessages.clear();
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
