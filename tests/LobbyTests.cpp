/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/Lobby.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	using namespace PvzMultiplayer;

	[[noreturn]] void Fail(const std::string& theMessage)
	{
		std::cerr << theMessage << '\n';
		std::exit(1);
	}

	Welcome ExpectWelcome(const HandshakeResponse& theResponse)
	{
		if (!std::holds_alternative<Welcome>(theResponse))
			Fail("expected welcome response");
		return std::get<Welcome>(theResponse);
	}

	Reject ExpectReject(const HandshakeResponse& theResponse, RejectReason theReason)
	{
		if (!std::holds_alternative<Reject>(theResponse))
			Fail("expected rejection response");
		const Reject& aReject = std::get<Reject>(theResponse);
		if (aReject.mReason != theReason)
			Fail("unexpected rejection reason");
		return aReject;
	}
}

int main()
{
	using namespace PvzMultiplayer;
	constexpr uint64_t SESSION_ID = 0x8877665544332211ULL;
	constexpr uint32_t RULESET_ID = 0x50563935;

	if (!IsValidDisplayName("房主", MAX_PLAYER_NAME_LENGTH) ||
		IsValidDisplayName("   ", MAX_PLAYER_NAME_LENGTH) ||
		IsValidDisplayName("bad\nname", MAX_PLAYER_NAME_LENGTH) ||
		IsValidDisplayName(std::string("\xC0\xAF", 2), MAX_PLAYER_NAME_LENGTH))
		Fail("display-name validation failed");

	HostLobby aLobby;
	if (!aLobby.Start({SESSION_ID, RULESET_ID, 100, 3, "向日葵房间", "房主"}))
		Fail("valid host lobby did not start");
	if (aLobby.GetPlayerCount() != 1 || !aLobby.GetPlayers()[0] || aLobby.GetPlayers()[0]->mPlayerId != 0)
		Fail("host player was not initialized");

	auto anOffer = aLobby.MakeDiscoveryOffer(43096);
	if (!anOffer || anOffer->mPlayerCount != 1 || anOffer->mRulesetId != RULESET_ID)
		Fail("discovery offer was not built from lobby state");

	ExpectReject(aLobby.HandleHello({1, 0x4F524947, 0, "Wrong Rules"}), RejectReason::RULESET_MISMATCH);
	ExpectReject(aLobby.HandleHello({2, RULESET_ID, 0, " \n"}), RejectReason::INVALID_NAME);
	ExpectReject(aLobby.HandleHello({0, RULESET_ID, 0, "No Nonce"}), RejectReason::INVALID_REQUEST);

	Welcome aPlayerOne = ExpectWelcome(aLobby.HandleHello({11, RULESET_ID, 0, "Player One"}));
	if (aPlayerOne.mPlayerId != 1 || aPlayerOne.mCursorRgb == aLobby.GetPlayers()[0]->mCursorRgb)
		Fail("first client assignment is invalid");
	if (ValidateWelcome(aPlayerOne, SESSION_ID, RULESET_ID) != WelcomeValidation::ACCEPTED)
		Fail("valid welcome was rejected by client validation");

	Welcome aRetry = ExpectWelcome(aLobby.HandleHello({11, RULESET_ID, 0, "Player One"}));
	if (aRetry != aPlayerOne || aLobby.GetPlayerCount() != 2)
		Fail("repeated client hello was not idempotent");

	Welcome aPlayerTwo = ExpectWelcome(aLobby.HandleHello({12, RULESET_ID, 0, "Player Two"}));
	if (aPlayerTwo.mPlayerId != 2 || aPlayerTwo.mCursorRgb == aPlayerOne.mCursorRgb)
		Fail("second client assignment is invalid");
	ExpectReject(aLobby.HandleHello({13, RULESET_ID, 0, "Player Three"}), RejectReason::SERVER_FULL);

	if (!aLobby.RemovePlayer(1) || aLobby.RemovePlayer(0) || aLobby.GetPlayerCount() != 2)
		Fail("player removal failed");
	Welcome aReusedSlot = ExpectWelcome(aLobby.HandleHello({13, RULESET_ID, 0, "Player Three"}));
	if (aReusedSlot.mPlayerId != 1)
		Fail("empty player slot was not reused");
	auto aPlayerNames = aLobby.MakePlayerNameSnapshot();
	if (aPlayerNames[0] != "房主" || aPlayerNames[1] != "Player Three" ||
		aPlayerNames[2] != "Player Two" || !aPlayerNames[3].empty())
		Fail("player-name snapshot did not preserve the player ID mapping");

	aLobby.SetSessionStarted(true);
	if (!aLobby.RemovePlayer(2))
		Fail("could not remove client after starting");
	ExpectReject(aLobby.HandleHello({14, RULESET_ID, 0, "Late Player"}), RejectReason::SESSION_STARTED);

	Welcome aWrongSession = aPlayerOne;
	aWrongSession.mSessionId++;
	if (ValidateWelcome(aWrongSession, SESSION_ID, RULESET_ID) != WelcomeValidation::WRONG_SESSION)
		Fail("wrong session was not detected");

	aLobby.Stop();
	ExpectReject(aLobby.HandleHello({15, RULESET_ID, 0, "Offline"}), RejectReason::INTERNAL_ERROR);

	std::cout << "PvZ 95 lobby tests passed\n";
	return 0;
}
