/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "Protocol.h"

#include <array>
#include <optional>
#include <string>
#include <variant>

namespace PvzMultiplayer
{
	struct LobbyPlayer
	{
		uint64_t mClientNonce{};
		uint32_t mCursorRgb{};
		PlayerId mPlayerId{};
		std::string mName;

		bool operator==(const LobbyPlayer&) const = default;
	};

	struct HostLobbyConfig
	{
		uint64_t mSessionId{};
		uint32_t mRulesetId{};
		uint16_t mTickRate{100};
		uint8_t mMaxPlayers{MAX_PLAYERS};
		std::string mSessionName;
		std::string mHostName;
	};

	using HandshakeResponse = std::variant<Welcome, Reject>;

	enum class WelcomeValidation : uint8_t
	{
		ACCEPTED,
		WRONG_SESSION,
		WRONG_RULESET,
		INVALID_PLAYER,
		INVALID_TICK_RATE
	};

	WelcomeValidation ValidateWelcome(const Welcome& theWelcome, uint64_t theExpectedSessionId, uint32_t theExpectedRulesetId);

	class HostLobby
	{
	public:
		bool Start(HostLobbyConfig theConfig);
		void Stop();
		HandshakeResponse HandleHello(const Hello& theHello);
		bool RemovePlayer(PlayerId thePlayerId);
		void SetSessionStarted(bool theStarted);

		bool IsRunning() const;
		bool HasSessionStarted() const;
		uint8_t GetPlayerCount() const;
		const HostLobbyConfig& GetConfig() const;
		const std::array<std::optional<LobbyPlayer>, MAX_PLAYERS>& GetPlayers() const;
		std::array<std::string, MAX_PLAYERS> MakePlayerNameSnapshot() const;
		std::optional<DiscoveryOffer> MakeDiscoveryOffer(uint16_t theGamePort) const;

	private:
		Welcome MakeWelcome(const LobbyPlayer& thePlayer) const;
		Reject MakeReject(RejectReason theReason, std::string theMessage) const;

		HostLobbyConfig mConfig;
		std::array<std::optional<LobbyPlayer>, MAX_PLAYERS> mPlayers;
		bool mRunning{};
		bool mSessionStarted{};
	};
}
