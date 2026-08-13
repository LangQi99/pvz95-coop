/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#include <time.h>
#include "LawnApp.h"
#include "Resources.h"
#include "Lawn/LawnCommon.h"
#include "Lawn/Board.h"
#include "Lawn/Coin.h"
#include "Lawn/CursorObject.h"
#include "Lawn/Plant.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/Zombie.h"
#include "Lawn/Cutscene.h"
#include "GameConstants.h"
#include "ProjectVersion.h"
#include "Lawn/Challenge.h"
#include "Lawn/ZenGarden.h"
#include "PvzpLib/Trail.h"
#include "Lawn/System/Music.h"
#include "Lawn/System/SaveGame.h"
#include "PvzpLib/PvzpDebug.h"
#include "PvzpLib/PvzpFoley.h"
#include "PvzpLib/Attachment.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/PoolEffect.h"
#include "Lawn/System/ProfileMgr.h"
#include "Lawn/Widget/GameButton.h"
#include "PvzpLib/Reanimator.h"
#include "Lawn/Widget/UserDialog.h"
#include "Lawn/System/TypingCheck.h"
#include "PvzpLib/PvzpParticle.h"
#include "Lawn/Widget/AwardScreen.h"
#include "Lawn/Widget/TitleScreen.h"
#include "Lawn/Widget/StoreScreen.h"
#include "Lawn/Widget/CheatDialog.h"
#include "Lawn/Widget/GameSelector.h"
#include "Lawn/Widget/CreditScreen.h"
#include "PvzpLib/EffectSystem.h"
#include "PvzpLib/FilterEffect.h"
#include "graphics/Graphics.h"
#include "graphics/Font.h"
#include "PvzpLib/PvzpStringFile.h"
#include "Lawn/Widget/AlmanacDialog.h"
#include "Lawn/Widget/NewUserDialog.h"
#include "Lawn/Widget/JoinLanDialog.h"
#include "Lawn/Widget/ContinueDialog.h"
#include "Lawn/System/ReanimationLawn.h"
#include "Lawn/Widget/ChallengeScreen.h"
#include "Lawn/Widget/NewOptionsDialog.h"
#include "Lawn/Widget/ZombatarTOS.h"
#include "Lawn/Widget/SeedChooserScreen.h"
#include "GameRules/Ruleset.h"
#include "Multiplayer/DeterministicHash.h"
#include "Multiplayer/LanCoordinator.h"
#include "Multiplayer/LocalInput.h"
#include "Multiplayer/RollingLog.h"
#include "widget/WidgetManager.h"
#include "misc/ResourceManager.h"
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <string>
#include <utility>

#include "widget/Checkbox.h"
#include "widget/Dialog.h"
#include "SexyAppFramework/resource.h"

bool gIsPartnerBuild = false;
bool gSlowMo = false;
bool gFastMo = false;
LawnApp* gLawnApp = nullptr;
int gSlowMoCounter = 0;

static bool HasUnshownAchievements(PlayerInfo* thePlayerInfo)
{
	if (thePlayerInfo == nullptr)
	{
		return false;
	}

	for (int i = 0; i < MAX_ACHIEVEMENTS; i++)
	{
		if (thePlayerInfo->mEarnedAchievements[i] && !thePlayerInfo->mShownAchievements[i])
		{
			return true;
		}
	}

	return false;
}

namespace
{
	constexpr uint64_t LAN_CURSOR_SEND_INTERVAL = 4;
	constexpr uint64_t LAN_CURSOR_KEEPALIVE_INTERVAL = 50;
	constexpr uint64_t LAN_CURSOR_TIMEOUT = 300;
	constexpr uint64_t LAN_INPUT_DELAY = 12;
	constexpr uint64_t LAN_TICK_SYNC_INTERVAL = 1;
	constexpr const char* LAN_TRACE_FILE = "lan-sync.log";
	constexpr std::size_t LAN_TRACE_FILE_BYTES = 4U * 1024U * 1024U;
	constexpr std::size_t LAN_TRACE_BACKUP_COUNT = 3;

	std::string LanTraceTimestamp()
	{
		using namespace std::chrono;
		auto aNow = system_clock::now();
		auto aTime = system_clock::to_time_t(aNow);
		std::tm aLocalTime{};
#ifdef _WIN32
		localtime_s(&aLocalTime, &aTime);
#else
		localtime_r(&aTime, &aLocalTime);
#endif
		char aBuffer[32]{};
		std::strftime(aBuffer, sizeof(aBuffer), "%Y-%m-%d %H:%M:%S", &aLocalTime);
		auto aMillis = duration_cast<milliseconds>(aNow.time_since_epoch()) % 1000;
		return Sexy::StrFormat("%s.%03u", aBuffer, static_cast<unsigned>(aMillis.count()));
	}

	void LanTrace(const char* theFormat, ...)
	{
		va_list anArgs;
		va_start(anArgs, theFormat);
		std::string aMessage = Sexy::VFormat(theFormat, anArgs);
		va_end(anArgs);
		if (aMessage.empty())
			return;
		if (aMessage.back() != '\n')
			aMessage.push_back('\n');
		std::string aRecord = "[" + LanTraceTimestamp() + "] " + aMessage;
		PvzMultiplayer::AppendRollingLog(
			Sexy::PathFromU8(Sexy::GetAppDataPath(LAN_TRACE_FILE)), aRecord,
			{LAN_TRACE_FILE_BYTES, LAN_TRACE_BACKUP_COUNT});
	}

	bool IsValidPointerClickCount(int theClickCount)
	{
		return theClickCount == -2 || theClickCount == -1 || theClickCount == 1 ||
			theClickCount == 2 || theClickCount == 3;
	}
}

bool LawnGetCloseRequest()
{
	if (gLawnApp == nullptr)
		return false;

	return gLawnApp->mCloseRequest;
}

bool LawnHasUsedCheatKeys()
{
	return gLawnApp && gLawnApp->mPlayerInfo && gLawnApp->mPlayerInfo->mHasUsedCheatKeys;
}

LawnApp::LawnApp()
{
	// Replace the base-class resource manager with the PvZP-capable subclass.
	delete mResourceManager;
	mResourceManager = new PvzpResourceManager(this);

	mBoard = nullptr;
	mGameSelector = nullptr;
	mChallengeScreen = nullptr;
	mSeedChooserScreen = nullptr;
	mAwardScreen = nullptr;
	mCreditScreen = nullptr;
	mTitleScreen = nullptr;
	mSoundSystem = nullptr;
	mMusic = nullptr;
	mKonamiCheck = nullptr;
	mMustacheCheck = nullptr;
	mMoustacheCheck = nullptr;
	mSuperMowerCheck = nullptr;
	mSuperMowerCheck2 = nullptr;
	mFutureCheck = nullptr;
	mPinataCheck = nullptr;
	mDanceCheck = nullptr;
	mDaisyCheck = nullptr;
	mSukhbirCheck = nullptr;
	mMustacheMode = false;
	mSuperMowerMode = false;
	mFutureMode = false;
	mPinataMode = false;
	mDanceMode = false;
	mDaisyMode = false;
	mSukhbirMode = false;
	mGameScene = GameScenes::SCENE_LOADING;
	mPoolEffect = nullptr;
	mZenGarden = nullptr;
	mEffectSystem = nullptr;
	mReanimatorCache = nullptr;
	mCloseRequest = false;
	mWidth = BOARD_WIDTH;
	mHeight = BOARD_HEIGHT;
	mFullscreenBits = 32;
	mAppCounter = 0;
	mAppRandSeed = time(0);
	mTrialType = TrialType::TRIALTYPE_NONE;
	mDebugTrialLocked = false;
	mMuteSoundsForCutscene = false;
	mMusicVolume = 0.85;
	mSfxVolume = 0.5525;
	mAutoStartLoadingThread = false;
	mDebugKeysEnabled = false;
	mProdName = "io.github.langqi99.pvz95-coop";
	mProductVersion = PVZP_VERSION;
	mBuildNum = PVZP_BUILD_NUMBER;
	mCommitDate = PVZP_COMMIT_DATE;
	std::string aTitleName = "PvZ 95 Co-op";
	mTitle = aTitleName;
	mCustomCursorsEnabled = false;
	mPlayerInfo = nullptr;
	mLastLevelStats = new LevelStats();
	mFirstTimeGameSelector = true;
	mGameMode = GameMode::GAMEMODE_ADVENTURE;
	mEasyPlantingCheat = false;
	mAutoEnable3D = true;
	Pvzp_SWTri_AddAllDrawTriFuncs();
	mLoadingZombiesThreadCompleted = true;
	mGamesPlayed = 0;
	mMaxExecutions = 0;
	mMaxPlays = 0;
	mMaxTime = 0;
	mCompletedLoadingThreadTasks = 0;
	mProfileMgr = new ProfileMgr();
	mRegisterResourcesLoaded = false;
	mCheatKeys = false;
	mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
	mCrazyDaveBlinkCounter = 0;
	mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveMessageIndex = -1;
	mLanCoordinator = std::make_unique<PvzMultiplayer::LanCoordinator>();
}

LawnApp::~LawnApp()
{
	while (!mDialogMap.empty())
	{
		KillDialog(mDialogMap.begin()->first);
	}

	if (mBoard)
	{
		mBoardResult = BoardResult::BOARDRESULT_QUIT_APP;
		WriteCurrentUserConfig();
		KillBoard();
	}
	ProcessSafeDeleteList();

	if (mTitleScreen)
	{
		mWidgetManager->RemoveWidget(mTitleScreen);
		delete mTitleScreen;
	}

	delete mSoundSystem;
	delete mMusic;

	if (mKonamiCheck)
	{
		delete mKonamiCheck;
	}
	if (mMustacheCheck)
	{
		delete mMustacheCheck;
	}
	if (mMoustacheCheck)
	{
		delete mMoustacheCheck;
	}
	if (mSuperMowerCheck)
	{
		delete mSuperMowerCheck;
	}
	if (mSuperMowerCheck2)
	{
		delete mSuperMowerCheck2;
	}
	if (mFutureCheck)
	{
		delete mFutureCheck;
	}
	if (mPinataCheck)
	{
		delete mPinataCheck;
	}
	if (mDanceCheck)
	{
		delete mDanceCheck;
	}
	if (mDaisyCheck)
	{
		delete mDaisyCheck;
	}
	if (mSukhbirCheck)
	{
		delete mSukhbirCheck;
	}

	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		delete mGameSelector;
	}
	if (mChallengeScreen)
	{
		mWidgetManager->RemoveWidget(mChallengeScreen);
		delete mChallengeScreen;
	}
	if (mSeedChooserScreen)
	{
		mWidgetManager->RemoveWidget(mSeedChooserScreen);
		delete mSeedChooserScreen;
	}
	if (mAwardScreen)
	{
		mWidgetManager->RemoveWidget(mAwardScreen);
		delete mAwardScreen;
	}
	if (mCreditScreen)
	{
		mWidgetManager->RemoveWidget(mCreditScreen);
		delete mCreditScreen;
	}

	if (mPoolEffect)
	{
		delete mPoolEffect;
	}

	if (mZenGarden)
	{
		delete mZenGarden;
	}

	if (mEffectSystem)
	{
		delete mEffectSystem;
	}

	if (mReanimatorCache)
	{
		delete mReanimatorCache;
	}

	FilterEffectDisposeForApp();
	PvzpParticleFreeDefinitions();
	ReanimatorFreeDefinitions();
	TrailFreeDefinitions();
	FreeGlobalAllocators();
	UpdateRegisterInfo();

	delete mProfileMgr;
	delete mLastLevelStats;

	mResourceManager->DeleteResources("");
}

void LawnApp::Shutdown()
{
	if (!mLoadingThreadCompleted)
	{
		mLoadingFailed = true;
		SexyAppBase::Shutdown();
		return;
	}

	if (!mShutdown)
	{
		SexyAppBase::Shutdown();
	}
}

void LawnApp::ShutdownHook()
{
	// Save mid-level game while the music is still alive, before Shutdown() stops it.
	if (mBoard)
	{
		mBoardResult = BoardResult::BOARDRESULT_QUIT_APP;
		mBoard->TryToSaveGame();
	}
}

void LawnApp::KillBoard()
{
	FinishModelessDialogs();
	KillSeedChooserScreen();
	if (mBoard)
	{
		if (mPlayerInfo && (
			mBoardResult == BoardResult::BOARDRESULT_WON ||
			mBoardResult == BoardResult::BOARDRESULT_LOST ||
			mBoardResult == BoardResult::BOARDRESULT_RESTART ||
			mBoardResult == BoardResult::BOARDRESULT_CHEAT))
		{
			std::string aFileName = GetSavedGameName(mGameMode, mPlayerInfo->mId);
			EraseFile(aFileName);
			std::string aLegacyFileName = GetLegacySavedGameName(mGameMode, mPlayerInfo->mId);
			EraseFile(aLegacyFileName);
		}

		mBoard->DisposeBoard();
		mWidgetManager->RemoveWidget(mBoard);
		SafeDeleteWidget(mBoard);
		mBoard = nullptr;
	}

	SetCursor(CURSOR_POINTER);
}

bool LawnApp::CanPauseNow()
{
	if (mBoard == nullptr)
		return false;
	if (IsLanGameplayActive())
		return false;

	if (mSeedChooserScreen && mSeedChooserScreen->mMouseVisible)
		return false;

	if (mBoard->mBoardFadeOutCounter >= 0)
		return false;

	if (mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		return false;

	return GetDialogCount() <= 0;
}

void LawnApp::GotFocus()
{
	if (mLanCoordinator)
		LanTrace("focus gained app=%u sim=%llu mode=%u seed=%d\n", mAppCounter,
			static_cast<unsigned long long>(mLanSimulationTick),
			static_cast<unsigned>(mLanCoordinator->GetMode()), mLocalLanSeedBankIndex);
}

void LawnApp::LostFocus()
{
	if (mLanCoordinator)
		LanTrace("focus lost app=%u sim=%llu mode=%u seed=%d\n", mAppCounter,
			static_cast<unsigned long long>(mLanSimulationTick),
			static_cast<unsigned>(mLanCoordinator->GetMode()), mLocalLanSeedBankIndex);
#if (defined(__ANDROID__) && !defined(__TERMUX__)) || defined(__IPHONEOS__)
	if (!mCheatKeys && CanPauseNow())
	{
		DoPauseDialog();
	}
#endif
}

void LawnApp::WriteToRegistry()
{
	PlayerInfo* aPersistentProfile = mLocalPlayerInfo ? mLocalPlayerInfo : mPlayerInfo;
	if (aPersistentProfile)
	{
		RegistryWriteString("CurUser", aPersistentProfile->mName);
		aPersistentProfile->SaveDetails();
	}

	SexyAppBase::WriteToRegistry();
}

void LawnApp::ReadFromRegistry()
{
	SexyApp::ReadFromRegistry();
}

bool LawnApp::WriteCurrentUserConfig()
{
	PlayerInfo* aPersistentProfile = mLocalPlayerInfo ? mLocalPlayerInfo : mPlayerInfo;
	if (aPersistentProfile)
		aPersistentProfile->SaveDetails();

	return true;
}

void LawnApp::PreNewGame(GameMode theGameMode, bool theLookForSavedGame)
{
	//if (NeedRegister())
	//{
	//	ShowGameSelector();
	//	return;
	//}

	if (mApplyingLanSessionStart)
	{
		mGameMode = theGameMode;
		NewGame();
		return;
	}
	PvzMultiplayer::LanMode aLanMode = mLanCoordinator->GetMode();
	if (PvzMultiplayer::IsLanClientWaitingForHost(aLanMode))
	{
		LanTrace("blocked client local game start mode=%u gameMode=%u\n",
			static_cast<unsigned>(aLanMode), static_cast<unsigned>(theGameMode));
		PlaySample(Sexy::SOUND_BUZZER);
		return;
	}
	if (BeginLanGame(theGameMode))
		return;

	mGameMode = theGameMode;
	if (theLookForSavedGame && TryLoadGame())
		return;

	std::string aFileName = GetSavedGameName(mGameMode, mPlayerInfo->mId);
	EraseFile(aFileName);
	std::string aLegacyFileName = GetLegacySavedGameName(mGameMode, mPlayerInfo->mId);
	EraseFile(aLegacyFileName);
	NewGame();
}

void LawnApp::MakeNewBoard()
{
	KillBoard();
	mBoard = new Board(this);
	mBoard->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mBoard);
	mWidgetManager->BringToBack(mBoard);
	mWidgetManager->SetFocus(mBoard);
}

void LawnApp::StartPlaying()
{
	KillSeedChooserScreen();
	mBoard->StartLevel();
	mGameScene = GameScenes::SCENE_PLAYING;
}

bool LawnApp::SaveFileExists()
{
	std::string aFileName = GetSavedGameName(GameMode::GAMEMODE_ADVENTURE, mPlayerInfo->mId);
	if (this->FileExists(aFileName))
		return true;
	std::string aLegacyFileName = GetLegacySavedGameName(GameMode::GAMEMODE_ADVENTURE, mPlayerInfo->mId);
	return this->FileExists(aLegacyFileName);
}

bool LawnApp::TryLoadGame()
{
	std::string aSaveName = GetSavedGameName(mGameMode, mPlayerInfo->mId);
	std::string aLegacySaveName = GetLegacySavedGameName(mGameMode, mPlayerInfo->mId);
	mMusic->StopAllMusic();

	if (this->FileExists(aSaveName))
	{
		MakeNewBoard();
		if (mBoard->LoadGame(aSaveName))
		{
			mFirstTimeGameSelector = false;
			if (mBoard->mLevelAwardSpawned) // Ensure save cleanup after award collection
				mBoardResult = BoardResult::BOARDRESULT_WON;
			DoContinueDialog();
			return true;
		}

		KillBoard();
	}
	if (this->FileExists(aLegacySaveName))
	{
		MakeNewBoard();
		if (mBoard->LoadGame(aLegacySaveName))
		{
			if (LawnSaveGame(mBoard, aSaveName))
			{
				EraseFile(aLegacySaveName);
			}
			mFirstTimeGameSelector = false;
			if (mBoard->mLevelAwardSpawned) // Ensure save cleanup after award collection
				mBoardResult = BoardResult::BOARDRESULT_WON;
			DoContinueDialog();
			return true;
		}

		KillBoard();
	}

	return false;
}

void LawnApp::NewGame()
{
	mFirstTimeGameSelector = false;

	MakeNewBoard();
	mBoard->InitLevel();
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mGameScene = GameScenes::SCENE_LEVEL_INTRO;

	ShowSeedChooserScreen();
	mBoard->mCutScene->StartLevelIntro();
}

void LawnApp::ShowGameSelector()
{
	KillBoard();
	//UpdateRegisterInfo();
	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		SafeDeleteWidget(mGameSelector);
	}

	mGameScene = GameScenes::SCENE_MENU;
	mGameSelector = new GameSelector(this);
	mGameSelector->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mGameSelector);
	mWidgetManager->BringToBack(mGameSelector);
	mWidgetManager->SetFocus(mGameSelector);

	//if (NeedRegister())
	//{
	//	DoNeedRegisterDialog();
	//}
}

void LawnApp::KillGameSelector()
{
	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		SafeDeleteWidget(mGameSelector);
		mGameSelector = nullptr;
	}
}

void LawnApp::ShowAwardScreen(AwardType theAwardType, bool theShowAchievements)
{
	mGameScene = GameScenes::SCENE_AWARD;
	mAwardScreen = new AwardScreen(this, theAwardType, theShowAchievements);
	mAwardScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mAwardScreen);
	mWidgetManager->BringToBack(mAwardScreen);
	mWidgetManager->SetFocus(mAwardScreen);
}

void LawnApp::KillAwardScreen()
{
	if (mAwardScreen)
	{
		mWidgetManager->RemoveWidget(mAwardScreen);
		SafeDeleteWidget(mAwardScreen);
		mAwardScreen = nullptr;
	}
}

void LawnApp::ShowCreditScreen()
{
	mCreditScreen = new CreditScreen(this);
	mCreditScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mCreditScreen);
	mWidgetManager->BringToBack(mCreditScreen);
	mWidgetManager->SetFocus(mCreditScreen);
}

void LawnApp::KillCreditScreen()
{
	if (mCreditScreen)
	{
		mWidgetManager->RemoveWidget(mCreditScreen);
		SafeDeleteWidget(mCreditScreen);
		mCreditScreen = nullptr;
	}
}

void LawnApp::ShowChallengeScreen(ChallengePage thePage)
{
	mGameScene = GameScenes::SCENE_CHALLENGE;
	mChallengeScreen = new ChallengeScreen(this, thePage);
	mChallengeScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mChallengeScreen);
	mWidgetManager->BringToBack(mChallengeScreen);
	mWidgetManager->SetFocus(mChallengeScreen);
}

void LawnApp::KillChallengeScreen()
{
	if (mChallengeScreen)
	{
		mWidgetManager->RemoveWidget(mChallengeScreen);
		SafeDeleteWidget(mChallengeScreen);
		mChallengeScreen = nullptr;
	}
}

StoreScreen* LawnApp::ShowStoreScreen()
{
	//FinishModelessDialogs();
	PVZP_ASSERT(!GetDialog(static_cast<int>(Dialogs::DIALOG_STORE)));

	StoreScreen* aStoreScreen = new StoreScreen(this);
	AddDialog(aStoreScreen);
	mWidgetManager->SetFocus(aStoreScreen);

	return aStoreScreen;
}

void LawnApp::KillStoreScreen()
{
	if (GetDialog(Dialogs::DIALOG_STORE))
	{
		KillDialog(Dialogs::DIALOG_STORE);
		ClearUpdateBacklog(false);
	}
}

void LawnApp::ShowSeedChooserScreen()
{
	PVZP_ASSERT(mSeedChooserScreen == nullptr);

	mSeedChooserScreen = new SeedChooserScreen();
	mSeedChooserScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mSeedChooserScreen);
	mWidgetManager->BringToBack(mSeedChooserScreen);
}

void LawnApp::KillSeedChooserScreen()
{
	if (mSeedChooserScreen)
	{
		mWidgetManager->RemoveWidget(mSeedChooserScreen);
		SafeDeleteWidget(mSeedChooserScreen);
		mSeedChooserScreen = nullptr;
	}
}

