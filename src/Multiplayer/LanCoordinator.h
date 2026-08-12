/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "ClientSession.h"
#include "HostSession.h"
#include "LanDiscovery.h"

#include <chrono>
#include <optional>

namespace PvzMultiplayer
{
	enum class LanMode : uint8_t
	{
		OFFLINE,
		HOSTING,
		SEARCHING,
		JOINING,
		CONNECTED,
		FAILED
	};

	constexpr bool IsLanClientWaitingForHost(LanMode theMode)
	{
		return theMode == LanMode::SEARCHING || theMode == LanMode::JOINING ||
			theMode == LanMode::CONNECTED;
	}

	class LanCoordinator
	{
	public:
		bool StartHosting(std::string theSessionName, std::string theHostName, uint32_t theRulesetId);
		bool StartJoining(std::string thePlayerName, uint32_t theRulesetId,
			std::optional<Ipv4Endpoint> theDiscoveryEndpoint = std::nullopt);
		void Stop();
		void Poll();
		bool SendCursor(CursorUpdate theCursor);
		bool SendAction(GameAction theAction);
		bool SendReady(SessionReady theReady);
		bool BroadcastFromHost(const Message& theMessage);
		void SetSessionStarted(bool theStarted);

		LanMode GetMode() const;
		const std::string& GetStatusText() const;
		const HostSession& GetHostSession() const;
		const ClientSession& GetClientSession() const;
		std::vector<HostSessionEvent> TakeHostEvents();
		std::vector<Message> TakeClientMessages();

	private:
		static uint64_t GenerateId();
		void SetError(std::string theError);
		void UpdateHostStatus();

		HostSession mHostSession;
		ClientSession mClientSession;
		LanDiscoveryHost mDiscoveryHost;
		LanDiscoveryClient mDiscoveryClient;
		std::vector<HostSessionEvent> mHostEvents;
		std::vector<Message> mClientMessages;
		LanMode mMode{LanMode::OFFLINE};
		uint64_t mClientNonce{};
		uint32_t mRulesetId{};
		std::optional<Ipv4Endpoint> mDiscoveryEndpoint;
		std::string mPlayerName;
		std::string mStatusText{"Offline"};
		std::chrono::steady_clock::time_point mLastDiscoveryQuery;
	};
}
