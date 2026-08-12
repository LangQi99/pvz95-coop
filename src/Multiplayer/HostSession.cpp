/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "HostSession.h"
#include "SharedInputState.h"

#include <algorithm>
#include <utility>

namespace PvzMultiplayer
{
	namespace
	{
		constexpr size_t MAX_PENDING_PEERS = MAX_PLAYERS * 2;
	}

	bool HostSession::Start(HostLobbyConfig theConfig, uint16_t thePort)
	{
		Stop();
		if (!mLobby.Start(std::move(theConfig)))
		{
			mLastError = "invalid host lobby configuration";
			return false;
		}
		if (!mListener.Listen(thePort))
		{
			mLastError = mListener.GetLastError();
			mLobby.Stop();
			return false;
		}
		return true;
	}

	void HostSession::Stop()
	{
		for (auto& aPeer : mPeers)
			aPeer.mChannel.Close();
		mPeers.clear();
		mEvents.clear();
		mListener.Close();
		mLobby.Stop();
		mLastError.clear();
	}

	void HostSession::Poll()
	{
		if (!IsRunning())
			return;

		while (mPeers.size() < MAX_PENDING_PEERS)
		{
			auto aSocket = mListener.Accept();
			if (!aSocket)
				break;
			mPeers.emplace_back(std::move(*aSocket));
		}

		for (size_t anIndex = 0; anIndex < mPeers.size();)
		{
			Peer& aPeer = mPeers[anIndex];
			ReliableChannelState aState = aPeer.mChannel.Poll();
			if (aState == ReliableChannelState::FAILED && mLastError.empty())
				mLastError = aPeer.mChannel.GetLastError();
			if (aState == ReliableChannelState::CLOSED || aState == ReliableChannelState::FAILED)
			{
				RemovePeer(anIndex);
				continue;
			}

			for (const Message& aMessage : aPeer.mChannel.TakeMessages())
			{
				HandleMessage(aPeer, aMessage);
				if (aPeer.mCloseAfterFlush)
					break;
			}

			if (aPeer.mChannel.GetState() == ReliableChannelState::CONNECTED && aPeer.mChannel.GetQueuedByteCount() > 0)
				aPeer.mChannel.Poll();
			if (aPeer.mCloseAfterFlush && aPeer.mChannel.GetQueuedByteCount() == 0)
				aPeer.mChannel.Close();

			if (aPeer.mChannel.GetState() == ReliableChannelState::CLOSED ||
				aPeer.mChannel.GetState() == ReliableChannelState::FAILED)
			{
				RemovePeer(anIndex);
				continue;
			}
			++anIndex;
		}
	}

	bool HostSession::Broadcast(const Message& theMessage)
	{
		bool aQueued = true;
		for (auto& aPeer : mPeers)
		{
			if (aPeer.mPlayerId && !aPeer.mCloseAfterFlush)
				aQueued = aPeer.mChannel.Queue(theMessage) && aQueued;
		}
		return aQueued;
	}

	bool HostSession::SendTo(PlayerId thePlayerId, const Message& theMessage)
	{
		for (auto& aPeer : mPeers)
		{
			if (aPeer.mPlayerId == thePlayerId && !aPeer.mCloseAfterFlush)
				return aPeer.mChannel.Queue(theMessage);
		}
		return false;
	}

	void HostSession::SetSessionStarted(bool theStarted)
	{
		mLobby.SetSessionStarted(theStarted);
	}

	std::vector<HostSessionEvent> HostSession::TakeEvents()
	{
		std::vector<HostSessionEvent> anEvents;
		anEvents.swap(mEvents);
		return anEvents;
	}

	bool HostSession::IsRunning() const
	{
		return mListener.IsListening() && mLobby.IsRunning();
	}

	uint16_t HostSession::GetLocalPort() const
	{
		return mListener.GetLocalPort();
	}

	const HostLobby& HostSession::GetLobby() const
	{
		return mLobby;
	}

	const std::string& HostSession::GetLastError() const
	{
		return mLastError;
	}

