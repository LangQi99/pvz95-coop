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
		OutgoingPacket anOutgoing{std::move(*aPacket)};
		if (std::holds_alternative<Hello>(theMessage) ||
			std::holds_alternative<Welcome>(theMessage) ||
			std::holds_alternative<Reject>(theMessage))
		{
			anOutgoing.mClass = OutgoingClass::HANDSHAKE;
		}
		else if (const auto* aCursor = std::get_if<CursorUpdate>(&theMessage))
		{
			anOutgoing.mClass = OutgoingClass::CURSOR;
			anOutgoing.mCoalesceKey = aCursor->mPlayerId;
		}
		else if (const auto* aTick = std::get_if<TickSync>(&theMessage))
		{
			anOutgoing.mClass = OutgoingClass::TICK_SYNC;
			anOutgoing.mCoalesceKey = aTick->mStartId;
		}
		else if (const auto* aHash = std::get_if<StateHash>(&theMessage))
		{
			anOutgoing.mClass = OutgoingClass::STATE_HASH;
			anOutgoing.mCoalesceKey = aHash->mStartId;
		}

		const bool aSupersedingLifecycle = std::holds_alternative<SessionStart>(theMessage) ||
			std::holds_alternative<SessionReady>(theMessage) ||
			std::holds_alternative<SessionEnd>(theMessage);
		const bool aPriorityLifecycle = aSupersedingLifecycle ||
			std::holds_alternative<SessionBegin>(theMessage);
		if (aSupersedingLifecycle)
		{
			// The new lifecycle root makes every fully unsent packet from the old
			// game obsolete, including actions. A partially written TCP packet must
			// finish, but the lifecycle packet is placed immediately after it.
			DiscardUnsent(true, true);
		}
		else if (aPriorityLifecycle)
		{
			DiscardUnsent(false, true);
		}
		else if (anOutgoing.mClass == OutgoingClass::CURSOR ||
			anOutgoing.mClass == OutgoingClass::TICK_SYNC ||
			anOutgoing.mClass == OutgoingClass::STATE_HASH)
		{
			DiscardCoalesced(anOutgoing.mClass, anOutgoing.mCoalesceKey);
		}

		if (anOutgoing.mBytes.size() > MAX_OUTGOING_BYTES - mQueuedBytes)
		{
			Fail("reliable send queue limit exceeded");
			return false;
		}

		mQueuedBytes += anOutgoing.mBytes.size();
		if (aPriorityLifecycle)
			QueuePriorityPacket(std::move(anOutgoing));
		else
			mOutgoing.push_back(std::move(anOutgoing));
		return true;
	}

	void ReliableChannel::DiscardUnsent(bool theDiscardReliable, bool theDiscardPresentation)
	{
		for (auto anIt = mOutgoing.begin(); anIt != mOutgoing.end();)
		{
			const bool aPartiallySent = anIt == mOutgoing.begin() && mOutgoingOffset != 0;
			const bool aPresentation = anIt->mClass == OutgoingClass::CURSOR ||
				anIt->mClass == OutgoingClass::TICK_SYNC ||
				anIt->mClass == OutgoingClass::STATE_HASH;
			const bool aReliableGameplay = anIt->mClass == OutgoingClass::RELIABLE;
			if (!aPartiallySent && ((aPresentation && theDiscardPresentation) ||
				(aReliableGameplay && theDiscardReliable)))
			{
				mQueuedBytes -= anIt->mBytes.size();
				anIt = mOutgoing.erase(anIt);
			}
			else
			{
				++anIt;
			}
		}
	}

	void ReliableChannel::DiscardCoalesced(OutgoingClass theClass, uint64_t theKey)
	{
		for (auto anIt = mOutgoing.begin(); anIt != mOutgoing.end();)
		{
			const bool aPartiallySent = anIt == mOutgoing.begin() && mOutgoingOffset != 0;
			if (!aPartiallySent && anIt->mClass == theClass && anIt->mCoalesceKey == theKey)
			{
				mQueuedBytes -= anIt->mBytes.size();
				anIt = mOutgoing.erase(anIt);
			}
			else
			{
				++anIt;
			}
		}
	}

	void ReliableChannel::QueuePriorityPacket(OutgoingPacket thePacket)
	{
		auto anInsertAt = mOutgoing.begin();
		if (anInsertAt != mOutgoing.end() && mOutgoingOffset != 0)
			++anInsertAt;
		while (anInsertAt != mOutgoing.end() && anInsertAt->mClass == OutgoingClass::HANDSHAKE)
			++anInsertAt;
		mOutgoing.insert(anInsertAt, std::move(thePacket));
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
			const auto& aPacket = mOutgoing.front().mBytes;
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
