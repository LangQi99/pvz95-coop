/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "SharedInputState.h"

#include <algorithm>
#include <cstdint>

namespace PvzMultiplayer
{
	namespace
	{
		constexpr std::array<uint32_t, MAX_PLAYERS> CURSOR_COLORS = {
			0xEF5350, // red
			0x42A5F5, // blue
			0x66BB6A, // green
			0xAB47BC  // purple
		};
	}

	uint16_t NormalizeCoordinate(int theCoordinate, int theExtent)
	{
		if (theExtent <= 1)
			return 0;
		int aCoordinate = std::clamp(theCoordinate, 0, theExtent - 1);
		uint64_t aScaled = static_cast<uint64_t>(aCoordinate) * UINT16_MAX +
			static_cast<uint64_t>(theExtent - 1) / 2;
		return static_cast<uint16_t>(aScaled / static_cast<uint64_t>(theExtent - 1));
	}

	int DenormalizeCoordinate(uint16_t theCoordinate, int theExtent)
	{
		if (theExtent <= 1)
			return 0;
		uint64_t aScaled = static_cast<uint64_t>(theCoordinate) * static_cast<uint64_t>(theExtent - 1) +
			UINT16_MAX / 2;
		return static_cast<int>(aScaled / UINT16_MAX);
	}

	uint32_t GetPlayerCursorColor(PlayerId thePlayerId)
	{
		return thePlayerId < CURSOR_COLORS.size() ? CURSOR_COLORS[thePlayerId] : 0;
	}

	bool IsSequenceNewer(uint32_t theSequence, uint32_t thePreviousSequence)
	{
		return static_cast<int32_t>(theSequence - thePreviousSequence) > 0;
	}

	uint16_t InterpolateCursorCoordinate(uint16_t theStart, uint16_t theTarget,
		uint64_t theStartTick, uint64_t theCurrentTick, uint64_t theDuration)
	{
		if (theDuration == 0 || (theCurrentTick >= theStartTick &&
			theCurrentTick - theStartTick >= theDuration))
			return theTarget;
		if (theCurrentTick <= theStartTick)
			return theStart;

		uint64_t anElapsed = theCurrentTick - theStartTick;
		int64_t aStart = static_cast<int64_t>(theStart);
		int64_t aDelta = static_cast<int64_t>(theTarget) - aStart;
		int64_t aScaledDelta = aDelta * static_cast<int64_t>(anElapsed);
		int64_t aHalfDuration = static_cast<int64_t>(theDuration / 2);
		// Round symmetrically so left/up movement is not biased by truncation.
		if (aScaledDelta < 0)
			aScaledDelta -= aHalfDuration;
		else
			aScaledDelta += aHalfDuration;
		int64_t aResult = aStart + aScaledDelta / static_cast<int64_t>(theDuration);
		return static_cast<uint16_t>(std::clamp<int64_t>(aResult, 0, UINT16_MAX));
	}

	CursorPosition SampleCursorPosition(const SharedCursor& theCursor, uint64_t theCurrentTick)
	{
		return {
			InterpolateCursorCoordinate(theCursor.mInterpolationStart.mNormalizedX,
				theCursor.mUpdate.mNormalizedX, theCursor.mInterpolationStartTick, theCurrentTick),
			InterpolateCursorCoordinate(theCursor.mInterpolationStart.mNormalizedY,
				theCursor.mUpdate.mNormalizedY, theCursor.mInterpolationStartTick, theCurrentTick)
		};
	}

	CursorLabelPosition ResolveCursorLabelPosition(int theCursorX, int theCursorY,
		int theLabelWidth, int theLabelHeight, int theViewportWidth, int theViewportHeight)
	{
		constexpr int VIEWPORT_MARGIN = 3;
		constexpr int POINTER_WIDTH = 20;
		constexpr int LABEL_GAP = 4;
		int aLabelWidth = std::max(theLabelWidth, 0);
		int aLabelHeight = std::max(theLabelHeight, 0);
		int aMaxX = std::max(VIEWPORT_MARGIN, theViewportWidth - VIEWPORT_MARGIN - aLabelWidth);
		int aMaxY = std::max(VIEWPORT_MARGIN, theViewportHeight - VIEWPORT_MARGIN - aLabelHeight);

		int aX = theCursorX + POINTER_WIDTH + LABEL_GAP;
		if (aX + aLabelWidth > theViewportWidth - VIEWPORT_MARGIN)
			aX = theCursorX - LABEL_GAP - aLabelWidth;
		int aY = theCursorY + 2;
		if (aY + aLabelHeight > theViewportHeight - VIEWPORT_MARGIN)
			aY = theCursorY - LABEL_GAP - aLabelHeight;

		return {std::clamp(aX, VIEWPORT_MARGIN, aMaxX),
			std::clamp(aY, VIEWPORT_MARGIN, aMaxY)};
	}