void LawnApp::EndLevel()
{
	// ZombieWonClick can also reach this method directly.  Keep every restart
	// path behind the same host-authoritative LAN transition.
	if (RequestLanLevelRestart())
		return;

	KillBoard();
	if (IsAdventureMode())
	{
		NewGame();
	}

	mFirstTimeGameSelector = true;

	MakeNewBoard();
	mBoard->InitLevel();
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mGameScene = GameScenes::SCENE_LEVEL_INTRO;
	ShowSeedChooserScreen();
	mBoard->mCutScene->StartLevelIntro();
}

void LawnApp::DoBackToMain()
{
	mMusic->StopAllMusic();
	mSoundSystem->CancelPausedFoley();
	WriteCurrentUserConfig();
	KillNewOptionsDialog();
	KillBoard();
	ShowGameSelector();
}

void LawnApp::DoConfirmBackToMain()
{
	LawnDialog* aDialog = (LawnDialog*)DoDialog(
		Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN,
		true,
		GetString("LEAVE_GAME_HEADER", "Leave Game?"),
		GetString("LEAVE_GAME",
			"Do you want to return\nto the main menu?\n\nYour game will be saved."),
		"",
		Dialog::BUTTONS_YES_NO
	);

	aDialog->mLawnYesButton->mLabel = PvzpStringTranslate("[LEAVE_BUTTON]");
	aDialog->mLawnNoButton->mLabel = PvzpStringTranslate("[DIALOG_BUTTON_CANCEL]");
	//aDialog->CalcSize(0, 0);
}

void LawnApp::DoNewOptions(bool theFromGameSelector)
{
	//FinishModelessDialogs();

	NewOptionsDialog* aDialog = new NewOptionsDialog(this, theFromGameSelector);
	CenterDialog(aDialog, IMAGE_OPTIONS_MENUBACK->mWidth, IMAGE_OPTIONS_MENUBACK->mHeight);
	AddDialog(Dialogs::DIALOG_NEWOPTIONS, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

void LawnApp::ShowZombatarTOS()
{
	ZombatarTOS* aDialog = new ZombatarTOS(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_ZOMBATAR_TOS, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

AlmanacDialog* LawnApp::DoAlmanacDialog(SeedType theSeedType, ZombieType theZombieType)
{
	PerfTimer mTimer;
	mTimer.Start();

	//FinishModelessDialogs();

	AlmanacDialog* aDialog = new AlmanacDialog(this);
	AddDialog(Dialogs::DIALOG_ALMANAC, aDialog);
	mWidgetManager->SetFocus(aDialog);

	if (theSeedType != SeedType::SEED_NONE)
	{
		aDialog->ShowPlant(theSeedType);
	}
	else if (theZombieType != ZombieType::ZOMBIE_INVALID)
	{
		aDialog->ShowZombie(theZombieType);
	}

	int aDuration = mTimer.GetDuration();
	PvzpTrace("almanac load time: %d ms", aDuration);

	return aDialog;
}

void LawnApp::DoContinueDialog()
{
	ContinueDialog* aDialog = new ContinueDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CONTINUE, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

void LawnApp::DoPauseDialog()
{
	mBoard->Pause(true);
	//FinishModelessDialogs();

	LawnDialog* aDialog = (LawnDialog*)DoDialog(
		Dialogs::DIALOG_PAUSED,
		true,
		GetString("GAME_PAUSED", "GAME PAUSED"),
		GetString("CLICK_TO_RESUME", "Click to resume game"),
		GetString("RESUME_GAME", "Resume Game"),
		Dialog::BUTTONS_FOOTER
	);

	aDialog->mReanimation->AddReanimation(72.0f, 42.0f, ReanimationType::REANIM_ZOMBIE_NEWSPAPER);
	aDialog->mSpaceAfterHeader = 155;
	aDialog->CalcSize(0, 10);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
}

int LawnApp::LawnMessageBox(int theDialogId, const char* theHeaderName, const char* theLinesName, const char* theButton1Name, const char* theButton2Name, int theButtonMode)
{
	Widget* aOldFocus = mWidgetManager->mFocusWidget;

	LawnDialog* aDialog = (LawnDialog*)DoDialog(theDialogId, true, theHeaderName, theLinesName, theButton1Name, theButtonMode);
	if (aDialog->mLawnYesButton)
	{
		aDialog->mLawnYesButton->mLabel = PvzpStringTranslate(theButton1Name);
	}
	if (aDialog->mLawnNoButton)
	{
		aDialog->mLawnNoButton->mLabel = PvzpStringTranslate(theButton2Name);
	}
	//aDialog->CalcSize(0, 0);

	mWidgetManager->SetFocus(aDialog);
	int aResult = aDialog->WaitForResult(true);
	mWidgetManager->SetFocus(aOldFocus);

	return aResult;
}

Dialog* LawnApp::DoDialog(int theDialogId, bool isModal, const std::string& theDialogHeader, const std::string& theDialogLines, const std::string& theDialogFooter, int theButtonMode)
{
	std::string aHeader = PvzpStringTranslate(theDialogHeader);
	std::string aLines = PvzpStringTranslate(theDialogLines);
	std::string aFooter = PvzpStringTranslate(theDialogFooter);

	Dialog* aDialog = SexyAppBase::DoDialog(theDialogId, isModal, aHeader, aLines, aFooter, theButtonMode);
	if (mWidgetManager->mFocusWidget == nullptr)
	{
		mWidgetManager->mFocusWidget = aDialog;
	}

	return aDialog;
}

Dialog* LawnApp::DoDialogDelay(int theDialogId, bool isModal, const std::string& theDialogHeader, const std::string& theDialogLines, const std::string& theDialogFooter, int theButtonMode)
{
	LawnDialog* aDialog = (LawnDialog*)SexyAppBase::DoDialog(theDialogId, isModal, theDialogHeader, theDialogLines, theDialogFooter, theButtonMode);
	aDialog->SetButtonDelay(30);
	return aDialog;
}

void LawnApp::DoUserDialog()
{
	KillDialog(Dialogs::DIALOG_USERDIALOG);

	UserDialog* aDialog = new UserDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_USERDIALOG, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

void LawnApp::FinishUserDialog(bool isYes)
{
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (aUserDialog)
	{
		if (isYes)
		{
			PlayerInfo* aProfile = mProfileMgr->GetProfile(aUserDialog->GetSelName());
			if (aProfile)
			{
				mPlayerInfo = aProfile;
				mWidgetManager->MarkAllDirty();

				if (mGameSelector)
				{
					mGameSelector->SyncProfile(true);
				}
			}
		}

		KillDialog(Dialogs::DIALOG_USERDIALOG);
	}
}

void LawnApp::DoCreateUserDialog()
{
	KillDialog(Dialogs::DIALOG_CREATEUSER);

	NewUserDialog* aDialog = new NewUserDialog(this, false);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CREATEUSER, aDialog);
}

void LawnApp::FinishCreateUserDialog(bool isYes)
{
	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(Dialogs::DIALOG_CREATEUSER);
	if (aNewUserDialog == nullptr)
		return;

	std::string aName = aNewUserDialog->GetName();

	if (isYes && aName.empty())
	{
		DoDialog(
			Dialogs::DIALOG_CREATEUSERERROR,
			true,
			GetString("ENTER_YOUR_NAME", "Enter Your Name"),
			GetString("USER_ERROR_MESSAGE",
				"Please enter your name to create a new user profile for storing high score data and game progress."),
			"[DIALOG_BUTTON_OK]",
			Dialog::BUTTONS_FOOTER
		);
	}
	else if (mPlayerInfo == nullptr && (!isYes || aName.empty()))
	{
		DoDialog(
			Dialogs::DIALOG_CREATEUSERERROR,
			true,
			GetString("ENTER_YOUR_NAME", "Enter Your Name"),
			GetString("USER_ERROR_MESSAGE",
				"Please enter your name to create a new user profile for storing high score data and game progress."),
			"[DIALOG_BUTTON_OK]",
			Dialog::BUTTONS_FOOTER
		);
	}
	else if (!isYes)
	{
		KillDialog(Dialogs::DIALOG_CREATEUSER);
	}
	else
	{
		PlayerInfo* aProfile = mProfileMgr->AddProfile(aName);
		if (aProfile == nullptr)
		{
			DoDialog(
				Dialogs::DIALOG_CREATEUSERERROR,
				true,
				GetString("NAME_CONFLICT", "Name Conflict"),
				GetString("ENTER_UNIQUE_PLAYER_NAME",
					"The name you entered is already being used.  Please enter a unique player name."),
				"[DIALOG_BUTTON_OK]",
				Dialog::BUTTONS_FOOTER
			);
		}
		else
		{
			mProfileMgr->Save();
			mPlayerInfo = aProfile;

			KillDialog(Dialogs::DIALOG_USERDIALOG);
			KillDialog(Dialogs::DIALOG_CREATEUSER);
			mWidgetManager->MarkAllDirty();

			if (mGameSelector)
			{
				mGameSelector->SyncProfile(true);
			}
		}
	}
}

void LawnApp::DoConfirmDeleteUserDialog(const std::string& theName)
{
	KillDialog(Dialogs::DIALOG_CONFIRMDELETEUSER);
	DoDialog(
		Dialogs::DIALOG_CONFIRMDELETEUSER,
		true,
		GetString("ARE_YOU_SURE", "Are You Sure?"),
		StrFormat(
			GetString("DELETE_USER_WARNING", "This will permanently remove '%s' from the player roster!").c_str(),
			theName.c_str()),
		"",
		Dialog::BUTTONS_YES_NO
	);
}

void LawnApp::FinishConfirmDeleteUserDialog(bool isYes)
{
	KillDialog(Dialogs::DIALOG_CONFIRMDELETEUSER);
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (aUserDialog == nullptr)
		return;

	mWidgetManager->SetFocus(aUserDialog);

	if (!isYes)
		return;

	std::string aCurName = mPlayerInfo ? mPlayerInfo->mName : "";
	std::string aName = aUserDialog->GetSelName();
	if (aName == aCurName)
	{
		mPlayerInfo = nullptr;
	}

	mProfileMgr->DeleteProfile(aName);
	aUserDialog->FinishDeleteUser();
	if (mPlayerInfo == nullptr)
	{
		mPlayerInfo = mProfileMgr->GetProfile(aUserDialog->GetSelName());
		if (mPlayerInfo == nullptr)
		{
			mPlayerInfo = mProfileMgr->GetAnyProfile();
		}
	}

	mProfileMgr->Save();
	if (mPlayerInfo == nullptr)
	{
		DoCreateUserDialog();
	}

	mWidgetManager->MarkAllDirty();
	if (mGameSelector != nullptr)
	{
		mGameSelector->SyncProfile(true);
	}
}

void LawnApp::DoRenameUserDialog(const std::string& theName)
{
	KillDialog(Dialogs::DIALOG_RENAMEUSER);

	NewUserDialog* aDialog = new NewUserDialog(this, true);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	aDialog->SetName(theName);
	AddDialog(Dialogs::DIALOG_RENAMEUSER, aDialog);
}

void LawnApp::FinishRenameUserDialog(bool isYes)
{
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (!isYes)
	{
		KillDialog(Dialogs::DIALOG_RENAMEUSER);
		mWidgetManager->SetFocus(aUserDialog);
		return;
	}

	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(Dialogs::DIALOG_RENAMEUSER);
	if (aUserDialog == nullptr || aNewUserDialog == nullptr)
		return;

	std::string anOldName = aUserDialog->GetSelName();
	std::string aNewName = aNewUserDialog->GetName();
	if (aNewName.empty())
		return;

	bool isCurrentUser = mProfileMgr->GetProfile(anOldName) == mPlayerInfo;
	if (!mProfileMgr->RenameProfile(anOldName, aNewName))
	{
		DoDialog(
			Dialogs::DIALOG_RENAMEUSERERROR,
			true,
			GetString("NAME_CONFLICT", "Name Conflict"),
			GetString("ENTER_UNIQUE_PLAYER_NAME",
				"The name you entered is already being used.  Please enter a unique player name."),
			"[DIALOG_BUTTON_OK]",
			Dialog::BUTTONS_FOOTER
		);
		return;
	}

	mProfileMgr->Save();
	if (isCurrentUser)
	{
		mPlayerInfo = mProfileMgr->GetProfile(aNewName);
	}

	aUserDialog->FinishRenameUser(aNewName);
	mWidgetManager->MarkAllDirty();
	KillDialog(Dialogs::DIALOG_RENAMEUSER);
	mWidgetManager->SetFocus(aUserDialog);
}

void LawnApp::FinishNameError(int theId)
{
	KillDialog(theId);

	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(theId == Dialogs::DIALOG_CREATEUSERERROR ? Dialogs::DIALOG_CREATEUSER : Dialogs::DIALOG_RENAMEUSER);
	if (aNewUserDialog)
	{
		mWidgetManager->SetFocus(aNewUserDialog->mNameEditWidget);
	}
}

void LawnApp::FinishRestartConfirmDialog()
{
	mSawYeti = mBoard->mKilledYeti;

	KillDialog(Dialogs::DIALOG_CONTINUE);
	KillDialog(Dialogs::DIALOG_RESTARTCONFIRM);
	KillBoard();

	PreNewGame(mGameMode, false);
}

void LawnApp::DoCheatDialog()
{
	KillDialog(Dialogs::DIALOG_CHEAT);

	CheatDialog* aDialog = new CheatDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CHEAT, aDialog);
}

void LawnApp::FinishCheatDialog(bool isYes)
{
	CheatDialog* aCheatDialog = (CheatDialog*)GetDialog(Dialogs::DIALOG_CHEAT);
	if (aCheatDialog == nullptr)
		return;

	if (isYes && !aCheatDialog->ApplyCheat())
		return;

	KillDialog(Dialogs::DIALOG_CHEAT);
	if (isYes)
	{
		mMusic->StopAllMusic();
		mBoardResult = BoardResult::BOARDRESULT_CHEAT;
		PreNewGame(mGameMode, false);
	}
}

void LawnApp::DoJoinLanDialog()
{
	KillDialog(Dialogs::DIALOG_JOIN_LAN);

	JoinLanDialog* aDialog = new JoinLanDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_JOIN_LAN, aDialog);
}

void LawnApp::FinishJoinLanDialog(bool isYes)
{
	JoinLanDialog* aDialog = static_cast<JoinLanDialog*>(GetDialog(Dialogs::DIALOG_JOIN_LAN));
	if (!aDialog)
		return;
	if (!isYes)
	{
		KillDialog(Dialogs::DIALOG_JOIN_LAN);
		return;
	}

	std::string anError;
	JoinLanField anInvalidField = JoinLanField::NONE;
	auto anEndpoint = aDialog->GetEndpoint(anError, anInvalidField);
	if (!anEndpoint)
	{
		aDialog->ShowValidationError(std::move(anError), anInvalidField);
		return;
	}

	std::string aPlayerName = mPlayerInfo && !mPlayerInfo->mName.empty() ? mPlayerInfo->mName : "Guest";
	if (!PvzMultiplayer::IsValidDisplayName(aPlayerName, PvzMultiplayer::MAX_PLAYER_NAME_LENGTH))
		aPlayerName = "Guest";
	if (!mLanCoordinator->StartDirectJoining(
		aPlayerName, PvzRules::GetActiveRulesetProtocolId(), *anEndpoint))
	{
		aDialog->ShowValidationError(mLanCoordinator->GetStatusText(), JoinLanField::ADDRESS);
		return;
	}

	KillDialog(Dialogs::DIALOG_JOIN_LAN);
}

void LawnApp::FinishTimesUpDialog()
{
	KillDialog(Dialogs::DIALOG_TIMESUP);
}

void LawnApp::DoConfirmSellDialog(const std::string& theMessage)
{
	DoDialog(Dialogs::DIALOG_ZEN_SELL, true, "[ZEN_SELL_HEADER]", theMessage, "", Dialog::BUTTONS_YES_NO);
}

Dialog* LawnApp::NewDialog(int theDialogId, bool isModal, const std::string& theDialogHeader, const std::string& theDialogLines, const std::string& theDialogFooter, int theButtonMode)
{
	LawnDialog* aDialog = new LawnDialog(
		this,
		theDialogId,
		isModal,
		theDialogHeader,
		theDialogLines,
		theDialogFooter,
		theButtonMode
	);

	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	return aDialog;
}

bool LawnApp::KillNewOptionsDialog()
{
	NewOptionsDialog* aNewOptionsDialog = (NewOptionsDialog*)GetDialog(Dialogs::DIALOG_NEWOPTIONS);
	if (aNewOptionsDialog == nullptr)
		return false;

	bool wantWindowed = !aNewOptionsDialog->mFullscreenCheckbox->IsChecked();
	bool want3D = aNewOptionsDialog->mHardwareAccelerationCheckbox->IsChecked();
	SwitchScreenMode(wantWindowed, want3D, false);

	KillDialog(Dialogs::DIALOG_NEWOPTIONS);
	ClearUpdateBacklog();
	return true;
}

bool LawnApp::KillAlmanacDialog()
{
	if (GetDialog(Dialogs::DIALOG_ALMANAC))
	{
		KillDialog(Dialogs::DIALOG_ALMANAC);
		ClearUpdateBacklog(false);
		return true;
	}

	return false;
}

bool LawnApp::NeedPauseGame()
{
	// Menus and dialogs are local overlays during LAN play.  Pausing one
	// machine's board would immediately fork the deterministic simulation.
	if (IsLanGameplayActive())
		return false;

	if (mDialogList.size() == 0)
		return false;

	if (mDialogList.size() == 1 && mDialogList.front()->mId != Dialogs::DIALOG_NEW_GAME)
	{
		int anId = mDialogList.front()->mId;
		if (anId == Dialogs::DIALOG_CHOOSER_WARNING || anId == Dialogs::DIALOG_PURCHASE_PACKET_SLOT || anId == Dialogs::DIALOG_IMITATER)
		{
			return false;
		}
	}

	return (mBoard == nullptr || mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN) && (mBoard == nullptr || mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM);
}

void LawnApp::ModalOpen()
{
	if (mBoard && NeedPauseGame())
	{
		mBoard->Pause(true);
	}
}

void LawnApp::ModalClose()
{
	if (mBoard && !NeedPauseGame())
	{
	mBoard->Pause(false);
	}
}

bool LawnApp::KillDialog(int theDialogId)
{
	if (SexyAppBase::KillDialog(theDialogId))
	{
		if (mDialogMap.size() == 0 && mWidgetManager->mFocusWidget == nullptr)
		{
			if (mBoard)
			{
				mWidgetManager->SetFocus(mBoard);
			}
			else if (mGameSelector)
			{
				mWidgetManager->SetFocus(mGameSelector);
			}
		}

		if (mBoard && !NeedPauseGame())
		{
			mBoard->Pause(false);
		}

		return true;
	}

	return false;
}

void LawnApp::ShowResourceError(bool doExit)
{
	SexyAppBase::ShowResourceError(doExit);
}

void LawnApp::Init()
{
	DoParseCmdLine();
	// Materialize the resolved view before assigning because original-mode passthrough
	// may return a view into mTitle itself.
	const std::string aResolvedTitle(PvzRules::ResolveApplicationTitle(mTitle));
	mTitle = aResolvedTitle;
	// Distinct explicit save roots are used for local host/client testing and
	// are safe to run concurrently; the normal profile location stays single-instance.
	mOnlyAllowOneCopyToRun = !mCheatKeys && mCustomSaveDir.empty();

	mSessionID = time(0);
	mPlayTimeActiveSession = 0;
	mPlayTimeInactiveSession = 0;
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mSawYeti = false;

	SexyApp::Init();

	if (mShutdown) // MakeWindow() failed
		return;
	LanTrace("=== application session started version=%s build=%d ruleset=%.*s log=%s "
		"segmentBytes=%zu backups=%zu ===\n", mProductVersion.c_str(), mBuildNum,
		static_cast<int>(PvzRules::GetActiveRulesetName().size()),
		PvzRules::GetActiveRulesetName().data(), Sexy::GetAppDataPath(LAN_TRACE_FILE).c_str(),
		LAN_TRACE_FILE_BYTES, LAN_TRACE_BACKUP_COUNT);

	if (mRecordingDemoBuffer || mPlayingDemoBuffer)
		mAppRandSeed = mRandSeed; // demo sessions derive the app-level seed from the recorded one

	// these debug checks break the whole exe in release mode
//#ifdef PVZ_DEBUG
	PvzpAssertInitForApp();
	PvzpLogLn("session id: %u", mSessionID);
	PvzpLogLn("ruleset: %.*s", static_cast<int>(PvzRules::GetActiveRulesetName().size()), PvzRules::GetActiveRulesetName().data());
//#endif

	if (!mResourceManager->ParseResourcesFile("properties/resources.xml"))
	{
		ShowResourceError(true);
		return;
	}

	if (!PvzpLoadResources("Init"))
	{
		return;
	}

	PerfTimer mTimer;
	mTimer.Start();

	mProfileMgr->Load();

	std::string aCurUser;
	if (mPlayerInfo == nullptr && RegistryReadString("CurUser", &aCurUser))
	{
		mPlayerInfo = mProfileMgr->GetProfile(aCurUser);
	}
	if (mPlayerInfo == nullptr)
	{
		mPlayerInfo = mProfileMgr->GetAnyProfile();
	}

	mMaxExecutions = GetInteger("MaxExecutions", 0);
	mMaxPlays = GetInteger("MaxPlays", 0);
	mMaxTime = GetInteger("MaxTime", 60);

	mTitleScreen = new TitleScreen(this);
	mTitleScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mTitleScreen);
	mWidgetManager->SetFocus(mTitleScreen);

#ifdef PVZ_DEBUG
	int aDuration = mTimer.GetDuration();
	PvzpTrace("loading: 'profiles' %d ms", aDuration);
#endif
	mTimer.Start();

	mMusic = new Music();
	mSoundSystem = new PvzpFoley();
	mEffectSystem = new EffectSystem();
	mEffectSystem->EffectSystemInitialize();

	mKonamiCheck = new TypingCheck();
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_UP);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_UP);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_DOWN);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_DOWN);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_LEFT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_RIGHT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_LEFT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_RIGHT);
	mKonamiCheck->AddChar('b');
	mKonamiCheck->AddChar('a');
	mMustacheCheck = new TypingCheck("mustache");
	mMoustacheCheck = new TypingCheck("moustache");
	mSuperMowerCheck = new TypingCheck("trickedout");
	mSuperMowerCheck2 = new TypingCheck("tricked out");
	mFutureCheck = new TypingCheck("future");
	mPinataCheck = new TypingCheck("pinata");
	mDanceCheck = new TypingCheck("dance");
	mDaisyCheck = new TypingCheck("daisies");
	mSukhbirCheck = new TypingCheck("sukhbir");

