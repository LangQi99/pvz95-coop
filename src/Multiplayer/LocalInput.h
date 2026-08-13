/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

namespace PvzMultiplayer
{

enum class PointerIntent
{
	NO_ACTION,
	PRIMARY_ACTION,
	CANCEL_SELECTION
};

// SDL reports a rapid second left click as 2 (and a rapid second right click
// as -2). PvZ has no separate double-click command, so retain the physical
// button meaning instead of dropping the second press.
constexpr PointerIntent DecodePointerIntent(int theClickCount)
{
	if (theClickCount == 1 || theClickCount == 2)
		return PointerIntent::PRIMARY_ACTION;
	if (theClickCount == -1 || theClickCount == -2)
		return PointerIntent::CANCEL_SELECTION;
	return PointerIntent::NO_ACTION;
}

// The level-4 shovel tutorial deliberately keeps the board in the intro scene
// while asking the player to use a gameplay tool.  LAN input must keep routing
// board actions during that tutorial instead of waiting for SCENE_PLAYING.
constexpr bool CanRouteLanBoardAction(bool theSceneIsPlaying, bool theShovelTutorialIsActive)
{
	return theSceneIsPlaying || theShovelTutorialIsActive;
}

}
