/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "DeterministicHash.h"

#ifndef PVZ95_HASH_PRIMITIVES_ONLY
#include "../Lawn/BoardInclude.h"
#include "../Lawn/Cutscene.h"
#include "../LawnApp.h"
#include "../SexyAppFramework/Common.h"
#endif

#include <bit>
#include <type_traits>

namespace PvzMultiplayer
{
	void DeterministicHash64::AddBytes(std::span<const uint8_t> theBytes)
	{
		for (uint8_t aByte : theBytes)
		{
			mHash ^= aByte;
			mHash *= 1099511628211ULL;
		}
	}

	void DeterministicHash64::AddString(std::string_view theString)
	{
		AddU64(theString.size());
		AddBytes(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(theString.data()), theString.size()));
	}

	void DeterministicHash64::AddBool(bool theValue)
	{
		AddU8(theValue ? 1 : 0);
	}

	void DeterministicHash64::AddU8(uint8_t theValue)
	{
		AddBytes(std::span<const uint8_t>(&theValue, 1));
	}

	void DeterministicHash64::AddU32(uint32_t theValue)
	{
		for (int aShift = 0; aShift < 32; aShift += 8)
			AddU8(static_cast<uint8_t>(theValue >> aShift));
	}

	void DeterministicHash64::AddU64(uint64_t theValue)
	{
		for (int aShift = 0; aShift < 64; aShift += 8)
			AddU8(static_cast<uint8_t>(theValue >> aShift));
	}

	void DeterministicHash64::AddI32(int32_t theValue)
	{
		AddU32(static_cast<uint32_t>(theValue));
	}

	void DeterministicHash64::AddFloat(float theValue)
	{
		AddU32(std::bit_cast<uint32_t>(theValue));
	}

	uint64_t DeterministicHash64::Finish() const
	{
		return mHash;
	}