#ifdef PVZ_DEBUG
	aDuration = mTimer.GetDuration();
	PvzpTrace("loading: 'system' %d ms", aDuration);
#endif
	mTimer.Start();

	ReanimatorLoadDefinitions(gLawnReanimationArray, ReanimationType::NUM_REANIMS);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LOADBAR_SPROUT, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LOADBAR_ZOMBIEHEAD, true);

#ifdef PVZ_DEBUG
	aDuration = mTimer.GetDuration();
	PvzpTrace("loading: 'loaderbar' %d ms", aDuration);
#endif
	mTimer.Start();
}

bool LawnApp::ChangeDirHook(const char* /*theIntendedPath*/)
{
	return false;
}

void LawnApp::Start()
{
	if (mLoadingFailed)
		return;

	SexyAppBase::Start();
}

bool LawnApp::DebugKeyDown(int theKey)
{
	return SexyAppBase::DebugKeyDown(theKey);
}

void LawnApp::HandleCmdLineParam(std::string_view theParamName, std::string_view theParamValue)
{
	if (theParamName == "-cheat")
	{
#ifdef PVZ_DEBUG
		mCheatKeys = true;
		mDebugKeysEnabled = true;
#endif
	}
	else if (theParamName == "-ruleset")
	{
		if (!PvzRules::SetActiveRuleset(theParamValue))
		{
			Popup("Invalid ruleset. Expected 'pvz95' or 'original'.");
			DoExit(1);
		}
	}
	else if (theParamName == "-lan-host")
	{
		mAutoHostLan = true;
		mAutoJoinLan = false;
	}
	else if (theParamName == "-lan-join")
	{
		mAutoJoinLan = true;
		mAutoHostLan = false;
	}
	else if (theParamName == "-lan-address")
	{
		mLanDiscoveryAddress = theParamValue;
	}
	else
	{
		SexyApp::HandleCmdLineParam(theParamName, theParamValue);
	}
}

bool LawnApp::UpdatePlayerProfileForFinishingLevel()
{
	bool aUnlockedNewChallenge = false;

	if (IsAdventureMode())
	{
		if (mBoard->mLevel == FINAL_LEVEL)
		{
			mPlayerInfo->SetLevel(1);
			mPlayerInfo->mFinishedAdventure++;
			if (mPlayerInfo->mFinishedAdventure == 1)
			{
				mPlayerInfo->mNeedsMessageOnGameSelector = 1;
			}
			ReportAchievement::GiveAchievement(this, HomeSecurity, false);
		}
		else
		{
			mPlayerInfo->SetLevel(mBoard->mLevel + 1);
		}

		if (!HasFinishedAdventure() && mBoard->mLevel == 34)
		{
			mPlayerInfo->mNeedsMagicTacoReward = 1;
		}
	}
	else if (IsSurvivalMode())
	{
		if (mBoard->IsFinalSurvivalStage())
		{
			aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
			mBoard->SurvivalSaveScore();

			if (aUnlockedNewChallenge && HasFinishedAdventure())
			{
				int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
				if (aNumTrophies != 8 && aNumTrophies != 9)
				{
					mPlayerInfo->mHasNewSurvival = true;
				}
			}
		}
	}
	else if (IsPuzzleMode())
	{
		aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
		mPlayerInfo->mChallengeRecords[GetCurrentChallengeIndex()]++;

		if (!HasFinishedAdventure() && (mGameMode == GameMode::GAMEMODE_SCARY_POTTER_3 || mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3))
		{
			aUnlockedNewChallenge = false;
		}

		if (aUnlockedNewChallenge)
		{
			if (IsScaryPotterLevel())
			{
				mPlayerInfo->mHasNewScaryPotter = 1;
			}
			else
			{
				mPlayerInfo->mHasNewIZombie = 1;
			}
		}
	}
	else
	{
		aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
		mPlayerInfo->mChallengeRecords[GetCurrentChallengeIndex()]++;

		if (aUnlockedNewChallenge && HasFinishedAdventure())
		{
			int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
			if (aNumTrophies <= 17)
			{
				mPlayerInfo->mHasNewMiniGame = 1;
			}
		}

		int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
		if (aNumTrophies == 20)
			ReportAchievement::GiveAchievement(this, BeyondTheGrave, false);
	}

	if ((IsAdventureMode() || IsSurvivalMode()) && !IsScaryPotterLevel() && !IsWhackAZombieLevel()) {
		if (mBoard->StageIsDayWithPool() && !mBoard->mPeaShooterUsed) {
			ReportAchievement::GiveAchievement(this, DontPea, false);
		} else if (mBoard->StageHasRoof() && !mBoard->HasConveyorBeltSeedBank() && !mBoard->mCatapultPlantsUsed) {
			ReportAchievement::GiveAchievement(this, Grounded, false);
		} else if (mBoard->StageIsDayWithoutPool() && mBoard->mMushroomAndCoffeeBeansOnly) {
			ReportAchievement::GiveAchievement(this, GoodMorning, false);
		}
		if (mBoard->StageIsNight() && !mBoard->mMushroomsUsed) {
			ReportAchievement::GiveAchievement(this, NoFungusAmongUs, false);
		}
	}

	WriteCurrentUserConfig();

	return aUnlockedNewChallenge;
}

void LawnApp::CheckForGameEnd()
{
	if (mBoard == nullptr || !mBoard->mLevelComplete)
		return;

	bool aUnlockedNewChallenge = UpdatePlayerProfileForFinishingLevel();

	if (IsAdventureMode())
	{
		int aLevel = mBoard->mLevel;
		KillBoard();

		if (IsFirstTimeAdventureMode() && aLevel < 50)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
		}
		else if (aLevel == FINAL_LEVEL)
		{
			if (mPlayerInfo->mFinishedAdventure == 1)
			{
				ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
			}
			else
			{
				ShowAwardScreen(AwardType::AWARD_CREDITS_ZOMBIENOTE, true);
			}
		}
		else if (aLevel == 9 || aLevel == 19 || aLevel == 29 || aLevel == 39 || aLevel == 49)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
		}
		else if (HasUnshownAchievements(mPlayerInfo))
		{
			ShowAwardScreen(AwardType::AWARD_ACHIEVEMENTONLY, true);
		}
		else
		{
			PreNewGame(mGameMode, false);
		}
	}
	else if (IsSurvivalMode())
	{
		if (mBoard->IsFinalSurvivalStage())
		{
			KillBoard();

			if (aUnlockedNewChallenge && HasFinishedAdventure())
			{
				ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
			}
			else if (HasUnshownAchievements(mPlayerInfo))
			{
				ShowAwardScreen(AwardType::AWARD_ACHIEVEMENTONLY, true);
			}
			else
			{
				ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
			}
		}
		else
		{
			mBoard->mChallenge->mSurvivalStage++;
			KillGameSelector();
			mBoard->InitSurvivalStage();
		}
	}
	else if (IsPuzzleMode())
	{
		KillBoard();

		if (aUnlockedNewChallenge)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
		}
		else if (HasUnshownAchievements(mPlayerInfo))
		{
			ShowAwardScreen(AwardType::AWARD_ACHIEVEMENTONLY, true);
		}
		else
		{
			ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_PUZZLE);
		}
	}
	else
	{
		KillBoard();

		if (aUnlockedNewChallenge && HasFinishedAdventure())
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
		}
		else if (HasUnshownAchievements(mPlayerInfo))
		{
			ShowAwardScreen(AwardType::AWARD_ACHIEVEMENTONLY, true);
		}
		else
		{
			ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
		}
	}
}

void LawnApp::UpdatePlayTimeStats()
{
	static int aLastTime = -1;

	int aTickCount = SDL_GetTicks();
	int aSession = (aTickCount - aLastTime) / 1000;

	if (mPlayerInfo && !mPlayerInfo->mHasUsedCheatKeys && !mDebugKeysEnabled && mCheatKeys)
	{
		mPlayerInfo->mHasUsedCheatKeys = 1;
	}

	if (aLastTime == -1)
	{
		aLastTime = aTickCount;
		return;
	}

	if (aSession > 0)
	{
		aLastTime = aTickCount;

		if ((mBoard == nullptr || !mBoard->mPaused) && mHasFocus && mLastTimerTime - mLastUserInputTick <= 10000)
		{
			mPlayTimeActiveSession += aSession;

			if (mBoard)
			{
				mBoard->mPlayTimeActiveLevel += aSession;
			}

			if (mPlayerInfo)
			{
				mPlayerInfo->mPlayTimeActivePlayer += aSession;
			}
		}
		else
		{
			mPlayTimeInactiveSession += aSession;

			if (mBoard)
			{
				mBoard->mPlayTimeInactiveLevel += aSession;
			}

			if (mPlayerInfo)
			{
				mPlayerInfo->mPlayTimeInactivePlayer += aSession;
			}
		}
	}
}

void LawnApp::UpdateFrames()
{
	if ((!mActive || mMinimized) && mBoard)
	{
		mBoard->ResetFPSStats();
	}

#ifdef PVZ_DEBUG
	UpdatePlayTimeStats();
#endif

	int aUpdateCount = 1;
	if (gSlowMo)
	{
		++gSlowMoCounter;
		if (gSlowMoCounter < 4)
		{
			aUpdateCount = 0;
		}
		else
		{
			gSlowMoCounter = 0;
		}
	}
	else if (gFastMo)
	{
		aUpdateCount = 20;
	}
	if (mLanCoordinator->GetMode() == PvzMultiplayer::LanMode::CONNECTED && mLanSessionBegun &&
		mLanTargetTick > mLanSimulationTick + 2)
	{
		uint64_t aCatchUpCount = std::min<uint64_t>(4, mLanTargetTick - mLanSimulationTick);
		aUpdateCount = std::max(aUpdateCount, static_cast<int>(aCatchUpCount));
	}

	for (int i = 0; i < aUpdateCount; i++)
	{
		mAppCounter++;
		mLanCoordinator->Poll();
		UpdateLanSession();
		bool anAdvanceLanBoard = mBoard && !mMinimized && ShouldAdvanceLanBoard();
		uint64_t anAdvancingLanStartId = mLanSessionStart ? mLanSessionStart->mStartId : 0;
		if (anAdvanceLanBoard)
			ProcessLanActionsForCurrentTick();

		if (mBoard)
		{
			mBoard->ProcessDeleteQueue();
		}
		if (mLoadingThreadCompleted && mEffectSystem)
		{
			mEffectSystem->ProcessDeleteQueue();
		}

		SexyApp::UpdateFrames();
		uint64_t aCurrentLanStartId = mLanSessionStart ? mLanSessionStart->mStartId : 0;
		if (anAdvanceLanBoard && mBoard && aCurrentLanStartId == anAdvancingLanStartId)
		{
			++mLanSimulationTick;
			if (mLanCoordinator->GetMode() == PvzMultiplayer::LanMode::HOSTING &&
				mLanSessionBegun && mLanSimulationTick % LAN_TICK_SYNC_INTERVAL == 0)
			{
				mLanCoordinator->BroadcastFromHost(PvzMultiplayer::TickSync{
					mLanSimulationTick, mLanSessionStart->mStartId});
			}
			PublishOrVerifyLanStateHash();
		}

		mMusic->MusicUpdate();

		CheckForGameEnd();
	}
}

void LawnApp::LocalMouseMove(int theX, int theY)
{
	mLocalLanCursorX = theX;
	mLocalLanCursorY = theY;
	mLocalLanCursorVisible = true;
}

bool LawnApp::LocalMouseButton(int theX, int theY, int theClickCount, bool theDown)
{
	LocalMouseMove(theX, theY);
	PvzMultiplayer::LanMode aMode = mLanCoordinator->GetMode();
	bool aHasGameInput = mBoard || mSeedChooserScreen;
	bool aBoardInput = aHasGameInput && IsBoardInputAt(theX, theY);
	if (theDown && (aMode == PvzMultiplayer::LanMode::HOSTING ||
		aMode == PvzMultiplayer::LanMode::CONNECTED))
	{
		LanTrace("mouse down app=%u sim=%llu xy=%d,%d click=%d active=%d board=%d boardInput=%d scene=%d begun=%d seed=%d\n",
			mAppCounter, static_cast<unsigned long long>(mLanSimulationTick), theX, theY, theClickCount,
			mActive ? 1 : 0, mBoard ? 1 : 0, aBoardInput ? 1 : 0, static_cast<int>(mGameScene),
			mLanSessionBegun ? 1 : 0, mLocalLanSeedBankIndex);
	}
	if (!aHasGameInput || !aBoardInput || !IsValidPointerClickCount(theClickCount))
		return false;

	if (aMode != PvzMultiplayer::LanMode::HOSTING && aMode != PvzMultiplayer::LanMode::CONNECTED)
		return false;
	if (!mLanSessionBegun)
		return mLanSessionStart.has_value();
	if (mSeedChooserScreen && mSeedChooserScreen->mMouseVisible &&
		mGameScene == GameScenes::SCENE_LEVEL_INTRO)
	{
		// SeedChooserScreen converts card changes and confirmation into ordered
		// LAN actions.  Its local-only buttons still need the normal widget path.
		return false;
	}
	if (mBoard)
	{
		int aMenuX = theX - mBoard->mX;
		int aMenuY = theY - mBoard->mY;
		if (mBoard->mMenuButton && Rect(mBoard->mMenuButton->mX, mBoard->mMenuButton->mY,
			mBoard->mMenuButton->mWidth, mBoard->mMenuButton->mHeight).Contains(aMenuX, aMenuY))
		{
			// The menu is intentionally local, just like ESC.  Both down and up
			// must continue through WidgetManager for the button to activate.
			return false;
		}
	}

	// Releases never enter the simulation protocol.  They remain useful to the
	// local window and cursor renderer, but PvZ actions are discrete commands.
	if (!theDown)
		return true;
	PvzMultiplayer::PointerIntent aPointerIntent = PvzMultiplayer::DecodePointerIntent(theClickCount);
	if (aPointerIntent == PvzMultiplayer::PointerIntent::PRIMARY_ACTION &&
		HandleLanCrazyDaveAdvanceInput())
	{
		return true;
	}
	if (aPointerIntent == PvzMultiplayer::PointerIntent::CANCEL_SELECTION)
	{
		mLocalLanSeedBankIndex = -1;
		mLocalLanShovelSelected = false;
		mLocalLanCobCannonPlantId = PlantID::PLANTID_NULL;
		return true;
	}
	bool aShovelTutorialIsActive = mBoard && mBoard->mCutScene &&
		mBoard->mCutScene->IsInShovelTutorial();
	bool aCanRouteBoardAction = PvzMultiplayer::CanRouteLanBoardAction(
		mGameScene == GameScenes::SCENE_PLAYING, aShovelTutorialIsActive);
	if (aPointerIntent != PvzMultiplayer::PointerIntent::PRIMARY_ACTION ||
		!mBoard || !aCanRouteBoardAction)
		return true;

	int aBoardX = theX - mBoard->mX;
	int aBoardY = theY - mBoard->mY;
	HitResult aHitResult{};
	mBoard->MouseHitTest(aBoardX, aBoardY, &aHitResult);
	LanTrace("mouse hit sim=%llu boardXY=%d,%d object=%u seed=%d shovel=%d cob=%u sun=%d\n",
		static_cast<unsigned long long>(mLanSimulationTick), aBoardX, aBoardY,
		static_cast<unsigned>(aHitResult.mObjectType), mLocalLanSeedBankIndex,
		mLocalLanShovelSelected ? 1 : 0, static_cast<unsigned>(mLocalLanCobCannonPlantId),
		mBoard->mSunMoney);
	auto QueueHitCoin = [&]()
	{
		if (aHitResult.mObjectType != GameObjectType::OBJECT_TYPE_COIN)
			return false;
		Coin* aCoin = static_cast<Coin*>(aHitResult.mObject);
		LanTrace("mouse coin sim=%llu id=%u\n", static_cast<unsigned long long>(mLanSimulationTick),
			static_cast<unsigned>(mBoard->mCoins.DataArrayGetID(aCoin)));
		QueueLocalLanAction({0, 0,
			static_cast<uint32_t>(mBoard->mCoins.DataArrayGetID(aCoin)), 0, 0, 0,
			PvzMultiplayer::ActionKind::COLLECT_COIN});
		return true;
	};
	if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_SEEDPACKET)
	{
		SeedPacket* aPacket = static_cast<SeedPacket*>(aHitResult.mObject);
		bool aCanPickUp = aPacket->CanPickUp();
		LanTrace("mouse seed packet sim=%llu index=%d type=%u canPickUp=%d active=%d sun=%d\n",
			static_cast<unsigned long long>(mLanSimulationTick), aPacket->mIndex,
			static_cast<unsigned>(aPacket->mPacketType), aCanPickUp ? 1 : 0,
			aPacket->mActive ? 1 : 0, mBoard->mSunMoney);
		if (mLocalLanSeedBankIndex == aPacket->mIndex)
		{
			mLocalLanSeedBankIndex = -1;
			PlayFoley(FoleyType::FOLEY_DROP);
		}
		else if (aCanPickUp)
		{
			mLocalLanSeedBankIndex = aPacket->mIndex;
			mLocalLanShovelSelected = false;
			mLocalLanCobCannonPlantId = PlantID::PLANTID_NULL;
		}
		else
		{
			PlaySample(SOUND_BUZZER);
		}
		return true;
	}
	// These local selections are presentation state.  Honor them before the
	// normal-cursor hit result so coins and packets cannot steal a planting,
	// shovel, or cob-cannon click.  This matches Board::MouseDown, where holding
	// a tool/plant suppresses coin hit testing.
	if (mLocalLanCobCannonPlantId != PlantID::PLANTID_NULL)
	{
		PlantID aCobCannonId = mLocalLanCobCannonPlantId;
		mLocalLanCobCannonPlantId = PlantID::PLANTID_NULL;
		QueueLocalLanAction({0, 0, static_cast<uint32_t>(aCobCannonId),
			PvzMultiplayer::NormalizeCoordinate(aBoardX, mBoard->mWidth),
			PvzMultiplayer::NormalizeCoordinate(aBoardY, mBoard->mHeight), 0,
			PvzMultiplayer::ActionKind::FIRE_COB_CANNON});
		return true;
	}
	if (mLocalLanShovelSelected)
	{
		mLocalLanShovelSelected = false;
		HitResult aPlantHit{};
		Plant* aPlant = mBoard->MouseHitTestPlant(aBoardX, aBoardY, &aPlantHit) ?
			mBoard->ToolHitTestHelper(&aPlantHit) : nullptr;
		if (!aPlant)
			return true;
		QueueLocalLanAction({0, 0,
			static_cast<uint32_t>(mBoard->mPlants.DataArrayGetID(aPlant)), 0, 0, 0,
			PvzMultiplayer::ActionKind::SHOVEL_PLANT});
		return true;
	}
	if (mLocalLanSeedBankIndex >= 0)
	{
		int aSeedBankIndex = mLocalLanSeedBankIndex;
		if (aSeedBankIndex >= mBoard->mSeedBank->mNumPackets)
		{
			mLocalLanSeedBankIndex = -1;
			return true;
		}
		SeedPacket& aPacket = mBoard->mSeedBank->mSeedPackets[aSeedBankIndex];
		SeedType aSeedType = aPacket.mPacketType == SeedType::SEED_IMITATER ?
			aPacket.mImitaterType : aPacket.mPacketType;
		int aGridX = mBoard->PlantingPixelToGridX(aBoardX, aBoardY, aSeedType);
		int aGridY = mBoard->PlantingPixelToGridY(aBoardX, aBoardY, aSeedType);
		if (aGridX < 0 || aGridX >= MAX_GRID_SIZE_X || aGridY < 0 || aGridY >= MAX_GRID_SIZE_Y)
		{
			LanTrace("mouse plant outside grid sim=%llu packet=%d pixel=%d,%d grid=%d,%d\n",
				static_cast<unsigned long long>(mLanSimulationTick), aSeedBankIndex,
				aBoardX, aBoardY, aGridX, aGridY);
			QueueHitCoin();
			return true;
		}
		PlantingReason aReason = mBoard->CanPlantAt(aGridX, aGridY, aSeedType);
		if (!IsIZombieLevel() && aReason != PlantingReason::PLANTING_OK)
		{
			LanTrace("mouse plant invalid sim=%llu packet=%d type=%u grid=%d,%d reason=%u\n",
				static_cast<unsigned long long>(mLanSimulationTick), aSeedBankIndex,
				static_cast<unsigned>(aSeedType), aGridX, aGridY, static_cast<unsigned>(aReason));
			if (!QueueHitCoin())
				PlayFoley(FoleyType::FOLEY_DROP);
			return true;
		}
		mLocalLanSeedBankIndex = -1;
		QueueLocalLanAction({0, 0, static_cast<uint32_t>(aSeedBankIndex),
			static_cast<uint16_t>(aGridX), static_cast<uint16_t>(aGridY), 0,
			PvzMultiplayer::ActionKind::PLANT_SEED});
		return true;
	}
	if (QueueHitCoin())
		return true;
	if (IsWhackAZombieLevel() &&
		mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER &&
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_NONE)
	{
		QueueLocalLanAction({0, 0, 0,
			PvzMultiplayer::NormalizeCoordinate(aBoardX, mBoard->mWidth),
			PvzMultiplayer::NormalizeCoordinate(aBoardY, mBoard->mHeight), 0,
			PvzMultiplayer::ActionKind::WHACK_ZOMBIE});
		return true;
	}
	if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_SHOVEL)
	{
		mLocalLanSeedBankIndex = -1;
		mLocalLanShovelSelected = true;
		mLocalLanCobCannonPlantId = PlantID::PLANTID_NULL;
		return true;
	}
	if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_PLANT)
	{
		Plant* aPlant = static_cast<Plant*>(aHitResult.mObject);
		if (aPlant->mSeedType == SeedType::SEED_COBCANNON &&
			aPlant->mState == PlantState::STATE_COBCANNON_READY)
		{
			mLocalLanCobCannonPlantId = static_cast<PlantID>(mBoard->mPlants.DataArrayGetID(aPlant));
		}
	}
	return true;
}

