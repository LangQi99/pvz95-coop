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
	static_assert(DEFAULT_GAME_PORT == 43096);
	if (IsLanClientWaitingForHost(LanMode::OFFLINE) ||
		IsLanClientWaitingForHost(LanMode::HOSTING) ||
		!IsLanClientWaitingForHost(LanMode::SEARCHING) ||
		!IsLanClientWaitingForHost(LanMode::JOINING) ||
		!IsLanClientWaitingForHost(LanMode::CONNECTED) ||
		IsLanClientWaitingForHost(LanMode::FAILED))
		Fail("client waiting-mode classification is incorrect");
	if (ResolveLanLifecycleDecision(LanMode::OFFLINE, 0, false) != LanLifecycleDecision::LOCAL ||
		ResolveLanLifecycleDecision(LanMode::FAILED, 0, false) != LanLifecycleDecision::LOCAL ||
		ResolveLanLifecycleDecision(LanMode::HOSTING, 1, false) != LanLifecycleDecision::LOCAL ||
		ResolveLanLifecycleDecision(LanMode::SEARCHING, 0, false) != LanLifecycleDecision::CLIENT_FOLLOW ||
		ResolveLanLifecycleDecision(LanMode::JOINING, 0, false) != LanLifecycleDecision::CLIENT_FOLLOW ||
		ResolveLanLifecycleDecision(LanMode::CONNECTED, 2, false) != LanLifecycleDecision::CLIENT_FOLLOW ||
		ResolveLanLifecycleDecision(LanMode::HOSTING, 2, false) != LanLifecycleDecision::HOST_START ||
		ResolveLanLifecycleDecision(LanMode::HOSTING, 2, true) != LanLifecycleDecision::HOST_PENDING)
		Fail("LAN lifecycle authority classification is incorrect");

	LanCoordinator aHost;
	if (!aHost.StartHosting("Test Garden", "Host", 0x50563935, 0))
		Fail("coordinator host failed: " + aHost.GetStatusText());
	uint16_t aHostGamePort = aHost.GetHostSession().GetLocalPort();
	if (aHostGamePort == 0 ||
		aHost.GetStatusText().find("TCP port " + std::to_string(aHostGamePort)) == std::string::npos)
		Fail("coordinator host status did not show its actual TCP game port");

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
	std::array<std::string, MAX_PLAYERS> aPlayerNames{"Host", "Guest", "", ""};
	SessionStart aStart{0, 77, 1234, 0, aProfile, aPlayerNames};
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

	// A terminal restart/next-level transition reuses the same connected room
	// and must perform a complete second Start -> Ready -> Begin barrier.
	GameplayProfile aNextProfile = aProfile;
	aNextProfile.mAdventureLevel = 9;
	SessionStart aNextStart{0, 78, 5678, 0, aNextProfile, aPlayerNames};
	if (!aHost.BroadcastFromHost(aNextStart))
		Fail("coordinator failed to broadcast replacement session start");
	aMessages.clear();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aMessages.empty())
	{
		aHost.Poll();
		aClient.Poll();
		aMessages = aClient.TakeClientMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aMessages.size() != 1 || aMessages.front() != Message{aNextStart})
		Fail("coordinator client did not follow replacement session start");
	if (!aClient.SendReady({aNextStart.mStartId, 99}))
		Fail("coordinator client failed to ready replacement session");
	anEvents.clear();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && anEvents.empty())
	{
		aClient.Poll();
		aHost.Poll();
		anEvents = aHost.TakeHostEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (anEvents.size() != 1 || !std::holds_alternative<SessionReady>(anEvents.front()) ||
		std::get<SessionReady>(anEvents.front()).mStartId != aNextStart.mStartId ||
		std::get<SessionReady>(anEvents.front()).mPlayerId != 1)
		Fail("coordinator did not bind replacement ready response");
	SessionBegin aNextBegin{0, aNextStart.mStartId};
	if (!aHost.BroadcastFromHost(aNextBegin))
		Fail("coordinator failed to broadcast replacement session begin");
	aMessages.clear();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aMessages.empty())
	{
		aHost.Poll();
		aClient.Poll();
		aMessages = aClient.TakeClientMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aMessages.size() != 1 || aMessages.front() != Message{aNextBegin})
		Fail("coordinator client did not receive replacement session begin");

	SessionEnd anEnd{aNextStart.mStartId};
	if (!aHost.BroadcastFromHost(anEnd))
		Fail("coordinator failed to broadcast session end");
	aMessages.clear();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aMessages.empty())
	{
		aHost.Poll();
		aClient.Poll();
		aMessages = aClient.TakeClientMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aMessages.size() != 1 || aMessages.front() != Message{anEnd})
		Fail("coordinator client did not follow session end");

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

	aClient.Stop();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline &&
		aHost.GetHostSession().GetLobby().GetPlayerCount() != 1)
	{
		aHost.Poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aHost.GetHostSession().GetLobby().GetPlayerCount() != 1)
		Fail("coordinator host did not observe the discovery client leaving");

	LanCoordinator aDirectClient;
	Ipv4Endpoint aDirectEndpoint = Ipv4Endpoint::Loopback(aHostGamePort);
	if (!aDirectClient.StartDirectJoining("Direct Guest", 0x50563935, aDirectEndpoint))
		Fail("direct coordinator client failed: " + aDirectClient.GetStatusText());
	if (aDirectClient.GetMode() != LanMode::JOINING ||
		aDirectClient.GetStatusText().find("127.0.0.1:" + std::to_string(aHostGamePort)) == std::string::npos)
		Fail("direct coordinator client did not enter the direct joining state");
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aDirectClient.GetMode() != LanMode::CONNECTED)
	{
		aHost.Poll();
		aDirectClient.Poll();
		if (aHost.GetMode() == LanMode::FAILED || aDirectClient.GetMode() == LanMode::FAILED)
			Fail("direct coordinator handshake error: " + aHost.GetStatusText() + "; " +
				aDirectClient.GetStatusText());
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aDirectClient.GetMode() != LanMode::CONNECTED ||
		aHost.GetHostSession().GetLobby().GetPlayerCount() != 2)
		Fail("direct coordinator handshake timed out");
	if (aHost.GetStatusText().find("2/4") == std::string::npos ||
		aHost.GetStatusText().find("TCP port " + std::to_string(aHostGamePort)) == std::string::npos)
		Fail("coordinator host status lost the player count or TCP port after a direct join");

	LanCoordinator anInvalidDirectClient;
	if (anInvalidDirectClient.StartDirectJoining("Guest", 0x50563935, Ipv4Endpoint::Loopback(0)) ||
		anInvalidDirectClient.GetMode() != LanMode::FAILED)
		Fail("direct coordinator accepted an invalid zero game port");

	std::cout << "PvZ 95 LAN coordinator test passed\n";
	return 0;
}
