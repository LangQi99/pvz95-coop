/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/Protocol.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	using namespace PvzMultiplayer;

	[[noreturn]] void Fail(const std::string& theMessage)
	{
		std::cerr << theMessage << '\n';
		std::exit(1);
	}

	void ExpectRoundTrip(const Message& theMessage)
	{
		auto aPacket = Encode(theMessage);
		if (!aPacket)
			Fail("message failed to encode");

		auto aDecoded = Decode(*aPacket);
		if (!aDecoded)
			Fail("message failed to decode: " + std::string(GetCodecErrorName(aDecoded.mError)));
		if (*aDecoded.mMessage != theMessage)
			Fail("round-trip message mismatch");
	}

	void ExpectDecodeError(const std::vector<uint8_t>& thePacket, CodecError theError)
	{
		auto aDecoded = Decode(thePacket);
		if (aDecoded || aDecoded.mError != theError)
			Fail("unexpected decode result");
	}
}

int main()
{
	using namespace PvzMultiplayer;
	GameplayProfile aProfile;
	aProfile.mProfileId = 17;
	aProfile.mAdventureLevel = 34;
	aProfile.mCoins = 12340;
	aProfile.mFinishedAdventure = 1;
	aProfile.mFlags = PROFILE_HAS_UNLOCKED_MINIGAMES | PROFILE_HAS_SEEN_UPSELL;
	aProfile.mChallengeRecords[7] = 3;
	aProfile.mPurchases[4] = 1002;

	ExpectRoundTrip(DiscoveryQuery{0x1020304050607080ULL});
	ExpectRoundTrip(DiscoveryOffer{0x8877665544332211ULL, 43096, 2, 4, 0x50563935, "Sunflower Room"});
	ExpectRoundTrip(Hello{0x0102030405060708ULL, 0x50563935, 3, "Player Two"});
	ExpectRoundTrip(Welcome{0x8877665544332211ULL, 0x50563935, 0x45A3FF, 100, 1, 4});
	ExpectRoundTrip(Reject{RejectReason::RULESET_MISMATCH, "PvZ95 rules required"});
	ExpectRoundTrip(CursorUpdate{4567, 99, 32768, 16384, 2, true, 3});
	ExpectRoundTrip(GameAction{4570, 100, 1, 4, 2, 2, ActionKind::PLANT_SEED});
	ExpectRoundTrip(SessionStart{4580, 0x1020304050607080ULL, 0x12345678, 0, aProfile});
	ExpectRoundTrip(SessionReady{0x1020304050607080ULL, 2});
	ExpectRoundTrip(SessionBegin{4590, 0x1020304050607080ULL});
	ExpectRoundTrip(TickSync{4595, 0x1020304050607080ULL});
	ExpectRoundTrip(StateHash{4600, 0x1020304050607080ULL, 0xDEADBEEFCAFEBABEULL});

	ExpectDecodeError({}, CodecError::PACKET_TOO_SHORT);

	auto aPacket = *Encode(Message(DiscoveryQuery{1}));
	aPacket[0] = 'X';
	ExpectDecodeError(aPacket, CodecError::BAD_MAGIC);

	aPacket = *Encode(Message(DiscoveryQuery{1}));
	aPacket[4] = 0xFF;
	ExpectDecodeError(aPacket, CodecError::UNSUPPORTED_VERSION);

	aPacket = *Encode(Message(DiscoveryQuery{1}));
	aPacket[6] = 0xFF;
	ExpectDecodeError(aPacket, CodecError::UNKNOWN_MESSAGE);

	aPacket = *Encode(Message(DiscoveryQuery{1}));
	aPacket.pop_back();
	ExpectDecodeError(aPacket, CodecError::BAD_LENGTH);

	if (Encode(Message(Hello{1, 0x50563935, 0, ""})))
		Fail("empty player name encoded successfully");
	if (Encode(Message(Hello{1, 0x50563935, 0, std::string(MAX_PLAYER_NAME_LENGTH + 1, 'x')})))
		Fail("oversized player name encoded successfully");
	if (Encode(Message(Welcome{1, 0x50563935, 0xFFFFFF, 100, MAX_PLAYERS, MAX_PLAYERS})))
		Fail("invalid player id encoded successfully");
	if (Encode(Message(Welcome{1, 0x50563935, 0x1000000, 100, 1, MAX_PLAYERS})))
		Fail("invalid cursor color encoded successfully");
	if (Encode(Message(DiscoveryOffer{1, 43096, 1, MAX_PLAYERS, 0x50563935, ""})))
		Fail("empty session name encoded successfully");
	if (Encode(Message(CursorUpdate{1, 1, 0, 0, MAX_PLAYERS, true})))
		Fail("invalid cursor player encoded successfully");
	if (Encode(Message(CursorUpdate{1, 1, 0, 0, 1, true,
		static_cast<uint8_t>(MAX_CURSOR_SEED_BANK_INDEX + 1)})))
		Fail("invalid cursor held seed index encoded successfully");
	if (Encode(Message(GameAction{1, 1, 0, 0, 0, 1, static_cast<ActionKind>(0)})))
		Fail("invalid game action kind encoded successfully");
	aProfile.mFlags = 0x80000000U;
	if (Encode(Message(SessionStart{1, 1, 1, 0, aProfile})))
		Fail("invalid session profile flags encoded successfully");
	if (Encode(Message(SessionReady{0, 1})))
		Fail("zero session start ID encoded successfully");

	std::cout << "PvZ 95 multiplayer protocol tests passed\n";
	return 0;
}