bool LawnApp::QueueLocalLanAction(PvzMultiplayer::GameAction theAction)
{
	using namespace PvzMultiplayer;

	if (!IsValidLanAction(theAction) || !mLanSessionBegun)
	{
		LanTrace("queue reject tick=%llu begun=%d kind=%u parameter=%u target=%u,%u player=%u\n",
			static_cast<unsigned long long>(mLanSimulationTick), mLanSessionBegun ? 1 : 0,
			static_cast<unsigned>(theAction.mKind), theAction.mParameter, theAction.mTargetX,
			theAction.mTargetY, theAction.mPlayerId);
		return false;
	}
	theAction.mSequence = ++mLanActionSequence;
	theAction.mPlayerId = mSharedInputState.GetLocalPlayerId();
	LanMode aMode = mLanCoordinator->GetMode();
	if (aMode == LanMode::HOSTING)
	{
		theAction.mHostTick = mLanSimulationTick + LAN_INPUT_DELAY;
		ScheduleResult aResult = mLanActionTimeline.Schedule(theAction, mLanSimulationTick);
		if (aResult != ScheduleResult::ACCEPTED)
		{
			LanTrace("queue host schedule failed local=%llu host=%llu seq=%u player=%u kind=%u result=%u\n",
				static_cast<unsigned long long>(mLanSimulationTick),
				static_cast<unsigned long long>(theAction.mHostTick), theAction.mSequence,
				theAction.mPlayerId, static_cast<unsigned>(theAction.mKind), static_cast<unsigned>(aResult));
			return false;
		}
		bool aSent = mLanCoordinator->BroadcastFromHost(theAction);
		LanTrace("queue host local=%llu host=%llu seq=%u player=%u kind=%u parameter=%u target=%u,%u sent=%d\n",
			static_cast<unsigned long long>(mLanSimulationTick),
			static_cast<unsigned long long>(theAction.mHostTick), theAction.mSequence,
			theAction.mPlayerId, static_cast<unsigned>(theAction.mKind), theAction.mParameter,
			theAction.mTargetX, theAction.mTargetY, aSent ? 1 : 0);
		return aSent;
	}
	if (aMode == LanMode::CONNECTED)
	{
		bool aSent = mLanCoordinator->SendAction(theAction);
		LanTrace("queue client local=%llu seq=%u player=%u kind=%u parameter=%u target=%u,%u sent=%d\n",
			static_cast<unsigned long long>(mLanSimulationTick), theAction.mSequence,
			theAction.mPlayerId, static_cast<unsigned>(theAction.mKind), theAction.mParameter,
			theAction.mTargetX, theAction.mTargetY, aSent ? 1 : 0);
		return aSent;
	}
	LanTrace("queue rejected by mode=%u\n", static_cast<unsigned>(aMode));
	return false;
}

bool LawnApp::HandleLanCrazyDaveAdvanceInput()
{
	using namespace PvzMultiplayer;

	if (!mLanCoordinator || !mLanSessionStart || !mLanSessionBegun || !mBoard ||
		mCrazyDaveMessageIndex < 0 ||
		static_cast<uint32_t>(mCrazyDaveMessageIndex) > MAX_CRAZY_DAVE_MESSAGE_INDEX)
	{
		return false;
	}

	bool anIntroDialog = mGameScene == GameScenes::SCENE_LEVEL_INTRO && mBoard->mCutScene &&
		mBoard->mCutScene->IsShowingCrazyDave();
	bool aScaryPotterDialog = mGameScene == GameScenes::SCENE_PLAYING &&
		mBoard->IsScaryPotterDaveTalking();
	if (!anIntroDialog && !aScaryPotterDialog)
		return false;

	LanMode aMode = mLanCoordinator->GetMode();
	if (aMode != LanMode::HOSTING && aMode != LanMode::CONNECTED)
		return false;

	if (aMode == LanMode::HOSTING)
	{
		bool aQueued = QueueLocalLanAction({0, 0,
			static_cast<uint32_t>(mCrazyDaveMessageIndex), 0, 0, 0,
			ActionKind::ADVANCE_CRAZY_DAVE_DIALOG});
		LanTrace("host Dave advance request sim=%llu message=%d queued=%d\n",
			static_cast<unsigned long long>(mLanSimulationTick), mCrazyDaveMessageIndex,
			aQueued ? 1 : 0);
	}
	else
	{
		// Dialog progression is deliberately host-authoritative.  A client click
		// is consumed locally and waits for the host's ordered action.
		LanTrace("client ignored Dave advance request sim=%llu message=%d\n",
			static_cast<unsigned long long>(mLanSimulationTick), mCrazyDaveMessageIndex);
	}
	return true;
}

bool LawnApp::IsLocalLanShovelSelected() const
{
	return mLanSessionStart.has_value() && mLanSessionBegun && mLocalLanShovelSelected;
}

bool LawnApp::IsLanSeedChooserInputActive() const
{
	using namespace PvzMultiplayer;

	if (!mLanCoordinator || !mLanSessionStart || !mLanSessionBegun || !mSeedChooserScreen ||
		!mSeedChooserScreen->mMouseVisible || mGameScene != GameScenes::SCENE_LEVEL_INTRO)
		return false;
	LanMode aMode = mLanCoordinator->GetMode();
	return aMode == LanMode::HOSTING || aMode == LanMode::CONNECTED;
}

bool LawnApp::IsLanSeedChooserHost() const
{
	return IsLanSeedChooserInputActive() &&
		mLanCoordinator->GetMode() == PvzMultiplayer::LanMode::HOSTING;
}

bool LawnApp::RequestLanSeedChoice(SeedType theSeedType, bool theAdd, SeedType theImitaterType)
{
	using namespace PvzMultiplayer;

	if (!mLanCoordinator)
		return false;
	LanMode aMode = mLanCoordinator->GetMode();
	bool aLanSession = mLanSessionStart.has_value() &&
		(aMode == LanMode::HOSTING || aMode == LanMode::CONNECTED);
	if (!aLanSession)
		return false;
	if (!IsLanSeedChooserInputActive())
	{
		LanTrace("ignored seed chooser request outside active chooser seed=%d imitater=%d\n",
			static_cast<int>(theSeedType), static_cast<int>(theImitaterType));
		return true;
	}
	if (mLanSeedChooserCommitPending)
	{
		LanTrace("ignored seed chooser request while confirmation is pending seed=%d\n",
			static_cast<int>(theSeedType));
		return true;
	}

	ActionKind aKind = theAdd ? ActionKind::ADD_SEED_CHOICE : ActionKind::REMOVE_SEED_CHOICE;
	uint32_t aParameter = static_cast<uint32_t>(theSeedType);
	if (theImitaterType != SEED_NONE)
	{
		if (theSeedType != SEED_IMITATER)
			return true;
		aKind = ActionKind::CHOOSE_IMITATER;
		aParameter = static_cast<uint32_t>(theImitaterType);
	}
	bool aQueued = QueueLocalLanAction({0, 0, aParameter, 0, 0, 0, aKind});
	LanTrace("seed chooser request seed=%d imitater=%d kind=%u queued=%d\n",
		static_cast<int>(theSeedType), static_cast<int>(theImitaterType),
		static_cast<unsigned>(aKind), aQueued ? 1 : 0);
	return true;
}

bool LawnApp::RequestLanSeedChooserStart()
{
	using namespace PvzMultiplayer;

	if (!mLanCoordinator)
		return false;
	LanMode aMode = mLanCoordinator->GetMode();
	bool aLanSession = mLanSessionStart.has_value() &&
		(aMode == LanMode::HOSTING || aMode == LanMode::CONNECTED);
	if (!aLanSession)
		return false;
	if (!IsLanSeedChooserInputActive())
	{
		LanTrace("ignored seed chooser confirmation outside active chooser\n");
		return true;
	}
	if (aMode != LanMode::HOSTING)
	{
		PlaySample(SOUND_BUZZER);
		LanTrace("blocked client seed chooser confirmation\n");
		return true;
	}
	if (!mLanSeedChooserCommitPending && !BeginLanSeedChooserConfirmation())
		return true;

	uint64_t aSignature = mLanSeedChooserCommitSignature;
	bool aQueued = QueueLocalLanAction({0, 0, static_cast<uint32_t>(aSignature),
		static_cast<uint16_t>(aSignature >> 32U), static_cast<uint16_t>(aSignature >> 48U), 0,
		ActionKind::CONFIRM_SEED_CHOICES});
	if (!aQueued)
		CancelLanSeedChooserConfirmation();
	LanTrace("seed chooser confirmation queued=%d\n", aQueued ? 1 : 0);
	return true;
}

bool LawnApp::BeginLanSeedChooserConfirmation()
{
	if (!IsLanSeedChooserHost() || mLanSeedChooserCommitPending || !mSeedChooserScreen)
		return false;
	uint64_t aSignature = mSeedChooserScreen->GetLanSeedChoiceSignature();
	if (aSignature == 0)
		return false;
	mLanSeedChooserCommitPending = true;
	mLanSeedChooserCommitSignature = aSignature;
	LanTrace("seed chooser confirmation locked signature=%016llx\n",
		static_cast<unsigned long long>(mLanSeedChooserCommitSignature));
	return true;
}

void LawnApp::CancelLanSeedChooserConfirmation()
{
	if (mLanSeedChooserCommitPending)
		LanTrace("seed chooser confirmation unlocked\n");
	mLanSeedChooserCommitPending = false;
	mLanSeedChooserCommitSignature = 0;
}

bool LawnApp::IsValidLanAction(const PvzMultiplayer::GameAction& theAction) const
{
	using namespace PvzMultiplayer;

	if (theAction.mPlayerId >= MAX_PLAYERS)
		return false;
	switch (theAction.mKind)
	{
	case ActionKind::PLANT_SEED:
		return theAction.mParameter < SEEDBANK_MAX && theAction.mTargetX < MAX_GRID_SIZE_X &&
			theAction.mTargetY < MAX_GRID_SIZE_Y;
	case ActionKind::COLLECT_COIN:
		return theAction.mParameter != static_cast<uint32_t>(CoinID::COINID_NULL) &&
			theAction.mTargetX == 0 && theAction.mTargetY == 0;
	case ActionKind::SHOVEL_PLANT:
		return theAction.mParameter != static_cast<uint32_t>(PlantID::PLANTID_NULL) &&
			theAction.mTargetX == 0 && theAction.mTargetY == 0;
	case ActionKind::FIRE_COB_CANNON:
		return theAction.mParameter != static_cast<uint32_t>(PlantID::PLANTID_NULL);
	case ActionKind::ADD_SEED_CHOICE:
	case ActionKind::REMOVE_SEED_CHOICE:
		return theAction.mParameter < NUM_SEEDS_IN_CHOOSER &&
			theAction.mTargetX == 0 && theAction.mTargetY == 0;
	case ActionKind::CHOOSE_IMITATER:
		return theAction.mParameter < static_cast<uint32_t>(SEED_GATLINGPEA) &&
			theAction.mTargetX == 0 && theAction.mTargetY == 0;
	case ActionKind::CONFIRM_SEED_CHOICES:
		return true;
	case ActionKind::ADVANCE_CRAZY_DAVE_DIALOG:
		return theAction.mPlayerId == 0 &&
			theAction.mParameter <= MAX_CRAZY_DAVE_MESSAGE_INDEX &&
			theAction.mTargetX == 0 && theAction.mTargetY == 0;
	case ActionKind::WHACK_ZOMBIE:
		return theAction.mParameter == 0;
	}
	return false;
}

void LawnApp::UpdateLanSession()
{
	using namespace PvzMultiplayer;

	LanMode aMode = mLanCoordinator->GetMode();
	uint8_t aModeValue = static_cast<uint8_t>(aMode);
	if (aModeValue != mLastLanModeValue)
	{
		bool aLanGameWasActive = mLanSessionStart.has_value();
		ResetLanGameState();
		if (aLanGameWasActive && aMode != LanMode::HOSTING && aMode != LanMode::CONNECTED)
		{
			FinishModelessDialogs();
			KillDialog(Dialogs::DIALOG_GAME_OVER);
			KillDialog(Dialogs::DIALOG_LEVEL_COMPLETE);
			KillDialog(Dialogs::DIALOG_CONTINUE);
			KillDialog(Dialogs::DIALOG_RESTARTCONFIRM);
			KillAwardScreen();
			KillChallengeScreen();
			KillCreditScreen();
			KillBoard();
			if (!mGameSelector)
				ShowGameSelector();
		}
		PlayerId aLocalPlayerId = 0;
		if (aMode == LanMode::CONNECTED && mLanCoordinator->GetClientSession().GetWelcome())
			aLocalPlayerId = mLanCoordinator->GetClientSession().GetWelcome()->mPlayerId;
		mSharedInputState.Reset(aLocalPlayerId);
		mLanCursorSequence = 0;
		mLanActionSequence = 0;
		mLastLanCursorSendTick = 0;
		mHasSentLanCursor = false;
		mLastLanModeValue = aModeValue;
		LanTrace("mode changed mode=%u localPlayer=%u status=\"%s\"\n",
			static_cast<unsigned>(aMode), aLocalPlayerId,
			mLanCoordinator->GetStatusText().c_str());
	}

	if (aMode == LanMode::HOSTING)
	{
		mLanCoordinator->SetSessionStarted(mBoard != nullptr || mLanSessionStart.has_value());
		for (HostSessionEvent& anEvent : mLanCoordinator->TakeHostEvents())
		{
			if (const auto* aPlayerJoined = std::get_if<PlayerJoined>(&anEvent))
			{
				LanTrace("player joined id=%u name=\"%s\" count=%u\n",
					aPlayerJoined->mPlayer.mPlayerId, aPlayerJoined->mPlayer.mName.c_str(),
					mLanCoordinator->GetHostSession().GetLobby().GetPlayerCount());
				continue;
			}
			if (const auto* aPlayerLeft = std::get_if<PlayerLeft>(&anEvent))
			{
				LanTrace("player left id=%u count=%u\n", aPlayerLeft->mPlayerId,
					mLanCoordinator->GetHostSession().GetLobby().GetPlayerCount());
				mSharedInputState.RemovePlayer(aPlayerLeft->mPlayerId);
				mLanSessionBarrier.RemovePlayer(aPlayerLeft->mPlayerId);
				MaybeBeginLanSession();
				continue;
			}
			if (const auto* aCursor = std::get_if<CursorUpdate>(&anEvent))
			{
				CursorUpdate anAcceptedCursor = *aCursor;
				anAcceptedCursor.mHostTick = mAppCounter;
				if (mSharedInputState.ApplyCursor(anAcceptedCursor, mAppCounter))
					mLanCoordinator->BroadcastFromHost(anAcceptedCursor);
				continue;
			}
			if (const auto* anInput = std::get_if<GameAction>(&anEvent))
			{
				GameAction anAcceptedInput = *anInput;
				bool aSeedChooserAction = anAcceptedInput.mKind == ActionKind::ADD_SEED_CHOICE ||
					anAcceptedInput.mKind == ActionKind::REMOVE_SEED_CHOICE ||
					anAcceptedInput.mKind == ActionKind::CHOOSE_IMITATER ||
					anAcceptedInput.mKind == ActionKind::CONFIRM_SEED_CHOICES;
				bool aClientTriedToConfirm = anAcceptedInput.mKind == ActionKind::CONFIRM_SEED_CHOICES;
				bool aClientTriedToAdvanceDave =
					anAcceptedInput.mKind == ActionKind::ADVANCE_CRAZY_DAVE_DIALOG;
				bool aWrongScene = aSeedChooserAction &&
					(!mSeedChooserScreen || !mBoard || !mBoard->mCutScene ||
						!mBoard->mCutScene->mSeedChoosing || mGameScene != GameScenes::SCENE_LEVEL_INTRO);
				bool aChooserLocked = aSeedChooserAction && mLanSeedChooserCommitPending;
				if (!mLanSessionBegun || !IsValidLanAction(anAcceptedInput) ||
					aClientTriedToConfirm || aClientTriedToAdvanceDave || aWrongScene || aChooserLocked)
				{
					LanTrace("host rejected remote action local=%llu begun=%d seq=%u player=%u kind=%u\n",
						static_cast<unsigned long long>(mLanSimulationTick), mLanSessionBegun ? 1 : 0,
						anAcceptedInput.mSequence, anAcceptedInput.mPlayerId,
						static_cast<unsigned>(anAcceptedInput.mKind));
					continue;
				}
				anAcceptedInput.mHostTick = mLanSimulationTick + LAN_INPUT_DELAY;
				ScheduleResult aResult = mLanActionTimeline.Schedule(anAcceptedInput, mLanSimulationTick);
				bool aSent = aResult == ScheduleResult::ACCEPTED &&
					mLanCoordinator->BroadcastFromHost(anAcceptedInput);
				LanTrace("host remote action local=%llu host=%llu seq=%u player=%u kind=%u parameter=%u target=%u,%u result=%u sent=%d\n",
					static_cast<unsigned long long>(mLanSimulationTick),
					static_cast<unsigned long long>(anAcceptedInput.mHostTick), anAcceptedInput.mSequence,
					anAcceptedInput.mPlayerId, static_cast<unsigned>(anAcceptedInput.mKind),
					anAcceptedInput.mParameter, anAcceptedInput.mTargetX, anAcceptedInput.mTargetY,
					static_cast<unsigned>(aResult), aSent ? 1 : 0);
				continue;
			}
			if (const auto* aReady = std::get_if<SessionReady>(&anEvent))
			{
				bool aMarkedReady = mLanSessionBarrier.MarkReady(*aReady);
				LanTrace("player ready id=%u start=%llu accepted=%d\n", aReady->mPlayerId,
					static_cast<unsigned long long>(aReady->mStartId), aMarkedReady ? 1 : 0);
				MaybeBeginLanSession();
			}
		}
	}
	else if (aMode == LanMode::CONNECTED)
	{
		for (Message& aMessage : mLanCoordinator->TakeClientMessages())
		{
			if (const auto* aCursor = std::get_if<CursorUpdate>(&aMessage))
			{
				mSharedInputState.ApplyCursor(*aCursor, mAppCounter);
			}
			else if (const auto* anInput = std::get_if<GameAction>(&aMessage))
			{
				if (!IsValidLanAction(*anInput))
				{
					Sexy::PrintF("LAN desync: host sent an invalid game action\n");
					LanTrace("client invalid action local=%llu host=%llu seq=%u player=%u kind=%u\n",
						static_cast<unsigned long long>(mLanSimulationTick),
						static_cast<unsigned long long>(anInput->mHostTick), anInput->mSequence,
						anInput->mPlayerId, static_cast<unsigned>(anInput->mKind));
					mLanDesynchronized = true;
				}
				else if (mLanSessionBegun && mLanSessionStart)
				{
					ScheduleResult aResult = mLanActionTimeline.Schedule(*anInput, mLanSimulationTick);
					LanTrace("client received action local=%llu host=%llu seq=%u player=%u kind=%u parameter=%u target=%u,%u result=%u\n",
						static_cast<unsigned long long>(mLanSimulationTick),
						static_cast<unsigned long long>(anInput->mHostTick), anInput->mSequence,
						anInput->mPlayerId, static_cast<unsigned>(anInput->mKind), anInput->mParameter,
						anInput->mTargetX, anInput->mTargetY, static_cast<unsigned>(aResult));
					if (aResult == ScheduleResult::PAST_TICK || aResult == ScheduleResult::FULL)
					{
						Sexy::PrintF("LAN desync: rejected action at local tick %llu for host tick %llu\n",
							static_cast<unsigned long long>(mLanSimulationTick),
							static_cast<unsigned long long>(anInput->mHostTick));
						mLanDesynchronized = true;
					}
				}
				else
					LanTrace("client dropped pre-begin action host=%llu seq=%u player=%u kind=%u\n",
						static_cast<unsigned long long>(anInput->mHostTick), anInput->mSequence,
						anInput->mPlayerId, static_cast<unsigned>(anInput->mKind));
			}
			else if (const auto* aStart = std::get_if<SessionStart>(&aMessage))
			{
				if (!ApplyLanSessionStart(*aStart, false))
				{
					Sexy::PrintF("LAN desync: rejected session start\n");
					mLanDesynchronized = true;
				}
			}
			else if (const auto* aBegin = std::get_if<SessionBegin>(&aMessage))
			{
				if (mLanSessionStart && aBegin->mStartId == mLanSessionStart->mStartId)
				{
					mLanTargetTick = aBegin->mHostTick;
					mLanWaitingForBegin = false;
					mLanSessionBegun = true;
					LanTrace("client begin start=%llu hostTick=%llu\n",
						static_cast<unsigned long long>(aBegin->mStartId),
						static_cast<unsigned long long>(aBegin->mHostTick));
				}
			}
			else if (const auto* aTick = std::get_if<TickSync>(&aMessage))
			{
				if (mLanSessionStart && aTick->mStartId == mLanSessionStart->mStartId)
					mLanTargetTick = std::max(mLanTargetTick, aTick->mHostTick);
			}
			else if (const auto* aHash = std::get_if<StateHash>(&aMessage))
			{
				if (mLanSessionStart && aHash->mStartId == mLanSessionStart->mStartId)
					HandleLanStateHashResult(mLanStateHashTimeline.ObserveRemote(
						aHash->mHostTick, aHash->mHash, mLanSimulationTick));
			}
		}
	}

	PublishLocalLanCursor();
}

