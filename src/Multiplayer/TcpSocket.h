/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "UdpSocket.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace PvzMultiplayer
{
	enum class ConnectionState : uint8_t
	{
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
		FAILED
	};

	enum class SocketIoStatus : uint8_t
	{
		COMPLETED,
		WOULD_BLOCK,
		CLOSED,
		ERROR
	};

	struct SocketIoResult
	{
		SocketIoStatus mStatus{SocketIoStatus::ERROR};
		size_t mByteCount{};
	};

	class TcpSocket
	{
	public:
		TcpSocket() = default;
		~TcpSocket();

		TcpSocket(const TcpSocket&) = delete;
		TcpSocket& operator=(const TcpSocket&) = delete;
		TcpSocket(TcpSocket&& theOther) noexcept;
		TcpSocket& operator=(TcpSocket&& theOther) noexcept;

		bool StartConnect(const Ipv4Endpoint& theEndpoint);
		ConnectionState PollConnect();
		SocketIoResult Send(std::span<const uint8_t> theBytes);
		SocketIoResult Receive(std::span<uint8_t> theBuffer);
		void Close();

		ConnectionState GetState() const;
		const Ipv4Endpoint& GetPeerEndpoint() const;
		const std::string& GetLastError() const;

	private:
		friend class TcpListener;
		bool Adopt(intptr_t theHandle, const Ipv4Endpoint& thePeerEndpoint);
		void SetError(std::string theOperation);

		intptr_t mHandle{-1};
		bool mRuntimeAcquired{};
		ConnectionState mState{ConnectionState::DISCONNECTED};
		Ipv4Endpoint mPeerEndpoint;
		std::string mLastError;
	};

	class TcpListener
	{
	public:
		TcpListener() = default;
		~TcpListener();

		TcpListener(const TcpListener&) = delete;
		TcpListener& operator=(const TcpListener&) = delete;
		TcpListener(TcpListener&& theOther) noexcept;
		TcpListener& operator=(TcpListener&& theOther) noexcept;

		bool Listen(uint16_t thePort, int theBacklog = 4);
		std::optional<TcpSocket> Accept();
		void Close();

		bool IsListening() const;
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
