/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Protocol.h"

#include <type_traits>
#include <utility>

namespace PvzMultiplayer
{
	namespace
	{
		constexpr uint32_t PACKET_MAGIC = 0x43353950; // "P95C" on the wire.
		class PacketWriter
		{
		public:
			void WriteU8(uint8_t theValue)
			{
				mBytes.push_back(theValue);
			}

			void WriteU16(uint16_t theValue)
			{
				WriteU8(static_cast<uint8_t>(theValue));
				WriteU8(static_cast<uint8_t>(theValue >> 8));
			}

			void WriteU32(uint32_t theValue)
			{
				for (int aShift = 0; aShift < 32; aShift += 8)
					WriteU8(static_cast<uint8_t>(theValue >> aShift));
			}

			void WriteU64(uint64_t theValue)
			{
				for (int aShift = 0; aShift < 64; aShift += 8)
					WriteU8(static_cast<uint8_t>(theValue >> aShift));
			}

			bool WriteString(std::string_view theValue, size_t theMaxLength)
			{
				if (theValue.size() > theMaxLength || theValue.size() > UINT8_MAX)
					return false;

				WriteU8(static_cast<uint8_t>(theValue.size()));
				mBytes.insert(mBytes.end(), theValue.begin(), theValue.end());
				return true;
			}

			std::vector<uint8_t> mBytes;
		};

		class PacketReader
		{
		public:
			explicit PacketReader(std::span<const uint8_t> theBytes) : mBytes(theBytes) {}

			bool ReadU8(uint8_t& theValue)
			{
				if (mOffset >= mBytes.size())
					return false;
				theValue = mBytes[mOffset++];
				return true;
			}

			bool ReadU16(uint16_t& theValue)
			{
				uint8_t aLow;
				uint8_t aHigh;
				if (!ReadU8(aLow) || !ReadU8(aHigh))
					return false;
				theValue = static_cast<uint16_t>(aLow | (static_cast<uint16_t>(aHigh) << 8));
				return true;
			}

			bool ReadU32(uint32_t& theValue)
			{
				theValue = 0;
				for (int aShift = 0; aShift < 32; aShift += 8)
				{
					uint8_t aByte;
					if (!ReadU8(aByte))
						return false;
					theValue |= static_cast<uint32_t>(aByte) << aShift;
				}
				return true;
			}

			bool ReadU64(uint64_t& theValue)
			{
				theValue = 0;
				for (int aShift = 0; aShift < 64; aShift += 8)
				{
					uint8_t aByte;
					if (!ReadU8(aByte))
						return false;
					theValue |= static_cast<uint64_t>(aByte) << aShift;
				}
				return true;
			}

			bool ReadString(std::string& theValue, size_t theMaxLength)
			{
				uint8_t aLength;
				if (!ReadU8(aLength) || aLength > theMaxLength || mBytes.size() - mOffset < aLength)
					return false;

				theValue.assign(reinterpret_cast<const char*>(mBytes.data() + mOffset), aLength);
				mOffset += aLength;
				return true;
			}

			bool AtEnd() const { return mOffset == mBytes.size(); }

		private:
			std::span<const uint8_t> mBytes;
			size_t mOffset{};
		};

		bool IsValidMessageKind(uint8_t theValue)
		{
			return theValue >= static_cast<uint8_t>(MessageKind::DISCOVERY_QUERY) &&
				theValue <= static_cast<uint8_t>(MessageKind::STATE_HASH);
		}

		bool IsValidRejectReason(uint8_t theValue)
		{
			return theValue >= static_cast<uint8_t>(RejectReason::SERVER_FULL) &&
				theValue <= static_cast<uint8_t>(RejectReason::INTERNAL_ERROR);
		}

		bool IsValidInputKind(uint8_t theValue)
		{
			return theValue >= static_cast<uint8_t>(InputKind::POINTER_DOWN) &&
				theValue <= static_cast<uint8_t>(InputKind::PAUSE_TOGGLE);
		}

