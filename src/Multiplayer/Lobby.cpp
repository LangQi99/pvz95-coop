/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Lobby.h"
#include "SharedInputState.h"

#include <algorithm>
#include <utility>

namespace PvzMultiplayer
{
	WelcomeValidation ValidateWelcome(const Welcome& theWelcome,
		std::optional<uint64_t> theExpectedSessionId, uint32_t theExpectedRulesetId)
	{
		if (theWelcome.mSessionId == 0 ||
			(theExpectedSessionId && theWelcome.mSessionId != *theExpectedSessionId))
			return WelcomeValidation::WRONG_SESSION;
		if (theWelcome.mRulesetId != theExpectedRulesetId)
			return WelcomeValidation::WRONG_RULESET;
		if (theWelcome.mPlayerId >= theWelcome.mMaxPlayers || theWelcome.mMaxPlayers == 0 ||
			theWelcome.mMaxPlayers > MAX_PLAYERS || theWelcome.mCursorRgb > 0xFFFFFFU)
			return WelcomeValidation::INVALID_PLAYER;
		if (theWelcome.mTickRate == 0)
			return WelcomeValidation::INVALID_TICK_RATE;
		return WelcomeValidation::ACCEPTED;
	}

	bool HostLobby::Start(HostLobbyConfig theConfig)
	{
		Stop();
		if (theConfig.mSessionId == 0 || theConfig.mRulesetId == 0 || theConfig.mTickRate == 0 ||
			theConfig.mMaxPlayers == 0 || theConfig.mMaxPlayers > MAX_PLAYERS ||
			!IsValidDisplayName(theConfig.mHostName, MAX_PLAYER_NAME_LENGTH) ||
			!IsValidDisplayName(theConfig.mSessionName, MAX_SESSION_NAME_LENGTH))
			return false;

		mConfig = std::move(theConfig);
		mPlayers[0] = LobbyPlayer{0, GetPlayerCursorColor(0), 0, mConfig.mHostName};
		mRunning = true;
		return true;
	}

	void HostLobby::Stop()
	{
		mPlayers.fill(std::nullopt);
		mConfig = {};
		mRunning = false;
		mSessionStarted = false;
	}

	HandshakeResponse HostLobby::HandleHello(const Hello& theHello)
	{
		if (!mRunning)
			return MakeReject(RejectReason::INTERNAL_ERROR, "Host lobby is not running");
		if (theHello.mClientNonce == 0)
			return MakeReject(RejectReason::INVALID_REQUEST, "Client nonce must not be zero");
		if (theHello.mRulesetId != mConfig.mRulesetId)
			return MakeReject(RejectReason::RULESET_MISMATCH, "The host is using a different ruleset");
		if (!IsValidDisplayName(theHello.mPlayerName, MAX_PLAYER_NAME_LENGTH))
			return MakeReject(RejectReason::INVALID_NAME, "Player name is invalid");

		for (const auto& aPlayer : mPlayers)
		{
			if (aPlayer && aPlayer->mClientNonce == theHello.mClientNonce)
				return MakeWelcome(*aPlayer);
		}

		if (mSessionStarted)
			return MakeReject(RejectReason::SESSION_STARTED, "The game has already started");

		for (PlayerId aPlayerId = 1; aPlayerId < mConfig.mMaxPlayers; ++aPlayerId)
		{
			if (mPlayers[aPlayerId])
				continue;

			LobbyPlayer aPlayer{theHello.mClientNonce, GetPlayerCursorColor(aPlayerId), aPlayerId, theHello.mPlayerName};
			mPlayers[aPlayerId] = aPlayer;
			return MakeWelcome(aPlayer);
		}

		return MakeReject(RejectReason::SERVER_FULL, "The room is full");
	}

	bool HostLobby::RemovePlayer(PlayerId thePlayerId)
	{
		if (!mRunning || thePlayerId == 0 || thePlayerId >= mConfig.mMaxPlayers || !mPlayers[thePlayerId])
			return false;
		mPlayers[thePlayerId].reset();
		return true;
	}

	void HostLobby::SetSessionStarted(bool theStarted)
	{
		mSessionStarted = mRunning && theStarted;
	}

	bool HostLobby::IsRunning() const
	{
		return mRunning;
	}

	bool HostLobby::HasSessionStarted() const
	{
		return mSessionStarted;
	}

	uint8_t HostLobby::GetPlayerCount() const
	{
		return static_cast<uint8_t>(std::count_if(mPlayers.begin(), mPlayers.end(), [](const auto& thePlayer)
		{
			return thePlayer.has_value();
		}));
	}

	const HostLobbyConfig& HostLobby::GetConfig() const
	{
		return mConfig;
	}

	const std::array<std::optional<LobbyPlayer>, MAX_PLAYERS>& HostLobby::GetPlayers() const
	{
		return mPlayers;
	}

	std::array<std::string, MAX_PLAYERS> HostLobby::MakePlayerNameSnapshot() const
	{
		std::array<std::string, MAX_PLAYERS> aPlayerNames{};
		for (const auto& aPlayer : mPlayers)
		{
			if (aPlayer)
				aPlayerNames[aPlayer->mPlayerId] = aPlayer->mName;
		}
		return aPlayerNames;
	}

	std::optional<DiscoveryOffer> HostLobby::MakeDiscoveryOffer(uint16_t theGamePort) const
	{
		if (!mRunning || theGamePort == 0)
			return std::nullopt;
		return DiscoveryOffer{
			mConfig.mSessionId,
			theGamePort,
			GetPlayerCount(),
			mConfig.mMaxPlayers,
			mConfig.mRulesetId,
			mConfig.mSessionName
		};
	}

	Welcome HostLobby::MakeWelcome(const LobbyPlayer& thePlayer) const
	{
		return Welcome{
			mConfig.mSessionId,
			mConfig.mRulesetId,
			thePlayer.mCursorRgb,
			mConfig.mTickRate,
			thePlayer.mPlayerId,
			mConfig.mMaxPlayers
		};
	}

	Reject HostLobby::MakeReject(RejectReason theReason, std::string theMessage) const
	{
		return Reject{theReason, std::move(theMessage)};
	}
}
