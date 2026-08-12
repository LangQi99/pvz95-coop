/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/PacketStream.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	[[noreturn]] void Fail(const std::string& theMessage)
	{
		std::cerr << theMessage << '\n';
		std::exit(1);
	}
}

int main()
{
	using namespace PvzMultiplayer;

	Message aHello = Hello{0x0102030405060708ULL, 0x50563935, 3, "Player Two"};
	Message aCursor = CursorUpdate{4567, 99, 32768, 16384, 2, true, 1};
	auto aHelloPacket = *Encode(aHello);
	auto aCursorPacket = *Encode(aCursor);

	PacketStreamDecoder aByteDecoder;
	for (uint8_t aByte : aHelloPacket)
	{
		if (!aByteDecoder.Feed(std::span<const uint8_t>(&aByte, 1)))
			Fail("byte-fragmented packet failed to decode");
	}
	auto aMessages = aByteDecoder.TakeMessages();
	if (aMessages.size() != 1 || aMessages[0] != aHello || aByteDecoder.GetBufferedByteCount() != 0)
		Fail("byte-fragmented packet produced the wrong message");

	std::vector<uint8_t> aCombined = aHelloPacket;
	aCombined.insert(aCombined.end(), aCursorPacket.begin(), aCursorPacket.end());
	PacketStreamDecoder aCombinedDecoder;
	if (!aCombinedDecoder.Feed(aCombined))
		Fail("combined packets failed to decode");
	aMessages = aCombinedDecoder.TakeMessages();
	if (aMessages.size() != 2 || aMessages[0] != aHello || aMessages[1] != aCursor)
		Fail("combined stream produced the wrong messages");

	PacketStreamDecoder aPartialDecoder;
	size_t aSplit = aHelloPacket.size() / 2;
	if (!aPartialDecoder.Feed(std::span<const uint8_t>(aHelloPacket).first(aSplit)) ||
		! aPartialDecoder.TakeMessages().empty() || aPartialDecoder.GetBufferedByteCount() != aSplit)
		Fail("partial packet was not buffered");
	if (!aPartialDecoder.Feed(std::span<const uint8_t>(aHelloPacket).subspan(aSplit)) ||
		aPartialDecoder.TakeMessages().size() != 1)
		Fail("partial packet did not finish");

	aHelloPacket[0] = 'X';
	PacketStreamDecoder aBadMagicDecoder;
	if (aBadMagicDecoder.Feed(aHelloPacket) || aBadMagicDecoder.GetError() != CodecError::BAD_MAGIC)
		Fail("bad packet magic was not rejected");

	aHelloPacket = *Encode(aHello);
	aHelloPacket[8] = 0xFF;
	aHelloPacket[9] = 0xFF;
	PacketStreamDecoder anOversizedDecoder;
	if (anOversizedDecoder.Feed(std::span<const uint8_t>(aHelloPacket).first(PACKET_HEADER_SIZE)) ||
		anOversizedDecoder.GetError() != CodecError::PACKET_TOO_LARGE)
		Fail("oversized stream frame was not rejected");
	anOversizedDecoder.Reset();
	if (anOversizedDecoder.GetError() != CodecError::NONE || anOversizedDecoder.GetBufferedByteCount() != 0)
		Fail("stream decoder did not reset");

	std::cout << "PvZ 95 packet stream tests passed\n";
	return 0;
}
