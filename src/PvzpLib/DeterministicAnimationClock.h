/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <cstdint>

// Advances normalized animation time using integer arithmetic. Gameplay reads
// Reanimation loop/event state, so accumulating platform floating-point deltas
// directly is unsafe for a cross-platform lockstep simulation.
class DeterministicAnimationClock
{
public:
	static constexpr int64_t PHASE_ONE = int64_t{1} << 32;

	void Reset();
	int64_t Advance(float thePublishedPhase, float theRate, int32_t theFrameCount);
	void PublishPhase(int64_t thePhase);

	int64_t GetPhase() const { return mPhase; }
	int64_t GetRemainder() const { return mRemainder; }
	float GetPublishedPhase() const;
	bool IsInitialized() const { return mInitialized; }

	static int64_t FloatToPhase(float theValue);
	static float PhaseToFloat(int64_t theValue);

private:
	void Synchronize(float thePhase, float theRate, int32_t theFrameCount);

	int64_t mPhase{};
	int64_t mRemainder{};
	uint32_t mPublishedPhaseBits{};
	uint32_t mRateBits{};
	int32_t mFrameCount{};
	bool mInitialized{};
};
