/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "Lobby.h"
#include "ReliableChannel.h"

#include <optional>
#include <string>
#include <vector>

namespace PvzMultiplayer
{
	enum class ClientSessionState : uint8_t
	{
		IDLE,
		CONNECTING,
		HANDSHAKING,
		CONNECTED,
		REJECTED,
		CLOSED,
		FAILED
	};

	struct ClientSessionConfig
	{
		Ipv4Endpoint mEndpoint;
		uint64_t mSessionId{};
		uint64_t mClientNonce{};
		uint32_t mRulesetId{};
		uint32_t mCapabilities{};
		std::string mPlayerName;
	};

	class ClientSession
	{
	public:
		bool Start(ClientSessionConfig theConfig);
		void Stop();
		void Poll();

		bool SendCursor(CursorUpdate theCursor);
		bool SendAction(GameAction theAction);
		bool SendReady(SessionReady theReady);
		std::vector<Message> TakeMessages();

		ClientSessionState GetState() const;
		const std::optional<Welcome>& GetWelcome() const;
		const std::optional<Reject>& GetReject() const;
		const std::string& GetLastError() const;

	private:
		void Fail(std::string theError);

		ClientSessionConfig mConfig;
		std::optional<ReliableChannel> mChannel;
		std::optional<Welcome> mWelcome;
		std::optional<Reject> mReject;
		std::vector<Message> mMessages;
		ClientSessionState mState{ClientSessionState::IDLE};
		uint32_t mLastCursorSequence{};
		uint32_t mLastActionSequence{};
		bool mHasCursorSequence{};
		bool mHasActionSequence{};
		std::string mLastError;
	};
}
