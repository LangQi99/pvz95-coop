/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ReliableChannel.h"

#include <array>
#include <utility>

namespace PvzMultiplayer
{
	namespace
	{
		constexpr size_t MAX_OUTGOING_BYTES = MAX_PACKET_SIZE * 256;
		constexpr size_t RECEIVE_BUFFER_SIZE = 4096;
	}

	ReliableChannel::ReliableChannel(TcpSocket theSocket) :
		mSocket(std::move(theSocket))
	{
		switch (mSocket.GetState())
		{
		case ConnectionState::CONNECTING:
			mState = ReliableChannelState::CONNECTING;
			break;
		case ConnectionState::CONNECTED:
			mState = ReliableChannelState::CONNECTED;
			break;
		case ConnectionState::DISCONNECTED:
			mState = ReliableChannelState::CLOSED;
			break;
		case ConnectionState::FAILED:
			mState = ReliableChannelState::FAILED;
			mLastError = mSocket.GetLastError();
			break;
		}
	}

	bool ReliableChannel::Queue(const Message& theMessage)
	{
		if (mState == ReliableChannelState::CLOSED || mState == ReliableChannelState::FAILED)
			return false;

		auto aPacket = Encode(theMessage);
		if (!aPacket)
		{
			Fail("message could not be encoded");
			return false;
		}
		if (aPacket->size() > MAX_OUTGOING_BYTES - mQueuedBytes)
		{
			Fail("reliable send queue limit exceeded");
			return false;
		}

		mQueuedBytes += aPacket->size();
		mOutgoing.push_back(std::move(*aPacket));
		return true;
	}

	ReliableChannelState ReliableChannel::Poll()
	{
		if (mState == ReliableChannelState::CLOSED || mState == ReliableChannelState::FAILED)
			return mState;

		if (mSocket.GetState() == ConnectionState::CONNECTING)
		{
			ConnectionState aConnectionState = mSocket.PollConnect();
			if (aConnectionState == ConnectionState::CONNECTING)
				return mState;
			if (aConnectionState != ConnectionState::CONNECTED)
			{
				Fail(mSocket.GetLastError().empty() ? "TCP connection failed" : mSocket.GetLastError());
				return mState;
			}
			mState = ReliableChannelState::CONNECTED;
		}

		while (!mOutgoing.empty())
		{
			const auto& aPacket = mOutgoing.front();
			auto aResult = mSocket.Send(std::span<const uint8_t>(aPacket).subspan(mOutgoingOffset));
			if (aResult.mStatus == SocketIoStatus::WOULD_BLOCK)
				break;
			if (aResult.mStatus == SocketIoStatus::CLOSED)
			{
				Close();
				return mState;
			}
			if (aResult.mStatus == SocketIoStatus::FAILED || aResult.mByteCount == 0)
			{
				Fail(mSocket.GetLastError().empty() ? "TCP send failed" : mSocket.GetLastError());
				return mState;
			}

			mOutgoingOffset += aResult.mByteCount;
			mQueuedBytes -= aResult.mByteCount;
			if (mOutgoingOffset == aPacket.size())
			{
				mOutgoing.pop_front();
				mOutgoingOffset = 0;
			}
		}

		std::array<uint8_t, RECEIVE_BUFFER_SIZE> aBuffer{};
		while (true)
		{
			auto aResult = mSocket.Receive(aBuffer);
			if (aResult.mStatus == SocketIoStatus::WOULD_BLOCK)
				break;
			if (aResult.mStatus == SocketIoStatus::CLOSED)
			{
				Close();
				return mState;
			}
			if (aResult.mStatus == SocketIoStatus::FAILED || aResult.mByteCount == 0)
			{
				Fail(mSocket.GetLastError().empty() ? "TCP receive failed" : mSocket.GetLastError());
				return mState;
			}
			if (!mDecoder.Feed(std::span<const uint8_t>(aBuffer).first(aResult.mByteCount)))
			{
				Fail("invalid reliable packet stream: " + std::string(GetCodecErrorName(mDecoder.GetError())));
				return mState;
			}
		}

		return mState;
	}

	std::vector<Message> ReliableChannel::TakeMessages()
	{
		return mDecoder.TakeMessages();
	}

	void ReliableChannel::Close()
	{
		mSocket.Close();
		mOutgoing.clear();
		mOutgoingOffset = 0;
		mQueuedBytes = 0;
		if (mState != ReliableChannelState::FAILED)
			mState = ReliableChannelState::CLOSED;
	}

	ReliableChannelState ReliableChannel::GetState() const
	{
		return mState;
	}

	size_t ReliableChannel::GetQueuedByteCount() const
	{
		return mQueuedBytes;
	}

	const Ipv4Endpoint& ReliableChannel::GetPeerEndpoint() const
	{
		return mSocket.GetPeerEndpoint();
	}

	const std::string& ReliableChannel::GetLastError() const
	{
		return mLastError;
	}

	void ReliableChannel::Fail(std::string theError)
	{
		mLastError = std::move(theError);
		mState = ReliableChannelState::FAILED;
		mSocket.Close();
		mOutgoing.clear();
		mOutgoingOffset = 0;
		mQueuedBytes = 0;
	}
}
