/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "PacketStream.h"

#include <utility>

namespace PvzMultiplayer
{
	namespace
	{
		// A board/resource transition can briefly stop the app from draining the
		// socket while the peer continues to send clock and cursor updates.  Keep
		// the limit finite, but large enough to absorb a full short-lived TCP
		// backlog without treating valid LAN traffic as a malformed stream.
		constexpr size_t MAX_QUEUED_MESSAGES = 4096;
		constexpr size_t MAX_STREAM_BUFFER = MAX_PACKET_SIZE * MAX_QUEUED_MESSAGES;

		uint32_t ReadPayloadLength(const std::vector<uint8_t>& theBuffer)
		{
			return static_cast<uint32_t>(theBuffer[8]) |
				(static_cast<uint32_t>(theBuffer[9]) << 8) |
				(static_cast<uint32_t>(theBuffer[10]) << 16) |
				(static_cast<uint32_t>(theBuffer[11]) << 24);
		}
	}

	bool PacketStreamDecoder::Feed(std::span<const uint8_t> theBytes)
	{
		if (mError != CodecError::NONE)
			return false;
		if (theBytes.size() > MAX_STREAM_BUFFER || mBuffer.size() + theBytes.size() > MAX_STREAM_BUFFER)
		{
			mError = CodecError::PACKET_TOO_LARGE;
			return false;
		}

		mBuffer.insert(mBuffer.end(), theBytes.begin(), theBytes.end());
		while (mBuffer.size() >= PACKET_HEADER_SIZE)
		{
			size_t aPacketSize = PACKET_HEADER_SIZE + ReadPayloadLength(mBuffer);
			if (aPacketSize > MAX_PACKET_SIZE)
			{
				mError = CodecError::PACKET_TOO_LARGE;
				return false;
			}
			if (mBuffer.size() < aPacketSize)
				return true;

			auto aDecoded = Decode(std::span<const uint8_t>(mBuffer.data(), aPacketSize));
			if (!aDecoded)
			{
				mError = aDecoded.mError;
				return false;
			}
			if (mMessages.size() >= MAX_QUEUED_MESSAGES)
			{
				mError = CodecError::PACKET_TOO_LARGE;
				return false;
			}

			mMessages.push_back(std::move(*aDecoded.mMessage));
			mBuffer.erase(mBuffer.begin(), mBuffer.begin() + static_cast<ptrdiff_t>(aPacketSize));
		}
		return true;
	}

	std::vector<Message> PacketStreamDecoder::TakeMessages()
	{
		std::vector<Message> aMessages;
		aMessages.swap(mMessages);
		return aMessages;
	}

	void PacketStreamDecoder::Reset()
	{
		mBuffer.clear();
		mMessages.clear();
		mError = CodecError::NONE;
	}

	CodecError PacketStreamDecoder::GetError() const
	{
		return mError;
	}

	size_t PacketStreamDecoder::GetBufferedByteCount() const
	{
		return mBuffer.size();
	}
}
