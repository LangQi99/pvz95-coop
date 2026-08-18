/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/ReliableChannel.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
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

	TcpListener aListener;
	if (!aListener.Listen(0))
		Fail("listener failed: " + aListener.GetLastError());

	TcpSocket aClientSocket;
	if (!aClientSocket.StartConnect(Ipv4Endpoint::Loopback(aListener.GetLocalPort())))
		Fail("connect failed: " + aClientSocket.GetLastError());
	ReliableChannel aClient(std::move(aClientSocket));

	Message aHello = Hello{1234, 0x50563935, 0, "Client"};
	Message anOldCursor = CursorUpdate{76, 1, 11000, 33000, 1, true, 1};
	Message aCursor = CursorUpdate{77, 2, 12000, 34000, 1, true, 2};
	if (!aClient.Queue(aHello) || !aClient.Queue(anOldCursor) || !aClient.Queue(aCursor))
		Fail("client could not queue messages");

	std::optional<ReliableChannel> aServer;
	std::vector<Message> aServerMessages;
	auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline)
	{
		if (!aServer)
		{
			auto aSocket = aListener.Accept();
			if (aSocket)
				aServer.emplace(std::move(*aSocket));
		}
		aClient.Poll();
		if (aServer)
		{
			aServer->Poll();
			auto aMessages = aServer->TakeMessages();
			aServerMessages.insert(aServerMessages.end(), aMessages.begin(), aMessages.end());
			if (aServerMessages.size() == 2)
			{
				if (aServerMessages[0] != aHello || aServerMessages[1] != aCursor)
					Fail("server received wrong messages");
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (!aServer || aClient.GetQueuedByteCount() != 0)
		Fail("reliable channel timed out");

	Message aWelcome = Welcome{99, 0x50563935, 0x42A5F5, 100, 1, 4};
	if (!aServer->Queue(aWelcome))
		Fail("server could not queue welcome");
	bool aReceivedWelcome = false;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline)
	{
		aServer->Poll();
		aClient.Poll();
		auto aMessages = aClient.TakeMessages();
		if (!aMessages.empty())
		{
			aReceivedWelcome = aMessages.size() == 1 && aMessages.front() == aWelcome;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (!aReceivedWelcome)
		Fail("client did not receive welcome");

	// A busy peer can leave thousands of small packets in the TCP receive
	// buffer while this process is loading or temporarily starved.  Poll must
	// return in bounded batches so its decoder can be drained instead of
	// failing at the internal message-queue limit.
	constexpr uint32_t BURST_ACTION_COUNT = 5000;
	for (uint32_t i = 0; i < BURST_ACTION_COUNT; ++i)
	{
		Message anAction = GameAction{i + 1, 100 + i, 1,
			static_cast<uint16_t>(i % 9), static_cast<uint16_t>(i % 6), 0,
			ActionKind::PLANT_SEED};
		if (!aServer->Queue(anAction))
			Fail("server could not queue burst action");
	}
	uint32_t aBurstReceived = 0;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (std::chrono::steady_clock::now() < aDeadline && aBurstReceived < BURST_ACTION_COUNT)
	{
		aServer->Poll();
		aClient.Poll();
		if (aClient.GetState() != ReliableChannelState::CONNECTED)
			Fail("client failed while draining a valid burst");
		aBurstReceived += static_cast<uint32_t>(aClient.TakeMessages().size());
	}
	if (aBurstReceived != BURST_ACTION_COUNT)
		Fail("valid burst was not fully drained");

	// A lifecycle root must not sit behind obsolete actions or high-frequency
	// telemetry from the previous board.  This is the failure mode that used to
	// strand a lagging client in the first-run intro while the host waited on the
	// following Adventure session.
	Message anOldAction = GameAction{81, 1, 0, 2, 3, 1, ActionKind::PLANT_SEED};
	Message anOldTick = TickSync{82, 10};
	Message anOldHash = StateHash{80, 10, 0x1234};
	Message aReady = SessionReady{11, 1};
	if (!aClient.Queue(anOldAction) || !aClient.Queue(anOldTick) ||
		!aClient.Queue(aCursor) || !aClient.Queue(anOldHash) || !aClient.Queue(aReady))
	{
		Fail("client could not queue superseding ready");
	}
	std::vector<Message> aReadyMessages;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aReadyMessages.empty())
	{
		aClient.Poll();
		aServer->Poll();
		aReadyMessages = aServer->TakeMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aReadyMessages.size() != 1 || aReadyMessages.front() != aReady)
		Fail("session ready did not preempt obsolete client backlog");

	// SessionBegin is also priority traffic, but ordered gameplay is retained
	// and delivered after it.  Coalescible synchronization packets are dropped.
	Message aFirstAction = GameAction{90, 2, 0, 1, 1, 0, ActionKind::PLANT_SEED};
	Message aSecondAction = GameAction{91, 3, 0, 2, 1, 0, ActionKind::PLANT_SEED};
	Message aBegin = SessionBegin{88, 11};
	if (!aServer->Queue(aFirstAction) || !aServer->Queue(anOldTick) ||
		!aServer->Queue(aSecondAction) || !aServer->Queue(anOldHash) || !aServer->Queue(aBegin))
	{
		Fail("server could not queue priority begin");
	}
	std::vector<Message> aBeginMessages;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aBeginMessages.size() < 3)
	{
		aServer->Poll();
		aClient.Poll();
		auto aMessages = aClient.TakeMessages();
		aBeginMessages.insert(aBeginMessages.end(), aMessages.begin(), aMessages.end());
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aBeginMessages.size() != 3 || aBeginMessages[0] != aBegin ||
		aBeginMessages[1] != aFirstAction || aBeginMessages[2] != aSecondAction)
	{
		Fail("session begin did not preempt telemetry while preserving ordered actions");
	}

	GameplayProfile aProfile;
	aProfile.mProfileId = 1;
	std::array<std::string, MAX_PLAYERS> aNames{"Host", "Client", "", ""};
	Message aStart = SessionStart{0, 12, 12345, 0, aProfile, aNames};
	if (!aServer->Queue(aFirstAction) || !aServer->Queue(anOldTick) ||
		!aServer->Queue(aCursor) || !aServer->Queue(anOldHash) || !aServer->Queue(aStart))
	{
		Fail("server could not queue superseding start");
	}
	std::vector<Message> aStartMessages;
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aStartMessages.empty())
	{
		aServer->Poll();
		aClient.Poll();
		aStartMessages = aClient.TakeMessages();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aStartMessages.size() != 1 || aStartMessages.front() != aStart)
		Fail("session start did not preempt obsolete server backlog");

	aServer->Close();
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline && aClient.GetState() == ReliableChannelState::CONNECTED)
	{
		aClient.Poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aClient.GetState() != ReliableChannelState::CLOSED)
		Fail("peer close was not propagated");

	std::cout << "PvZ 95 reliable channel test passed\n";
	return 0;
}
