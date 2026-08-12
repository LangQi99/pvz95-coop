/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "UdpSocket.h"

#include "Protocol.h"
#include "TcpSocket.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <limits>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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

		bool ConnectInProgress(int theError)
		{
#ifdef _WIN32
			return theError == WSAEWOULDBLOCK || theError == WSAEINPROGRESS || theError == WSAEALREADY;
#else
			return theError == EINPROGRESS || theError == EALREADY || WouldBlock(theError);
#endif
		}

		void CloseNativeSocket(intptr_t theHandle)
		{
#ifdef _WIN32
			closesocket(static_cast<SOCKET>(theHandle));
#else
			close(static_cast<int>(theHandle));
#endif
		}

		bool SetNativeNonBlocking(intptr_t theHandle)
		{
#ifdef _WIN32
			u_long aNonBlocking = 1;
			return ioctlsocket(static_cast<SOCKET>(theHandle), FIONBIO, &aNonBlocking) == 0;
#else
			int aFlags = fcntl(static_cast<int>(theHandle), F_GETFL, 0);
			return aFlags >= 0 && fcntl(static_cast<int>(theHandle), F_SETFL, aFlags | O_NONBLOCK) == 0;
#endif
		}

		bool SetNativeNoDelay(intptr_t theHandle)
		{
			int anEnabled = 1;
#ifdef _WIN32
			return setsockopt(static_cast<SOCKET>(theHandle), IPPROTO_TCP, TCP_NODELAY,
				reinterpret_cast<const char*>(&anEnabled), sizeof(anEnabled)) == 0;
#else
			if (setsockopt(static_cast<int>(theHandle), IPPROTO_TCP, TCP_NODELAY, &anEnabled, sizeof(anEnabled)) != 0)
				return false;
#ifdef SO_NOSIGPIPE
			if (setsockopt(static_cast<int>(theHandle), SOL_SOCKET, SO_NOSIGPIPE, &anEnabled, sizeof(anEnabled)) != 0)
				return false;
#endif
			return true;
#endif
		}

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

		if (!SetNativeNonBlocking(mHandle))
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
			CloseNativeSocket(mHandle);
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

	TcpSocket::~TcpSocket()
	{
		Close();
	}

	TcpSocket::TcpSocket(TcpSocket&& theOther) noexcept
	{
		*this = std::move(theOther);
	}

	TcpSocket& TcpSocket::operator=(TcpSocket&& theOther) noexcept
	{
		if (this == &theOther)
			return *this;

		Close();
		mHandle = std::exchange(theOther.mHandle, -1);
		mRuntimeAcquired = std::exchange(theOther.mRuntimeAcquired, false);
		mState = std::exchange(theOther.mState, ConnectionState::DISCONNECTED);
		mPeerEndpoint = std::exchange(theOther.mPeerEndpoint, {});
		mLastError = std::move(theOther.mLastError);
		return *this;
	}

	bool TcpSocket::StartConnect(const Ipv4Endpoint& theEndpoint)
	{
		Close();
		mLastError.clear();
		if (theEndpoint.mPort == 0 || !AcquireSocketRuntime())
		{
			mLastError = "invalid TCP endpoint or socket runtime unavailable";
			mState = ConnectionState::FAILED;
			return false;
		}
		mRuntimeAcquired = true;

#ifdef _WIN32
		SOCKET aSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (aSocket == INVALID_SOCKET)
#else
		int aSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (aSocket < 0)
#endif
		{
			SetError("socket");
			mState = ConnectionState::FAILED;
			Close();
			mState = ConnectionState::FAILED;
			return false;
		}
		mHandle = static_cast<intptr_t>(aSocket);
		mPeerEndpoint = theEndpoint;

		if (!SetNativeNonBlocking(mHandle) || !SetNativeNoDelay(mHandle))
		{
			SetError("configure TCP socket");
			Close();
			mState = ConnectionState::FAILED;
			return false;
		}

		sockaddr_in anAddress = ToSockAddr(theEndpoint);
#ifdef _WIN32
		int aResult = connect(static_cast<SOCKET>(mHandle), reinterpret_cast<const sockaddr*>(&anAddress), sizeof(anAddress));
		if (aResult == SOCKET_ERROR)
#else
		int aResult = connect(static_cast<int>(mHandle), reinterpret_cast<const sockaddr*>(&anAddress), sizeof(anAddress));
		if (aResult < 0)
#endif
		{
			int anError = LastSocketError();
			if (ConnectInProgress(anError))
			{
				mState = ConnectionState::CONNECTING;
				return true;
			}
			mLastError = SocketErrorString("connect", anError);
			Close();
			mState = ConnectionState::FAILED;
			return false;
		}

		mState = ConnectionState::CONNECTED;
		return true;
	}

	ConnectionState TcpSocket::PollConnect()
	{
		if (mState != ConnectionState::CONNECTING)
			return mState;

		fd_set aWriteSet;
		fd_set anErrorSet;
		FD_ZERO(&aWriteSet);
		FD_ZERO(&anErrorSet);
#ifdef _WIN32
		SOCKET aSocket = static_cast<SOCKET>(mHandle);
#else
		int aSocket = static_cast<int>(mHandle);
#endif
		FD_SET(aSocket, &aWriteSet);
		FD_SET(aSocket, &anErrorSet);
		timeval aTimeout{};
		int aResult = select(static_cast<int>(mHandle + 1), nullptr, &aWriteSet, &anErrorSet, &aTimeout);
		if (aResult == 0)
			return mState;
		if (aResult < 0)
		{
			SetError("select");
			Close();
			mState = ConnectionState::FAILED;
			return mState;
		}

		int aSocketError = 0;
#ifdef _WIN32
		int anErrorLength = sizeof(aSocketError);
		if (getsockopt(aSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&aSocketError), &anErrorLength) != 0)