		bool IsValidPlayer(PlayerId thePlayerId)
		{
			return thePlayerId < MAX_PLAYERS;
		}

		bool IsValidInputPayload(const InputCommand& theInput)
		{
			if (!IsValidPlayer(theInput.mPlayerId) ||
				!IsValidInputKind(static_cast<uint8_t>(theInput.mKind)))
				return false;

			switch (theInput.mKind)
			{
			case InputKind::POINTER_DOWN:
			case InputKind::POINTER_UP:
			{
				int aClickCount = static_cast<int32_t>(theInput.mCode);
				return theInput.mModifiers == 0 && (aClickCount == -2 || aClickCount == -1 ||
					aClickCount == 1 || aClickCount == 2 || aClickCount == 3);
			}
			case InputKind::KEY_DOWN:
			case InputKind::KEY_UP:
				return theInput.mCode < 0xFFU;
			case InputKind::PAUSE_TOGGLE:
				return theInput.mCode == 0 && theInput.mModifiers == 0;
			}
			return false;
		}

		bool IsValidGameplayProfile(const GameplayProfile& theProfile)
		{
			return theProfile.mProfileId != 0 && theProfile.mAdventureLevel >= 1 &&
				theProfile.mAdventureLevel <= MAX_ADVENTURE_LEVEL &&
				theProfile.mCoins <= static_cast<uint32_t>(INT32_MAX) &&
				(theProfile.mFlags & ~SESSION_PROFILE_KNOWN_FLAGS) == 0;
		}

		void WriteGameplayProfile(PacketWriter& theWriter, const GameplayProfile& theProfile)
		{
			theWriter.WriteU32(theProfile.mProfileId);
			theWriter.WriteU32(theProfile.mAdventureLevel);
			theWriter.WriteU32(theProfile.mCoins);
			theWriter.WriteU32(theProfile.mFinishedAdventure);
			theWriter.WriteU32(theProfile.mFlags);
			for (uint32_t aRecord : theProfile.mChallengeRecords)
				theWriter.WriteU32(aRecord);
			for (uint32_t aPurchase : theProfile.mPurchases)
				theWriter.WriteU32(aPurchase);
		}

		bool ReadGameplayProfile(PacketReader& theReader, GameplayProfile& theProfile)
		{
			if (!theReader.ReadU32(theProfile.mProfileId) || !theReader.ReadU32(theProfile.mAdventureLevel) ||
				!theReader.ReadU32(theProfile.mCoins) || !theReader.ReadU32(theProfile.mFinishedAdventure) ||
				!theReader.ReadU32(theProfile.mFlags))
				return false;
			for (uint32_t& aRecord : theProfile.mChallengeRecords)
			{
				if (!theReader.ReadU32(aRecord))
					return false;
			}
			for (uint32_t& aPurchase : theProfile.mPurchases)
			{
				if (!theReader.ReadU32(aPurchase))
					return false;
			}
			return IsValidGameplayProfile(theProfile);
		}

