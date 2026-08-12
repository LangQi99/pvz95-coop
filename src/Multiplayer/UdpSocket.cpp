/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "UdpSocket.h"

#include "Protocol.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace PvzMultiplayer
{
	namespace
	{
#ifdef _WIN32
		std::mutex gWinsockMutex;
		unsigned int gWinsockUsers{};

		bool AcquireSocketRuntime()
		{
			std::scoped_lock aLock(gWinsockMutex);
			if (gWinsockUsers == 0)
			{
				WSADATA aData;
				if (WSAStartup(MAKEWORD(2, 2), &aData) != 0)
					return false;
			}
			++gWinsockUsers;
			return true;
		}

		void ReleaseSocketRuntime()
		{
			std::scoped_lock aLock(gWinsockMutex);
			if (gWinsockUsers > 0 && --gWinsockUsers == 0)
				WSACleanup();
		}

		int LastSocketError()
		{
			return WSAGetLastError();
		}

		bool WouldBlock(int theError)
		{
			return theError == WSAEWOULDBLOCK;
		}
#else
		bool AcquireSocketRuntime() { return true; }
		void ReleaseSocketRuntime() {}
		int LastSocketError() { return errno; }
		bool WouldBlock(int theError) { return theError == EAGAIN || theError == EWOULDBLOCK; }
#endif

		std::string SocketErrorString(const std::string& theOperation, int theError)
		{
#ifdef _WIN32
			return theOperation + " failed with Winsock error " + std::to_string(theError);
#else
			return theOperation + " failed: " + std::strerror(theError);
#endif
		}

		sockaddr_in ToSockAddr(const Ipv4Endpoint& theEndpoint)
		{
			sockaddr_in anAddress{};
			anAddress.sin_family = AF_INET;
			anAddress.sin_port = htons(theEndpoint.mPort);
			std::memcpy(&anAddress.sin_addr.s_addr, theEndpoint.mAddress.data(), theEndpoint.mAddress.size());
			return anAddress;
		}

		Ipv4Endpoint FromSockAddr(const sockaddr_in& theAddress)
		{
			Ipv4Endpoint anEndpoint;
			std::memcpy(anEndpoint.mAddress.data(), &theAddress.sin_addr.s_addr, anEndpoint.mAddress.size());
			anEndpoint.mPort = ntohs(theAddress.sin_port);
			return anEndpoint;
		}
	}

	Ipv4Endpoint Ipv4Endpoint::Any(uint16_t thePort)
	{
		return {{{0, 0, 0, 0}}, thePort};
	}

	Ipv4Endpoint Ipv4Endpoint::Loopback(uint16_t thePort)
	{
		return {{{127, 0, 0, 1}}, thePort};
	}

	Ipv4Endpoint Ipv4Endpoint::Broadcast(uint16_t thePort)
	{
		return {{{255, 255, 255, 255}}, thePort};
	}

	std::optional<Ipv4Endpoint> Ipv4Endpoint::Parse(std::string_view theAddress, uint16_t thePort)
	{
		if (theAddress.empty() || theAddress.find('\0') != std::string_view::npos)
			return std::nullopt;

		std::string anAddressString(theAddress);
		in_addr anAddress{};
		if (inet_pton(AF_INET, anAddressString.c_str(), &anAddress) != 1)
			return std::nullopt;

		Ipv4Endpoint anEndpoint;
		std::memcpy(anEndpoint.mAddress.data(), &anAddress.s_addr, anEndpoint.mAddress.size());
		anEndpoint.mPort = thePort;
		return anEndpoint;
	}

	std::string Ipv4Endpoint::AddressString() const
	{
		in_addr anAddress{};
		std::memcpy(&anAddress.s_addr, mAddress.data(), mAddress.size());
		std::array<char, INET_ADDRSTRLEN> aBuffer{};
		if (inet_ntop(AF_INET, &anAddress, aBuffer.data(), aBuffer.size()) == nullptr)
			return {};
		return aBuffer.data();
	}

	UdpSocket::~UdpSocket()
	{
		Close();
	}

	UdpSocket::UdpSocket(UdpSocket&& theOther) noexcept
	{
		*this = std::move(theOther);
	}

	UdpSocket& UdpSocket::operator=(UdpSocket&& theOther) noexcept
	{
		if (this == &theOther)
			return *this;

		Close();
		mHandle = std::exchange(theOther.mHandle, -1);
		mLocalPort = std::exchange(theOther.mLocalPort, 0);
		mRuntimeAcquired = std::exchange(theOther.mRuntimeAcquired, false);
		mLastError = std::move(theOther.mLastError);
		return *this;
	}

	bool UdpSocket::Open()
	{
		if (IsOpen())
			return true;
		mLastError.clear();

		if (!AcquireSocketRuntime())
		{
			mLastError = "failed to initialize socket runtime";
			return false;
		}
		mRuntimeAcquired = true;

#ifdef _WIN32
		SOCKET aSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (aSocket == INVALID_SOCKET)
#else
		int aSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (aSocket < 0)
#endif
		{
			SetError("socket");
			Close();
			return false;
		}
		mHandle = static_cast<intptr_t>(aSocket);

#ifdef _WIN32
		u_long aNonBlocking = 1;
		if (ioctlsocket(static_cast<SOCKET>(mHandle), FIONBIO, &aNonBlocking) != 0)
#else
		int aFlags = fcntl(static_cast<int>(mHandle), F_GETFL, 0);
		if (aFlags < 0 || fcntl(static_cast<int>(mHandle), F_SETFL, aFlags | O_NONBLOCK) != 0)
#endif
		{
			SetError("set non-blocking mode");
			Close();
			return false;
		}

		return true;
	}

	bool UdpSocket::Bind(uint16_t thePort)
	{
		if (!Open())
			return false;

		int anEnabled = 1;
#ifdef _WIN32
		setsockopt(static_cast<SOCKET>(mHandle), SOL_SOCKET, SO_REUSEADDR,
			reinterpret_cast<const char*>(&anEnabled), sizeof(anEnabled));
#else
		setsockopt(static_cast<int>(mHandle), SOL_SOCKET, SO_REUSEADDR, &anEnabled, sizeof(anEnabled));
#endif

		sockaddr_in anAddress = ToSockAddr(Ipv4Endpoint::Any(thePort));
#ifdef _WIN32
		if (bind(static_cast<SOCKET>(mHandle), reinterpret_cast<const sockaddr*>(&anAddress), sizeof(anAddress)) != 0)
#else
		if (bind(static_cast<int>(mHandle), reinterpret_cast<const sockaddr*>(&anAddress), sizeof(anAddress)) != 0)
#endif
		{
			SetError("bind");
			Close();
			return false;
		}

		sockaddr_in aBoundAddress{};
#ifdef _WIN32
		int anAddressLength = sizeof(aBoundAddress);
		if (getsockname(static_cast<SOCKET>(mHandle), reinterpret_cast<sockaddr*>(&aBoundAddress), &anAddressLength) != 0)
#else
		socklen_t anAddressLength = sizeof(aBoundAddress);
		if (getsockname(static_cast<int>(mHandle), reinterpret_cast<sockaddr*>(&aBoundAddress), &anAddressLength) != 0)
#endif
		{
			SetError("getsockname");
			Close();
			return false;
		}

		mLocalPort = ntohs(aBoundAddress.sin_port);
		mLastError.clear();
		return true;
	}

	bool UdpSocket::SetBroadcastEnabled(bool theEnabled)
	{
		if (!Open())
			return false;

		int aValue = theEnabled ? 1 : 0;
#ifdef _WIN32
		int aResult = setsockopt(static_cast<SOCKET>(mHandle), SOL_SOCKET, SO_BROADCAST,
			reinterpret_cast<const char*>(&aValue), sizeof(aValue));
#else
		int aResult = setsockopt(static_cast<int>(mHandle), SOL_SOCKET, SO_BROADCAST, &aValue, sizeof(aValue));
#endif
		if (aResult != 0)
		{
			SetError("set broadcast mode");
			return false;
		}
		mLastError.clear();
		return true;
	}

	bool UdpSocket::SendTo(const Ipv4Endpoint& theEndpoint, std::span<const uint8_t> thePayload)
	{
		if (!IsOpen() || theEndpoint.mPort == 0 || thePayload.empty() || thePayload.size() > MAX_PACKET_SIZE)
		{
			mLastError = "invalid UDP send request";
			return false;
		}

		sockaddr_in anAddress = ToSockAddr(theEndpoint);
#ifdef _WIN32
		int aSent = sendto(static_cast<SOCKET>(mHandle), reinterpret_cast<const char*>(thePayload.data()),
			static_cast<int>(thePayload.size()), 0, reinterpret_cast<const sockaddr*>(&anAddress), sizeof(anAddress));
#else
		ssize_t aSent = sendto(static_cast<int>(mHandle), thePayload.data(), thePayload.size(), 0,
			reinterpret_cast<const sockaddr*>(&anAddress), sizeof(anAddress));
#endif
		if (aSent < 0 || static_cast<size_t>(aSent) != thePayload.size())
		{
			SetError("sendto");
			return false;
		}
		mLastError.clear();
		return true;
	}

	std::optional<UdpDatagram> UdpSocket::Receive()
	{
		if (!IsOpen())
			return std::nullopt;

		std::array<uint8_t, MAX_PACKET_SIZE + 1> aBuffer{};
		sockaddr_in aSource{};
#ifdef _WIN32
		int aSourceLength = sizeof(aSource);
		int aReceived = recvfrom(static_cast<SOCKET>(mHandle), reinterpret_cast<char*>(aBuffer.data()),
			static_cast<int>(aBuffer.size()), 0, reinterpret_cast<sockaddr*>(&aSource), &aSourceLength);
		if (aReceived == SOCKET_ERROR)
#else
		socklen_t aSourceLength = sizeof(aSource);
		ssize_t aReceived = recvfrom(static_cast<int>(mHandle), aBuffer.data(), aBuffer.size(), 0,
			reinterpret_cast<sockaddr*>(&aSource), &aSourceLength);
		if (aReceived < 0)
#endif
		{
			int anError = LastSocketError();
			if (!WouldBlock(anError))
				mLastError = SocketErrorString("recvfrom", anError);
			return std::nullopt;
		}

		if (aReceived == 0 || static_cast<size_t>(aReceived) > MAX_PACKET_SIZE)
		{
			mLastError = "received invalid UDP datagram size";
			return std::nullopt;
		}

		UdpDatagram aDatagram;
		aDatagram.mSource = FromSockAddr(aSource);
		aDatagram.mPayload.assign(aBuffer.begin(), aBuffer.begin() + aReceived);
		mLastError.clear();
		return aDatagram;
	}

	void UdpSocket::Close()
	{
		if (IsOpen())
		{
#ifdef _WIN32
			closesocket(static_cast<SOCKET>(mHandle));
#else
			close(static_cast<int>(mHandle));
#endif
		}
		mHandle = -1;
		mLocalPort = 0;
		if (mRuntimeAcquired)
		{
			ReleaseSocketRuntime();
			mRuntimeAcquired = false;
		}
	}

	bool UdpSocket::IsOpen() const
	{
		return mHandle != -1;
	}

	uint16_t UdpSocket::GetLocalPort() const
	{
		return mLocalPort;
	}

	const std::string& UdpSocket::GetLastError() const
	{
		return mLastError;
	}

	void UdpSocket::SetError(std::string theOperation)
	{
		mLastError = SocketErrorString(theOperation, LastSocketError());
	}
}
