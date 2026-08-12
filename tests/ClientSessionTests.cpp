/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/ClientSession.h"
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

	void PollUntilConnected(HostSession& theHost, ClientSession& theClient)
	{
		auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (std::chrono::steady_clock::now() < aDeadline)
		{
			theClient.Poll();
			theHost.Poll();
			if (theClient.GetState() == ClientSessionState::CONNECTED)
				return;
			if (theClient.GetState() == ClientSessionState::FAILED || theClient.GetState() == ClientSessionState::REJECTED)
				Fail("client handshake failed: " + theClient.GetLastError());
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		Fail("client handshake timed out");
	}
}

int main()
{
	using namespace PvzMultiplayer;

	HostSession aHost;
	HostLobbyConfig aHostConfig{987654321, 0x50563935, 100, 4, "Garden", "Host"};
	if (!aHost.Start(aHostConfig, 0))
		Fail("host failed to start");

	ClientSession aClient;
	ClientSessionConfig aClientConfig{
		Ipv4Endpoint::Loopback(aHost.GetLocalPort()), aHostConfig.mSessionId, 11223344,
		aHostConfig.mRulesetId, 0, "Guest"
	};
	if (!aClient.Start(aClientConfig))
		Fail("client failed to start: " + aClient.GetLastError());
	PollUntilConnected(aHost, aClient);
	if (!aClient.GetWelcome() || aClient.GetWelcome()->mPlayerId != 1)
		Fail("client did not retain its Welcome");
	aHost.TakeEvents();

	CursorUpdate aCursor{55, 1, 12000, 34000, 99, 1, true};
	if (!aClient.SendCursor(aCursor) || aClient.SendCursor(aCursor))
		Fail("client cursor sequence validation failed");

	auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	std::vector<HostSessionEvent> anEvents;
	while (std::chrono::steady_clock::now() < aDeadline && anEvents.empty())
	{
		aClient.Poll();
		aHost.Poll();
		anEvents = aHost.TakeEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (anEvents.size() != 1 || !std::holds_alternative<CursorUpdate>(anEvents.front()) ||
		std::get<CursorUpdate>(anEvents.front()).mPlayerId != 1)
		Fail("host did not receive the client cursor with the assigned player ID");

	SessionReady aReady{77, 99};
	if (!aClient.SendReady(aReady))
		Fail("client failed to send session-ready response");
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	anEvents.clear();
	while (std::chrono::steady_clock::now() < aDeadline && anEvents.empty())
	{
		aClient.Poll();
		aHost.Poll();
		anEvents = aHost.TakeEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (anEvents.size() != 1 || !std::holds_alternative<SessionReady>(anEvents.front()) ||
		std::get<SessionReady>(anEvents.front()).mPlayerId != 1)
		Fail("host did not bind the ready response to the assigned player ID");

	StateHash aHash{500, 77, 0xAABBCCDDEEFF0011ULL};
	if (!aHost.Broadcast(aHash))
		Fail("host broadcast failed");
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	std::vector<Message> aMessages;
	while (std::chrono::steady_clock::now() < aDeadline && aMessages.empty())
	{
		aHost.Poll();
		aClient.Poll();
		aMessages = aClient.TakeMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aMessages.size() != 1 || aMessages.front() != Message{aHash})
		Fail("client did not receive the state hash");

	aHost.Stop();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aClient.GetState() == ClientSessionState::CONNECTED)
	{
		aClient.Poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aClient.GetState() != ClientSessionState::CLOSED)
		Fail("client did not observe host shutdown");

	if (!aHost.Start(aHostConfig, 0))
		Fail("host failed to restart");
	aClient.Stop();
	aClientConfig.mEndpoint = Ipv4Endpoint::Loopback(aHost.GetLocalPort());
	aClientConfig.mRulesetId = 0xDEADBEEF;
	if (!aClient.Start(aClientConfig))
		Fail("mismatched client failed to start");
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline &&
		aClient.GetState() != ClientSessionState::REJECTED && aClient.GetState() != ClientSessionState::FAILED)
	{
		aClient.Poll();
		aHost.Poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aClient.GetState() != ClientSessionState::REJECTED || !aClient.GetReject() ||
		aClient.GetReject()->mReason != RejectReason::RULESET_MISMATCH)
		Fail("ruleset mismatch was not rejected cleanly");

	std::cout << "PvZ 95 client session test passed\n";
	return 0;
}