		bool WritePayload(PacketWriter& theWriter, const Message& theMessage)
		{
			return std::visit([&](const auto& thePayload)
			{
				using Payload = std::decay_t<decltype(thePayload)>;
				if constexpr (std::is_same_v<Payload, DiscoveryQuery>)
				{
					theWriter.WriteU64(thePayload.mClientNonce);
				}
				else if constexpr (std::is_same_v<Payload, DiscoveryOffer>)
				{
					if (thePayload.mGamePort == 0 || thePayload.mMaxPlayers == 0 ||
						thePayload.mMaxPlayers > MAX_PLAYERS || thePayload.mPlayerCount > thePayload.mMaxPlayers ||
						thePayload.mSessionName.empty())
						return false;
					theWriter.WriteU64(thePayload.mSessionId);
					theWriter.WriteU16(thePayload.mGamePort);
					theWriter.WriteU8(thePayload.mPlayerCount);
					theWriter.WriteU8(thePayload.mMaxPlayers);
					theWriter.WriteU32(thePayload.mRulesetId);
					return theWriter.WriteString(thePayload.mSessionName, MAX_SESSION_NAME_LENGTH);
				}
				else if constexpr (std::is_same_v<Payload, Hello>)
				{
					theWriter.WriteU64(thePayload.mClientNonce);
					theWriter.WriteU32(thePayload.mRulesetId);
					theWriter.WriteU32(thePayload.mCapabilities);
					return !thePayload.mPlayerName.empty() &&
						theWriter.WriteString(thePayload.mPlayerName, MAX_PLAYER_NAME_LENGTH);
				}
				else if constexpr (std::is_same_v<Payload, Welcome>)
				{
					if (!IsValidPlayer(thePayload.mPlayerId) || thePayload.mMaxPlayers == 0 ||
						thePayload.mMaxPlayers > MAX_PLAYERS || thePayload.mTickRate == 0 ||
						thePayload.mCursorRgb > 0xFFFFFFU)
						return false;
					theWriter.WriteU64(thePayload.mSessionId);
					theWriter.WriteU32(thePayload.mRulesetId);
					theWriter.WriteU32(thePayload.mCursorRgb);
					theWriter.WriteU16(thePayload.mTickRate);
					theWriter.WriteU8(thePayload.mPlayerId);
					theWriter.WriteU8(thePayload.mMaxPlayers);
				}
				else if constexpr (std::is_same_v<Payload, Reject>)
				{
					if (!IsValidRejectReason(static_cast<uint8_t>(thePayload.mReason)))
						return false;
					theWriter.WriteU8(static_cast<uint8_t>(thePayload.mReason));
					return theWriter.WriteString(thePayload.mMessage, MAX_REJECT_MESSAGE_LENGTH);
				}
				else if constexpr (std::is_same_v<Payload, CursorUpdate>)
				{
					if (!IsValidPlayer(thePayload.mPlayerId) || (thePayload.mButtons & 0xE0U) != 0)
						return false;
					theWriter.WriteU64(thePayload.mHostTick);
					theWriter.WriteU32(thePayload.mSequence);
					theWriter.WriteU16(thePayload.mNormalizedX);
					theWriter.WriteU16(thePayload.mNormalizedY);
					theWriter.WriteU8(thePayload.mPlayerId);
					theWriter.WriteU8(thePayload.mButtons);
					theWriter.WriteU8(thePayload.mVisible ? 1 : 0);
				}
				else if constexpr (std::is_same_v<Payload, InputCommand>)
				{
					if (!IsValidInputPayload(thePayload))
						return false;
					theWriter.WriteU64(thePayload.mHostTick);
					theWriter.WriteU32(thePayload.mSequence);
					theWriter.WriteU32(thePayload.mCode);
					theWriter.WriteU16(thePayload.mNormalizedX);
					theWriter.WriteU16(thePayload.mNormalizedY);
					theWriter.WriteU16(thePayload.mModifiers);
					theWriter.WriteU8(thePayload.mPlayerId);
					theWriter.WriteU8(static_cast<uint8_t>(thePayload.mKind));
				}
				else if constexpr (std::is_same_v<Payload, SessionStart>)
				{
					if (thePayload.mStartId == 0 || thePayload.mSimulationSeed == 0 ||
						thePayload.mGameMode > MAX_GAME_MODE_VALUE || !IsValidGameplayProfile(thePayload.mProfile))
						return false;
					theWriter.WriteU64(thePayload.mHostTick);
					theWriter.WriteU64(thePayload.mStartId);
					theWriter.WriteU32(thePayload.mSimulationSeed);
					theWriter.WriteU16(thePayload.mGameMode);
					WriteGameplayProfile(theWriter, thePayload.mProfile);
				}
				else if constexpr (std::is_same_v<Payload, SessionReady>)
				{
					if (thePayload.mStartId == 0 || !IsValidPlayer(thePayload.mPlayerId))
						return false;
					theWriter.WriteU64(thePayload.mStartId);
					theWriter.WriteU8(thePayload.mPlayerId);
				}
				else if constexpr (std::is_same_v<Payload, SessionBegin>)
				{
					if (thePayload.mStartId == 0)
						return false;
					theWriter.WriteU64(thePayload.mHostTick);
					theWriter.WriteU64(thePayload.mStartId);
				}
				else if constexpr (std::is_same_v<Payload, TickSync>)
				{
					if (thePayload.mStartId == 0)
						return false;
					theWriter.WriteU64(thePayload.mHostTick);
					theWriter.WriteU64(thePayload.mStartId);
				}
				else if constexpr (std::is_same_v<Payload, StateHash>)
				{
					if (thePayload.mStartId == 0)
						return false;
					theWriter.WriteU64(thePayload.mHostTick);
					theWriter.WriteU64(thePayload.mStartId);
					theWriter.WriteU64(thePayload.mHash);
				}
				return true;
			}, theMessage);
		}

