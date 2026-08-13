/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <array>
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
	// Version 8 adds a host-authoritative Crazy Dave dialog action and extends
	// the canonical gameplay checksum with dialog/tutorial state.  Older peers
	// must fail the handshake instead of silently advancing cutscenes locally.
	constexpr uint16_t PROTOCOL_VERSION = 8;
	constexpr uint16_t DEFAULT_DISCOVERY_PORT = 43095;
	constexpr uint16_t DEFAULT_GAME_PORT = 43096;
	constexpr uint8_t MAX_PLAYERS = 4;
	constexpr size_t MAX_PACKET_SIZE = 1024;
	constexpr size_t PACKET_HEADER_SIZE = 12;
	constexpr size_t MAX_PLAYER_NAME_LENGTH = 24;
	constexpr size_t MAX_SESSION_NAME_LENGTH = 48;
	constexpr size_t MAX_REJECT_MESSAGE_LENGTH = 96;
	constexpr uint8_t MAX_CURSOR_SEED_BANK_INDEX = 9;
	constexpr uint8_t NO_CURSOR_SEED_BANK_INDEX = UINT8_MAX;
	constexpr uint16_t MAX_GAME_MODE_VALUE = 72;
	constexpr uint16_t MAX_ADVENTURE_LEVEL = 50;
	constexpr uint32_t MAX_CRAZY_DAVE_MESSAGE_INDEX = 9999;
	constexpr size_t GAMEPLAY_CHALLENGE_RECORD_COUNT = 100;
	constexpr size_t GAMEPLAY_PURCHASE_COUNT = 80;

	using PlayerId = uint8_t;

	enum class MessageKind : uint8_t
	{
		DISCOVERY_QUERY = 1,
		DISCOVERY_OFFER,
		HELLO,
		WELCOME,
		REJECT,
		CURSOR_UPDATE,
		GAME_ACTION,
		SESSION_START,
		SESSION_READY,
		SESSION_BEGIN,
		TICK_SYNC,
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

	enum class ActionKind : uint8_t
	{
		PLANT_SEED = 1,
		COLLECT_COIN,
		SHOVEL_PLANT,
		FIRE_COB_CANNON,
		ADD_SEED_CHOICE,
		REMOVE_SEED_CHOICE,
		CHOOSE_IMITATER,
		CONFIRM_SEED_CHOICES,
		ADVANCE_CRAZY_DAVE_DIALOG
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
		bool mVisible{};
		uint8_t mHeldSeedBankIndex{NO_CURSOR_SEED_BANK_INDEX};

		bool operator==(const CursorUpdate&) const = default;
	};

	struct GameAction
	{
		uint64_t mHostTick{};
		uint32_t mSequence{};
		uint32_t mParameter{};
		uint16_t mTargetX{};
		uint16_t mTargetY{};
		PlayerId mPlayerId{};
		ActionKind mKind{ActionKind::PLANT_SEED};

		bool operator==(const GameAction&) const = default;
	};

	enum SessionProfileFlags : uint32_t
	{
		PROFILE_DIDNT_PURCHASE_PACKET_UPGRADE = 1U << 0,
		PROFILE_HAS_WOKEN_STINKY = 1U << 1,
		PROFILE_HAS_UNLOCKED_MINIGAMES = 1U << 2,
		PROFILE_HAS_UNLOCKED_PUZZLE = 1U << 3,
		PROFILE_HAS_NEW_MINIGAME = 1U << 4,
		PROFILE_HAS_NEW_SCARY_POTTER = 1U << 5,
		PROFILE_HAS_NEW_I_ZOMBIE = 1U << 6,
		PROFILE_HAS_NEW_SURVIVAL = 1U << 7,
		PROFILE_HAS_UNLOCKED_SURVIVAL = 1U << 8,
		PROFILE_NEEDS_MAGIC_TACO_REWARD = 1U << 9,
		PROFILE_HAS_SEEN_STINKY = 1U << 10,
		PROFILE_HAS_SEEN_UPSELL = 1U << 11
	};

	constexpr uint32_t SESSION_PROFILE_KNOWN_FLAGS = (1U << 12) - 1;

	struct GameplayProfile
	{
		uint32_t mProfileId{};
		uint32_t mAdventureLevel{1};
		uint32_t mCoins{};
		uint32_t mFinishedAdventure{};
		uint32_t mFlags{};
		std::array<uint32_t, GAMEPLAY_CHALLENGE_RECORD_COUNT> mChallengeRecords{};
		std::array<uint32_t, GAMEPLAY_PURCHASE_COUNT> mPurchases{};

		bool operator==(const GameplayProfile&) const = default;
	};

	struct SessionStart
	{
		uint64_t mHostTick{};
		uint64_t mStartId{};
		uint32_t mSimulationSeed{};
		uint16_t mGameMode{};
		GameplayProfile mProfile;
		std::array<std::string, MAX_PLAYERS> mPlayerNames;

		bool operator==(const SessionStart&) const = default;
	};

	struct SessionReady
	{
		uint64_t mStartId{};
		PlayerId mPlayerId{};

		bool operator==(const SessionReady&) const = default;
	};

	struct SessionBegin
	{
		uint64_t mHostTick{};
		uint64_t mStartId{};

		bool operator==(const SessionBegin&) const = default;
	};

	struct TickSync
	{
		uint64_t mHostTick{};
		uint64_t mStartId{};

		bool operator==(const TickSync&) const = default;
	};

	struct StateHash
	{
		uint64_t mHostTick{};
		uint64_t mStartId{};
		uint64_t mHash{};

		bool operator==(const StateHash&) const = default;
	};

	using Message = std::variant<DiscoveryQuery, DiscoveryOffer, Hello, Welcome, Reject, CursorUpdate,
		GameAction, SessionStart, SessionReady, SessionBegin, TickSync, StateHash>;

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
	bool IsValidDisplayName(std::string_view theName, size_t theMaxBytes);
}
