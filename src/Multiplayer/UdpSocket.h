/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PvzMultiplayer
{
	struct Ipv4Endpoint
	{
		std::array<uint8_t, 4> mAddress{};
		uint16_t mPort{};

		static Ipv4Endpoint Any(uint16_t thePort);
		static Ipv4Endpoint Loopback(uint16_t thePort);
		static Ipv4Endpoint Broadcast(uint16_t thePort);
		static std::optional<Ipv4Endpoint> Parse(std::string_view theAddress, uint16_t thePort);
		std::string AddressString() const;

		bool operator==(const Ipv4Endpoint&) const = default;
	};

	struct UdpDatagram
	{
		Ipv4Endpoint mSource;
		std::vector<uint8_t> mPayload;
	};

	class UdpSocket
	{
	public:
		UdpSocket() = default;
		~UdpSocket();

		UdpSocket(const UdpSocket&) = delete;
		UdpSocket& operator=(const UdpSocket&) = delete;
		UdpSocket(UdpSocket&& theOther) noexcept;
		UdpSocket& operator=(UdpSocket&& theOther) noexcept;

		bool Open();
		bool Bind(uint16_t thePort);
		bool SetBroadcastEnabled(bool theEnabled);
		bool SendTo(const Ipv4Endpoint& theEndpoint, std::span<const uint8_t> thePayload);
		std::optional<UdpDatagram> Receive();
		void Close();

		bool IsOpen() const;
		uint16_t GetLocalPort() const;
		const std::string& GetLastError() const;

	private:
		void SetError(std::string theOperation);

		intptr_t mHandle{-1};
		uint16_t mLocalPort{};
		bool mRuntimeAcquired{};
		std::string mLastError;
	};
}