	void HostSession::HandleMessage(Peer& thePeer, const Message& theMessage)
	{
		if (!thePeer.mPlayerId)
		{
			if (const auto* aHello = std::get_if<Hello>(&theMessage))
				HandleHello(thePeer, *aHello);
			else
				RejectPeer(thePeer, RejectReason::INVALID_REQUEST, "The first message must be Hello");
			return;
		}

		if (const auto* aCursor = std::get_if<CursorUpdate>(&theMessage))
		{
			if (aCursor->mPlayerId != *thePeer.mPlayerId || (aCursor->mButtons & 0xE0U) != 0)
			{
				RejectPeer(thePeer, RejectReason::INVALID_REQUEST, "Invalid cursor update");
				return;
			}
			if (thePeer.mHasCursorSequence && !IsSequenceNewer(aCursor->mSequence, thePeer.mLastCursorSequence))
				return;
			thePeer.mLastCursorSequence = aCursor->mSequence;
			thePeer.mHasCursorSequence = true;
			mEvents.emplace_back(*aCursor);
			return;
		}

		if (const auto* anInput = std::get_if<InputCommand>(&theMessage))
		{
			if (anInput->mPlayerId != *thePeer.mPlayerId)
			{
				RejectPeer(thePeer, RejectReason::INVALID_REQUEST, "Invalid input command");
				return;
			}
			if (thePeer.mHasInputSequence && !IsSequenceNewer(anInput->mSequence, thePeer.mLastInputSequence))
				return;
			thePeer.mLastInputSequence = anInput->mSequence;
			thePeer.mHasInputSequence = true;
			mEvents.emplace_back(*anInput);
			return;
		}

		if (const auto* aReady = std::get_if<SessionReady>(&theMessage))
		{
			if (aReady->mPlayerId != *thePeer.mPlayerId)
			{
				RejectPeer(thePeer, RejectReason::INVALID_REQUEST, "Invalid session-ready response");
				return;
			}
			mEvents.emplace_back(*aReady);
			return;
		}

		RejectPeer(thePeer, RejectReason::INVALID_REQUEST, "Message is not valid for a connected client");
	}

	void HostSession::HandleHello(Peer& thePeer, const Hello& theHello)
	{
		HandshakeResponse aResponse = mLobby.HandleHello(theHello);
		if (const auto* aReject = std::get_if<Reject>(&aResponse))
		{
			thePeer.mChannel.Queue(*aReject);
			thePeer.mCloseAfterFlush = true;
			return;
		}

		const Welcome& aWelcome = std::get<Welcome>(aResponse);
		for (const auto& anExistingPeer : mPeers)
		{
			if (&anExistingPeer != &thePeer && anExistingPeer.mPlayerId == aWelcome.mPlayerId &&
				!anExistingPeer.mCloseAfterFlush)
			{
				RejectPeer(thePeer, RejectReason::INVALID_REQUEST, "This player is already connected");
				return;
			}
		}

		if (!thePeer.mChannel.Queue(aWelcome))
			return;
		thePeer.mPlayerId = aWelcome.mPlayerId;
		const auto& aPlayer = mLobby.GetPlayers()[aWelcome.mPlayerId];
		if (aPlayer)
			mEvents.emplace_back(PlayerJoined{*aPlayer});
	}

	void HostSession::RejectPeer(Peer& thePeer, RejectReason theReason, std::string theMessage)
	{
		thePeer.mChannel.Queue(Reject{theReason, std::move(theMessage)});
		thePeer.mCloseAfterFlush = true;
	}

	void HostSession::RemovePeer(size_t theIndex)
	{
		if (mPeers[theIndex].mPlayerId)
		{
			PlayerId aPlayerId = *mPeers[theIndex].mPlayerId;
			mLobby.RemovePlayer(aPlayerId);
			mEvents.emplace_back(PlayerLeft{aPlayerId});
		}
		mPeers.erase(mPeers.begin() + static_cast<ptrdiff_t>(theIndex));
	}
}