	void SharedInputState::Reset(PlayerId theLocalPlayerId)
	{
		mCursors.fill(std::nullopt);
		mLocalPlayerId = theLocalPlayerId < MAX_PLAYERS ? theLocalPlayerId : 0;
	}

	bool SharedInputState::ApplyCursor(CursorUpdate theCursor, uint64_t theReceivedAtTick)
	{
		if (theCursor.mPlayerId >= MAX_PLAYERS)
			return false;
		auto& aSlot = mCursors[theCursor.mPlayerId];
		if (aSlot && !IsSequenceNewer(theCursor.mSequence, aSlot->mUpdate.mSequence))
			return false;

		CursorPosition aTarget{theCursor.mNormalizedX, theCursor.mNormalizedY};
		CursorPosition aStart = aTarget;
		uint64_t anInterpolationStartTick = theReceivedAtTick;
		if (aSlot && aSlot->mUpdate.mVisible && theCursor.mVisible)
		{
			CursorPosition aCurrent = SampleCursorPosition(*aSlot, theReceivedAtTick);
			uint64_t aReceiveGap = theReceivedAtTick >= aSlot->mReceivedAtTick ?
				theReceivedAtTick - aSlot->mReceivedAtTick : UINT64_MAX;
			uint32_t aDistanceX = aCurrent.mNormalizedX > aTarget.mNormalizedX ?
				aCurrent.mNormalizedX - aTarget.mNormalizedX :
				aTarget.mNormalizedX - aCurrent.mNormalizedX;
			uint32_t aDistanceY = aCurrent.mNormalizedY > aTarget.mNormalizedY ?
				aCurrent.mNormalizedY - aTarget.mNormalizedY :
				aTarget.mNormalizedY - aCurrent.mNormalizedY;
			// Multiple snapshots consumed in one Poll are a TCP backlog, not fresh
			// motion samples.  Snap through that burst so stale packets do not create
			// a delayed sweep after a network stall.
			bool aShouldInterpolate = aReceiveGap > 0 &&
				aReceiveGap <= CURSOR_INTERPOLATION_GAP_TICKS &&
				aDistanceX <= CURSOR_INTERPOLATION_SNAP_DISTANCE &&
				aDistanceY <= CURSOR_INTERPOLATION_SNAP_DISTANCE;
			if (aShouldInterpolate)
				aStart = aCurrent;
			// A keepalive with the same target must not restart a completed tween.
			// Preserve both its source and timeline so a stationary cursor remains
			// stationary and an in-flight tween keeps the same velocity.
			if (aShouldInterpolate && aTarget.mNormalizedX == aSlot->mUpdate.mNormalizedX &&
				aTarget.mNormalizedY == aSlot->mUpdate.mNormalizedY)
			{
				aStart = aSlot->mInterpolationStart;
				anInterpolationStartTick = aSlot->mInterpolationStartTick;
			}
		}

		aSlot = SharedCursor{theCursor, GetPlayerCursorColor(theCursor.mPlayerId),
			theReceivedAtTick, aStart, anInterpolationStartTick};
		return true;
	}

	void SharedInputState::RemovePlayer(PlayerId thePlayerId)
	{
		if (thePlayerId < MAX_PLAYERS)
			mCursors[thePlayerId].reset();
	}

	PlayerId SharedInputState::GetLocalPlayerId() const
	{
		return mLocalPlayerId;
	}

	const std::array<std::optional<SharedCursor>, MAX_PLAYERS>& SharedInputState::GetCursors() const
	{
		return mCursors;
	}
}