#ifndef PVZ95_HASH_PRIMITIVES_ONLY
	namespace
	{
		template <typename TEnum>
		void AddEnum(DeterministicHash64& theHash, TEnum theValue)
		{
			static_assert(std::is_enum_v<TEnum>);
			theHash.AddI32(static_cast<int32_t>(theValue));
		}

		void AddGameObject(DeterministicHash64& theHash, const GameObject& theObject)
		{
			theHash.AddI32(theObject.mX);
			theHash.AddI32(theObject.mY);
			theHash.AddI32(theObject.mWidth);
			theHash.AddI32(theObject.mHeight);
			theHash.AddBool(theObject.mVisible);
			theHash.AddI32(theObject.mRow);
		}

		template <typename T, size_t N>
		void AddI32Array(DeterministicHash64& theHash, const T (&theValues)[N])
		{
			for (T aValue : theValues)
				theHash.AddI32(static_cast<int32_t>(aValue));
		}

		void AddPlant(DeterministicHash64& h, const Plant& p)
		{
			AddGameObject(h, p);
			AddEnum(h, p.mSeedType); h.AddI32(p.mPlantCol); h.AddI32(p.mAnimCounter); h.AddI32(p.mFrame);
			h.AddI32(p.mFrameLength); h.AddI32(p.mNumFrames); AddEnum(h, p.mState); h.AddI32(p.mPlantHealth);
			h.AddI32(p.mPlantMaxHealth); h.AddI32(p.mSubclass); h.AddI32(p.mDisappearCountdown);
			h.AddI32(p.mDoSpecialCountdown); h.AddI32(p.mStateCountdown); h.AddI32(p.mLaunchCounter);
			h.AddI32(p.mLaunchRate); h.AddI32(p.mTargetX); h.AddI32(p.mTargetY); h.AddI32(p.mStartRow);
			h.AddI32(p.mShootingCounter); h.AddI32(p.mBlinkCountdown); h.AddI32(p.mRecentlyEatenCountdown);
			h.AddI32(p.mEatenFlashCountdown); h.AddI32(p.mBeghouledFlashCountdown); h.AddU32(p.mTargetZombieID);
			h.AddI32(p.mWakeUpCounter); AddEnum(h, p.mOnBungeeState); AddEnum(h, p.mImitaterType);
			h.AddI32(p.mPottedPlantIndex); h.AddBool(p.mAnimPing); h.AddBool(p.mDead); h.AddBool(p.mSquished);
			h.AddBool(p.mIsAsleep); h.AddBool(p.mIsOnBoard);
			for (const MagnetItem& anItem : p.mMagnetItems)
			{
				h.AddFloat(anItem.mPosX); h.AddFloat(anItem.mPosY); h.AddFloat(anItem.mDestOffsetX);
				h.AddFloat(anItem.mDestOffsetY); AddEnum(h, anItem.mItemType);
			}
		}

		void AddZombie(DeterministicHash64& h, const Zombie& z)
		{
			AddGameObject(h, z);
			AddEnum(h, z.mZombieType); AddEnum(h, z.mZombiePhase); h.AddFloat(z.mPosX); h.AddFloat(z.mPosY);
			h.AddFloat(z.mVelX); h.AddI32(z.mAnimCounter); h.AddI32(z.mGroanCounter); h.AddI32(z.mAnimTicksPerFrame);
			h.AddI32(z.mAnimFrames); h.AddI32(z.mFrame); h.AddI32(z.mPrevFrame); h.AddBool(z.mVariant);
			h.AddBool(z.mIsEating); h.AddI32(z.mJustGotShotCounter); h.AddI32(z.mShieldJustGotShotCounter);
			h.AddI32(z.mShieldRecoilCounter); h.AddI32(z.mZombieAge); AddEnum(h, z.mZombieHeight);
			h.AddI32(z.mPhaseCounter); h.AddI32(z.mFromWave); h.AddBool(z.mDroppedLoot); h.AddI32(z.mZombieFade);
			h.AddBool(z.mFlatTires); h.AddI32(z.mUseLadderCol); h.AddI32(z.mTargetCol); h.AddFloat(z.mAltitude);
			h.AddBool(z.mHitUmbrella); h.AddI32(z.mChilledCounter); h.AddI32(z.mButteredCounter); h.AddI32(z.mIceTrapCounter);
			h.AddBool(z.mMindControlled); h.AddBool(z.mBlowingAway); h.AddBool(z.mHasHead); h.AddBool(z.mHasArm);
			h.AddBool(z.mHasObject); h.AddBool(z.mInPool); h.AddBool(z.mOnHighGround); h.AddBool(z.mYuckyFace);
			h.AddI32(z.mYuckyFaceCounter); AddEnum(h, z.mHelmType); h.AddI32(z.mBodyHealth); h.AddI32(z.mBodyMaxHealth);
			h.AddI32(z.mHelmHealth); h.AddI32(z.mHelmMaxHealth); AddEnum(h, z.mShieldType); h.AddI32(z.mShieldHealth);
			h.AddI32(z.mShieldMaxHealth); h.AddI32(z.mFlyingHealth); h.AddI32(z.mFlyingMaxHealth); h.AddBool(z.mDead);
			h.AddU32(z.mRelatedZombieID); for (ZombieID anId : z.mFollowerZombieID) h.AddU32(anId);
			h.AddBool(z.mPlayingSong); h.AddI32(z.mSummonCounter); h.AddFloat(z.mScaleZombie); h.AddFloat(z.mVelZ);
			h.AddFloat(z.mOriginalAnimRate); h.AddU32(z.mTargetPlantID); h.AddI32(z.mBossMode); h.AddI32(z.mTargetRow);
			h.AddI32(z.mBossBungeeCounter); h.AddI32(z.mBossStompCounter); h.AddI32(z.mBossHeadCounter);
			h.AddI32(z.mFireballRow); h.AddBool(z.mIsFireBall); h.AddI32(z.mLastPortalX);
		}

		void AddProjectile(DeterministicHash64& h, const Projectile& p)
		{
			AddGameObject(h, p);
			h.AddI32(p.mFrame); h.AddI32(p.mNumFrames); h.AddI32(p.mAnimCounter); h.AddFloat(p.mPosX);
			h.AddFloat(p.mPosY); h.AddFloat(p.mPosZ); h.AddFloat(p.mVelX); h.AddFloat(p.mVelY); h.AddFloat(p.mVelZ);
			h.AddFloat(p.mAccZ); h.AddFloat(p.mShadowY); h.AddBool(p.mDead); h.AddI32(p.mAnimTicksPerFrame);
			AddEnum(h, p.mMotionType); AddEnum(h, p.mProjectileType); h.AddI32(p.mProjectileAge);
			h.AddI32(p.mClickBackoffCounter); h.AddFloat(p.mRotation); h.AddFloat(p.mRotationSpeed);
			h.AddBool(p.mOnHighGround); h.AddI32(p.mDamageRangeFlags); h.AddI32(p.mHitTorchwoodGridX);
			h.AddFloat(p.mCobTargetX); h.AddI32(p.mCobTargetRow); h.AddU32(p.mTargetZombieID); h.AddI32(p.mLastPortalX);
		}

		void AddCoin(DeterministicHash64& h, const Coin& c)
		{
			AddGameObject(h, c);
			h.AddFloat(c.mPosX); h.AddFloat(c.mPosY); h.AddFloat(c.mVelX); h.AddFloat(c.mVelY); h.AddFloat(c.mScale);
			h.AddBool(c.mDead); h.AddI32(c.mFadeCount); h.AddFloat(c.mCollectX); h.AddFloat(c.mCollectY);
			h.AddI32(c.mGroundY); h.AddI32(c.mCoinAge); h.AddBool(c.mIsBeingCollected); h.AddI32(c.mDisappearCounter);
			AddEnum(h, c.mType); AddEnum(h, c.mCoinMotion); h.AddFloat(c.mCollectionDistance); AddEnum(h, c.mUsableSeedType);
			h.AddBool(c.mNeedsBouncyArrow); h.AddBool(c.mHasBouncyArrow); h.AddBool(c.mHitGround); h.AddI32(c.mTimesDropped);
		}

		void AddMower(DeterministicHash64& h, const LawnMower& m)
		{
			h.AddFloat(m.mPosX); h.AddFloat(m.mPosY); h.AddI32(m.mRow); h.AddI32(m.mAnimTicksPerFrame);
			h.AddI32(m.mChompCounter); h.AddI32(m.mRollingInCounter); h.AddI32(m.mSquishedCounter);
			AddEnum(h, m.mMowerState); h.AddBool(m.mDead); h.AddBool(m.mVisible); AddEnum(h, m.mMowerType);
			h.AddFloat(m.mAltitude); AddEnum(h, m.mMowerHeight); h.AddI32(m.mLastPortalX);
		}

		void AddGridItem(DeterministicHash64& h, const GridItem& i)
		{
			AddEnum(h, i.mGridItemType); AddEnum(h, i.mGridItemState); h.AddI32(i.mGridX); h.AddI32(i.mGridY);
			h.AddI32(i.mGridItemCounter); h.AddBool(i.mDead); h.AddFloat(i.mPosX); h.AddFloat(i.mPosY);
			h.AddFloat(i.mGoalX); h.AddFloat(i.mGoalY); AddEnum(h, i.mZombieType); AddEnum(h, i.mSeedType);
			AddEnum(h, i.mScaryPotType); h.AddI32(i.mTransparentCounter); h.AddI32(i.mSunCount);
			for (const MotionTrailFrame& aFrame : i.mMotionTrailFrames)
			{
				h.AddFloat(aFrame.mPosX); h.AddFloat(aFrame.mPosY); h.AddFloat(aFrame.mAnimTime);
			}
			h.AddI32(i.mMotionTrailCount);
		}
	}

	BoardStateHashBreakdown ComputeBoardStateHashBreakdown(const Board& b)
	{
		BoardStateHashBreakdown aBreakdown;
		DeterministicHash64 h;
		h.AddU32(1); // Canonical hash schema version.
		h.AddI32(static_cast<int32_t>(b.mApp->mGameMode));
		h.AddI32(static_cast<int32_t>(b.mApp->mGameScene));
		h.AddString(Sexy::GetRandState());
		h.AddBool(b.mPaused); h.AddI32(b.mLevel); h.AddI32(b.mSunMoney); h.AddU32(b.mMainCounter);
		h.AddU32(b.mEffectCounter); h.AddU32(b.mBoardUpdateCounter); h.AddI32(b.mCurrentWave);
		h.AddI32(b.mTotalSpawnedWaves); h.AddI32(b.mZombieCountDown); h.AddI32(b.mZombieCountDownStart);
		h.AddI32(b.mHugeWaveCountDown); h.AddI32(b.mZombieHealthToNextWave); h.AddI32(b.mZombieHealthWaveStart);
		h.AddI32(b.mSunCountDown); h.AddI32(b.mNumSunsFallen); h.AddI32(b.mRiseFromGraveCounter);
		h.AddI32(b.mTimeStopCounter); h.AddI32(b.mBoardRandSeed); h.AddBool(b.mLevelComplete);
		h.AddI32(b.mBoardFadeOutCounter); h.AddI32(b.mNextSurvivalStageCounter); h.AddBool(b.mLevelAwardSpawned);
		h.AddI32(b.mFlagRaiseCounter); h.AddI32(b.mIceTrapCounter); h.AddI32(b.mFwooshCountDown);
		h.AddBool(b.mDroppedFirstCoin); h.AddI32(b.mFinalWaveSoundCounter); h.AddBool(b.mFinalBossKilled);
		h.AddI32(b.mTriggeredLawnMowers); h.AddU32(b.mGravesCleared); h.AddU32(b.mPlantsEaten);
		h.AddU32(b.mPlantsShoveled); h.AddU32(b.mLevelCoinsCollected); h.AddU32(b.mCoinsCollected);
		const CutScene& aCutScene = *b.mCutScene;
		h.AddI32(aCutScene.mCutsceneTime); h.AddI32(aCutScene.mSodTime);
		h.AddI32(aCutScene.mGraveStoneTime); h.AddI32(aCutScene.mReadySetPlantTime);
		h.AddI32(aCutScene.mFogTime); h.AddI32(aCutScene.mBossTime); h.AddI32(aCutScene.mCrazyDaveTime);
		h.AddI32(aCutScene.mLawnMowerTime); h.AddI32(aCutScene.mCrazyDaveDialogStart);
		h.AddBool(aCutScene.mSeedChoosing); h.AddBool(aCutScene.mPreloaded);
		h.AddBool(aCutScene.mPlacedZombies); h.AddBool(aCutScene.mPlacedLawnItems);
		h.AddI32(aCutScene.mCrazyDaveCountDown); h.AddI32(aCutScene.mCrazyDaveLastTalkIndex);
		h.AddBool(aCutScene.mUpsellHideBoard); h.AddBool(aCutScene.mPreUpdatingBoard);
		aBreakdown.mCore = h.Finish();
		for (const auto& aColumn : b.mGridSquareType) for (auto aValue : aColumn) AddEnum(h, aValue);
		aBreakdown.mGrid = h.Finish();
		for (const auto& aColumn : b.mGridCelFog) for (int32_t aValue : aColumn) h.AddI32(aValue);
		aBreakdown.mFog = h.Finish();
		for (PlantRowType aValue : b.mPlantRow) AddEnum(h, aValue);
		for (int32_t aValue : b.mIceMinX) h.AddI32(aValue);
		for (int32_t aValue : b.mIceTimer) h.AddI32(aValue);
		aBreakdown.mRowsAndIce = h.Finish();
		// Only the prefix ending at ZOMBIE_INVALID is gameplay state.  PvZ leaves
		// the rest of every fixed-size wave row untouched, so hashing all 50 slots
		// compares uninitialized padding and produces false desyncs between
		// otherwise identical processes.
		h.AddI32(b.mNumWaves);
		for (int aWaveIndex = 0; aWaveIndex < b.mNumWaves; ++aWaveIndex)
		{
			int aZombieCount = 0;
			while (aZombieCount < MAX_ZOMBIES_IN_WAVE &&
				b.mZombiesInWave[aWaveIndex][aZombieCount] != ZombieType::ZOMBIE_INVALID)
			{
				++aZombieCount;
			}
			h.AddI32(aZombieCount);
			for (int aZombieIndex = 0; aZombieIndex < aZombieCount; ++aZombieIndex)
				AddEnum(h, b.mZombiesInWave[aWaveIndex][aZombieIndex]);
		}
		aBreakdown.mWaves = h.Finish();

		h.AddU32(b.mSeedBank->mNumPackets); h.AddI32(b.mSeedBank->mConveyorBeltCounter);
		for (const SeedPacket& p : b.mSeedBank->mSeedPackets)
		{
			h.AddI32(p.mRefreshCounter); h.AddI32(p.mRefreshTime); h.AddI32(p.mIndex); h.AddI32(p.mOffsetX);
			AddEnum(h, p.mPacketType); AddEnum(h, p.mImitaterType); h.AddI32(p.mSlotMachineCountDown);
			AddEnum(h, p.mSlotMachiningNextSeed); h.AddFloat(p.mSlotMachiningPosition); h.AddBool(p.mActive);
			h.AddBool(p.mRefreshing); h.AddI32(p.mTimesUsed);
		}
		aBreakdown.mSeedBank = h.Finish();

		const Challenge& c = *b.mChallenge;
		h.AddI32(c.mBeghouledMouseCapture); h.AddI32(c.mBeghouledMouseDownX); h.AddI32(c.mBeghouledMouseDownY);
		for (const auto& aColumn : c.mBeghouledEated) for (int32_t aValue : aColumn) h.AddI32(aValue);
		AddI32Array(h, c.mBeghouledPurcasedUpgrade); h.AddI32(c.mBeghouledMatchesThisMove);
		AddEnum(h, c.mChallengeState); h.AddI32(c.mChallengeStateCounter); h.AddI32(c.mConveyorBeltCounter);
		h.AddI32(c.mChallengeScore); h.AddI32(c.mShowBowlingLine); AddEnum(h, c.mLastConveyorSeedType);
		h.AddI32(c.mSurvivalStage); h.AddI32(c.mSlotMachineRollCount); AddI32Array(h, c.mCloudsCounter);
		h.AddI32(c.mChallengeGridX); h.AddI32(c.mChallengeGridY); h.AddI32(c.mScaryPotterPots);
		h.AddI32(c.mRainCounter); h.AddI32(c.mTreeOfWisdomTalkIndex);
		aBreakdown.mChallenge = h.Finish();

		h.AddU32(b.mPlants.mSize); for (const Plant* p : b.mPlants) AddPlant(h, *p);
		aBreakdown.mPlants = h.Finish();
		h.AddU32(b.mZombies.mSize); for (const Zombie* z : b.mZombies) AddZombie(h, *z);
		aBreakdown.mZombies = h.Finish();
		h.AddU32(b.mProjectiles.mSize); for (const Projectile* p : b.mProjectiles) AddProjectile(h, *p);
		aBreakdown.mProjectiles = h.Finish();
		h.AddU32(b.mCoins.mSize); for (const Coin* c2 : b.mCoins) AddCoin(h, *c2);
		aBreakdown.mCoins = h.Finish();
		h.AddU32(b.mLawnMowers.mSize); for (const LawnMower* m : b.mLawnMowers) AddMower(h, *m);
		aBreakdown.mMowers = h.Finish();
		h.AddU32(b.mGridItems.mSize); for (const GridItem* i : b.mGridItems) AddGridItem(h, *i);
		aBreakdown.mGridItems = h.Finish();
		return aBreakdown;
	}

	uint64_t ComputeBoardStateHash(const Board& b)
	{
		return ComputeBoardStateHashBreakdown(b).mGridItems;
	}
#endif
}
