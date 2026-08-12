/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Lobby.h"

#include <algorithm>
#include <utility>

namespace PvzMultiplayer
{
	namespace
	{
		constexpr std::array<uint32_t, MAX_PLAYERS> CURSOR_COLORS = {
			0xEF5350, // red
			0x42A5F5, // blue
			0x66BB6A, // green
			0xAB47BC  // purple
		};

		bool IsContinuationByte(uint8_t theByte)
		{
			return (theByte & 0xC0U) == 0x80U;
		}

		bool IsValidUtf8(std::string_view theText)
		{
			for (size_t anIndex = 0; anIndex < theText.size();)
			{
				uint8_t aLead = static_cast<uint8_t>(theText[anIndex]);
				if (aLead <= 0x7F)
				{
					++anIndex;
					continue;
				}

				size_t aContinuationCount;
				uint32_t aCodePoint;
				if ((aLead & 0xE0U) == 0xC0U)
				{
					aContinuationCount = 1;
					aCodePoint = aLead & 0x1FU;
				}
				else if ((aLead & 0xF0U) == 0xE0U)
				{
					aContinuationCount = 2;
					aCodePoint = aLead & 0x0FU;
				}
				else if ((aLead & 0xF8U) == 0xF0U)
				{
					aContinuationCount = 3;
					aCodePoint = aLead & 0x07U;
				}
				else
				{
					return false;
				}

				if (anIndex + aContinuationCount >= theText.size())
					return false;
				for (size_t aContinuationIndex = 1; aContinuationIndex <= aContinuationCount; ++aContinuationIndex)
				{
					uint8_t aByte = static_cast<uint8_t>(theText[anIndex + aContinuationIndex]);
					if (!IsContinuationByte(aByte))
						return false;
					aCodePoint = (aCodePoint << 6) | (aByte & 0x3FU);
				}

				if ((aContinuationCount == 1 && aCodePoint < 0x80) ||
					(aContinuationCount == 2 && aCodePoint < 0x800) ||
					(aContinuationCount == 3 && aCodePoint < 0x10000) ||
					aCodePoint > 0x10FFFF || (aCodePoint >= 0xD800 && aCodePoint <= 0xDFFF))
					return false;

				anIndex += aContinuationCount + 1;
			}
			return true;
		}
	}

	bool IsValidDisplayName(std::string_view theName, size_t theMaxBytes)
	{
		if (theName.empty() || theName.size() > theMaxBytes || !IsValidUtf8(theName))
			return false;

		bool aHasNonSpace = false;
		for (unsigned char aByte : theName)
		{
			if (aByte < 0x20 || aByte == 0x7F)
				return false;
			if (aByte != ' ')
				aHasNonSpace = true;
		}
		return aHasNonSpace;
	}

	WelcomeValidation ValidateWelcome(const Welcome& theWelcome, uint64_t theExpectedSessionId, uint32_t theExpectedRulesetId)
	{
		if (theWelcome.mSessionId != theExpectedSessionId)
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
		mPlayers[0] = LobbyPlayer{0, CURSOR_COLORS[0], 0, mConfig.mHostName};
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

			LobbyPlayer aPlayer{theHello.mClientNonce, CURSOR_COLORS[aPlayerId], aPlayerId, theHello.mPlayerName};
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
