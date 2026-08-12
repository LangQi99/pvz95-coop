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
	Message aCursor = CursorUpdate{77, 1, 12000, 34000, 1, 0, true};
	if (!aClient.Queue(aHello) || !aClient.Queue(aCursor))
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