void LawnApp::PublishLocalLanCursor()
{
	using namespace PvzMultiplayer;

	LanMode aMode = mLanCoordinator->GetMode();
	if (!mBoard || (aMode != LanMode::HOSTING && aMode != LanMode::CONNECTED))
		return;
	if (mHasSentLanCursor && mAppCounter - mLastLanCursorSendTick < LAN_CURSOR_SEND_INTERVAL)
		return;

	uint16_t aNormalizedX = NormalizeCoordinate(mLocalLanCursorX, mWidth);
	uint16_t aNormalizedY = NormalizeCoordinate(mLocalLanCursorY, mHeight);
	uint8_t aHeldSeedBankIndex = mLocalLanSeedBankIndex >= 0 &&
		mLocalLanSeedBankIndex <= MAX_CURSOR_SEED_BANK_INDEX ?
		static_cast<uint8_t>(mLocalLanSeedBankIndex) : NO_CURSOR_SEED_BANK_INDEX;
	bool aChanged = !mHasSentLanCursor || aNormalizedX != mLastLanCursorX ||
		aNormalizedY != mLastLanCursorY || mLocalLanCursorVisible != mLastLanCursorVisible ||
		aHeldSeedBankIndex != mLastLanHeldSeedBankIndex;
	if (!aChanged && mAppCounter - mLastLanCursorSendTick < LAN_CURSOR_KEEPALIVE_INTERVAL)
		return;

	CursorUpdate aCursor{
		mAppCounter,
		++mLanCursorSequence,
		aNormalizedX,
		aNormalizedY,
		mSharedInputState.GetLocalPlayerId(),
		mLocalLanCursorVisible,
		aHeldSeedBankIndex
	};
	if (aMode == LanMode::HOSTING)
	{
		mSharedInputState.ApplyCursor(aCursor, mAppCounter);
		mLanCoordinator->BroadcastFromHost(aCursor);
	}
	else
	{
		mLanCoordinator->SendCursor(aCursor);
	}

	mLastLanCursorSendTick = mAppCounter;
	mLastLanCursorX = aNormalizedX;
	mLastLanCursorY = aNormalizedY;
	mLastLanCursorVisible = mLocalLanCursorVisible;
	mLastLanHeldSeedBankIndex = aHeldSeedBankIndex;
	mHasSentLanCursor = true;
}

bool LawnApp::ApplyLanAction(const PvzMultiplayer::GameAction& theAction)
{
	using namespace PvzMultiplayer;

	if (!mBoard || !IsValidLanAction(theAction))
		return false;
	switch (theAction.mKind)
	{
	case ActionKind::PLANT_SEED:
		return mBoard->PlantSeedFromBank(static_cast<int>(theAction.mParameter),
			static_cast<int>(theAction.mTargetX), static_cast<int>(theAction.mTargetY));
	case ActionKind::COLLECT_COIN:
	{
		Coin* aCoin = mBoard->mCoins.DataArrayTryToGet(static_cast<CoinID>(theAction.mParameter));
		if (aCoin)
			aCoin->MouseDown(0, 0, 1);
		return true;
	}
	case ActionKind::SHOVEL_PLANT:
		return mBoard->ShovelPlantById(static_cast<PlantID>(theAction.mParameter));
	case ActionKind::FIRE_COB_CANNON:
		return mBoard->FireCobCannonById(static_cast<PlantID>(theAction.mParameter),
			DenormalizeCoordinate(theAction.mTargetX, mBoard->mWidth),
			DenormalizeCoordinate(theAction.mTargetY, mBoard->mHeight));
	case ActionKind::ADD_SEED_CHOICE:
		return !mSeedChooserScreen || mSeedChooserScreen->ApplyLanSeedChoice(
			static_cast<SeedType>(theAction.mParameter), SEED_NONE, true);
	case ActionKind::REMOVE_SEED_CHOICE:
		return !mSeedChooserScreen || mSeedChooserScreen->ApplyLanSeedChoice(
			static_cast<SeedType>(theAction.mParameter), SEED_NONE, false);
	case ActionKind::CHOOSE_IMITATER:
		return !mSeedChooserScreen || mSeedChooserScreen->ApplyLanSeedChoice(
			SEED_IMITATER, static_cast<SeedType>(theAction.mParameter));
	case ActionKind::CONFIRM_SEED_CHOICES:
	{
		uint64_t aSignature = static_cast<uint64_t>(theAction.mParameter) |
			(static_cast<uint64_t>(theAction.mTargetX) << 32U) |
			(static_cast<uint64_t>(theAction.mTargetY) << 48U);
		bool anApplied = !mSeedChooserScreen ||
			mSeedChooserScreen->ApplyLanSeedChooserStart(aSignature);
		CancelLanSeedChooserConfirmation();
		return anApplied;
	}
	case ActionKind::ADVANCE_CRAZY_DAVE_DIALOG:
	{
		// Rapid clicks can queue the same page more than once during the input
		// delay.  Once the first action advances it, identical later actions are
		// deterministic no-ops on every peer.
		if (mCrazyDaveMessageIndex != static_cast<int>(theAction.mParameter))
			return true;
		if (mGameScene == GameScenes::SCENE_LEVEL_INTRO && mBoard->mCutScene &&
			mBoard->mCutScene->IsShowingCrazyDave())
		{
			mBoard->mCutScene->AdvanceCrazyDaveDialog(false);
			return true;
		}
		if (mGameScene == GameScenes::SCENE_PLAYING && mBoard->IsScaryPotterDaveTalking())
		{
			mBoard->mChallenge->AdvanceCrazyDaveDialog();
			return true;
		}
		return false;
	}
	case ActionKind::WHACK_ZOMBIE:
		// A click queued just before the award appears is a harmless stale input.
		// Otherwise replay the original challenge logic at the ordered tick so
		// helmet damage, loot and target selection are identical on every peer.
		if (!IsWhackAZombieLevel() || mGameScene != GameScenes::SCENE_PLAYING ||
			mBoard->HasLevelAwardDropped())
		{
			return true;
		}
		mBoard->mChallenge->MouseDownWhackAZombie(
			DenormalizeCoordinate(theAction.mTargetX, mBoard->mWidth),
			DenormalizeCoordinate(theAction.mTargetY, mBoard->mHeight));
		return true;
	}
	return false;
}

bool LawnApp::IsBoardInputAt(int theX, int theY)
{
	if (!mWidgetManager)
		return false;
	Sexy::WidgetContainer* aWidget = mWidgetManager->GetWidgetAt(theX, theY, nullptr, nullptr);
	while (aWidget)
	{
		if (aWidget == mBoard || aWidget == mSeedChooserScreen)
			return true;
		aWidget = aWidget->mParent;
	}
	return false;
}

bool LawnApp::BeginLanGame(GameMode theGameMode)
{
	using namespace PvzMultiplayer;

	LanMode aMode = mLanCoordinator->GetMode();
	const HostLobby* aLobby = aMode == LanMode::HOSTING ?
		&mLanCoordinator->GetHostSession().GetLobby() : nullptr;
	size_t aPlayerCount = aLobby ? aLobby->GetPlayerCount() : 0;
	switch (ResolveLanLifecycleDecision(aMode, aPlayerCount, mLanWaitingForBegin))
	{
	case LanLifecycleDecision::CLIENT_FOLLOW:
	case LanLifecycleDecision::HOST_PENDING:
		// The client follows SessionStart, and repeated host activation while a
		// barrier is pending must not create a competing start ID.
		return true;
	case LanLifecycleDecision::LOCAL:
		// A departed final guest turns this into a local game.  Clear the old
		// deterministic session before allowing the normal local fallback.
		if (aMode == LanMode::HOSTING && mLanSessionStart)
			ResetLanGameState();
		return false;
	case LanLifecycleDecision::HOST_START:
		break;
	}

	std::array<PlayerId, MAX_PLAYERS> aPlayers{};
	aPlayerCount = 0;
	for (const auto& aPlayer : aLobby->GetPlayers())
	{
		if (aPlayer)
			aPlayers[aPlayerCount++] = aPlayer->mPlayerId;
	}
	std::array<std::string, MAX_PLAYERS> aPlayerNames = aLobby->MakePlayerNameSnapshot();

	uint64_t aStartId = aLobby->GetConfig().mSessionId ^
		(static_cast<uint64_t>(mAppCounter) << 32) ^ ++mLanStartSerial;
	if (aStartId == 0)
		aStartId = ++mLanStartSerial;
	uint32_t aSimulationSeed = static_cast<uint32_t>(aStartId ^ (aStartId >> 32));
	if (aSimulationSeed == 0)
		aSimulationSeed = 1;

	SessionStart aStart{
		0,
		aStartId,
		aSimulationSeed,
		static_cast<uint16_t>(theGameMode),
		CaptureGameplayProfile(),
		std::move(aPlayerNames)
	};
	LanTrace("host session start requested start=%llu seed=%u mode=%u players=%zu app=%u\n",
		static_cast<unsigned long long>(aStartId), aSimulationSeed, static_cast<unsigned>(theGameMode),
		aPlayerCount, mAppCounter);
	// Stage the new barrier separately.  The current session must remain intact
	// until SESSION_START has actually been queued for every connected peer.
	SessionBarrier aPendingBarrier;
	if (!aPendingBarrier.Start(aStartId, std::span<const PlayerId>(aPlayers.data(), aPlayerCount)) ||
		!mLanCoordinator->BroadcastFromHost(aStart))
	{
		LanTrace("host session start broadcast failed start=%llu\n",
			static_cast<unsigned long long>(aStartId));
		// Broadcast queues are flushed only by Poll.  Abort immediately so a
		// partially queued transition cannot reach just part of the room.
		AbortLanSession("Could not synchronize the next LAN game.");
		return true;
	}

	if (!ApplyLanSessionStart(aStart, true))
	{
		LanTrace("host session start apply failed start=%llu\n",
			static_cast<unsigned long long>(aStartId));
		AbortLanSession("The host could not apply the next LAN game.");
		return true;
	}
	mLanSessionBarrier = std::move(aPendingBarrier);
	MaybeBeginLanSession();
	return true;
}

bool LawnApp::ApplyLanSessionStart(const PvzMultiplayer::SessionStart& theStart, bool theHost)
{
	using namespace PvzMultiplayer;

	PlayerId aLocalPlayerId = mSharedInputState.GetLocalPlayerId();
	bool aNamesValid = aLocalPlayerId < MAX_PLAYERS &&
		!theStart.mPlayerNames[0].empty() && !theStart.mPlayerNames[aLocalPlayerId].empty();
	for (const std::string& aName : theStart.mPlayerNames)
		aNamesValid = aNamesValid && (aName.empty() || IsValidDisplayName(aName, MAX_PLAYER_NAME_LENGTH));
	if (theStart.mStartId == 0 || theStart.mSimulationSeed == 0 ||
		theStart.mGameMode > MAX_GAME_MODE_VALUE || theStart.mProfile.mProfileId == 0 || !aNamesValid)
		return false;
	if (mLanSessionStart)
	{
		if (mLanSessionStart->mStartId == theStart.mStartId)
			return true;
		ResetLanGameState();
	}

	mLanSessionStart = theStart;
	mLanActionTimeline.Reset();
	mLanSimulationTick = 0;
	mLanTargetTick = 0;
	mLanWaitingForBegin = true;
	mLanSessionBegun = false;
	mLanDesynchronized = false;
	LanTrace("apply session start role=%s start=%llu seed=%u mode=%u\n", theHost ? "host" : "client",
		static_cast<unsigned long long>(theStart.mStartId), theStart.mSimulationSeed,
		static_cast<unsigned>(theStart.mGameMode));

	if (!theHost)
		InstallLanGameplayProfile(theStart.mProfile);

	// SESSION_START is the authoritative root-screen transition.  Remove every
	// terminal overlay before constructing the replacement board so clients do
	// not need to click through a stale game-over/award screen themselves.
	FinishModelessDialogs();
	KillDialog(Dialogs::DIALOG_GAME_OVER);
	KillDialog(Dialogs::DIALOG_LEVEL_COMPLETE);
	KillDialog(Dialogs::DIALOG_CONTINUE);
	KillDialog(Dialogs::DIALOG_RESTARTCONFIRM);
	KillAwardScreen();
	KillChallengeScreen();
	KillGameSelector();
	mAppRandSeed = static_cast<int>(theStart.mSimulationSeed);
	mRandSeed = theStart.mSimulationSeed;
	Sexy::SRand(theStart.mSimulationSeed);
	mApplyingLanSessionStart = true;
	PreNewGame(static_cast<GameMode>(theStart.mGameMode), false);
	mApplyingLanSessionStart = false;
	if (!mBoard)
		return false;
	if (!theHost && !mLanCoordinator->SendReady(SessionReady{
		theStart.mStartId, mSharedInputState.GetLocalPlayerId()}))
		return false;
	return true;
}

void LawnApp::MaybeBeginLanSession()
{
	using namespace PvzMultiplayer;

	if (mLanCoordinator->GetMode() != LanMode::HOSTING || !mLanWaitingForBegin ||
		!mLanSessionStart || !mLanSessionBarrier.AllReady())
		return;

	SessionBegin aBegin{mLanSimulationTick, mLanSessionStart->mStartId};
	if (!mLanCoordinator->BroadcastFromHost(aBegin))
	{
		LanTrace("host begin broadcast failed start=%llu tick=%llu\n",
			static_cast<unsigned long long>(aBegin.mStartId),
			static_cast<unsigned long long>(aBegin.mHostTick));
		AbortLanSession("Could not begin the synchronized LAN game.");
		return;
	}
	mLanTargetTick = aBegin.mHostTick;
	mLanWaitingForBegin = false;
	mLanSessionBegun = true;
	LanTrace("host begin start=%llu tick=%llu\n", static_cast<unsigned long long>(aBegin.mStartId),
		static_cast<unsigned long long>(aBegin.mHostTick));
}

void LawnApp::ProcessLanActionsForCurrentTick()
{
	for (const PvzMultiplayer::GameAction& anInput : mLanActionTimeline.TakeForTick(mLanSimulationTick))
	{
		bool anApplied = ApplyLanAction(anInput);
		LanTrace("apply action tick=%llu seq=%u player=%u kind=%u parameter=%u target=%u,%u applied=%d sun=%d plants=%u coins=%u\n",
			static_cast<unsigned long long>(mLanSimulationTick), anInput.mSequence, anInput.mPlayerId,
			static_cast<unsigned>(anInput.mKind), anInput.mParameter, anInput.mTargetX,
			anInput.mTargetY, anApplied ? 1 : 0, mBoard ? mBoard->mSunMoney : -1,
			mBoard ? mBoard->mPlants.mSize : 0, mBoard ? mBoard->mCoins.mSize : 0);
		if (!anApplied)
			mLanDesynchronized = true;
	}
}

void LawnApp::PublishOrVerifyLanStateHash()
{
	using namespace PvzMultiplayer;

	if (!mBoard || !mLanSessionStart || mLanSimulationTick % 100 != 0)
		return;
	uint64_t aHash = ComputeBoardStateHash(*mBoard);
	PvzMultiplayer::BoardStateHashBreakdown aBoardHash =
		PvzMultiplayer::ComputeBoardStateHashBreakdown(*mBoard);
	PvzMultiplayer::DeterministicHash64 aRandHash;
	aRandHash.AddString(Sexy::GetRandState());
	Sexy::PrintF("LAN state tick %llu: full=%016llx core=%016llx grid=%016llx fog=%016llx rows=%016llx waves=%016llx "
		"seeds=%016llx challenge=%016llx "
		"plants=%016llx zombies=%016llx projectiles=%016llx coins=%016llx mowers=%016llx items=%016llx "
		"rand=%016llx scene=%d main=%u update=%u sun=%d\n",
		static_cast<unsigned long long>(mLanSimulationTick),
		static_cast<unsigned long long>(aHash),
		static_cast<unsigned long long>(aBoardHash.mCore),
		static_cast<unsigned long long>(aBoardHash.mGrid),
		static_cast<unsigned long long>(aBoardHash.mFog),
		static_cast<unsigned long long>(aBoardHash.mRowsAndIce),
		static_cast<unsigned long long>(aBoardHash.mWaves),
		static_cast<unsigned long long>(aBoardHash.mSeedBank),
		static_cast<unsigned long long>(aBoardHash.mChallenge),
		static_cast<unsigned long long>(aBoardHash.mPlants),
		static_cast<unsigned long long>(aBoardHash.mZombies),
		static_cast<unsigned long long>(aBoardHash.mProjectiles),
		static_cast<unsigned long long>(aBoardHash.mCoins),
		static_cast<unsigned long long>(aBoardHash.mMowers),
		static_cast<unsigned long long>(aBoardHash.mGridItems),
		static_cast<unsigned long long>(aRandHash.Finish()),
		static_cast<int>(mGameScene), mBoard->mMainCounter, mBoard->mBoardUpdateCounter, mBoard->mSunMoney);
	LanTrace("state tick=%llu hash=%016llx rand=%016llx scene=%d level=%d main=%u update=%u sun=%d "
		"core=%016llx grid=%016llx fog=%016llx rows=%016llx waves=%016llx seeds=%016llx challenge=%016llx "
		"plants=%016llx zombies=%016llx projectiles=%016llx coins=%016llx mowers=%016llx items=%016llx\n",
		static_cast<unsigned long long>(mLanSimulationTick), static_cast<unsigned long long>(aHash),
		static_cast<unsigned long long>(aRandHash.Finish()), static_cast<int>(mGameScene),
		mBoard->mLevel, mBoard->mMainCounter, mBoard->mBoardUpdateCounter, mBoard->mSunMoney,
		static_cast<unsigned long long>(aBoardHash.mCore),
		static_cast<unsigned long long>(aBoardHash.mGrid),
		static_cast<unsigned long long>(aBoardHash.mFog),
		static_cast<unsigned long long>(aBoardHash.mRowsAndIce),
		static_cast<unsigned long long>(aBoardHash.mWaves),
		static_cast<unsigned long long>(aBoardHash.mSeedBank),
		static_cast<unsigned long long>(aBoardHash.mChallenge),
		static_cast<unsigned long long>(aBoardHash.mPlants),
		static_cast<unsigned long long>(aBoardHash.mZombies),
		static_cast<unsigned long long>(aBoardHash.mProjectiles),
		static_cast<unsigned long long>(aBoardHash.mCoins),
		static_cast<unsigned long long>(aBoardHash.mMowers),
		static_cast<unsigned long long>(aBoardHash.mGridItems));
	if (mLanCoordinator->GetMode() == LanMode::HOSTING)
	{
		mLanCoordinator->BroadcastFromHost(StateHash{
			mLanSimulationTick, mLanSessionStart->mStartId, aHash});
		return;
	}

	HandleLanStateHashResult(mLanStateHashTimeline.ObserveLocal(
		mLanSimulationTick, aHash, mLanSimulationTick));
}

void LawnApp::HandleLanStateHashResult(const PvzMultiplayer::StateHashResult& theResult)
{
	using namespace PvzMultiplayer;

	if (theResult.mKind == StateHashResultKind::WAITING)
		return;
	if (theResult.mKind == StateHashResultKind::MATCHED)
	{
		LanTrace("state hash matched tick=%llu hash=%016llx\n",
			static_cast<unsigned long long>(theResult.mTick),
			static_cast<unsigned long long>(theResult.mLocalHash));
		return;
	}

	const char* aReason = "mismatch";
	if (theResult.mKind == StateHashResultKind::CONFLICT)
		aReason = "conflicting duplicate";
	else if (theResult.mKind == StateHashResultKind::EXPIRED)
		aReason = "grace window expired";
	else if (theResult.mKind == StateHashResultKind::FULL)
		aReason = "pending hash capacity exhausted";
	Sexy::PrintF("LAN desync: state hash %s at tick %llu (host %016llx, local %016llx)\n",
		aReason, static_cast<unsigned long long>(theResult.mTick),
		static_cast<unsigned long long>(theResult.mRemoteHash),
		static_cast<unsigned long long>(theResult.mLocalHash));
	LanTrace("desync state-hash reason=%s tick=%llu host=%016llx local=%016llx\n",
		aReason, static_cast<unsigned long long>(theResult.mTick),
		static_cast<unsigned long long>(theResult.mRemoteHash),
		static_cast<unsigned long long>(theResult.mLocalHash));
	mLanDesynchronized = true;
}

void LawnApp::ResetLanGameState()
{
	mLanSessionBarrier.Reset();
	mLanActionTimeline.Reset();
	mLanSessionStart.reset();
	mLanStateHashTimeline.Reset();
	mLanSimulationTick = 0;
	mLanTargetTick = 0;
	mLanWaitingForBegin = false;
	mLanSessionBegun = false;
	mLanSeedChooserCommitPending = false;
	mLanSeedChooserCommitSignature = 0;
	mLanDesynchronized = false;
	mLocalLanSeedBankIndex = -1;
	mLastLanHeldSeedBankIndex = PvzMultiplayer::NO_CURSOR_SEED_BANK_INDEX;
	mLocalLanShovelSelected = false;
	mLocalLanCobCannonPlantId = PlantID::PLANTID_NULL;
	RestoreLocalPlayerProfile();
}

