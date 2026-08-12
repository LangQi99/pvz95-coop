/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace PvzMultiplayer
{
	constexpr uint16_t PROTOCOL_VERSION = 1;
	constexpr uint16_t DEFAULT_DISCOVERY_PORT = 43095;
	constexpr uint8_t MAX_PLAYERS = 4;
	constexpr size_t MAX_PACKET_SIZE = 1024;
	constexpr size_t MAX_PLAYER_NAME_LENGTH = 24;
	constexpr size_t MAX_SESSION_NAME_LENGTH = 48;
	constexpr size_t MAX_REJECT_MESSAGE_LENGTH = 96;

	using PlayerId = uint8_t;

	enum class MessageKind : uint8_t
	{
		DISCOVERY_QUERY = 1,
		DISCOVERY_OFFER,
		HELLO,
		WELCOME,
		REJECT,
		CURSOR_UPDATE,
		INPUT_COMMAND,
		STATE_HASH
	};

	enum class RejectReason : uint8_t
	{
		SERVER_FULL = 1,
		PROTOCOL_MISMATCH,
		RULESET_MISMATCH,
		SESSION_STARTED,
		INVALID_NAME,
		INVALID_REQUEST,
		INTERNAL_ERROR
	};

	enum class InputKind : uint8_t
	{
		POINTER_DOWN = 1,
		POINTER_UP,
		KEY_DOWN,
		KEY_UP,
		PAUSE_TOGGLE
	};

	enum class CodecError : uint8_t
	{
		NONE,
		PACKET_TOO_SHORT,
		PACKET_TOO_LARGE,
		BAD_MAGIC,
		UNSUPPORTED_VERSION,
		UNKNOWN_MESSAGE,
		BAD_LENGTH,
		INVALID_PAYLOAD
	};

	struct DiscoveryQuery
	{
		uint64_t mClientNonce{};

		bool operator==(const DiscoveryQuery&) const = default;
	};

	struct DiscoveryOffer
	{
		uint64_t mSessionId{};
		uint16_t mGamePort{};
		uint8_t mPlayerCount{};
		uint8_t mMaxPlayers{MAX_PLAYERS};
		uint32_t mRulesetId{};
		std::string mSessionName;

		bool operator==(const DiscoveryOffer&) const = default;
	};

	struct Hello
	{
		uint64_t mClientNonce{};
		uint32_t mRulesetId{};
		uint32_t mCapabilities{};
		std::string mPlayerName;

		bool operator==(const Hello&) const = default;
	};

	struct Welcome
	{
		uint64_t mSessionId{};
		uint32_t mRulesetId{};
		uint32_t mCursorRgb{};
		uint16_t mTickRate{};
		PlayerId mPlayerId{};
		uint8_t mMaxPlayers{MAX_PLAYERS};

		bool operator==(const Welcome&) const = default;
	};

	struct Reject
	{
		RejectReason mReason{RejectReason::INTERNAL_ERROR};
		std::string mMessage;

		bool operator==(const Reject&) const = default;
	};

	struct CursorUpdate
	{
		uint64_t mHostTick{};
		uint32_t mSequence{};
		uint16_t mNormalizedX{};
		uint16_t mNormalizedY{};
		PlayerId mPlayerId{};
		uint8_t mButtons{};
		bool mVisible{};

		bool operator==(const CursorUpdate&) const = default;
	};

	struct InputCommand
	{
		uint64_t mHostTick{};
		uint32_t mSequence{};
		uint32_t mCode{};
		uint16_t mNormalizedX{};
		uint16_t mNormalizedY{};
		uint16_t mModifiers{};
		PlayerId mPlayerId{};
		InputKind mKind{InputKind::POINTER_DOWN};

		bool operator==(const InputCommand&) const = default;
	};

	struct StateHash
	{
		uint64_t mHostTick{};
		uint64_t mHash{};

		bool operator==(const StateHash&) const = default;
	};

	using Message = std::variant<DiscoveryQuery, DiscoveryOffer, Hello, Welcome, Reject, CursorUpdate, InputCommand, StateHash>;

	struct DecodeResult
	{
		std::optional<Message> mMessage;
		CodecError mError{CodecError::NONE};

		explicit operator bool() const { return mMessage.has_value(); }
	};

	MessageKind GetMessageKind(const Message& theMessage);
	std::optional<std::vector<uint8_t>> Encode(const Message& theMessage);
	DecodeResult Decode(std::span<const uint8_t> thePacket);
	std::string_view GetCodecErrorName(CodecError theError);
}
