/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "Lobby.h"
#include "ReliableChannel.h"

#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace PvzMultiplayer
{
	struct PlayerJoined
	{
		LobbyPlayer mPlayer;

		bool operator==(const PlayerJoined&) const = default;
	};

	struct PlayerLeft
	{
		PlayerId mPlayerId{};

		bool operator==(const PlayerLeft&) const = default;
	};

	using HostSessionEvent = std::variant<PlayerJoined, PlayerLeft, CursorUpdate, InputCommand, SessionReady>;

	class HostSession
	{
	public:
		bool Start(HostLobbyConfig theConfig, uint16_t thePort = 0);
		void Stop();
		void Poll();

		bool Broadcast(const Message& theMessage);
		bool SendTo(PlayerId thePlayerId, const Message& theMessage);
		void SetSessionStarted(bool theStarted);
		std::vector<HostSessionEvent> TakeEvents();

		bool IsRunning() const;
		uint16_t GetLocalPort() const;
		const HostLobby& GetLobby() const;
		const std::string& GetLastError() const;

	private:
		struct Peer
		{
			explicit Peer(TcpSocket theSocket) : mChannel(std::move(theSocket)) {}

			ReliableChannel mChannel;
			std::optional<PlayerId> mPlayerId;
			uint32_t mLastCursorSequence{};
			uint32_t mLastInputSequence{};
			bool mHasCursorSequence{};
			bool mHasInputSequence{};
			bool mCloseAfterFlush{};
		};

		void HandleMessage(Peer& thePeer, const Message& theMessage);
		void HandleHello(Peer& thePeer, const Hello& theHello);
		void RejectPeer(Peer& thePeer, RejectReason theReason, std::string theMessage);
		void RemovePeer(size_t theIndex);

		TcpListener mListener;
		HostLobby mLobby;
		std::vector<Peer> mPeers;
		std::vector<HostSessionEvent> mEvents;
		std::string mLastError;
	};
}