void LawnApp::InstallLanGameplayProfile(const PvzMultiplayer::GameplayProfile& theProfile)
{
	using namespace PvzMultiplayer;

	if (!mLocalPlayerInfo)
		mLocalPlayerInfo = mPlayerInfo;
	if (!mLocalPlayerInfo)
		return;

	mLanGameplayProfile = std::make_unique<PlayerInfo>(*mLocalPlayerInfo);
	PlayerInfo& aProfile = *mLanGameplayProfile;
	aProfile.mId = theProfile.mProfileId;
	aProfile.mLevel = static_cast<int32_t>(theProfile.mAdventureLevel);
	aProfile.mCoins = static_cast<int32_t>(theProfile.mCoins);
	aProfile.mFinishedAdventure = theProfile.mFinishedAdventure;
	std::copy(theProfile.mChallengeRecords.begin(), theProfile.mChallengeRecords.end(), aProfile.mChallengeRecords);
	std::copy(theProfile.mPurchases.begin(), theProfile.mPurchases.end(), aProfile.mPurchases);
	aProfile.mDidntPurchasePacketUpgrade = (theProfile.mFlags & PROFILE_DIDNT_PURCHASE_PACKET_UPGRADE) != 0;
	aProfile.mHasWokenStinky = (theProfile.mFlags & PROFILE_HAS_WOKEN_STINKY) != 0;
	aProfile.mHasUnlockedMinigames = (theProfile.mFlags & PROFILE_HAS_UNLOCKED_MINIGAMES) != 0;
	aProfile.mHasUnlockedPuzzleMode = (theProfile.mFlags & PROFILE_HAS_UNLOCKED_PUZZLE) != 0;
	aProfile.mHasNewMiniGame = (theProfile.mFlags & PROFILE_HAS_NEW_MINIGAME) != 0;
	aProfile.mHasNewScaryPotter = (theProfile.mFlags & PROFILE_HAS_NEW_SCARY_POTTER) != 0;
	aProfile.mHasNewIZombie = (theProfile.mFlags & PROFILE_HAS_NEW_I_ZOMBIE) != 0;
	aProfile.mHasNewSurvival = (theProfile.mFlags & PROFILE_HAS_NEW_SURVIVAL) != 0;
	aProfile.mHasUnlockedSurvivalMode = (theProfile.mFlags & PROFILE_HAS_UNLOCKED_SURVIVAL) != 0;
	aProfile.mNeedsMagicTacoReward = (theProfile.mFlags & PROFILE_NEEDS_MAGIC_TACO_REWARD) != 0;
	aProfile.mHasSeenStinky = (theProfile.mFlags & PROFILE_HAS_SEEN_STINKY) != 0;
	aProfile.mHasSeenUpsell = (theProfile.mFlags & PROFILE_HAS_SEEN_UPSELL) != 0;
	mPlayerInfo = &aProfile;
}

void LawnApp::RestoreLocalPlayerProfile()
{
	if (!mLocalPlayerInfo)
		return;
	mPlayerInfo = mLocalPlayerInfo;
	mLocalPlayerInfo = nullptr;
	mLanGameplayProfile.reset();
}

PvzMultiplayer::GameplayProfile LawnApp::CaptureGameplayProfile() const
{
	using namespace PvzMultiplayer;

	GameplayProfile aSnapshot;
	if (!mPlayerInfo)
		return aSnapshot;
	aSnapshot.mProfileId = std::max<uint32_t>(1, mPlayerInfo->mId);
	aSnapshot.mAdventureLevel = static_cast<uint32_t>(std::clamp(mPlayerInfo->mLevel, 1,
		static_cast<int>(MAX_ADVENTURE_LEVEL)));
	aSnapshot.mCoins = static_cast<uint32_t>(std::max(0, mPlayerInfo->mCoins));
	aSnapshot.mFinishedAdventure = mPlayerInfo->mFinishedAdventure;
	std::copy(std::begin(mPlayerInfo->mChallengeRecords), std::end(mPlayerInfo->mChallengeRecords),
		aSnapshot.mChallengeRecords.begin());
	std::copy(std::begin(mPlayerInfo->mPurchases), std::end(mPlayerInfo->mPurchases),
		aSnapshot.mPurchases.begin());
	if (mPlayerInfo->mDidntPurchasePacketUpgrade) aSnapshot.mFlags |= PROFILE_DIDNT_PURCHASE_PACKET_UPGRADE;
	if (mPlayerInfo->mHasWokenStinky) aSnapshot.mFlags |= PROFILE_HAS_WOKEN_STINKY;
	if (mPlayerInfo->mHasUnlockedMinigames) aSnapshot.mFlags |= PROFILE_HAS_UNLOCKED_MINIGAMES;
	if (mPlayerInfo->mHasUnlockedPuzzleMode) aSnapshot.mFlags |= PROFILE_HAS_UNLOCKED_PUZZLE;
	if (mPlayerInfo->mHasNewMiniGame) aSnapshot.mFlags |= PROFILE_HAS_NEW_MINIGAME;
	if (mPlayerInfo->mHasNewScaryPotter) aSnapshot.mFlags |= PROFILE_HAS_NEW_SCARY_POTTER;
	if (mPlayerInfo->mHasNewIZombie) aSnapshot.mFlags |= PROFILE_HAS_NEW_I_ZOMBIE;
	if (mPlayerInfo->mHasNewSurvival) aSnapshot.mFlags |= PROFILE_HAS_NEW_SURVIVAL;
	if (mPlayerInfo->mHasUnlockedSurvivalMode) aSnapshot.mFlags |= PROFILE_HAS_UNLOCKED_SURVIVAL;
	if (mPlayerInfo->mNeedsMagicTacoReward) aSnapshot.mFlags |= PROFILE_NEEDS_MAGIC_TACO_REWARD;
	if (mPlayerInfo->mHasSeenStinky) aSnapshot.mFlags |= PROFILE_HAS_SEEN_STINKY;
	if (mPlayerInfo->mHasSeenUpsell) aSnapshot.mFlags |= PROFILE_HAS_SEEN_UPSELL;
	return aSnapshot;
}

bool LawnApp::ShouldAdvanceLanBoard() const
{
	using namespace PvzMultiplayer;

	LanMode aMode = mLanCoordinator->GetMode();
	if (!mLanSessionStart || (aMode != LanMode::HOSTING && aMode != LanMode::CONNECTED))
		return true;
	if (mLanWaitingForBegin || !mLanSessionBegun || mLanDesynchronized)
		return false;
	if (aMode == LanMode::CONNECTED)
		return mLanSimulationTick < mLanTargetTick;
	return true;
}

bool LawnApp::IsLanGameplayActive() const
{
	return mLanSessionStart.has_value();
}

bool LawnApp::ShouldBlockLanLifecycleInput() const
{
	return mLanCoordinator &&
		PvzMultiplayer::IsLanClientWaitingForHost(mLanCoordinator->GetMode());
}

bool LawnApp::RequestLanGameStart(GameMode theGameMode)
{
	using namespace PvzMultiplayer;

	if (!mLanCoordinator)
		return false;
	LanMode aMode = mLanCoordinator->GetMode();
	const HostLobby* aLobby = aMode == LanMode::HOSTING ?
		&mLanCoordinator->GetHostSession().GetLobby() : nullptr;
	size_t aPlayerCount = aLobby ? aLobby->GetPlayerCount() : 0;
	switch (ResolveLanLifecycleDecision(aMode, aPlayerCount, mLanWaitingForBegin))
	{
	case LanLifecycleDecision::CLIENT_FOLLOW:
		LanTrace("blocked client lifecycle start mode=%u gameMode=%u\n",
			static_cast<unsigned>(aMode), static_cast<unsigned>(theGameMode));
		return true;
	case LanLifecycleDecision::HOST_PENDING:
		LanTrace("ignored duplicate host lifecycle start gameMode=%u\n",
			static_cast<unsigned>(theGameMode));
		return true;
	case LanLifecycleDecision::HOST_START:
		LanTrace("host lifecycle start requested gameMode=%u\n",
			static_cast<unsigned>(theGameMode));
		PreNewGame(theGameMode, false);
		return true;
	case LanLifecycleDecision::LOCAL:
		if (aMode == LanMode::HOSTING && mLanSessionStart)
			ResetLanGameState();
		return false;
	}
	return false;
}

bool LawnApp::RequestLanLevelRestart()
{
	LanTrace("host lifecycle restart requested mode=%u gameMode=%u\n",
		mLanCoordinator ? static_cast<unsigned>(mLanCoordinator->GetMode()) : 0U,
		static_cast<unsigned>(mGameMode));
	return RequestLanGameStart(mGameMode);
}

bool LawnApp::StartGameFromAward(GameMode theGameMode)
{
	if (RequestLanGameStart(theGameMode))
		return !ShouldBlockLanLifecycleInput() && mBoard && mGameMode == theGameMode;

	KillAwardScreen();
	PreNewGame(theGameMode, false);
	return mBoard && mGameMode == theGameMode;
}

void LawnApp::AbortLanSession(const std::string& theReason)
{
	LanTrace("LAN session aborted: %s\n", theReason.c_str());
	mLanDesynchronized = true;
	if (mLanCoordinator)
		mLanCoordinator->AbortWithError(theReason);
}

void LawnApp::DrawLanCursorPreviews(Sexy::Graphics* theGraphics) const
{
	using namespace PvzMultiplayer;

	if (!mBoard || !mLanSessionStart || !mLanSessionBegun || mLanDesynchronized ||
		mGameScene != GameScenes::SCENE_PLAYING)
		return;

	auto DrawPreview = [&](uint8_t theSeedBankIndex, int theAppX, int theAppY)
	{
		if (theSeedBankIndex == NO_CURSOR_SEED_BANK_INDEX ||
			theSeedBankIndex >= mBoard->mSeedBank->mNumPackets)
			return;

		const SeedPacket& aPacket = mBoard->mSeedBank->mSeedPackets[theSeedBankIndex];
		if (aPacket.mPacketType == SeedType::SEED_NONE)
			return;
		SeedType aSeedType = aPacket.mPacketType == SeedType::SEED_IMITATER ?
			aPacket.mImitaterType : aPacket.mPacketType;
		int aBoardX = theAppX - mBoard->mX;
		int aBoardY = theAppY - mBoard->mY;
		int aGridX = mBoard->PlantingPixelToGridX(aBoardX, aBoardY, aSeedType);
		int aGridY = mBoard->PlantingPixelToGridY(aBoardX, aBoardY, aSeedType);
		if (aGridX < 0 || aGridX >= MAX_GRID_SIZE_X || aGridY < 0 || aGridY >= MAX_GRID_SIZE_Y ||
			mBoard->CanPlantAt(aGridX, aGridY, aSeedType) != PlantingReason::PLANTING_OK)
			return;

		Sexy::GraphicsAutoState aState(theGraphics);
		theGraphics->SetColorizeImages(true);
		theGraphics->SetColor(Sexy::Color(255, 255, 255, 100));
		float aPreviewX = static_cast<float>(mBoard->GridToPixelX(aGridX, aGridY));
		float aPreviewY = static_cast<float>(mBoard->GridToPixelY(aGridX, aGridY));
		if (mBoard->mApp->IsIZombieLevel())
		{
			float aHeight = PlantDrawHeightOffset(mBoard, nullptr, aSeedType, aGridX, aGridY);
			if (aSeedType == SeedType::SEED_ZOMBIE_GARGANTUAR)
				aHeight -= 30.0f;
			aPreviewX -= 49.0f;
			aPreviewY += aHeight - 78.0f;
		}
		else
		{
			aPreviewY += PlantDrawHeightOffset(mBoard, nullptr, aSeedType, aGridX, aGridY);
		}
		Plant::DrawSeedType(theGraphics, aPacket.mPacketType, aPacket.mImitaterType,
			DrawVariation::VARIATION_NORMAL, aPreviewX, aPreviewY);

		if (mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
		{
			for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; ++aRow)
			{
				if (aRow == aGridY || mBoard->CanPlantAt(aGridX, aRow, aSeedType) != PlantingReason::PLANTING_OK)
					continue;
				float aRowY = static_cast<float>(mBoard->GridToPixelY(aGridX, aRow)) +
					PlantDrawHeightOffset(mBoard, nullptr, aSeedType, aGridX, aRow);
				Plant::DrawSeedType(theGraphics, aPacket.mPacketType, aPacket.mImitaterType,
					DrawVariation::VARIATION_NORMAL, aPreviewX, aRowY);
			}
		}
	};

	if (mLocalLanCursorVisible)
	{
		uint8_t aLocalSeed = mLocalLanSeedBankIndex >= 0 &&
			mLocalLanSeedBankIndex <= MAX_CURSOR_SEED_BANK_INDEX ?
			static_cast<uint8_t>(mLocalLanSeedBankIndex) : NO_CURSOR_SEED_BANK_INDEX;
		DrawPreview(aLocalSeed, mLocalLanCursorX, mLocalLanCursorY);
	}
	for (const auto& aCursorSlot : mSharedInputState.GetCursors())
	{
		if (!aCursorSlot || aCursorSlot->mUpdate.mPlayerId == mSharedInputState.GetLocalPlayerId() ||
			!aCursorSlot->mUpdate.mVisible || mAppCounter - aCursorSlot->mReceivedAtTick > LAN_CURSOR_TIMEOUT)
			continue;
		CursorPosition aPosition = SampleCursorPosition(*aCursorSlot, mAppCounter);
		DrawPreview(aCursorSlot->mUpdate.mHeldSeedBankIndex,
			DenormalizeCoordinate(aPosition.mNormalizedX, mWidth),
			DenormalizeCoordinate(aPosition.mNormalizedY, mHeight));
	}
}

void LawnApp::DrawSharedCursors(Sexy::Graphics* theGraphics, int theOriginX, int theOriginY) const
{
	using namespace PvzMultiplayer;

	auto DrawHeldSeed = [&](uint8_t theSeedBankIndex, int theAppX, int theAppY)
	{
		if (!mBoard || theSeedBankIndex == NO_CURSOR_SEED_BANK_INDEX ||
			theSeedBankIndex >= mBoard->mSeedBank->mNumPackets)
			return;
		const SeedPacket& aPacket = mBoard->mSeedBank->mSeedPackets[theSeedBankIndex];
		if (aPacket.mPacketType == SeedType::SEED_NONE)
			return;
		SeedType aSeedType = aPacket.mPacketType == SeedType::SEED_IMITATER ?
			aPacket.mImitaterType : aPacket.mPacketType;
		float aX = static_cast<float>(theAppX - theOriginX - 35);
		float aY = static_cast<float>(theAppY - theOriginY) +
			PlantDrawHeightOffset(mBoard, nullptr, aSeedType, -1, -1) - 60.0f;
		if (Plant::IsFlying(aSeedType) || aSeedType == SeedType::SEED_GRAVEBUSTER)
			aY += 30.0f;
		if (mBoard->mApp->IsIZombieLevel())
		{
			aX -= 55.0f;
			aY -= 70.0f;
		}
		Plant::DrawSeedType(theGraphics, aPacket.mPacketType, aPacket.mImitaterType,
			DrawVariation::VARIATION_NORMAL, aX, aY);
	};

	if (mLocalLanCursorVisible)
	{
		uint8_t aLocalSeed = mLocalLanSeedBankIndex >= 0 &&
			mLocalLanSeedBankIndex <= MAX_CURSOR_SEED_BANK_INDEX ?
			static_cast<uint8_t>(mLocalLanSeedBankIndex) : NO_CURSOR_SEED_BANK_INDEX;
		DrawHeldSeed(aLocalSeed, mLocalLanCursorX, mLocalLanCursorY);
		if (mLocalLanShovelSelected)
		{
			theGraphics->DrawImage(Sexy::IMAGE_SHOVEL,
				mLocalLanCursorX - theOriginX + 10,
				mLocalLanCursorY - theOriginY - 30);
		}
	}

	for (const auto& aCursorSlot : mSharedInputState.GetCursors())
	{
		if (!aCursorSlot || aCursorSlot->mUpdate.mPlayerId == mSharedInputState.GetLocalPlayerId() ||
			!aCursorSlot->mUpdate.mVisible || mAppCounter - aCursorSlot->mReceivedAtTick > LAN_CURSOR_TIMEOUT)
			continue;

		CursorPosition aPosition = SampleCursorPosition(*aCursorSlot, mAppCounter);
		int anAppX = DenormalizeCoordinate(aPosition.mNormalizedX, mWidth);
		int anAppY = DenormalizeCoordinate(aPosition.mNormalizedY, mHeight);
		int aX = anAppX - theOriginX;
		int aY = anAppY - theOriginY;
		DrawHeldSeed(aCursorSlot->mUpdate.mHeldSeedBankIndex, aX + theOriginX, aY + theOriginY);
		Sexy::GraphicsAutoState aState(theGraphics);
		Sexy::Point aPointer[] = {
			{aX, aY}, {aX + 3, aY + 18}, {aX + 8, aY + 13}, {aX + 13, aY + 22},
			{aX + 17, aY + 20}, {aX + 12, aY + 11}, {aX + 20, aY + 9}
		};
		uint32_t aRgb = aCursorSlot->mRgb;
		theGraphics->SetColor(Sexy::Color(
			static_cast<int>((aRgb >> 16) & 0xFFU),
			static_cast<int>((aRgb >> 8) & 0xFFU),
			static_cast<int>(aRgb & 0xFFU)));
		theGraphics->PolyFillAA(aPointer, static_cast<int>(std::size(aPointer)), true);
		theGraphics->SetColor(Sexy::Color(25, 25, 25));
		for (size_t anIndex = 0; anIndex < std::size(aPointer); ++anIndex)
		{
			const Sexy::Point& aStart = aPointer[anIndex];
			const Sexy::Point& anEnd = aPointer[(anIndex + 1) % std::size(aPointer)];
			theGraphics->DrawLineAA(aStart.mX, aStart.mY, anEnd.mX, anEnd.mY);
		}

		if (!mLanSessionStart)
			continue;
		const std::string& aPlayerName =
			mLanSessionStart->mPlayerNames[aCursorSlot->mUpdate.mPlayerId];
		if (aPlayerName.empty())
			continue;

		constexpr int LABEL_HORIZONTAL_PADDING = 5;
		constexpr int LABEL_VERTICAL_PADDING = 2;
		// BRIANNETOD12's legacy 95 atlas can resolve dynamic glyph rectangles to
		// the same source cell.  PICO129 is already used for arbitrary runtime
		// strings (tooltips/debug text), so player names retain their real glyphs.
		Sexy::_Font* aFont = Sexy::FONT_PICO129;
		int aLabelWidth = aFont->StringWidth(aPlayerName) + LABEL_HORIZONTAL_PADDING * 2;
		int aLabelHeight = aFont->GetHeight() + LABEL_VERTICAL_PADDING * 2;
		CursorLabelPosition aLabelPosition = ResolveCursorLabelPosition(
			anAppX, anAppY, aLabelWidth, aLabelHeight, mWidth, mHeight);
		int aLabelX = aLabelPosition.mX - theOriginX;
		int aLabelY = aLabelPosition.mY - theOriginY;

		theGraphics->SetColor(Sexy::Color(20, 20, 20, 205));
		theGraphics->FillRect(aLabelX, aLabelY, aLabelWidth, aLabelHeight);
		theGraphics->SetColor(Sexy::Color(
			static_cast<int>((aRgb >> 16) & 0xFFU),
			static_cast<int>((aRgb >> 8) & 0xFFU),
			static_cast<int>(aRgb & 0xFFU)));
		theGraphics->DrawRect(aLabelX, aLabelY, aLabelWidth - 1, aLabelHeight - 1);
		theGraphics->SetFont(aFont);
		theGraphics->SetColor(Sexy::Color::White);
		// Use the raw font path: player names are UTF-8 data, not localization keys.
		theGraphics->DrawString(aPlayerName, aLabelX + LABEL_HORIZONTAL_PADDING,
			aLabelY + LABEL_VERTICAL_PADDING + aFont->GetAscent());
	}
}

void LawnApp::ToggleSlowMo()
{
	gSlowMoCounter = 0;
	gSlowMo = !gSlowMo;
	gFastMo = false;
}

void LawnApp::ToggleFastMo()
{
	gSlowMo = false;
	gFastMo = !gFastMo;
}

void LawnApp::LoadGroup(const char* theGroupName, int theGroupAveMsToLoad)
{
	PerfTimer aTimer;
	aTimer.Start();

	mResourceManager->StartLoadResources(theGroupName);
	while (!mShutdown && !mCloseRequest && !mLoadingFailed && PvzpLoadNextResource())
	{
		mCompletedLoadingThreadTasks += theGroupAveMsToLoad;
	}

	if (mShutdown || mCloseRequest)
		return;

	if (mResourceManager->HadError() || !ExtractResourcesByName(mResourceManager, theGroupName))
	{
		ShowResourceError();
		mLoadingFailed = true;
	}
}

void LawnApp::LoadingThreadProc()
{
	if (!PvzpLoadResources("LoaderBar"))
		return;

	PvzpStringListLoad("Properties/LawnStrings.txt");
	PvzpStringListReadFile("Properties/ZombatarTOS.txt");

	// Load localized properties AFTER LawnStrings so they can override string values
	LoadProperties("properties/default.xml", false, false);
	LoadProperties("properties/Layout.xml", false, false);

	if (mTitleScreen)
	{
		mTitleScreen->mLoaderScreenIsLoaded = true;
	}

	const char* groups[] = { "LoadingFonts", "LoadingImages", "LoadingSounds" };
	int group_ave_ms_to_load[] = { 54, 9, 54 };
	for (int i = 0; i < 3; i++)
	{
		mNumLoadingThreadTasks += mResourceManager->GetNumResources(groups[i]) * group_ave_ms_to_load[i];
	}
	mNumLoadingThreadTasks += 636;
	mNumLoadingThreadTasks += GetNumPreloadingTasks();
	mNumLoadingThreadTasks += Music::MUSIC_LOADING_TASKS;

	PerfTimer aTimer;
	aTimer.Start();

	PvzpHesitationTrace("start loading");
	PvzpHesitationBracket aHesitationResources("Resources");
	PvzpHesitationTrace("loading thread start");

	LoadGroup("LoadingImages", 9);
	LoadGroup("LoadingFonts", 54);
	if (mLoadingFailed || mShutdown || mCloseRequest)
		return;
	mDefaultFont = FONT_PICO129; // framework widgets fall back to this when no font is set

	aHesitationResources.EndBracket();
	PvzpTrace("loading '%s' %d ms", "resources", static_cast<int>(aTimer.GetDuration()));

	mMusic->MusicInit();
	// aDuration goes unused
	//int aDuration = max(aTimer.GetDuration(), 0.0);
	aTimer.Start();

	mPoolEffect = new PoolEffect();
	mPoolEffect->PoolEffectInitialize();
	mZenGarden = new ZenGarden();
	mReanimatorCache = new ReanimatorCache();
	mReanimatorCache->ReanimatorCacheInitialize();
	PvzpFoleyInitialize(gLawnFoleyParamArray, LENGTH(gLawnFoleyParamArray));

	PvzpTrace("loading '%s' %d ms", "stuff", static_cast<int>(aTimer.GetDuration()));
	aTimer.Start();

	TrailLoadDefinitions(gLawnTrailArray, LENGTH(gLawnTrailArray));
	PvzpTrace("loading '%s' %d ms", "trail", static_cast<int>(aTimer.GetDuration()));
	aTimer.Start();
	PvzpHesitationTrace("trail");

	PvzpParticleLoadDefinitions(gLawnParticleArray, LENGTH(gLawnParticleArray));
	//aDuration = max(aTimer.GetDuration(), 0.0);
	aTimer.Start();

	PreloadForUser();
	if (mLoadingFailed || mShutdown || mCloseRequest)
		return;

	//aDuration = max(aTimer.GetDuration(), 0.0);
	aTimer.Start();

	GetNumPreloadingTasks();
	LoadGroup("LoadingSounds", 54);
	PvzpHesitationTrace("finished loading");
}