#else
		socklen_t anErrorLength = sizeof(aSocketError);
		if (getsockopt(aSocket, SOL_SOCKET, SO_ERROR, &aSocketError, &anErrorLength) != 0)
#endif
		{
			SetError("getsockopt");
			Close();
			mState = ConnectionState::FAILED;
			return mState;
		}

		if (aSocketError != 0)
		{
			mLastError = SocketErrorString("connect", aSocketError);
			Close();
			mState = ConnectionState::FAILED;
			return mState;
		}

		mState = ConnectionState::CONNECTED;
		mLastError.clear();
		return mState;
	}

	SocketIoResult TcpSocket::Send(std::span<const uint8_t> theBytes)
	{
		if (mState == ConnectionState::CONNECTING)
			return {SocketIoStatus::WOULD_BLOCK, 0};
		if (mState != ConnectionState::CONNECTED)
			return {SocketIoStatus::CLOSED, 0};
		if (theBytes.empty())
			return {SocketIoStatus::COMPLETED, 0};
		if (theBytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			mLastError = "TCP send is too large";
			return {SocketIoStatus::ERROR, 0};
		}

#ifdef _WIN32
		int aSent = send(static_cast<SOCKET>(mHandle), reinterpret_cast<const char*>(theBytes.data()),
			static_cast<int>(theBytes.size()), 0);
		if (aSent == SOCKET_ERROR)
#else
#ifdef MSG_NOSIGNAL
		constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#else
		constexpr int SEND_FLAGS = 0;
#endif
		ssize_t aSent = send(static_cast<int>(mHandle), theBytes.data(), theBytes.size(), SEND_FLAGS);
		if (aSent < 0)
#endif
		{
			int anError = LastSocketError();
			if (WouldBlock(anError))
				return {SocketIoStatus::WOULD_BLOCK, 0};
			mLastError = SocketErrorString("send", anError);
			mState = ConnectionState::FAILED;
			return {SocketIoStatus::ERROR, 0};
		}

		if (aSent == 0)
		{
			mState = ConnectionState::DISCONNECTED;
			return {SocketIoStatus::CLOSED, 0};
		}
		mLastError.clear();
		return {SocketIoStatus::COMPLETED, static_cast<size_t>(aSent)};
	}

	SocketIoResult TcpSocket::Receive(std::span<uint8_t> theBuffer)
	{
		if (mState == ConnectionState::CONNECTING)
			return {SocketIoStatus::WOULD_BLOCK, 0};
		if (mState != ConnectionState::CONNECTED)
			return {SocketIoStatus::CLOSED, 0};
		if (theBuffer.empty())
			return {SocketIoStatus::COMPLETED, 0};
		if (theBuffer.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			mLastError = "TCP receive buffer is too large";
			return {SocketIoStatus::ERROR, 0};
		}

#ifdef _WIN32
		int aReceived = recv(static_cast<SOCKET>(mHandle), reinterpret_cast<char*>(theBuffer.data()),
			static_cast<int>(theBuffer.size()), 0);
		if (aReceived == SOCKET_ERROR)
#else
		ssize_t aReceived = recv(static_cast<int>(mHandle), theBuffer.data(), theBuffer.size(), 0);
		if (aReceived < 0)
#endif
		{
			int anError = LastSocketError();
			if (WouldBlock(anError))
				return {SocketIoStatus::WOULD_BLOCK, 0};
			mLastError = SocketErrorString("recv", anError);
			mState = ConnectionState::FAILED;
			return {SocketIoStatus::ERROR, 0};
		}

		if (aReceived == 0)
		{
			mState = ConnectionState::DISCONNECTED;
			return {SocketIoStatus::CLOSED, 0};
		}
		mLastError.clear();
		return {SocketIoStatus::COMPLETED, static_cast<size_t>(aReceived)};
	}

	void TcpSocket::Close()
	{
		if (mHandle != -1)
			CloseNativeSocket(mHandle);
		mHandle = -1;
		mState = ConnectionState::DISCONNECTED;
		mPeerEndpoint = {};
		if (mRuntimeAcquired)
		{
			ReleaseSocketRuntime();
			mRuntimeAcquired = false;
		}
	}

	ConnectionState TcpSocket::GetState() const
	{
		return mState;
	}

	const Ipv4Endpoint& TcpSocket::GetPeerEndpoint() const
	{
		return mPeerEndpoint;
	}

	const std::string& TcpSocket::GetLastError() const
	{
		return mLastError;
	}

	bool TcpSocket::Adopt(intptr_t theHandle, const Ipv4Endpoint& thePeerEndpoint)
	{
		Close();
		if (theHandle == -1)
			return false;
		mHandle = theHandle;
		if (!AcquireSocketRuntime())
		{
			CloseNativeSocket(mHandle);
			mHandle = -1;
			return false;
		}
		mRuntimeAcquired = true;
		mPeerEndpoint = thePeerEndpoint;
		if (!SetNativeNonBlocking(mHandle) || !SetNativeNoDelay(mHandle))
		{
			SetError("configure accepted TCP socket");
			Close();
			return false;
		}
		mState = ConnectionState::CONNECTED;
		mLastError.clear();
		return true;
	}

	void TcpSocket::SetError(std::string theOperation)
	{
		mLastError = SocketErrorString(theOperation, LastSocketError());
	}

	TcpListener::~TcpListener()
	{
		Close();
	}

	TcpListener::TcpListener(TcpListener&& theOther) noexcept
	{
		*this = std::move(theOther);
	}

	TcpListener& TcpListener::operator=(TcpListener&& theOther) noexcept
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

	bool TcpListener::Listen(uint16_t thePort, int theBacklog)
	{
		Close();
		mLastError.clear();
		if (theBacklog <= 0 || !AcquireSocketRuntime())
		{
			mLastError = "invalid TCP listener configuration or socket runtime unavailable";
			return false;
		}
		mRuntimeAcquired = true;

#ifdef _WIN32
		SOCKET aSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (aSocket == INVALID_SOCKET)
#else
		int aSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (aSocket < 0)
#endif
		{
			SetError("socket");
			Close();
			return false;
		}
		mHandle = static_cast<intptr_t>(aSocket);

		int anEnabled = 1;
#ifdef _WIN32
		setsockopt(aSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&anEnabled), sizeof(anEnabled));
