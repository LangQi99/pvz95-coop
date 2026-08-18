/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "Protocol.h"

#include <span>
#include <vector>

namespace PvzMultiplayer
{
	class PacketStreamDecoder
	{
	public:
		bool Feed(std::span<const uint8_t> theBytes);
		std::vector<Message> TakeMessages();
		void Reset();

		CodecError GetError() const;
		size_t GetBufferedByteCount() const;
		size_t GetQueuedMessageCount() const;

	private:
		std::vector<uint8_t> mBuffer;
		std::vector<Message> mMessages;
		CodecError mError{CodecError::NONE};
	};
}