void LawnApp::FastLoad(GameMode theGameMode)
{
	if (!mShutdown)
	{
		mWidgetManager->RemoveWidget(mTitleScreen);
		SafeDeleteWidget(mTitleScreen);
		mTitleScreen = nullptr;

		PreNewGame(theGameMode, false);
	}
}

void LawnApp::LoadingThreadCompleted()
{
}

void LawnApp::LoadingCompleted()
{
	mWidgetManager->RemoveWidget(mTitleScreen);
	SafeDeleteWidget(mTitleScreen);
	mTitleScreen = nullptr;

	mResourceManager->DeleteImage("IMAGE_TITLESCREEN");

	ShowGameSelector();
	std::string aPlayerName = mPlayerInfo && !mPlayerInfo->mName.empty() ? mPlayerInfo->mName :
		(mAutoHostLan ? "Host" : "Guest");
	if (!PvzMultiplayer::IsValidDisplayName(aPlayerName, PvzMultiplayer::MAX_PLAYER_NAME_LENGTH))
		aPlayerName = mAutoHostLan ? "Host" : "Guest";
	if (mAutoHostLan)
		mLanCoordinator->StartHosting("PvZ 95 LAN", aPlayerName, PvzRules::GetActiveRulesetProtocolId());
	else if (mAutoJoinLan)
	{
		std::optional<PvzMultiplayer::Ipv4Endpoint> aDiscoveryEndpoint;
		if (!mLanDiscoveryAddress.empty())
			aDiscoveryEndpoint = PvzMultiplayer::Ipv4Endpoint::Parse(
				mLanDiscoveryAddress, PvzMultiplayer::DEFAULT_DISCOVERY_PORT);
		if (!mLanDiscoveryAddress.empty() && !aDiscoveryEndpoint)
		{
			Popup("Invalid -lan-address IPv4 address.");
			return;
		}
		mLanCoordinator->StartJoining(aPlayerName, PvzRules::GetActiveRulesetProtocolId(), aDiscoveryEndpoint);
	}
}

void LawnApp::URLOpenFailed(const std::string& theURL)
{
	SexyAppBase::URLOpenFailed(theURL);
	KillDialog(Dialogs::DIALOG_OPENURL_WAIT);
	CopyToClipboard(theURL);

	std::string aString =
		StrFormat(
			GetString("OPEN_URL", "Please open the following URL in your browser\n\n%s\n\nFor your convenience, this URL has already been copied to your clipboard.").c_str(),
			theURL.c_str());

	DoDialog(Dialogs::DIALOG_OPENURL_WAIT, true, GetString("OPEN_BROWSER", "Open Browser"), "[DIALOG_BUTTON_OK]", aString, Dialog::BUTTONS_FOOTER);
}

void LawnApp::URLOpenSucceeded(const std::string& theURL)
{
	SexyAppBase::URLOpenSucceeded(theURL);
	KillDialog(Dialogs::DIALOG_OPENURL_WAIT);
}

bool LawnApp::OpenURL(const std::string& theURL, bool shutdownOnOpen)
{
	DoDialog(
		Dialogs::DIALOG_OPENURL_WAIT,
		true,
		GetString("OPENING_BROWSER", "Opening Browser"),
		GetString("OPENING_BROWSER", "Opening Browser"),
		"",
		Dialog::BUTTONS_NONE
	);

	DrawDirtyStuff();

	return SexyAppBase::OpenURL(theURL, shutdownOnOpen);
}

void LawnApp::ConfirmQuit()
{
	std::string aBody = PvzpStringTranslate("[QUIT_MESSAGE]");
	std::string aHeader = PvzpStringTranslate("[QUIT_HEADER]");
	LawnDialog* aDialog = (LawnDialog*)DoDialog(Dialogs::DIALOG_QUIT, true, aHeader, aBody, "", Dialog::BUTTONS_OK_CANCEL);
	aDialog->mLawnYesButton->mLabel = PvzpStringTranslate("[QUIT_BUTTON]");
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
}

void LawnApp::PreDisplayHook()
{
	SexyApp::PreDisplayHook();
}


void LawnApp::ButtonPress(int) {}
void LawnApp::ButtonDownTick(int) {}
void LawnApp::ButtonMouseEnter(int) {}
void LawnApp::ButtonMouseLeave(int) {}
void LawnApp::ButtonMouseMove(int, int, int) {}

void LawnApp::ButtonDepress(int theId)
{
	if (theId % 10000 >= 2000 && theId % 10000 < 3000)  // ids in [2000, 3000): the "Yes" button of dialog (theId - 2000)
	{
		switch (theId - 2000)
		{
		case Dialogs::DIALOG_NEW_GAME:
			KillDialog(Dialogs::DIALOG_NEW_GAME);
			ShowGameSelector();
			return;

		case Dialogs::DIALOG_NEWOPTIONS:
			KillNewOptionsDialog();
			return;

		case Dialogs::DIALOG_PREGAME_NAG:
			DoRegister();
			return;

		case Dialogs::DIALOG_LOAD_GAME:
			return;

		case Dialogs::DIALOG_CONFIRM_UPDATE_CHECK:
			KillDialog(Dialogs::DIALOG_CONFIRM_UPDATE_CHECK);
			CheckForUpdates();
			return;

		case Dialogs::DIALOG_QUIT:
			KillDialog(Dialogs::DIALOG_QUIT);
#if defined(__ANDROID__) && !defined(__TERMUX__)
			// Android should move the task to the background instead of forcing a quit.
			SDL_MinimizeWindow(static_cast<SDL_Window*>(mWindow));
#elif !defined(__IPHONEOS__) // iOS apps must not quit or programmatically return to the Home screen.
			CloseRequestAsync();
#endif
			return;

		case Dialogs::DIALOG_NAG:
			KillDialog(Dialogs::DIALOG_NAG);
			DoRegister();
			return;

		case Dialogs::DIALOG_INFO:
			KillDialog(Dialogs::DIALOG_INFO);
			return;

		case Dialogs::DIALOG_PAUSED:
			KillDialog(Dialogs::DIALOG_PAUSED);
			return;

		case Dialogs::DIALOG_NO_MORE_MONEY:
			KillDialog(Dialogs::DIALOG_NO_MORE_MONEY);
			mBoard->AddSunMoney(100);
			return;

		case Dialogs::DIALOG_BONUS:
			KillDialog(Dialogs::DIALOG_BONUS);
			return;

		case Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN:
			KillDialog(Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN);
			mBoardResult = BoardResult::BOARDRESULT_QUIT;
			mBoard->TryToSaveGame();
			DoBackToMain();
			return;

		case Dialogs::DIALOG_USERDIALOG:
			FinishUserDialog(true);
			return;

		case Dialogs::DIALOG_CREATEUSER:
			FinishCreateUserDialog(true);
			return;

		case Dialogs::DIALOG_CONFIRMDELETEUSER:
			FinishConfirmDeleteUserDialog(true);
			return;

		case Dialogs::DIALOG_RENAMEUSER:
			FinishRenameUserDialog(true);
			return;

		case Dialogs::DIALOG_CREATEUSERERROR:
		case Dialogs::DIALOG_RENAMEUSERERROR:
			FinishNameError(theId - 2000);
			return;

		case Dialogs::DIALOG_CHEAT:
			FinishCheatDialog(true);
			return;

		case Dialogs::DIALOG_JOIN_LAN:
			FinishJoinLanDialog(true);
			return;

		case Dialogs::DIALOG_RESTARTCONFIRM:
			FinishRestartConfirmDialog();
			return;

		case Dialogs::DIALOG_TIMESUP:
			FinishTimesUpDialog();
			return;

		case 20008:
			KillDialog(20008);
			KillDialog(Dialogs::DIALOG_CHECKING_UPDATES);
			return;

		default:
			KillDialog(theId - 2000);
			return;
		}
	}

	if (theId % 10000 >= 3000 && theId < 4000)  // ids in [3000, 4000): the "No" button of dialog (theId - 3000)
	{
		switch (theId - 3000)
		{
		case Dialogs::DIALOG_PREGAME_NAG:
			KillDialog(Dialogs::DIALOG_PREGAME_NAG);
			Shutdown();
			return;

		case Dialogs::DIALOG_LOAD_GAME:
			KillDialog(Dialogs::DIALOG_LOAD_GAME);
			return;

		case Dialogs::DIALOG_USERDIALOG:
			FinishUserDialog(false);
			return;

		case Dialogs::DIALOG_CREATEUSER:
			FinishCreateUserDialog(false);
			return;

		case Dialogs::DIALOG_CONFIRMDELETEUSER:
			FinishConfirmDeleteUserDialog(false);
			return;

		case Dialogs::DIALOG_RENAMEUSER:
			FinishRenameUserDialog(false);
			return;

		case Dialogs::DIALOG_CHEAT:
			FinishCheatDialog(false);
			return;

		case Dialogs::DIALOG_JOIN_LAN:
			FinishJoinLanDialog(false);
			return;

		case Dialogs::DIALOG_TIMESUP:
			FinishTimesUpDialog();
			return;

		case 10008:
			KillDialog(10008);
			KillDialog(Dialogs::DIALOG_CHECKING_UPDATES);
			return;

		default:
			KillDialog(theId - 3000);
			return;
		}
	}
}

void LawnApp::CenterDialog(Dialog* theDialog, int theWidth, int theHeight)
{
	theDialog->Resize((BOARD_WIDTH - theWidth) / 2, (BOARD_HEIGHT - theHeight) / 2, theWidth, theHeight);
}

void LawnApp::PlayFoley(FoleyType theFoleyType)
{
	if (!mMuteSoundsForCutscene)
	{
		mSoundSystem->PlayFoley(theFoleyType);
	}
}

void LawnApp::PlayFoleyPitch(FoleyType theFoleyType, float thePitch)
{
	if (!mMuteSoundsForCutscene)
	{
		mSoundSystem->PlayFoleyPitch(theFoleyType, thePitch);
	}
}

std::string LawnApp::GetStageString(int theLevel)
{
	int aArea = std::clamp((theLevel - 1) / LEVELS_PER_AREA + 1, 1, ADVENTURE_AREAS + 1);
	int aSub = theLevel - (aArea - 1) * LEVELS_PER_AREA;
	return StrFormat("%d-%d", aArea, aSub);
}

bool LawnApp::IsAdventureMode()
{
	return mGameMode == GameMode::GAMEMODE_ADVENTURE;
}

bool LawnApp::IsSurvivalMode()
{
	return mGameMode >= GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1 && mGameMode <= GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_5;
}

bool LawnApp::IsPuzzleMode()
{
	return
		(mGameMode >= GameMode::GAMEMODE_SCARY_POTTER_1 && mGameMode <= GameMode::GAMEMODE_SCARY_POTTER_ENDLESS) ||
		(mGameMode >= GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 && mGameMode <= GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS);
}

bool LawnApp::IsChallengeMode()
{
	return !IsAdventureMode() && !IsPuzzleMode() && !IsSurvivalMode();
}

bool LawnApp::IsSurvivalNormal(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

bool LawnApp::IsSurvivalHard(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

bool LawnApp::IsSurvivalEndless(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

bool LawnApp::IsEndlessScaryPotter(GameMode theGameMode)
{
	return theGameMode == GameMode::GAMEMODE_SCARY_POTTER_ENDLESS;
}

bool LawnApp::IsEndlessIZombie(GameMode theGameMode)
{
	return theGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS;
}

bool LawnApp::IsContinuousChallenge()
{
	return
		IsArtChallenge() ||
		IsSlotMachineLevel() ||
		IsFinalBossLevel() ||
		mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		mGameMode == GameMode::GAMEMODE_UPSELL ||
		mGameMode == GameMode::GAMEMODE_INTRO ||
		mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST;
}

bool LawnApp::IsArtChallenge()
{
	if (mBoard == nullptr)
		return false;

	return
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_WALLNUT ||
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_SUNFLOWER ||
		mGameMode == GameMode::GAMEMODE_CHALLENGE_SEEING_STARS;
}

bool LawnApp::IsSquirrelLevel()
{
	return mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SQUIRREL;
}

bool LawnApp::IsIZombieLevel()
{
	if (mBoard == nullptr)
		return false;

	return
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_2 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_4 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_5 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_6 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_7 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_8 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_9 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS;
}

bool LawnApp::IsShovelLevel()
{
	return mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SHOVEL;
}

bool LawnApp::IsWallnutBowlingLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING || mGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING_2)
		return true;

	return IsAdventureMode() && mBoard->mLevel == 5;
}

bool LawnApp::IsSlotMachineLevel()
{
	return (mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE);
}

bool LawnApp::IsWhackAZombieLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_WHACK_A_ZOMBIE)
		return true;

	return IsAdventureMode() && mBoard->mLevel == 15;
}

bool LawnApp::IsLittleTroubleLevel()
{
	return (mBoard && (mGameMode == GameMode::GAMEMODE_CHALLENGE_LITTLE_TROUBLE || (mGameMode == GameMode::GAMEMODE_ADVENTURE && mBoard->mLevel == 25)));
}

bool LawnApp::IsScaryPotterLevel()
{
	if (mGameMode >= GameMode::GAMEMODE_SCARY_POTTER_1 && mGameMode <= GameMode::GAMEMODE_SCARY_POTTER_ENDLESS)
		return true;

	return IsAdventureMode() && mBoard && mBoard->mLevel == 35;
}

bool LawnApp::IsStormyNightLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_STORMY_NIGHT)
		return true;

	return IsAdventureMode() && mBoard->mLevel == 40;
}

bool LawnApp::IsBungeeBlitzLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_BUNGEE_BLITZ)
		return true;

	return IsAdventureMode() && mBoard->mLevel == 45;
}

bool LawnApp::IsMiniBossLevel()
{
	if (mBoard == nullptr)
		return false;

	return IsAdventureMode() && (mBoard->mLevel == 10 || mBoard->mLevel == 20 || mBoard->mLevel == 30);
}

bool LawnApp::IsFinalBossLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_FINAL_BOSS)
		return true;

	return IsAdventureMode() && mBoard->mLevel == 50;
}

bool LawnApp::IsChallengeWithoutSeedBank()
{
	return
		mGameMode == GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS ||
		mGameMode == GameMode::GAMEMODE_UPSELL ||
		mGameMode == GameMode::GAMEMODE_INTRO ||
		IsWhackAZombieLevel() ||
		IsSquirrelLevel() ||
		IsScaryPotterLevel() ||
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM;
}

bool LawnApp::IsNight()
{
	if (IsIceDemo() || mPlayerInfo == nullptr)
		return false;

	return (mPlayerInfo->mLevel >= 11 && mPlayerInfo->mLevel <= 20) || (mPlayerInfo->mLevel >= 31 && mPlayerInfo->mLevel <= 40) || mPlayerInfo->mLevel == 50;
}

int LawnApp::GetCurrentChallengeIndex()
{
	return static_cast<int>(mGameMode) - static_cast<int>(GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1);
}

const ChallengeDefinition& LawnApp::GetCurrentChallengeDef()
{
	return GetChallengeDefinition(GetCurrentChallengeIndex());
}

PottedPlant* LawnApp::GetPottedPlantByIndex(int thePottedPlantIndex)
{
	PVZP_ASSERT(thePottedPlantIndex >= 0 && thePottedPlantIndex < mPlayerInfo->mNumPottedPlants);
	return &mPlayerInfo->mPottedPlant[thePottedPlantIndex];
}

bool LawnApp::UpdateAppStep(bool* updated)
{
	if (mCloseRequest)
	{
		Shutdown();
		return false;
	}
	return SexyAppBase::UpdateAppStep(updated);
}

bool LawnApp::UpdateApp()
{
	if (mCloseRequest)
	{
		Shutdown();
		return false;
	}

	//if (mLoadingThreadCompleted)
	//{
	//	LoadingThreadCompleted();
	//}

	bool updated = SexyAppBase::UpdateApp();

	//if (mLoadingThreadCompleted && !mExitToTop)
	//{
	//	CheckForUpdates();
	//}

	return updated;
}

void LawnApp::CloseRequestAsync()
{
	mExitToTop = true;
	mCloseRequest = true;
}

SeedType LawnApp::GetAwardSeedForLevel(int theLevel)
{
	int aArea = (theLevel - 1) / LEVELS_PER_AREA + 1;
	int aSub = (theLevel - 1) % LEVELS_PER_AREA + 1;
	int aSeedsHasGot = (aArea - 1) * 8 + aSub;  // in general, each area awards 8 plants and each level awards 1
	if (aSub >= 10)
	{
		aSeedsHasGot -= 2;  // 2 levels in this area don't award a new plant
	}
	else if (aSub >= 5)
	{
		aSeedsHasGot -= 1;  // 1 level in this area doesn't award a new plant
	}
	if (aSeedsHasGot > 40)
	{
		aSeedsHasGot = 40;
	}

	return (SeedType)aSeedsHasGot;
}

int LawnApp::GetSeedsAvailable()
{
	int aLevel = mPlayerInfo->GetLevel();
	if (HasFinishedAdventure() || aLevel > 50)
	{
		return 49;
	}

	SeedType aSeedTypeMax = GetAwardSeedForLevel(aLevel);
	return std::min(NUM_SEEDS_IN_CHOOSER, aSeedTypeMax);
}

bool LawnApp::HasSeedType(SeedType theSeedType)
{
	if (IsTrialStageLocked() && theSeedType >= SeedType::SEED_JALAPENO)
		return false;

	/*  optimization
	if (theSeedType >= SeedType::SEED_GATLINGPEA && theSeedType <= SeedType::SEED_IMITATER)
		return mPlayerInfo->mPurchases[theSeedType - SeedType::SEED_GATLINGPEA];
	*/

	switch (theSeedType)
	{
	case SeedType::SEED_GATLINGPEA:
		return mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_GATLINGPEA] > 0;
	case SeedType::SEED_TWINSUNFLOWER:
		return mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_TWINSUNFLOWER] > 0;
	case SeedType::SEED_GLOOMSHROOM:
		return mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_GLOOMSHROOM] > 0;
	case SeedType::SEED_CATTAIL:
		return mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_CATTAIL] > 0;
	case SeedType::SEED_WINTERMELON:
		return mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_WINTERMELON] > 0;
	case SeedType::SEED_GOLD_MAGNET:
		return mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_GOLD_MAGNET] > 0;
	case SeedType::SEED_SPIKEROCK:
		return mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_SPIKEROCK] > 0;
	case SeedType::SEED_COBCANNON:
		return mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_COBCANNON] > 0;
	case SeedType::SEED_IMITATER:
		return mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_IMITATER] > 0;
	default:
		return theSeedType < GetSeedsAvailable();
	}
}

Reanimation* LawnApp::AddReanimation(float theX, float theY, int theRenderOrder, ReanimationType theReanimationType)
{
	return mEffectSystem->mReanimationHolder->AllocReanimation(theX, theY, theRenderOrder, theReanimationType);
}

PvzpParticleSystem* LawnApp::AddPvzpParticle(float theX, float theY, int theRenderOrder, ParticleEffect theEffect)
{
	return mEffectSystem->mParticleHolder->AllocParticleSystem(theX, theY, theRenderOrder, theEffect);
}

ParticleSystemID LawnApp::ParticleGetID(PvzpParticleSystem* theParticle)
{
	return (ParticleSystemID)mEffectSystem->mParticleHolder->mParticleSystems.DataArrayGetID(theParticle);
}

ReanimationID LawnApp::ReanimationGetID(Reanimation* theReanimation)
{
	return static_cast<ReanimationID>(mEffectSystem->mReanimationHolder->mReanimations.DataArrayGetID(theReanimation));
}

PvzpParticleSystem* LawnApp::ParticleGet(ParticleSystemID theParticleID)
{
	return mEffectSystem->mParticleHolder->mParticleSystems.DataArrayGet(static_cast<unsigned int>(theParticleID));
}

PvzpParticleSystem* LawnApp::ParticleTryToGet(ParticleSystemID theParticleID)
{
	return mEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(static_cast<unsigned int>(theParticleID));
}

Reanimation* LawnApp::ReanimationGet(ReanimationID theReanimationID)
{
	return mEffectSystem->mReanimationHolder->mReanimations.DataArrayGet(static_cast<unsigned int>(theReanimationID));
}

Reanimation* LawnApp::ReanimationTryToGet(ReanimationID theReanimationID)
{
	return mEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet(static_cast<unsigned int>(theReanimationID));
}

void LawnApp::RemoveReanimation(ReanimationID theReanimationID)
{
	Reanimation* aReanim = ReanimationTryToGet(theReanimationID);
	if (aReanim)
	{
		aReanim->ReanimationDie();
	}
}

void LawnApp::RemoveParticle(ParticleSystemID theParticleID)
{
	PvzpParticleSystem* aParticle = ParticleTryToGet(theParticleID);
	if (aParticle)
	{
		aParticle->ParticleSystemDie();
	}
}

bool LawnApp::AdvanceCrazyDaveText()
{
	std::string aMessageName = StrFormat("[CRAZY_DAVE_%d]", mCrazyDaveMessageIndex + 1);
	if (!PvzpStringListExists(aMessageName))
	{
		return false;
	}

	CrazyDaveTalkIndex(mCrazyDaveMessageIndex + 1);
	return true;
}

std::string LawnApp::GetCrazyDaveText(int theMessageIndex)
{
	std::string aMessage = StrFormat("[CRAZY_DAVE_%d]", theMessageIndex);
	aMessage = PvzpReplaceString(aMessage, "{PLAYER_NAME}", mPlayerInfo->mName);
	aMessage = PvzpReplaceString(aMessage, "{MONEY}", GetMoneyString(mPlayerInfo->mCoins));
	int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);
	aMessage = PvzpReplaceString(aMessage, "{UPGRADE_COST}", GetMoneyString(aCost));
	return aMessage;
}

