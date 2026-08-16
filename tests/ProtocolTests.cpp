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
	aProfile.mPottedPlants.push_back(GameplayPottedPlant{
		38, 0, 4, 2, 1, 123456789, 7, 3, 4, 5, 2, 123456000, 123455000, 123454000, 0});
	std::array<std::string, MAX_PLAYERS> aPlayerNames{"房主", "Guest", "", ""};

	ExpectRoundTrip(DiscoveryQuery{0x1020304050607080ULL});
	ExpectRoundTrip(DiscoveryOffer{0x8877665544332211ULL, 43096, 2, 4, 0x50563935, "Sunflower Room"});
	ExpectRoundTrip(Hello{0x0102030405060708ULL, 0x50563935, 3, "Player Two"});
	ExpectRoundTrip(Welcome{0x8877665544332211ULL, 0x50563935, 0x45A3FF, 100, 1, 4});
	ExpectRoundTrip(Reject{RejectReason::RULESET_MISMATCH, "PvZ95 rules required"});
	ExpectRoundTrip(CursorUpdate{4567, 99, 32768, 16384, 2, true, 3});
	ExpectRoundTrip(GameAction{4570, 100, 1, 4, 2, 2, ActionKind::PLANT_SEED});
	ExpectRoundTrip(GameAction{4571, 101, 7, 0, 0, 1, ActionKind::ADD_SEED_CHOICE});
	ExpectRoundTrip(GameAction{4572, 102, 7, 0, 0, 1, ActionKind::REMOVE_SEED_CHOICE});
	ExpectRoundTrip(GameAction{4573, 103, 3, 0, 0, 1, ActionKind::CHOOSE_IMITATER});
	ExpectRoundTrip(GameAction{4574, 104, 0, 0, 0, 0, ActionKind::CONFIRM_SEED_CHOICES});
	ExpectRoundTrip(GameAction{4575, 105, 2406, 0, 0, 0, ActionKind::ADVANCE_CRAZY_DAVE_DIALOG});
	ExpectRoundTrip(GameAction{4576, 106, 0, 32768, 49151, 2, ActionKind::WHACK_ZOMBIE});
	ExpectRoundTrip(GameAction{4577, 107, 1503, 1, 0, 0, ActionKind::RESOLVE_PACKET_UPGRADE});
	ExpectRoundTrip(GameAction{4578, 108, 0, 7, 4, 2, ActionKind::SMASH_SCARY_POT});
	ExpectRoundTrip(GameAction{4579, 109, 42, 5, 3, 1, ActionKind::PLANT_USABLE_SEED});
	ExpectRoundTrip(GameAction{4580, 110, 42, 0, 0, 1, ActionKind::DROP_USABLE_SEED});
	ExpectRoundTrip(GameAction{4581, 111, 0, 0, 0, 2, ActionKind::PULL_SLOT_MACHINE});
	SessionStart aSessionStart{4580, 0x1020304050607080ULL, 0x12345678, 0, aProfile, aPlayerNames};
	ExpectRoundTrip(aSessionStart);
	SessionStart aMaximumNameStart = aSessionStart;
	for (std::string& aName : aMaximumNameStart.mPlayerNames)
		aName.assign(MAX_PLAYER_NAME_LENGTH, 'W');
	auto aSessionPacket = Encode(Message(aMaximumNameStart));
	if (!aSessionPacket || aSessionPacket->size() > MAX_PACKET_SIZE)
		Fail("session start with player names exceeds the packet limit");
	SessionStart aMaximumGardenStart = aSessionStart;
	aMaximumGardenStart.mProfile.mPottedPlants.resize(GAMEPLAY_POTTED_PLANT_COUNT,
		GameplayPottedPlant{38, 0, 4, 2, 1, 123456789, 7, 3, 4, 5, 2, 123456000, 123455000, 123454000, 0});
	aSessionPacket = Encode(Message(aMaximumGardenStart));
	if (!aSessionPacket || aSessionPacket->size() > MAX_PACKET_SIZE)
		Fail("session start with a full Zen Garden exceeds the packet limit");
	ExpectRoundTrip(aMaximumGardenStart);
	ExpectRoundTrip(SessionReady{0x1020304050607080ULL, 2});
	ExpectRoundTrip(SessionBegin{4590, 0x1020304050607080ULL});
	ExpectRoundTrip(TickSync{4595, 0x1020304050607080ULL});
	ExpectRoundTrip(StateHash{4600, 0x1020304050607080ULL, 0xDEADBEEFCAFEBABEULL});
	ExpectRoundTrip(SessionEnd{0x1020304050607080ULL});

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
	SessionStart anInvalidGardenStart = aSessionStart;
	anInvalidGardenStart.mProfile.mPottedPlants[0].mGardenType = GAMEPLAY_GARDEN_TYPE_COUNT;
	if (Encode(Message(anInvalidGardenStart)))
		Fail("invalid Zen Garden plant encoded successfully");
	anInvalidGardenStart = aSessionStart;
	anInvalidGardenStart.mProfile.mPottedPlants.resize(GAMEPLAY_POTTED_PLANT_COUNT + 1);
	if (Encode(Message(anInvalidGardenStart)))
		Fail("oversized Zen Garden profile encoded successfully");
	if (Encode(Message(CursorUpdate{1, 1, 0, 0, MAX_PLAYERS, true})))
		Fail("invalid cursor player encoded successfully");
	if (Encode(Message(CursorUpdate{1, 1, 0, 0, 1, true,
		static_cast<uint8_t>(MAX_CURSOR_SEED_BANK_INDEX + 1)})))
		Fail("invalid cursor held seed index encoded successfully");
	if (Encode(Message(GameAction{1, 1, 0, 0, 0, 1, static_cast<ActionKind>(0)})))
		Fail("invalid game action kind encoded successfully");
	if (Encode(Message(GameAction{1, 1, 0, 0, 0, 1,
		static_cast<ActionKind>(static_cast<uint8_t>(ActionKind::PULL_SLOT_MACHINE) + 1)})))
		Fail("out-of-range game action kind encoded successfully");
	if (Encode(Message(GameAction{1, 1, 2406, 0, 0, 1, ActionKind::ADVANCE_CRAZY_DAVE_DIALOG})))
		Fail("client-owned Crazy Dave action encoded successfully");
	if (Encode(Message(GameAction{1, 1, MAX_CRAZY_DAVE_MESSAGE_INDEX + 1, 0, 0, 0,
		ActionKind::ADVANCE_CRAZY_DAVE_DIALOG})))
		Fail("out-of-range Crazy Dave message encoded successfully");
	if (Encode(Message(GameAction{1, 1, 2406, 1, 0, 0, ActionKind::ADVANCE_CRAZY_DAVE_DIALOG})))
		Fail("Crazy Dave action with coordinates encoded successfully");
	if (Encode(Message(GameAction{1, 1, 1, 32768, 49151, 0, ActionKind::WHACK_ZOMBIE})))
		Fail("Whack-a-Zombie action with a nonzero parameter encoded successfully");
	if (Encode(Message(GameAction{1, 1, 1503, 1, 0, 1, ActionKind::RESOLVE_PACKET_UPGRADE})))
		Fail("client-owned packet upgrade resolution encoded successfully");
	if (Encode(Message(GameAction{1, 1, 1504, 1, 0, 0, ActionKind::RESOLVE_PACKET_UPGRADE})))
		Fail("packet upgrade resolution with an invalid prompt encoded successfully");
	if (Encode(Message(GameAction{1, 1, 1553, 2, 0, 0, ActionKind::RESOLVE_PACKET_UPGRADE})))
		Fail("packet upgrade resolution with an invalid choice encoded successfully");
	if (Encode(Message(GameAction{1, 1, 1553, 0, 1, 0, ActionKind::RESOLVE_PACKET_UPGRADE})))
		Fail("packet upgrade resolution with an invalid coordinate encoded successfully");
	if (Encode(Message(GameAction{1, 1, 1, 4, 2, 0, ActionKind::SMASH_SCARY_POT})))
		Fail("vase-smash action with a nonzero parameter encoded successfully");
	if (Encode(Message(GameAction{1, 1, 0, MAX_SCARY_POT_GRID_X + 1, 2, 0,
		ActionKind::SMASH_SCARY_POT})))
		Fail("vase-smash action with an invalid column encoded successfully");
	if (Encode(Message(GameAction{1, 1, 0, 4, MAX_SCARY_POT_GRID_Y + 1, 0,
		ActionKind::SMASH_SCARY_POT})))
		Fail("vase-smash action with an invalid row encoded successfully");
	if (Encode(Message(GameAction{1, 1, 0, 4, 2, 0, ActionKind::PLANT_USABLE_SEED})))
		Fail("usable-seed action without a coin id encoded successfully");
	if (Encode(Message(GameAction{1, 1, 42, MAX_SCARY_POT_GRID_X + 1, 2, 0,
		ActionKind::PLANT_USABLE_SEED})))
		Fail("usable-seed action with an invalid column encoded successfully");
	if (Encode(Message(GameAction{1, 1, 42, 4, MAX_SCARY_POT_GRID_Y + 1, 0,
		ActionKind::PLANT_USABLE_SEED})))
		Fail("usable-seed action with an invalid row encoded successfully");
	if (Encode(Message(GameAction{1, 1, 0, 0, 0, 0, ActionKind::DROP_USABLE_SEED})))
		Fail("usable-seed drop without a coin id encoded successfully");
	if (Encode(Message(GameAction{1, 1, 42, 1, 0, 0, ActionKind::DROP_USABLE_SEED})))
		Fail("usable-seed drop with coordinates encoded successfully");
	if (Encode(Message(GameAction{1, 1, 1, 0, 0, 0, ActionKind::PULL_SLOT_MACHINE})))
		Fail("slot-machine pull with a nonzero parameter encoded successfully");
	if (Encode(Message(GameAction{1, 1, 0, 1, 0, 0, ActionKind::PULL_SLOT_MACHINE})))
		Fail("slot-machine pull with coordinates encoded successfully");
	SessionStart anInvalidNamesStart = aSessionStart;
	anInvalidNamesStart.mPlayerNames[0].clear();
	if (Encode(Message(anInvalidNamesStart)))
		Fail("session start without a host name encoded successfully");
	anInvalidNamesStart = aSessionStart;
	anInvalidNamesStart.mPlayerNames[1] = std::string(MAX_PLAYER_NAME_LENGTH + 1, 'x');
	if (Encode(Message(anInvalidNamesStart)))
		Fail("session start with an oversized player name encoded successfully");
	anInvalidNamesStart = aSessionStart;
	anInvalidNamesStart.mPlayerNames[1] = std::string("\xC0\xAF", 2);
	if (Encode(Message(anInvalidNamesStart)))
		Fail("session start with invalid UTF-8 encoded successfully");
	aProfile.mFlags = 0x80000000U;
	if (Encode(Message(SessionStart{1, 1, 1, 0, aProfile, aPlayerNames})))
		Fail("invalid session profile flags encoded successfully");
	if (Encode(Message(SessionReady{0, 1})))
		Fail("zero session start ID encoded successfully");
	if (Encode(Message(SessionEnd{0})))
		Fail("zero session end ID encoded successfully");

	std::cout << "PvZ 95 multiplayer protocol tests passed\n";
	return 0;
}