		template <typename Payload>
		DecodeResult FinishDecode(PacketReader& theReader, Payload&& thePayload)
		{
			if (!theReader.AtEnd())
				return {std::nullopt, CodecError::BAD_LENGTH};
			return {Message(std::forward<Payload>(thePayload)), CodecError::NONE};
		}
	}

	MessageKind GetMessageKind(const Message& theMessage)
	{
		return std::visit([](const auto& thePayload)
		{
			using Payload = std::decay_t<decltype(thePayload)>;
			if constexpr (std::is_same_v<Payload, DiscoveryQuery>) return MessageKind::DISCOVERY_QUERY;
			if constexpr (std::is_same_v<Payload, DiscoveryOffer>) return MessageKind::DISCOVERY_OFFER;
			if constexpr (std::is_same_v<Payload, Hello>) return MessageKind::HELLO;
			if constexpr (std::is_same_v<Payload, Welcome>) return MessageKind::WELCOME;
			if constexpr (std::is_same_v<Payload, Reject>) return MessageKind::REJECT;
			if constexpr (std::is_same_v<Payload, CursorUpdate>) return MessageKind::CURSOR_UPDATE;
			if constexpr (std::is_same_v<Payload, InputCommand>) return MessageKind::INPUT_COMMAND;
			if constexpr (std::is_same_v<Payload, SessionStart>) return MessageKind::SESSION_START;
			if constexpr (std::is_same_v<Payload, SessionReady>) return MessageKind::SESSION_READY;
			if constexpr (std::is_same_v<Payload, SessionBegin>) return MessageKind::SESSION_BEGIN;
			if constexpr (std::is_same_v<Payload, TickSync>) return MessageKind::TICK_SYNC;
			return MessageKind::STATE_HASH;
		}, theMessage);
	}

	std::optional<std::vector<uint8_t>> Encode(const Message& theMessage)
	{
		PacketWriter aPayload;
		if (!WritePayload(aPayload, theMessage) || aPayload.mBytes.size() > MAX_PACKET_SIZE - PACKET_HEADER_SIZE)
			return std::nullopt;

		PacketWriter aPacket;
		aPacket.WriteU32(PACKET_MAGIC);
		aPacket.WriteU16(PROTOCOL_VERSION);
		aPacket.WriteU8(static_cast<uint8_t>(GetMessageKind(theMessage)));
		aPacket.WriteU8(0);
		aPacket.WriteU32(static_cast<uint32_t>(aPayload.mBytes.size()));
		aPacket.mBytes.insert(aPacket.mBytes.end(), aPayload.mBytes.begin(), aPayload.mBytes.end());
		return aPacket.mBytes;
	}