bool LawnApp::CanShowAlmanac()
{
	if (IsIceDemo())
		return false;

	if (mPlayerInfo == nullptr)
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mLevel >= 15;
}

bool LawnApp::CanShowStore()
{
	if (IsIceDemo())
		return false;

	if (mPlayerInfo == nullptr)
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mHasSeenUpsell || mPlayerInfo->mLevel >= 25;
}

bool LawnApp::CanShowZenGarden()
{
	if (mPlayerInfo == nullptr)
		return false;

	if (IsTrialStageLocked())
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mLevel >= 45;
}

bool LawnApp::CanSpawnYetis()
{
	const ZombieDefinition& aZombieDef = GetZombieDefinition(ZombieType::ZOMBIE_YETI);
	return HasFinishedAdventure() && (mPlayerInfo->mFinishedAdventure >= 2 || mPlayerInfo->mLevel >= aZombieDef.mStartingLevel);
}

bool LawnApp::HasBeatenChallenge(GameMode theGameMode)
{
	if (mPlayerInfo == nullptr)
		return false;

	int aChallengeIndex = theGameMode - GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	PVZP_ASSERT(aChallengeIndex >= 0 && aChallengeIndex < NUM_CHALLENGE_MODES);
	if (IsSurvivalNormal(theGameMode))
	{
		return mPlayerInfo->mChallengeRecords[aChallengeIndex] >= SURVIVAL_NORMAL_FLAGS;
	}
	if (IsSurvivalHard(theGameMode))
	{
		return mPlayerInfo->mChallengeRecords[aChallengeIndex] >= SURVIVAL_HARD_FLAGS;
	}
	if (IsSurvivalEndless(theGameMode) || IsEndlessScaryPotter(theGameMode) || IsEndlessIZombie(theGameMode))
	{
		return false;
	}
	return mPlayerInfo->mChallengeRecords[aChallengeIndex] > 0;
}

bool LawnApp::HasFinishedAdventure()
{
	return mPlayerInfo && mPlayerInfo->mFinishedAdventure > 0;
}

bool LawnApp::IsFirstTimeAdventureMode()
{
	return IsAdventureMode() && !HasFinishedAdventure();
}

void LawnApp::CrazyDaveEnter()
{
	PVZP_ASSERT(mCrazyDaveState == CRAZY_DAVE_OFF);
	PVZP_ASSERT(!ReanimationTryToGet(mCrazyDaveReanimID));

	Reanimation* aCrazyDaveReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
	aCrazyDaveReanim->mIsAttachment = true;
	aCrazyDaveReanim->SetBasePoseFromAnim("anim_idle_handing");
	mCrazyDaveReanimID = ReanimationGetID(aCrazyDaveReanim);
	aCrazyDaveReanim->PlayReanim("anim_enter", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);

	mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_ENTERING;
	mCrazyDaveMessageIndex = -1;
	mCrazyDaveMessageText.clear();
	mCrazyDaveBlinkCounter = RandRangeInt(400, 800);

	if (mGameScene == GameScenes::SCENE_LEVEL_INTRO && IsStormyNightLevel())
	{
		aCrazyDaveReanim->mColorOverride = Color(64, 64, 64);
	}
}

void LawnApp::CrazyDaveDie()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim)
	{
		aCrazyDaveReanim->ReanimationDie();
	}

	mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
	mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveMessageIndex = -1;
	mCrazyDaveMessageText.clear();

	CrazyDaveStopSound();
}

void LawnApp::CrazyDaveLeave()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim)
	{
		if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
		{
			CrazyDaveDoneHanding();
		}

		aCrazyDaveReanim->PlayReanim("anim_leave", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
		aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);

		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_LEAVING;
		mCrazyDaveMessageIndex = -1;
		mCrazyDaveMessageText.clear();

		CrazyDaveStopSound();
	}
}

void LawnApp::CrazyDaveTalkIndex(int theMessageIndex)
{
	mCrazyDaveMessageIndex = theMessageIndex;
	std::string aMessageText = GetCrazyDaveText(theMessageIndex);
	CrazyDaveTalkMessage(aMessageText);
}

void LawnApp::CrazyDaveDoneHanding()
{
	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);
	ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
	AttachmentDie(aHandTrackInstance->mAttachmentID);

	PvzpTrace("DoneHanding");
}

void LawnApp::CrazyDaveStopSound()
{
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_EXTRA_LONG);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_CRAZY);
}

void LawnApp::CrazyDaveTalkMessage(const std::string& theMessage)
{
	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);

	bool doHanding = false;
	if (theMessage.find("{HANDING}") != std::string::npos)
	{
		doHanding = true;
	}
	if ((mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING) && !doHanding)
	{
		CrazyDaveDoneHanding();
	}

	bool doSound = true;
	if (theMessage.find("{NO_SOUND}") != std::string::npos)
	{
		doSound = false;
	}
	else
	{
		CrazyDaveStopSound();
	}

	int aWordsCount = 0;
	bool isControlWord = false;
	for (size_t i = 0; i < theMessage.size(); i++)  // byte count tracks CJK talk duration better than code points
	{
		if (theMessage[i] == '{')
		{
			isControlWord = true;
		}
		else if (theMessage[i] == '}')
		{
			isControlWord = false;
		}
		else if (!isControlWord)
		{
			aWordsCount++;
		}
	}

	aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);

	if (mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_TALKING || doSound)
	{
		if (doHanding)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			if (doSound)
			{
				if (theMessage.find("{SHORT_SOUND}") != std::string::npos)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
				}
				else if (theMessage.find("{SCREAM}") != std::string::npos)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM);
				}
				else
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
				}
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find("{SHAKE}") != std::string::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_crazy", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_CRAZY);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find("{SCREAM}") != std::string::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_smalltalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find("{SCREAM2}") != std::string::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_mediumtalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM_2);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find("{SHOW_WALLNUT}") != std::string::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aWallnutReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_WALLNUT);
			aWallnutReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
			PvzpTrace("Handed");

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachEffect* aAttachEffect = AttachReanim(aHandTrackInstance->mAttachmentID, aWallnutReanim, 100.0f, 393.0f);
			aAttachEffect->mOffset.m00 = 1.2f;
			aAttachEffect->mOffset.m11 = 1.2f;

			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM_2);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find("{SHOW_HAMMER}") != std::string::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aHammerReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_HAMMER);
			aHammerReanim->PlayReanim("anim_whack_zombie", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aHammerReanim->mAnimTime = 1.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachEffect* aAttachEffect = AttachReanim(aHandTrackInstance->mAttachmentID, aHammerReanim, 62.0f, 445.0f);
			aAttachEffect->mOffset.m00 = 1.5f;
			aAttachEffect->mOffset.m11 = 1.5f;

			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find("{SHOW_FERTILIZER}") != std::string::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aFertilizerReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZENGARDEN_FERTILIZER);
			aFertilizerReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aFertilizerReanim->mAnimRate = 0.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aFertilizerReanim, 102.0f, 412.0f);
			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find("{SHOW_TREE_FOOD}") != std::string::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aTreeFoodReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_TREEOFWISDOM_TREEFOOD);
			aTreeFoodReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aTreeFoodReanim->mAnimRate = 0.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aTreeFoodReanim, 102.0f, 412.0f);
			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find("{SHOW_MONEYBAG}") != std::string::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aMoneyBagReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZENGARDEN_FERTILIZER);
			aMoneyBagReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aMoneyBagReanim->mAnimRate = 0.0f;
			aMoneyBagReanim->SetImageOverride("bag", IMAGE_MONEYBAG);

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aMoneyBagReanim, 90.0f, 405.0f);
			aCrazyDaveReanim->Update();
			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else
		{
			if (aWordsCount < 23)
			{
				aCrazyDaveReanim->PlayReanim("anim_smalltalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
			else if (aWordsCount < 52)
			{
				aCrazyDaveReanim->PlayReanim("anim_mediumtalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
			else
			{
				aCrazyDaveReanim->PlayReanim("anim_blahblah", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_EXTRA_LONG);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
		}
	}

	mCrazyDaveMessageText = theMessage;
}

void LawnApp::CrazyDaveStopTalking()
{
	bool aDoneHanding = true;
	if (mGameMode == GameMode::GAMEMODE_UPSELL)
	{
		aDoneHanding = false;
	}
	if (aDoneHanding && mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		CrazyDaveDoneHanding();
	}

	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);
	aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);
	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING && !aDoneHanding)
	{
		aCrazyDaveReanim->PlayReanim("anim_idle_handing", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		aCrazyDaveReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
	}

	mCrazyDaveMessageIndex = -1;
	mCrazyDaveMessageText.clear();
	CrazyDaveStopSound();
}

void LawnApp::UpdateCrazyDave()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim == nullptr)
		return;

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_ENTERING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING)
	{
		if (aCrazyDaveReanim->mLoopCount > 0)
		{
			aCrazyDaveReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
		}
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		if (aCrazyDaveReanim->mLoopCount > 0)
		{
			aCrazyDaveReanim->PlayReanim("anim_idle_handing", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
		}
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_LEAVING && aCrazyDaveReanim->mLoopCount > 0)
	{
		CrazyDaveDie();
	}

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_IDLING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		if (mCrazyDaveMessageText.find("{MOUTH_BIG_SMILE}") != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH1);
		}
		else if (mCrazyDaveMessageText.find("{MOUTH_SMALL_SMILE}") != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH5);
		}
		else if (mCrazyDaveMessageText.find("{MOUTH_BIG_OH}") != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH4);
		}
		else if (mCrazyDaveMessageText.find("{MOUTH_SMALL_OH}") != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH6);
		}
	}

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_IDLING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING ||
		mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		mCrazyDaveBlinkCounter--;
		if (mCrazyDaveBlinkCounter <= 0)
		{
			mCrazyDaveBlinkCounter = RandRangeInt(400, 800);
			Reanimation* aBlinkReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
			aBlinkReanim->SetFramesForLayer("anim_blink");
			aBlinkReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD;
			aBlinkReanim->mAnimRate = 15.0f;
			aBlinkReanim->AttachToAnotherReanimation(aCrazyDaveReanim, "Dave_head");
			aBlinkReanim->mColorOverride = aCrazyDaveReanim->mColorOverride;
			aCrazyDaveReanim->AssignRenderGroupToTrack("Dave_eye", RENDER_GROUP_HIDDEN);
			mCrazyDaveBlinkReanimID = ReanimationGetID(aBlinkReanim);
		}
	}

	Reanimation* aBlinkReanim = ReanimationTryToGet(mCrazyDaveBlinkReanimID);
	if (aBlinkReanim && aBlinkReanim->mLoopCount > 0)
	{
		aCrazyDaveReanim->AssignRenderGroupToTrack("Dave_eye", RENDER_GROUP_NORMAL);
		RemoveReanimation(mCrazyDaveBlinkReanimID);
		mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	aCrazyDaveReanim->Update();
}

void LawnApp::DrawCrazyDave(Graphics* g)
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim == nullptr)
		return;

	if (mCrazyDaveMessageText.size())
	{
		Image* aBubbleImage = IMAGE_STORE_SPEECHBUBBLE2;
		int aPosX = 285;
		int aPosY = 20;
		if (GetDialog(Dialogs::DIALOG_STORE))
		{
			aBubbleImage = IMAGE_STORE_SPEECHBUBBLE;
			aPosX -= 180;
			aPosY -= 78;
		}
		else if (mGameMode == GameMode::GAMEMODE_UPSELL)
		{
			aPosX += 130;
			aPosY += 70;
		}
		g->DrawImage(aBubbleImage, aPosX, aPosY);

		std::string aBubbleText = mCrazyDaveMessageText;
		Rect aRect(aPosX + 25, aPosY + 6, 233, 144);
		if (aBubbleText.find("{SHAKE}") != std::string::npos)
		{
			aBubbleText = PvzpReplaceString(aBubbleText, "{SHAKE}", "");
			aRect.mX += rand() % 2;
			aRect.mY += rand() % 2;
		}

		bool clickToContinue = true;
		if (mGameMode == GameMode::GAMEMODE_UPSELL)
		{
			clickToContinue = false;
		}
		else if (aBubbleText.find("{NO_CLICK}") != std::string::npos)
		{
			aBubbleText = PvzpReplaceString(aBubbleText, "{NO_CLICK}", "");
			clickToContinue = false;
		}

		auto aWrapEnum = static_cast<DrawStringJustification>(GetInteger("CRAZY_DAVE_MESSAGE_TEXT_WRAP_ENUM", DS_ALIGN_CENTER_VERTICAL_MIDDLE));
		PvzpDrawStringWrapped(g, aBubbleText, aRect, FONT_BRIANNETOD16, Color::Black, aWrapEnum);
		if (clickToContinue)
		{
			PvzpDrawString(g, GetString("CLICK_TO_CONTINUE", "click to continue"), aPosX + 139, aPosY + 140, FONT_PICO129, Color::Black, DrawStringJustification::DS_ALIGN_CENTER);
		}
	}

	aCrazyDaveReanim->Draw(g);
}

int LawnApp::GetNumPreloadingTasks()
{
#ifdef LOW_MEMORY
	return 0;
#endif

	int aTaskCount = 10;
	if (mPlayerInfo)
	{
		for (SeedType i = SeedType::SEED_PEASHOOTER; i < SeedType::NUM_SEED_TYPES; i = static_cast<SeedType>(static_cast<int>(i) + 1))
		{
			if (HasSeedType(i) || HasFinishedAdventure())
			{
				aTaskCount++;
			}
		}

		for (ZombieType i = ZombieType::ZOMBIE_NORMAL; i < ZombieType::NUM_ZOMBIE_TYPES; i = static_cast<ZombieType>(static_cast<int>(i) + 1))
		{
			if (HasFinishedAdventure() || mPlayerInfo->mLevel >= GetZombieDefinition(i).mStartingLevel)
			{
				if (i != ZombieType::ZOMBIE_BOSS &&
					i != ZombieType::ZOMBIE_CATAPULT &&
					i != ZombieType::ZOMBIE_GARGANTUAR &&
					i != ZombieType::ZOMBIE_DIGGER &&
					i != ZombieType::ZOMBIE_ZAMBONI)
				{
					aTaskCount++;
				}
			}
		}
	}
	return aTaskCount * 68;
}

void LawnApp::PreloadForUser()
{
	int aNumTasks = mCompletedLoadingThreadTasks + GetNumPreloadingTasks();
	if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
	{
		PvzpTrace("preload canceled\n");
		mNumLoadingThreadTasks = aNumTasks;
		return;
	}

#ifndef LOW_MEMORY
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PUFF, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_READYSETPLANT, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_FINAL_WAVE, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SUN, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_TEXT_FADE_ON, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_NEWSPAPER, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SELECTOR_SCREEN, true);
	mCompletedLoadingThreadTasks += 340;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_HAND, true);
	mCompletedLoadingThreadTasks += 68;

	if (mPlayerInfo)
	{
		for (SeedType i = SeedType::SEED_PEASHOOTER; i < SeedType::NUM_SEED_TYPES; i = static_cast<SeedType>(static_cast<int>(i) + 1))
		{
			if (HasSeedType(i) || HasFinishedAdventure())
			{
				Plant::PreloadPlantResources(i);
				if (mCompletedLoadingThreadTasks < aNumTasks)
				{
					mCompletedLoadingThreadTasks += 68;
				}

				if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
				{
					PvzpTrace("preload canceled\n");
					mNumLoadingThreadTasks = aNumTasks;
					return;
				}

				if (mShutdown || mCloseRequest)
				{
					return;
				}
			}
		}

		for (ZombieType i = ZombieType::ZOMBIE_NORMAL; i < ZombieType::NUM_ZOMBIE_TYPES; i = static_cast<ZombieType>(static_cast<int>(i) + 1))
		{
			if (!HasFinishedAdventure() && mPlayerInfo->mLevel < GetZombieDefinition(i).mStartingLevel)
			{
				continue;
			}
			if (i == ZombieType::ZOMBIE_BOSS || i == ZombieType::ZOMBIE_CATAPULT || i == ZombieType::ZOMBIE_GARGANTUAR ||
				i == ZombieType::ZOMBIE_DIGGER || i == ZombieType::ZOMBIE_ZAMBONI)
			{
				continue;
			}

			Zombie::PreloadZombieResources(i);
			if (mCompletedLoadingThreadTasks < aNumTasks)
			{
				mCompletedLoadingThreadTasks += 68;
			}

			if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
			{
				PvzpTrace("preload canceled\n");
				mNumLoadingThreadTasks = aNumTasks;
				return;
			}

			if (mShutdown || mCloseRequest)
			{
				return;
			}
		}
	}
#endif

	if (mCompletedLoadingThreadTasks != aNumTasks)
	{
		PvzpTrace("num preload tasks wasn't calculated correctly");
		mCompletedLoadingThreadTasks = aNumTasks;
	}
}

std::string LawnApp::Pluralize(int theCount, const char* theSingular, const char* thePlural)
{
	if (theCount == 1)
	{
		return PvzpReplaceNumberString(theSingular, "{COUNT}", theCount);
	}

	return PvzpReplaceNumberString(thePlural, "{COUNT}", theCount);
}

int LawnApp::GetNumTrophies(ChallengePage thePage)
{
	int aNumTrophies = 0;

	for (int i = 0; i < NUM_CHALLENGE_MODES; i++)
	{
		const ChallengeDefinition& aDef = GetChallengeDefinition(i);
		if (aDef.mPage == thePage && HasBeatenChallenge(aDef.mChallengeMode))
		{
			aNumTrophies++;
		}
	}

	return aNumTrophies;
}

int LawnApp::TrophiesNeedForGoldSunflower()
{
	return 48 - GetNumTrophies(CHALLENGE_PAGE_SURVIVAL) - GetNumTrophies(CHALLENGE_PAGE_CHALLENGE) - GetNumTrophies(CHALLENGE_PAGE_PUZZLE);
}

bool LawnApp::EarnedGoldTrophy()
{
	return HasFinishedAdventure() && TrophiesNeedForGoldSunflower() <= 0;
}

void LawnApp::FinishZenGardenToturial()
{
	mBoardResult = BoardResult::BOARDRESULT_WON;
	KillBoard();
	PreNewGame(GameMode::GAMEMODE_ADVENTURE, false);
}

bool LawnApp::IsTrialStageLocked()
{
	if (mDebugTrialLocked)
		return true;

	return mTrialType == TrialType::TRIALTYPE_STAGELOCKED;
}

void LawnApp::InitHook()
{
	mTrialType = TrialType::TRIALTYPE_NONE;
}

std::string LawnApp::GetMoneyString(int theAmount)
{
	int aValue = theAmount * 10;
	if (aValue > 999999)
	{
		return StrFormat("$%d,%03d,%03d", aValue / 1000000, (aValue - aValue / 1000000 * 1000000) / 1000, aValue - aValue / 1000 * 1000);
	}
	else if (aValue > 9999)
	{
		return StrFormat("$%d,%03d", aValue / 1000, aValue - aValue / 1000 * 1000);
	}
	else
	{
		return StrFormat("$%d", aValue);
	}
}

std::string LawnGetCurrentLevelName()
{
	if (gLawnApp == nullptr)
	{
		return "Before App";
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_LOADING)
	{
		return "Game Loading";
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_MENU)
	{
		return "Game Selector";
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_AWARD)
	{
		return "Award Screen";
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_CHALLENGE)
	{
		return "Challenge Screen";
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_CREDIT)
	{
		return "Credits";
	}
	if (gLawnApp->mBoard == nullptr)
	{
		return "Not Playing";
	}

	if (gLawnApp->IsFirstTimeAdventureMode())
	{
		return gLawnApp->GetStageString(gLawnApp->mBoard->mLevel);
	}
	if (gLawnApp->IsAdventureMode())
	{
		return StrFormat("F%d", gLawnApp->GetStageString(gLawnApp->mBoard->mLevel).c_str());
	}

	return gLawnApp->GetCurrentChallengeDef().mChallengeName;
}

bool LawnApp::CanDoPinataMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[static_cast<int>(GameMode::GAMEMODE_TREE_OF_WISDOM) - static_cast<int>(GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1)] >= 1000;
}

bool LawnApp::CanDoDanceMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[static_cast<int>(GameMode::GAMEMODE_TREE_OF_WISDOM) - static_cast<int>(GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1)] >= 500;
}

bool LawnApp::CanDoDaisyMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[static_cast<int>(GameMode::GAMEMODE_TREE_OF_WISDOM) - static_cast<int>(GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1)] >= 100;
}

void LawnApp::PlaySample(intptr_t theSoundNum)
{
	if (!mMuteSoundsForCutscene)
	{
		SexyAppBase::PlaySample(theSoundNum);
	}
}

void LawnApp::SwitchScreenMode(bool wantWindowed, bool is3d, bool force)
{
	SexyAppBase::SwitchScreenMode(wantWindowed, is3d, force);

	NewOptionsDialog* aNewOptionsDialog = (NewOptionsDialog*)GetDialog(Dialogs::DIALOG_NEWOPTIONS);
	if (aNewOptionsDialog)
	{
		aNewOptionsDialog->mFullscreenCheckbox->SetChecked(!mIsWindowed);
	}
}

void LawnApp::DoHighScoreDialog()
{

}

void LawnApp::DoRegister()
{

}

void LawnApp::DoRegisterError()
{

}

bool LawnApp::CanDoRegisterDialog()
{
	return false;
}

void LawnApp::DoNeedRegisterDialog()
{

}

void LawnApp::FinishModelessDialogs()
{
	// Kill dialogs bound to the board; a killed dialog counts as cancelled and deletion is deferred
	KillDialog(Dialogs::DIALOG_CONFIRM_RESTART);
	KillDialog(Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN);
	KillDialog(Dialogs::DIALOG_PAUSED);
	KillDialog(Dialogs::DIALOG_ALMANAC);  // may be mid-WaitForResult with the options dialog parked under it
	KillNewOptionsDialog();
}

bool LawnApp::NeedRegister()
{
	return false;
}

void LawnApp::UpdateRegisterInfo()
{

}
