/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/PacketStream.h"
#include "Multiplayer/TcpSocket.h"

#include <array>
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

	void SendPacket(TcpSocket& theSocket, const std::vector<uint8_t>& thePacket)
	{
		size_t anOffset = 0;
		auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (anOffset < thePacket.size() && std::chrono::steady_clock::now() < aDeadline)
		{
			auto aResult = theSocket.Send(std::span<const uint8_t>(thePacket).subspan(anOffset));
			if (aResult.mStatus == SocketIoStatus::COMPLETED)
				anOffset += aResult.mByteCount;
			else if (aResult.mStatus != SocketIoStatus::WOULD_BLOCK)
				Fail("TCP send failed: " + theSocket.GetLastError());
			if (anOffset < thePacket.size())
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		if (anOffset != thePacket.size())
			Fail("TCP send timed out");
	}

	Message ReceiveMessage(TcpSocket& theSocket)
	{
		PacketStreamDecoder aDecoder;
		std::array<uint8_t, 256> aBuffer{};
		auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (std::chrono::steady_clock::now() < aDeadline)
		{
			auto aResult = theSocket.Receive(aBuffer);
			if (aResult.mStatus == SocketIoStatus::COMPLETED)
			{
				if (!aDecoder.Feed(std::span<const uint8_t>(aBuffer).first(aResult.mByteCount)))
					Fail("TCP stream decode failed");
				auto aMessages = aDecoder.TakeMessages();
				if (!aMessages.empty())
					return std::move(aMessages.front());
			}
			else if (aResult.mStatus != SocketIoStatus::WOULD_BLOCK)
			{
				Fail("TCP receive failed: " + theSocket.GetLastError());
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		Fail("TCP receive timed out");
	}
}

int main()
{
	using namespace PvzMultiplayer;

	TcpListener aListener;
	if (!aListener.Listen(0) || aListener.GetLocalPort() == 0)
		Fail("TCP listener failed: " + aListener.GetLastError());

	TcpSocket aClient;
	if (!aClient.StartConnect(Ipv4Endpoint::Loopback(aListener.GetLocalPort())))
		Fail("TCP client failed to start: " + aClient.GetLastError());

	std::optional<TcpSocket> aServer;
	auto aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline)
	{
		aClient.PollConnect();
		if (!aServer)
			aServer = aListener.Accept();
		if (aClient.GetState() == ConnectionState::CONNECTED && aServer)
			break;
		if (aClient.GetState() == ConnectionState::FAILED)
			Fail("TCP connect failed: " + aClient.GetLastError());
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (aClient.GetState() != ConnectionState::CONNECTED || !aServer)
		Fail("TCP loopback connection timed out");

	Message aHello = Hello{0x0102030405060708ULL, 0x50563935, 3, "Player Two"};
	SendPacket(aClient, *Encode(aHello));
	if (ReceiveMessage(*aServer) != aHello)
		Fail("server received the wrong message");

	Message aWelcome = Welcome{0x8877665544332211ULL, 0x50563935, 0x42A5F5, 100, 1, 4};
	SendPacket(*aServer, *Encode(aWelcome));
	if (ReceiveMessage(aClient) != aWelcome)
		Fail("client received the wrong message");

	aServer->Close();
	bool aSawClose = false;
	std::array<uint8_t, 16> aBuffer{};
	aDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < aDeadline)
	{
		auto aResult = aClient.Receive(aBuffer);
		if (aResult.mStatus == SocketIoStatus::CLOSED)
		{
			aSawClose = true;
			break;
		}
		if (aResult.mStatus == SocketIoStatus::ERROR)
			Fail("client errored while waiting for close");
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (!aSawClose)
		Fail("TCP peer close was not observed");

	std::cout << "PvZ 95 TCP socket test passed\n";
	return 0;
}
