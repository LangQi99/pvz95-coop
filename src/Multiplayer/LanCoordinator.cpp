/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "LanCoordinator.h"

#include <atomic>
#include <iterator>
#include <utility>

namespace PvzMultiplayer
{
	namespace
	{
		constexpr auto DIRECT_JOIN_TIMEOUT = std::chrono::seconds(10);

		std::string DescribeRejection(const Reject& theReject)
		{
			switch (theReject.mReason)
			{
			case RejectReason::PROTOCOL_MISMATCH:
				return "Game version mismatch. Every player must use the same PvZ 95 Co-op release.";
			case RejectReason::RULESET_MISMATCH:
				return "Ruleset mismatch. Every player must select the same ruleset and game release.";
			default:
				return theReject.mMessage.empty() ? "The host rejected the connection" : theReject.mMessage;
			}
		}

		std::string DescribeHandshakeFailure(const ClientSession& theSession)
		{
			const std::string& anError = theSession.GetLastError();
			if (anError.find("unsupported version") != std::string::npos)
				return "Game version mismatch. Every player must use the same PvZ 95 Co-op release.";
			if (!theSession.GetWelcome())
			{
				std::string aPrefix = anError.empty() ? "The connection failed during the handshake." : anError;
				return aPrefix + " Check that every player uses the same game release, then verify the address, port, and firewall.";
			}
			return anError;
		}
	}

	bool LanCoordinator::StartHosting(std::string theSessionName, std::string theHostName, uint32_t theRulesetId,
		uint16_t theGamePort)
	{
		Stop();
		HostLobbyConfig aConfig{
			GenerateId(),
			theRulesetId,
			100,
			MAX_PLAYERS,
			std::move(theSessionName),
			std::move(theHostName)
		};
		if (!mHostSession.Start(aConfig, theGamePort))
		{
			SetError(mHostSession.GetLastError());
			return false;
		}

		auto anOffer = mHostSession.GetLobby().MakeDiscoveryOffer(mHostSession.GetLocalPort());
		if (!anOffer || !mDiscoveryHost.Start(*anOffer))
		{
			std::string anError = mDiscoveryHost.GetLastError();
			mHostSession.Stop();
			SetError(anError.empty() ? "LAN discovery port is unavailable" : std::move(anError));
			return false;
		}

		mMode = LanMode::HOSTING;
		mRulesetId = theRulesetId;
		UpdateHostStatus();
		return true;
	}

	bool LanCoordinator::StartDirectJoining(std::string thePlayerName, uint32_t theRulesetId,
		const Ipv4Endpoint& theGameEndpoint)
	{
		Stop();
		mClientNonce = GenerateId();
		ClientSessionConfig aConfig{
			theGameEndpoint,
			std::nullopt,
			mClientNonce,
			theRulesetId,
			0,
			thePlayerName
		};
		if (!mClientSession.Start(std::move(aConfig)))
		{
			SetError(mClientSession.GetLastError());
			return false;
		}

		mPlayerName = std::move(thePlayerName);
		mRulesetId = theRulesetId;
		mDirectJoinDeadline = std::chrono::steady_clock::now() + DIRECT_JOIN_TIMEOUT;
		mMode = LanMode::JOINING;
		mStatusText = "Connecting to " + theGameEndpoint.AddressString() + ":" +
			std::to_string(theGameEndpoint.mPort) + "...";
		return true;
	}

