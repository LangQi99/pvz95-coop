/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "SexyAppFramework/misc/Rect.h"

#include <cstdint>

namespace Sexy
{

// OpenGL viewports use drawable pixels, while SDL mouse events use window
// coordinates. These differ on Retina/high-DPI displays.
inline Rect PresentationRectForWindow(const Rect& theDrawableRect,
	int theDrawableWidth, int theDrawableHeight, int theWindowWidth, int theWindowHeight)
{
	if (theDrawableWidth <= 0 || theDrawableHeight <= 0 ||
		theWindowWidth <= 0 || theWindowHeight <= 0)
		return theDrawableRect;

	auto ScaleX = [&](int theValue)
	{
		return static_cast<int>(static_cast<int64_t>(theValue) * theWindowWidth / theDrawableWidth);
	};
	auto ScaleY = [&](int theValue)
	{
		return static_cast<int>(static_cast<int64_t>(theValue) * theWindowHeight / theDrawableHeight);
	};

	int aLeft = ScaleX(theDrawableRect.mX);
	int aTop = ScaleY(theDrawableRect.mY);
	int aRight = ScaleX(theDrawableRect.mX + theDrawableRect.mWidth);
	int aBottom = ScaleY(theDrawableRect.mY + theDrawableRect.mHeight);
	return Rect(aLeft, aTop, aRight - aLeft, aBottom - aTop);
}

}