#else
		setsockopt(aSocket, SOL_SOCKET, SO_REUSEADDR, &anEnabled, sizeof(anEnabled));
#endif
		if (!SetNativeNonBlocking(mHandle))
		{
			SetError("configure TCP listener");
			Close();
			return false;
		}

		sockaddr_in anAddress = ToSockAddr(Ipv4Endpoint::Any(thePort));
#ifdef _WIN32
		if (bind(aSocket, reinterpret_cast<const sockaddr*>(&anAddress), sizeof(anAddress)) != 0 ||
			listen(aSocket, theBacklog) != 0)
#else
		if (bind(aSocket, reinterpret_cast<const sockaddr*>(&anAddress), sizeof(anAddress)) != 0 ||
			listen(aSocket, theBacklog) != 0)
#endif
		{
			SetError("bind/listen");
			Close();
			return false;
		}

		sockaddr_in aBoundAddress{};
#ifdef _WIN32
		int anAddressLength = sizeof(aBoundAddress);
		if (getsockname(aSocket, reinterpret_cast<sockaddr*>(&aBoundAddress), &anAddressLength) != 0)
#else
		socklen_t anAddressLength = sizeof(aBoundAddress);
		if (getsockname(aSocket, reinterpret_cast<sockaddr*>(&aBoundAddress), &anAddressLength) != 0)
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

	std::optional<TcpSocket> TcpListener::Accept()
	{
		if (!IsListening())
			return std::nullopt;

		sockaddr_in aPeerAddress{};
#ifdef _WIN32
		int anAddressLength = sizeof(aPeerAddress);
		SOCKET anAccepted = accept(static_cast<SOCKET>(mHandle), reinterpret_cast<sockaddr*>(&aPeerAddress), &anAddressLength);
		if (anAccepted == INVALID_SOCKET)
#else
		socklen_t anAddressLength = sizeof(aPeerAddress);
		int anAccepted = accept(static_cast<int>(mHandle), reinterpret_cast<sockaddr*>(&aPeerAddress), &anAddressLength);
		if (anAccepted < 0)
#endif
		{
			int anError = LastSocketError();
			if (!WouldBlock(anError))
				mLastError = SocketErrorString("accept", anError);
			return std::nullopt;
		}

		TcpSocket aSocket;
		if (!aSocket.Adopt(static_cast<intptr_t>(anAccepted), FromSockAddr(aPeerAddress)))
		{
			mLastError = "failed to configure accepted TCP socket";
			return std::nullopt;
		}
		mLastError.clear();
		return aSocket;
	}

	void TcpListener::Close()
	{
		if (mHandle != -1)
			CloseNativeSocket(mHandle);
		mHandle = -1;
		mLocalPort = 0;
		if (mRuntimeAcquired)
		{
			ReleaseSocketRuntime();
			mRuntimeAcquired = false;
		}
	}

	bool TcpListener::IsListening() const
	{
		return mHandle != -1;
	}

	uint16_t TcpListener::GetLocalPort() const
	{
		return mLocalPort;
	}

	const std::string& TcpListener::GetLastError() const
	{
		return mLastError;
	}

	void TcpListener::SetError(std::string theOperation)
	{
		mLastError = SocketErrorString(theOperation, LastSocketError());
	}
}