	bool LanCoordinator::StartJoining(std::string thePlayerName, uint32_t theRulesetId,
		std::optional<Ipv4Endpoint> theDiscoveryEndpoint)
	{
		Stop();
		mClientNonce = GenerateId();
		mDiscoveryEndpoint = theDiscoveryEndpoint;
		bool aQuerySent = mDiscoveryClient.Start(mClientNonce) &&
			(mDiscoveryEndpoint ? mDiscoveryClient.SendQueryTo(*mDiscoveryEndpoint) : mDiscoveryClient.SendBroadcastQuery());
		if (!aQuerySent)
		{
			SetError(mDiscoveryClient.GetLastError().empty() ? "Could not send a LAN discovery query" :
				mDiscoveryClient.GetLastError());
			return false;
		}

		mPlayerName = std::move(thePlayerName);
		mRulesetId = theRulesetId;
		mLastDiscoveryQuery = std::chrono::steady_clock::now();
		mMode = LanMode::SEARCHING;
		mStatusText = "Searching for a PvZ 95 LAN room...";
		return true;
	}

	void LanCoordinator::Stop()
	{
		mDiscoveryHost.Stop();
		mDiscoveryClient.Stop();
		mHostSession.Stop();
		mClientSession.Stop();
		mHostEvents.clear();
		mClientMessages.clear();
		mMode = LanMode::OFFLINE;
		mClientNonce = 0;
		mRulesetId = 0;
		mDiscoveryEndpoint.reset();
		mDirectJoinDeadline.reset();
		mPlayerName.clear();
		mStatusText = "Offline";
	}

	void LanCoordinator::Poll()
	{
		if (mMode == LanMode::HOSTING)
		{
			mHostSession.Poll();
			auto anEvents = mHostSession.TakeEvents();
			mHostEvents.insert(mHostEvents.end(),
				std::make_move_iterator(anEvents.begin()), std::make_move_iterator(anEvents.end()));
			auto anOffer = mHostSession.GetLobby().MakeDiscoveryOffer(mHostSession.GetLocalPort());
			if (anOffer)
				mDiscoveryHost.SetOffer(*anOffer);
			mDiscoveryHost.Poll();
			UpdateHostStatus();
			return;
		}

		if (mMode == LanMode::SEARCHING)
		{
			for (const auto& aSession : mDiscoveryClient.Poll())
			{
				if (aSession.mOffer.mRulesetId != mRulesetId)
				{
					mStatusText = "Found an incompatible LAN room. Use the same game version and ruleset.";
					continue;
				}
				if (aSession.mOffer.mPlayerCount >= aSession.mOffer.mMaxPlayers)
				{
					mStatusText = "Found a compatible LAN room, but it is full.";
					continue;
				}

				ClientSessionConfig aConfig{
					aSession.mGameEndpoint,
					aSession.mOffer.mSessionId,
					mClientNonce,
					mRulesetId,
					0,
					mPlayerName
				};
				mDiscoveryClient.Stop();
				if (!mClientSession.Start(std::move(aConfig)))
				{
					SetError(mClientSession.GetLastError());
					return;
				}
				mMode = LanMode::JOINING;
				mStatusText = "Joining \"" + aSession.mOffer.mSessionName + "\"...";
				break;
			}

			if (mMode == LanMode::SEARCHING &&
				std::chrono::steady_clock::now() - mLastDiscoveryQuery >= std::chrono::seconds(1))
			{
				bool aQuerySent = mDiscoveryEndpoint ? mDiscoveryClient.SendQueryTo(*mDiscoveryEndpoint) :
					mDiscoveryClient.SendBroadcastQuery();
				if (!aQuerySent)
				{
					SetError(mDiscoveryClient.GetLastError());
					return;
				}
				mLastDiscoveryQuery = std::chrono::steady_clock::now();
			}
		}

		if (mMode == LanMode::JOINING || mMode == LanMode::CONNECTED)
		{
			mClientSession.Poll();
			auto aMessages = mClientSession.TakeMessages();
			mClientMessages.insert(mClientMessages.end(),
				std::make_move_iterator(aMessages.begin()), std::make_move_iterator(aMessages.end()));

			switch (mClientSession.GetState())
			{
			case ClientSessionState::CONNECTED:
				mMode = LanMode::CONNECTED;
				mDirectJoinDeadline.reset();
				mStatusText = "Connected to LAN room as player " +
					std::to_string(static_cast<unsigned int>(mClientSession.GetWelcome()->mPlayerId) + 1);
				break;
			case ClientSessionState::REJECTED:
				SetError(mClientSession.GetReject() ? DescribeRejection(*mClientSession.GetReject()) :
					"The host rejected the connection");
				break;
			case ClientSessionState::CLOSED:
				SetError(mClientSession.GetWelcome() ? "The host closed the connection" :
					"The host closed the connection during the handshake. The game versions may not match; every player must use the same release.");
				break;
			case ClientSessionState::FAILED:
				SetError(DescribeHandshakeFailure(mClientSession));
				break;
			default:
				break;
			}

			if (mMode == LanMode::JOINING && mDirectJoinDeadline &&
				std::chrono::steady_clock::now() >= *mDirectJoinDeadline)
			{
				SetError("The direct connection timed out. Check the host address or domain, game port, firewall, and that everyone uses the same release.");
				return;
			}
		}
	}

