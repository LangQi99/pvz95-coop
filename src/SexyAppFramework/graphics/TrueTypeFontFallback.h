/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace Sexy
{

class Image;
class SexyAppBase;

struct TrueTypeFallbackGlyph
{
	Image* mImage{};
	int mBearingX{};
	int mBearingY{};
	int mAdvance{};
	int mWidth{};
	int mHeight{};
};

class TrueTypeFontFallback
{
public:
	static TrueTypeFontFallback& Instance();

	bool GetGlyph(SexyAppBase* theApp, char32_t theChar, int thePixelHeight,
		TrueTypeFallbackGlyph& theGlyph);

private:
	TrueTypeFontFallback();
	~TrueTypeFontFallback();
	TrueTypeFontFallback(const TrueTypeFontFallback&) = delete;
	TrueTypeFontFallback& operator=(const TrueTypeFontFallback&) = delete;

	struct Impl;
	Impl* mImpl;
};

}
