/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ClientSession.h"
#include "SharedInputState.h"

#include <utility>

namespace PvzMultiplayer
{
	bool ClientSession::Start(ClientSessionConfig theConfig)
	{
		Stop();
		if (theConfig.mEndpoint.mPort == 0 || theConfig.mSessionId == 0 || theConfig.mClientNonce == 0 ||
			theConfig.mRulesetId == 0 || !IsValidDisplayName(theConfig.mPlayerName, MAX_PLAYER_NAME_LENGTH))
		{
			mLastError = "invalid client session configuration";
			mState = ClientSessionState::FAILED;
			return false;
		}

		TcpSocket aSocket;
		if (!aSocket.StartConnect(theConfig.mEndpoint))
		{
			mLastError = aSocket.GetLastError();
			mState = ClientSessionState::FAILED;
			return false;
		}

		mConfig = std::move(theConfig);
		mChannel.emplace(std::move(aSocket));
		if (!mChannel->Queue(Hello{mConfig.mClientNonce, mConfig.mRulesetId, mConfig.mCapabilities, mConfig.mPlayerName}))
		{
			Fail(mChannel->GetLastError());
			return false;
		}
		mState = mChannel->GetState() == ReliableChannelState::CONNECTED ?
			ClientSessionState::HANDSHAKING : ClientSessionState::CONNECTING;
		return true;
	}

	void ClientSession::Stop()
	{
		if (mChannel)
			mChannel->Close();
		mConfig = {};
		mChannel.reset();
		mWelcome.reset();
		mReject.reset();
		mMessages.clear();
		mState = ClientSessionState::IDLE;
		mLastCursorSequence = 0;
		mLastInputSequence = 0;
		mHasCursorSequence = false;
		mHasInputSequence = false;
		mLastError.clear();
	}

	void ClientSession::Poll()
	{
		if (!mChannel || mState == ClientSessionState::IDLE || mState == ClientSessionState::REJECTED ||
			mState == ClientSessionState::CLOSED || mState == ClientSessionState::FAILED)
			return;

		ReliableChannelState aChannelState = mChannel->Poll();
		if (aChannelState == ReliableChannelState::CONNECTED && mState == ClientSessionState::CONNECTING)
			mState = ClientSessionState::HANDSHAKING;

		for (Message& aMessage : mChannel->TakeMessages())
		{
			if (!mWelcome)
			{
				if (const auto* aWelcome = std::get_if<Welcome>(&aMessage))
				{
					WelcomeValidation aValidation = ValidateWelcome(*aWelcome, mConfig.mSessionId, mConfig.mRulesetId);
					if (aValidation != WelcomeValidation::ACCEPTED)
					{
						Fail("host welcome did not match the selected room");
						return;
					}
					mWelcome = *aWelcome;
					mState = ClientSessionState::CONNECTED;
					continue;
				}
				if (const auto* aReject = std::get_if<Reject>(&aMessage))
				{
					mReject = *aReject;
					mState = ClientSessionState::REJECTED;
					mChannel->Close();
					return;
				}
				Fail("host sent a gameplay message before Welcome");
				return;
			}

			if (const auto* aCursor = std::get_if<CursorUpdate>(&aMessage))
			{
				if (aCursor->mPlayerId >= mWelcome->mMaxPlayers || (aCursor->mButtons & 0xE0U) != 0)
				{
					Fail("host sent an invalid cursor update");
					return;
				}
				mMessages.push_back(std::move(aMessage));
				continue;
			}
			if (const auto* anInput = std::get_if<InputCommand>(&aMessage))
			{
				if (anInput->mPlayerId >= mWelcome->mMaxPlayers)
				{
					Fail("host sent an invalid input command");
					return;
				}
				mMessages.push_back(std::move(aMessage));
				continue;
			}
			if (std::holds_alternative<StateHash>(aMessage))
			{
				mMessages.push_back(std::move(aMessage));
				continue;
			}

			Fail("host sent a message that is invalid after Welcome");
			return;
		}

		if (mState == ClientSessionState::REJECTED || mState == ClientSessionState::FAILED)
			return;
		if (aChannelState == ReliableChannelState::FAILED)
		{
			Fail(mChannel->GetLastError());
			return;
		}
		if (aChannelState == ReliableChannelState::CLOSED)
			mState = ClientSessionState::CLOSED;
	}

	bool ClientSession::SendCursor(CursorUpdate theCursor)
	{
		if (mState != ClientSessionState::CONNECTED || !mChannel || !mWelcome ||
			(mHasCursorSequence && !IsSequenceNewer(theCursor.mSequence, mLastCursorSequence)) || (theCursor.mButtons & 0xE0U) != 0)
			return false;

		theCursor.mPlayerId = mWelcome->mPlayerId;
		if (!mChannel->Queue(theCursor))
			return false;
		mLastCursorSequence = theCursor.mSequence;
		mHasCursorSequence = true;
		return true;
	}

	bool ClientSession::SendInput(InputCommand theInput)
	{
		if (mState != ClientSessionState::CONNECTED || !mChannel || !mWelcome ||
			(mHasInputSequence && !IsSequenceNewer(theInput.mSequence, mLastInputSequence)))
			return false;

		theInput.mPlayerId = mWelcome->mPlayerId;
		if (!mChannel->Queue(theInput))
			return false;
		mLastInputSequence = theInput.mSequence;
		mHasInputSequence = true;
		return true;
	}

	std::vector<Message> ClientSession::TakeMessages()
	{
		std::vector<Message> aMessages;
		aMessages.swap(mMessages);
		return aMessages;
	}

	ClientSessionState ClientSession::GetState() const
	{
		return mState;
	}

	const std::optional<Welcome>& ClientSession::GetWelcome() const
	{
		return mWelcome;
	}

	const std::optional<Reject>& ClientSession::GetReject() const
	{
		return mReject;
	}

	const std::string& ClientSession::GetLastError() const
	{
		return mLastError;
	}

	void ClientSession::Fail(std::string theError)
	{
		mLastError = std::move(theError);
		mState = ClientSessionState::FAILED;
		if (mChannel)
			mChannel->Close();
	}
}
