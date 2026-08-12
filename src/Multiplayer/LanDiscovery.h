/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "Protocol.h"
#include "UdpSocket.h"

#include <vector>

namespace PvzMultiplayer
{
	struct DiscoveredSession
	{
		DiscoveryOffer mOffer;
		Ipv4Endpoint mGameEndpoint;

		bool operator==(const DiscoveredSession&) const = default;
	};

	class LanDiscoveryHost
	{
	public:
		bool Start(DiscoveryOffer theOffer, uint16_t theDiscoveryPort = DEFAULT_DISCOVERY_PORT);
		void Stop();
		size_t Poll(size_t theMaxDatagrams = 32);

		bool IsRunning() const;
		uint16_t GetDiscoveryPort() const;
		const std::string& GetLastError() const;

	private:
		UdpSocket mSocket;
		DiscoveryOffer mOffer;
	};

	class LanDiscoveryClient
	{
	public:
		bool Start(uint64_t theClientNonce);
		void Stop();
		bool SendBroadcastQuery(uint16_t theDiscoveryPort = DEFAULT_DISCOVERY_PORT);
		bool SendQueryTo(const Ipv4Endpoint& theEndpoint);
		std::vector<DiscoveredSession> Poll(size_t theMaxDatagrams = 32);

		bool IsRunning() const;
		const std::string& GetLastError() const;

	private:
		UdpSocket mSocket;
		DiscoveryQuery mQuery;
	};
}