	bool LanCoordinator::SendCursor(CursorUpdate theCursor)
	{
		return mMode == LanMode::CONNECTED && mClientSession.SendCursor(theCursor);
	}

	bool LanCoordinator::SendAction(GameAction theAction)
	{
		return mMode == LanMode::CONNECTED && mClientSession.SendAction(theAction);
	}

	bool LanCoordinator::SendReady(SessionReady theReady)
	{
		return mMode == LanMode::CONNECTED && mClientSession.SendReady(theReady);
	}

	bool LanCoordinator::BroadcastFromHost(const Message& theMessage)
	{
		return mMode == LanMode::HOSTING && mHostSession.Broadcast(theMessage);
	}

	void LanCoordinator::SetSessionStarted(bool theStarted)
	{
		if (mMode == LanMode::HOSTING)
			mHostSession.SetSessionStarted(theStarted);
	}

	void LanCoordinator::AbortWithError(std::string theError)
	{
		SetError(std::move(theError));
	}

	LanMode LanCoordinator::GetMode() const
	{
		return mMode;
	}

	const std::string& LanCoordinator::GetStatusText() const
	{
		return mStatusText;
	}

	const HostSession& LanCoordinator::GetHostSession() const
	{
		return mHostSession;
	}

	const ClientSession& LanCoordinator::GetClientSession() const
	{
		return mClientSession;
	}

	std::vector<HostSessionEvent> LanCoordinator::TakeHostEvents()
	{
		std::vector<HostSessionEvent> anEvents;
		anEvents.swap(mHostEvents);
		return anEvents;
	}

	std::vector<Message> LanCoordinator::TakeClientMessages()
	{
		std::vector<Message> aMessages;
		aMessages.swap(mClientMessages);
		return aMessages;
	}

	uint64_t LanCoordinator::GenerateId()
	{
		static std::atomic<uint64_t> aCounter{1};
		uint64_t aTime = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
		uint64_t anId = aTime ^ (aCounter.fetch_add(1) * 0x9E3779B97F4A7C15ULL);
		return anId == 0 ? 1 : anId;
	}

	void LanCoordinator::SetError(std::string theError)
	{
		mDiscoveryHost.Stop();
		mDiscoveryClient.Stop();
		mHostSession.Stop();
		mClientSession.Stop();
		mDirectJoinDeadline.reset();
		mMode = LanMode::FAILED;
		mStatusText = "LAN error: " + (theError.empty() ? std::string("unknown error") : std::move(theError));
	}

	void LanCoordinator::UpdateHostStatus()
	{
		const HostLobby& aLobby = mHostSession.GetLobby();
		mStatusText = "Hosting \"" + aLobby.GetConfig().mSessionName + "\" (" +
			std::to_string(aLobby.GetPlayerCount()) + "/" + std::to_string(aLobby.GetConfig().mMaxPlayers) +
			", TCP port " + std::to_string(mHostSession.GetLocalPort()) + ")";
	}
}
