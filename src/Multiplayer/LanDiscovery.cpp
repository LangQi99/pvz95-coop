/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "LanDiscovery.h"

#include <utility>

namespace PvzMultiplayer
{
	bool LanDiscoveryHost::Start(DiscoveryOffer theOffer, uint16_t theDiscoveryPort)
	{
		Stop();
		if (!Encode(Message(theOffer)) || !mSocket.Bind(theDiscoveryPort))
			return false;

		mOffer = std::move(theOffer);
		return true;
	}

	bool LanDiscoveryHost::SetOffer(DiscoveryOffer theOffer)
	{
		if (!IsRunning() || !Encode(Message(theOffer)))
			return false;
		mOffer = std::move(theOffer);
		return true;
	}

	void LanDiscoveryHost::Stop()
	{
		mSocket.Close();
	}

	size_t LanDiscoveryHost::Poll(size_t theMaxDatagrams)
	{
		size_t aReplyCount = 0;
		for (size_t aDatagramIndex = 0; aDatagramIndex < theMaxDatagrams; ++aDatagramIndex)
		{
			auto aDatagram = mSocket.Receive();
			if (!aDatagram)
				break;

			auto aDecoded = Decode(aDatagram->mPayload);
			if (!aDecoded || !std::holds_alternative<DiscoveryQuery>(*aDecoded.mMessage))
				continue;

			auto anOfferPacket = Encode(Message(mOffer));
			if (anOfferPacket && mSocket.SendTo(aDatagram->mSource, *anOfferPacket))
				++aReplyCount;
		}
		return aReplyCount;
	}

	bool LanDiscoveryHost::IsRunning() const
	{
		return mSocket.IsOpen();
	}

	uint16_t LanDiscoveryHost::GetDiscoveryPort() const
	{
		return mSocket.GetLocalPort();
	}

	const std::string& LanDiscoveryHost::GetLastError() const
	{
		return mSocket.GetLastError();
	}

	bool LanDiscoveryClient::Start(uint64_t theClientNonce)
	{
		Stop();
		if (!mSocket.Bind(0))
			return false;
		if (!mSocket.SetBroadcastEnabled(true))
		{
			mSocket.Close();
			return false;
		}

		mQuery.mClientNonce = theClientNonce;
		return true;
	}

	void LanDiscoveryClient::Stop()
	{
		mSocket.Close();
	}

	bool LanDiscoveryClient::SendBroadcastQuery(uint16_t theDiscoveryPort)
	{
		return SendQueryTo(Ipv4Endpoint::Broadcast(theDiscoveryPort));
	}

	bool LanDiscoveryClient::SendQueryTo(const Ipv4Endpoint& theEndpoint)
	{
		auto aPacket = Encode(Message(mQuery));
		return aPacket && mSocket.SendTo(theEndpoint, *aPacket);
	}

	std::vector<DiscoveredSession> LanDiscoveryClient::Poll(size_t theMaxDatagrams)
	{
		std::vector<DiscoveredSession> aSessions;
		for (size_t aDatagramIndex = 0; aDatagramIndex < theMaxDatagrams; ++aDatagramIndex)
		{
			auto aDatagram = mSocket.Receive();
			if (!aDatagram)
				break;

			auto aDecoded = Decode(aDatagram->mPayload);
			if (!aDecoded || !std::holds_alternative<DiscoveryOffer>(*aDecoded.mMessage))
				continue;

			DiscoveredSession aSession;
			aSession.mOffer = std::get<DiscoveryOffer>(std::move(*aDecoded.mMessage));
			aSession.mGameEndpoint = aDatagram->mSource;
			aSession.mGameEndpoint.mPort = aSession.mOffer.mGamePort;
			aSessions.push_back(std::move(aSession));
		}
		return aSessions;
	}

	bool LanDiscoveryClient::IsRunning() const
	{
		return mSocket.IsOpen();
	}

	const std::string& LanDiscoveryClient::GetLastError() const
	{
		return mSocket.GetLastError();
	}
}