	DecodeResult Decode(std::span<const uint8_t> thePacket)
	{
		if (thePacket.size() < PACKET_HEADER_SIZE)
			return {std::nullopt, CodecError::PACKET_TOO_SHORT};
		if (thePacket.size() > MAX_PACKET_SIZE)
			return {std::nullopt, CodecError::PACKET_TOO_LARGE};

		PacketReader aHeader(thePacket.first(PACKET_HEADER_SIZE));
		uint32_t aMagic;
		uint16_t aVersion;
		uint8_t aKindValue;
		uint8_t aFlags;
		uint32_t aPayloadLength;
		if (!aHeader.ReadU32(aMagic) || !aHeader.ReadU16(aVersion) || !aHeader.ReadU8(aKindValue) ||
			!aHeader.ReadU8(aFlags) || !aHeader.ReadU32(aPayloadLength))
			return {std::nullopt, CodecError::PACKET_TOO_SHORT};

		if (aMagic != PACKET_MAGIC)
			return {std::nullopt, CodecError::BAD_MAGIC};
		if (aVersion != PROTOCOL_VERSION)
			return {std::nullopt, CodecError::UNSUPPORTED_VERSION};
		if (!IsValidMessageKind(aKindValue))
			return {std::nullopt, CodecError::UNKNOWN_MESSAGE};
		if (aFlags != 0)
			return {std::nullopt, CodecError::INVALID_PAYLOAD};
		if (aPayloadLength != thePacket.size() - PACKET_HEADER_SIZE)
			return {std::nullopt, CodecError::BAD_LENGTH};

		PacketReader aReader(thePacket.subspan(PACKET_HEADER_SIZE));
		switch (static_cast<MessageKind>(aKindValue))
		{
		case MessageKind::DISCOVERY_QUERY:
		{
			DiscoveryQuery aMessage;
			if (!aReader.ReadU64(aMessage.mClientNonce))
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::DISCOVERY_OFFER:
		{
			DiscoveryOffer aMessage;
			if (!aReader.ReadU64(aMessage.mSessionId) || !aReader.ReadU16(aMessage.mGamePort) ||
				!aReader.ReadU8(aMessage.mPlayerCount) || !aReader.ReadU8(aMessage.mMaxPlayers) ||
				!aReader.ReadU32(aMessage.mRulesetId) ||
				!aReader.ReadString(aMessage.mSessionName, MAX_SESSION_NAME_LENGTH) ||
				aMessage.mGamePort == 0 || aMessage.mMaxPlayers == 0 || aMessage.mMaxPlayers > MAX_PLAYERS ||
				aMessage.mPlayerCount > aMessage.mMaxPlayers || aMessage.mSessionName.empty())
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::HELLO:
		{
			Hello aMessage;
			if (!aReader.ReadU64(aMessage.mClientNonce) || !aReader.ReadU32(aMessage.mRulesetId) ||
				!aReader.ReadU32(aMessage.mCapabilities) ||
				!aReader.ReadString(aMessage.mPlayerName, MAX_PLAYER_NAME_LENGTH) || aMessage.mPlayerName.empty())
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::WELCOME:
		{
			Welcome aMessage;
			if (!aReader.ReadU64(aMessage.mSessionId) || !aReader.ReadU32(aMessage.mRulesetId) ||
				!aReader.ReadU32(aMessage.mCursorRgb) || !aReader.ReadU16(aMessage.mTickRate) ||
				!aReader.ReadU8(aMessage.mPlayerId) || !aReader.ReadU8(aMessage.mMaxPlayers) ||
				!IsValidPlayer(aMessage.mPlayerId) || aMessage.mMaxPlayers == 0 ||
				aMessage.mMaxPlayers > MAX_PLAYERS || aMessage.mTickRate == 0 || aMessage.mCursorRgb > 0xFFFFFFU)
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::REJECT:
		{
			Reject aMessage;
			uint8_t aReason;
			if (!aReader.ReadU8(aReason) || !IsValidRejectReason(aReason) ||
				!aReader.ReadString(aMessage.mMessage, MAX_REJECT_MESSAGE_LENGTH))
				break;
			aMessage.mReason = static_cast<RejectReason>(aReason);
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::CURSOR_UPDATE:
		{
			CursorUpdate aMessage;
			uint8_t aVisible;
			if (!aReader.ReadU64(aMessage.mHostTick) || !aReader.ReadU32(aMessage.mSequence) ||
				!aReader.ReadU16(aMessage.mNormalizedX) || !aReader.ReadU16(aMessage.mNormalizedY) ||
				!aReader.ReadU8(aMessage.mPlayerId) || !aReader.ReadU8(aMessage.mButtons) ||
				!aReader.ReadU8(aVisible) || !IsValidPlayer(aMessage.mPlayerId) ||
				(aMessage.mButtons & 0xE0U) != 0 || aVisible > 1)
				break;
			aMessage.mVisible = aVisible != 0;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::INPUT_COMMAND:
		{
			InputCommand aMessage;
			uint8_t aKind;
			if (!aReader.ReadU64(aMessage.mHostTick) || !aReader.ReadU32(aMessage.mSequence) ||
				!aReader.ReadU32(aMessage.mCode) || !aReader.ReadU16(aMessage.mNormalizedX) ||
				!aReader.ReadU16(aMessage.mNormalizedY) || !aReader.ReadU16(aMessage.mModifiers) ||
				!aReader.ReadU8(aMessage.mPlayerId) || !aReader.ReadU8(aKind) || !IsValidInputKind(aKind))
				break;
			aMessage.mKind = static_cast<InputKind>(aKind);
			if (!IsValidInputPayload(aMessage))
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::SESSION_START:
		{
			SessionStart aMessage;
			if (!aReader.ReadU64(aMessage.mHostTick) || !aReader.ReadU64(aMessage.mStartId) ||
				!aReader.ReadU32(aMessage.mSimulationSeed) || !aReader.ReadU16(aMessage.mGameMode) ||
				aMessage.mStartId == 0 || aMessage.mSimulationSeed == 0 ||
				aMessage.mGameMode > MAX_GAME_MODE_VALUE || !ReadGameplayProfile(aReader, aMessage.mProfile))
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::SESSION_READY:
		{
			SessionReady aMessage;
			if (!aReader.ReadU64(aMessage.mStartId) || !aReader.ReadU8(aMessage.mPlayerId) ||
				aMessage.mStartId == 0 || !IsValidPlayer(aMessage.mPlayerId))
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::SESSION_BEGIN:
		{
			SessionBegin aMessage;
			if (!aReader.ReadU64(aMessage.mHostTick) || !aReader.ReadU64(aMessage.mStartId) ||
				aMessage.mStartId == 0)
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::TICK_SYNC:
		{
			TickSync aMessage;
			if (!aReader.ReadU64(aMessage.mHostTick) || !aReader.ReadU64(aMessage.mStartId) ||
				aMessage.mStartId == 0)
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::STATE_HASH:
		{
			StateHash aMessage;
			if (!aReader.ReadU64(aMessage.mHostTick) || !aReader.ReadU64(aMessage.mStartId) ||
				!aReader.ReadU64(aMessage.mHash) || aMessage.mStartId == 0)
				break;
			return FinishDecode(aReader, std::move(aMessage));
		}
		}

		return {std::nullopt, CodecError::INVALID_PAYLOAD};
	}

	std::string_view GetCodecErrorName(CodecError theError)
	{
		switch (theError)
		{
		case CodecError::NONE: return "none";
		case CodecError::PACKET_TOO_SHORT: return "packet too short";
		case CodecError::PACKET_TOO_LARGE: return "packet too large";
		case CodecError::BAD_MAGIC: return "bad magic";
		case CodecError::UNSUPPORTED_VERSION: return "unsupported version";
		case CodecError::UNKNOWN_MESSAGE: return "unknown message";
		case CodecError::BAD_LENGTH: return "bad length";
		case CodecError::INVALID_PAYLOAD: return "invalid payload";
		}
		return "unknown error";
	}
}
