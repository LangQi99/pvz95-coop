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

		bool IsContinuationByte(uint8_t theByte)
		{
			return (theByte & 0xC0U) == 0x80U;
		}

		bool IsValidUtf8(std::string_view theText)
		{
			for (size_t anIndex = 0; anIndex < theText.size();)
			{
				uint8_t aLead = static_cast<uint8_t>(theText[anIndex]);
				if (aLead <= 0x7F)
				{
					++anIndex;
					continue;
				}

				size_t aContinuationCount;
				uint32_t aCodePoint;
				if ((aLead & 0xE0U) == 0xC0U)
				{
					aContinuationCount = 1;
					aCodePoint = aLead & 0x1FU;
				}
				else if ((aLead & 0xF0U) == 0xE0U)
				{
					aContinuationCount = 2;
					aCodePoint = aLead & 0x0FU;
				}
				else if ((aLead & 0xF8U) == 0xF0U)
				{
					aContinuationCount = 3;
					aCodePoint = aLead & 0x07U;
				}
				else
				{
					return false;
				}

				if (anIndex + aContinuationCount >= theText.size())
					return false;
				for (size_t aContinuationIndex = 1; aContinuationIndex <= aContinuationCount; ++aContinuationIndex)
				{
					uint8_t aByte = static_cast<uint8_t>(theText[anIndex + aContinuationIndex]);
					if (!IsContinuationByte(aByte))
						return false;
					aCodePoint = (aCodePoint << 6) | (aByte & 0x3FU);
				}

				if ((aContinuationCount == 1 && aCodePoint < 0x80) ||
					(aContinuationCount == 2 && aCodePoint < 0x800) ||
					(aContinuationCount == 3 && aCodePoint < 0x10000) ||
					aCodePoint > 0x10FFFF || (aCodePoint >= 0xD800 && aCodePoint <= 0xDFFF))
					return false;

				anIndex += aContinuationCount + 1;
			}
			return true;
		}

		bool IsValidPlayerNames(const std::array<std::string, MAX_PLAYERS>& thePlayerNames)
		{
			if (thePlayerNames[0].empty())
				return false;
			for (const std::string& aName : thePlayerNames)
			{
				if (!aName.empty() && !IsValidDisplayName(aName, MAX_PLAYER_NAME_LENGTH))
					return false;
			}
			return true;
		}

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
				theValue <= static_cast<uint8_t>(MessageKind::SESSION_END);
		}

		bool IsValidRejectReason(uint8_t theValue)
		{
			return theValue >= static_cast<uint8_t>(RejectReason::SERVER_FULL) &&
				theValue <= static_cast<uint8_t>(RejectReason::INTERNAL_ERROR);
		}

		bool IsValidActionKind(uint8_t theValue)
		{
			return theValue >= static_cast<uint8_t>(ActionKind::PLANT_SEED) &&
				theValue <= static_cast<uint8_t>(ActionKind::SMASH_SCARY_POT);
		}

		bool IsValidPlayer(PlayerId thePlayerId)
		{
			return thePlayerId < MAX_PLAYERS;
		}

		bool IsValidCursorSeedBankIndex(uint8_t theIndex)
		{
			return theIndex == NO_CURSOR_SEED_BANK_INDEX || theIndex <= MAX_CURSOR_SEED_BANK_INDEX;
		}

		bool IsValidGameAction(const GameAction& theAction)
		{
			if (!IsValidPlayer(theAction.mPlayerId) ||
				!IsValidActionKind(static_cast<uint8_t>(theAction.mKind)))
				return false;
			if (theAction.mKind == ActionKind::ADVANCE_CRAZY_DAVE_DIALOG)
			{
				return theAction.mPlayerId == 0 &&
					theAction.mParameter <= MAX_CRAZY_DAVE_MESSAGE_INDEX &&
					theAction.mTargetX == 0 && theAction.mTargetY == 0;
			}
			if (theAction.mKind == ActionKind::WHACK_ZOMBIE)
				return theAction.mParameter == 0;
			if (theAction.mKind == ActionKind::RESOLVE_PACKET_UPGRADE)
			{
				return theAction.mPlayerId == 0 &&
					(theAction.mParameter == 1503 || theAction.mParameter == 1553) &&
					theAction.mTargetX <= 1 && theAction.mTargetY == 0;
			}
			if (theAction.mKind == ActionKind::SMASH_SCARY_POT)
			{
				return theAction.mParameter == 0 &&
					theAction.mTargetX <= MAX_SCARY_POT_GRID_X &&
					theAction.mTargetY <= MAX_SCARY_POT_GRID_Y;
			}

			return true;
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
					if (!IsValidPlayer(thePayload.mPlayerId) ||
						!IsValidCursorSeedBankIndex(thePayload.mHeldSeedBankIndex))
						return false;
					theWriter.WriteU64(thePayload.mHostTick);
					theWriter.WriteU32(thePayload.mSequence);
					theWriter.WriteU16(thePayload.mNormalizedX);
					theWriter.WriteU16(thePayload.mNormalizedY);
					theWriter.WriteU8(thePayload.mPlayerId);
					theWriter.WriteU8(thePayload.mVisible ? 1 : 0);
					theWriter.WriteU8(thePayload.mHeldSeedBankIndex);
				}
				else if constexpr (std::is_same_v<Payload, GameAction>)
				{
					if (!IsValidGameAction(thePayload))
						return false;
					theWriter.WriteU64(thePayload.mHostTick);
					theWriter.WriteU32(thePayload.mSequence);
					theWriter.WriteU32(thePayload.mParameter);
					theWriter.WriteU16(thePayload.mTargetX);
					theWriter.WriteU16(thePayload.mTargetY);
					theWriter.WriteU8(thePayload.mPlayerId);
					theWriter.WriteU8(static_cast<uint8_t>(thePayload.mKind));
				}
				else if constexpr (std::is_same_v<Payload, SessionStart>)
				{
					if (thePayload.mStartId == 0 || thePayload.mSimulationSeed == 0 ||
						thePayload.mGameMode > MAX_GAME_MODE_VALUE || !IsValidGameplayProfile(thePayload.mProfile) ||
						!IsValidPlayerNames(thePayload.mPlayerNames))
						return false;
					theWriter.WriteU64(thePayload.mHostTick);
					theWriter.WriteU64(thePayload.mStartId);
					theWriter.WriteU32(thePayload.mSimulationSeed);
					theWriter.WriteU16(thePayload.mGameMode);
					WriteGameplayProfile(theWriter, thePayload.mProfile);
					for (const std::string& aName : thePayload.mPlayerNames)
					{
						if (!theWriter.WriteString(aName, MAX_PLAYER_NAME_LENGTH))
							return false;
					}
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
				else if constexpr (std::is_same_v<Payload, SessionEnd>)
				{
					if (thePayload.mStartId == 0)
						return false;
					theWriter.WriteU64(thePayload.mStartId);
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
			if constexpr (std::is_same_v<Payload, GameAction>) return MessageKind::GAME_ACTION;
			if constexpr (std::is_same_v<Payload, SessionStart>) return MessageKind::SESSION_START;
			if constexpr (std::is_same_v<Payload, SessionReady>) return MessageKind::SESSION_READY;
			if constexpr (std::is_same_v<Payload, SessionBegin>) return MessageKind::SESSION_BEGIN;
			if constexpr (std::is_same_v<Payload, TickSync>) return MessageKind::TICK_SYNC;
			if constexpr (std::is_same_v<Payload, StateHash>) return MessageKind::STATE_HASH;
			return MessageKind::SESSION_END;
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
				!aReader.ReadU8(aMessage.mPlayerId) ||
				!aReader.ReadU8(aVisible) || !IsValidPlayer(aMessage.mPlayerId) ||
				aVisible > 1 || !aReader.ReadU8(aMessage.mHeldSeedBankIndex) ||
				!IsValidCursorSeedBankIndex(aMessage.mHeldSeedBankIndex))
				break;
			aMessage.mVisible = aVisible != 0;
			return FinishDecode(aReader, std::move(aMessage));
		}
		case MessageKind::GAME_ACTION:
		{
			GameAction aMessage;
			uint8_t aKind;
			if (!aReader.ReadU64(aMessage.mHostTick) || !aReader.ReadU32(aMessage.mSequence) ||
				!aReader.ReadU32(aMessage.mParameter) || !aReader.ReadU16(aMessage.mTargetX) ||
				!aReader.ReadU16(aMessage.mTargetY) ||
				!aReader.ReadU8(aMessage.mPlayerId) || !aReader.ReadU8(aKind) || !IsValidActionKind(aKind))
				break;
			aMessage.mKind = static_cast<ActionKind>(aKind);
			if (!IsValidGameAction(aMessage))
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
			for (std::string& aName : aMessage.mPlayerNames)
			{
				if (!aReader.ReadString(aName, MAX_PLAYER_NAME_LENGTH))
					return {std::nullopt, CodecError::INVALID_PAYLOAD};
			}
			if (!IsValidPlayerNames(aMessage.mPlayerNames))
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
		case MessageKind::SESSION_END:
		{
			SessionEnd aMessage;
			if (!aReader.ReadU64(aMessage.mStartId) || aMessage.mStartId == 0)
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

	bool IsValidDisplayName(std::string_view theName, size_t theMaxBytes)
	{
		if (theName.empty() || theName.size() > theMaxBytes || !IsValidUtf8(theName))
			return false;

		bool aHasNonSpace = false;
		for (unsigned char aByte : theName)
		{
			if (aByte < 0x20 || aByte == 0x7F)
				return false;
			if (aByte != ' ')
				aHasNonSpace = true;
		}
		return aHasNonSpace;
	}
}
