/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "PacketStream.h"
#include "TcpSocket.h"

#include <deque>
#include <string>

namespace PvzMultiplayer
{
	enum class ReliableChannelState : uint8_t
	{
		CONNECTING,
		CONNECTED,
		CLOSED,
		FAILED
	};

	class ReliableChannel
	{
	public:
		explicit ReliableChannel(TcpSocket theSocket);

		ReliableChannel(const ReliableChannel&) = delete;
		ReliableChannel& operator=(const ReliableChannel&) = delete;
		ReliableChannel(ReliableChannel&&) noexcept = default;
		ReliableChannel& operator=(ReliableChannel&&) noexcept = default;

		bool Queue(const Message& theMessage);
		ReliableChannelState Poll();
		std::vector<Message> TakeMessages();
		void Close();

		ReliableChannelState GetState() const;
		size_t GetQueuedByteCount() const;
		const Ipv4Endpoint& GetPeerEndpoint() const;
		const std::string& GetLastError() const;

	private:
		enum class OutgoingClass : uint8_t
		{
			HANDSHAKE,
			RELIABLE,
			CURSOR,
			TICK_SYNC,
			STATE_HASH
		};

		struct OutgoingPacket
		{
			std::vector<uint8_t> mBytes;
			OutgoingClass mClass{OutgoingClass::RELIABLE};
			uint64_t mCoalesceKey{};
		};

		void Fail(std::string theError);
		void DiscardUnsent(bool theDiscardReliable, bool theDiscardPresentation);
		void DiscardCoalesced(OutgoingClass theClass, uint64_t theKey);
		void QueuePriorityPacket(OutgoingPacket thePacket);

		TcpSocket mSocket;
		PacketStreamDecoder mDecoder;
		std::deque<OutgoingPacket> mOutgoing;
		size_t mOutgoingOffset{};
		size_t mQueuedBytes{};
		ReliableChannelState mState{ReliableChannelState::CONNECTING};
		std::string mLastError;
	};
}
