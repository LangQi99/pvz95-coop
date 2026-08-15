/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "DeterministicAnimationClock.h"

#include <bit>
#include <climits>
#include <cmath>

namespace
{
	constexpr int32_t UPDATES_PER_SECOND = 100;

	int64_t ShiftMantissaToPhase(uint32_t theMantissa, int theShift)
	{
		if (theShift >= 0)
		{
			if (theShift >= 63 || theMantissa > static_cast<uint64_t>(INT64_MAX) >> theShift)
				return INT64_MAX;
			return static_cast<int64_t>(static_cast<uint64_t>(theMantissa) << theShift);
		}

		const int aRightShift = -theShift;
		if (aRightShift >= 63)
			return 0;
		const uint64_t aHalf = uint64_t{1} << (aRightShift - 1);
		return static_cast<int64_t>((static_cast<uint64_t>(theMantissa) + aHalf) >> aRightShift);
	}
}

void DeterministicAnimationClock::Reset()
{
	mPhase = 0;
	mRemainder = 0;
	mPublishedPhaseBits = 0;
	mRateBits = 0;
	mFrameCount = 0;
	mInitialized = false;
}

int64_t DeterministicAnimationClock::FloatToPhase(float theValue)
{
	if (!std::isfinite(theValue))
		return 0;

	const uint32_t aBits = std::bit_cast<uint32_t>(theValue);
	const bool aNegative = (aBits >> 31) != 0;
	const uint32_t anExponent = (aBits >> 23) & 0xFFU;
	uint32_t aMantissa = aBits & 0x7FFFFFU;
	int aBinaryExponent;
	if (anExponent == 0)
	{
		aBinaryExponent = -126 - 23;
	}
	else
	{
		aMantissa |= 1U << 23;
		aBinaryExponent = static_cast<int>(anExponent) - 127 - 23;
	}

	const int64_t aPhase = ShiftMantissaToPhase(aMantissa, aBinaryExponent + 32);
	return aNegative ? -aPhase : aPhase;
}

float DeterministicAnimationClock::PhaseToFloat(int64_t theValue)
{
	return static_cast<float>(theValue) * (1.0f / 4294967296.0f);
}

void DeterministicAnimationClock::Synchronize(float thePhase, float theRate, int32_t theFrameCount)
{
	mPhase = FloatToPhase(thePhase);
	mRemainder = 0;
	mPublishedPhaseBits = std::bit_cast<uint32_t>(thePhase);
	mRateBits = std::bit_cast<uint32_t>(theRate);
	mFrameCount = theFrameCount;
	mInitialized = true;
}

int64_t DeterministicAnimationClock::Advance(float thePublishedPhase, float theRate, int32_t theFrameCount)
{
	const uint32_t aPhaseBits = std::bit_cast<uint32_t>(thePublishedPhase);
	const uint32_t aRateBits = std::bit_cast<uint32_t>(theRate);
	if (!mInitialized || aPhaseBits != mPublishedPhaseBits || aRateBits != mRateBits ||
		theFrameCount != mFrameCount)
	{
		Synchronize(thePublishedPhase, theRate, theFrameCount);
	}

	if (theFrameCount <= 0 || !std::isfinite(theRate))
		return mPhase;

	const int64_t aDenominator = static_cast<int64_t>(UPDATES_PER_SECOND) * theFrameCount;
	const int64_t aNumerator = FloatToPhase(theRate) + mRemainder;
	mPhase += aNumerator / aDenominator;
	mRemainder = aNumerator % aDenominator;
	mPublishedPhaseBits = std::bit_cast<uint32_t>(PhaseToFloat(mPhase));
	return mPhase;
}

void DeterministicAnimationClock::PublishPhase(int64_t thePhase)
{
	mPhase = thePhase;
	mPublishedPhaseBits = std::bit_cast<uint32_t>(PhaseToFloat(thePhase));
}

float DeterministicAnimationClock::GetPublishedPhase() const
{
	return PhaseToFloat(mPhase);
}
