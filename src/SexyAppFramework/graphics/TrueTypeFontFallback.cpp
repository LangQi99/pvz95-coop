/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "graphics/TrueTypeFontFallback.h"

#include "Common.h"
#include "SexyAppBase.h"
#include "graphics/MemoryImage.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

#ifdef PVZ_HAS_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

using namespace Sexy;

struct TrueTypeFontFallback::Impl
{
	struct CachedGlyph
	{
		std::unique_ptr<MemoryImage> mImage;
		int mBearingX{};
		int mBearingY{};
		int mAdvance{};
		int mWidth{};
		int mHeight{};
		bool mFound{};
	};

	std::mutex mMutex;
	bool mLoadAttempted{};
	std::vector<unsigned char> mFontBytes;
	std::map<std::tuple<int, char32_t>, CachedGlyph> mGlyphs;

#ifdef PVZ_HAS_FREETYPE
	FT_Library mLibrary{};
	FT_Face mFace{};

	~Impl()
	{
		if (mFace)
			FT_Done_Face(mFace);
		if (mLibrary)
			FT_Done_FreeType(mLibrary);
	}

	bool LoadFontFile(const std::filesystem::path& thePath)
	{
		std::ifstream aFile(thePath, std::ios::binary | std::ios::ate);
		if (!aFile)
			return false;
		std::streamsize aSize = aFile.tellg();
		if (aSize <= 0)
			return false;
		aFile.seekg(0, std::ios::beg);
		mFontBytes.resize(static_cast<size_t>(aSize));
		return static_cast<bool>(aFile.read(reinterpret_cast<char*>(mFontBytes.data()), aSize));
	}

	bool EnsureFace()
	{
		if (mLoadAttempted)
			return mFace != nullptr;
		mLoadAttempted = true;

		std::vector<std::filesystem::path> aCandidates = {
			PathFromU8(GetResourcePath("fzse_gbk.ttf")),
			PathFromU8(GetResourcePath("fonts/fzse_gbk.ttf"))
		};
#ifdef __APPLE__
		aCandidates.emplace_back("/System/Library/Fonts/PingFang.ttc");
		aCandidates.emplace_back("/System/Library/Fonts/STHeiti Light.ttc");
#elif defined(_WIN32)
		if (const char* aWindowsDir = std::getenv("WINDIR"))
		{
			aCandidates.emplace_back(std::filesystem::path(aWindowsDir) / "Fonts" / "msyh.ttc");
			aCandidates.emplace_back(std::filesystem::path(aWindowsDir) / "Fonts" / "simhei.ttf");
		}
#endif

		for (const auto& aPath : aCandidates)
		{
			mFontBytes.clear();
			if (LoadFontFile(aPath))
				break;
		}
		if (mFontBytes.empty() || FT_Init_FreeType(&mLibrary) != 0)
			return false;
		if (FT_New_Memory_Face(mLibrary, mFontBytes.data(),
			static_cast<FT_Long>(mFontBytes.size()), 0, &mFace) != 0)
		{
			mFace = nullptr;
			return false;
		}
		return true;
	}

	CachedGlyph& LoadGlyph(SexyAppBase* theApp, char32_t theChar, int thePixelHeight)
	{
		thePixelHeight = std::max(thePixelHeight, 8);
		auto [anIt, anInserted] = mGlyphs.try_emplace({thePixelHeight, theChar});
		CachedGlyph& aGlyph = anIt->second;
		if (!anInserted)
			return aGlyph;

		if (!theApp || !EnsureFace() || FT_Set_Pixel_Sizes(mFace, 0,
			static_cast<FT_UInt>(thePixelHeight)) != 0)
			return aGlyph;
		FT_UInt aGlyphIndex = FT_Get_Char_Index(mFace, static_cast<FT_ULong>(theChar));
		if (aGlyphIndex == 0 || FT_Load_Glyph(mFace, aGlyphIndex, FT_LOAD_DEFAULT) != 0 ||
			FT_Render_Glyph(mFace->glyph, FT_RENDER_MODE_NORMAL) != 0)
			return aGlyph;

		FT_GlyphSlot aSlot = mFace->glyph;
		const FT_Bitmap& aBitmap = aSlot->bitmap;
		aGlyph.mBearingX = aSlot->bitmap_left;
		aGlyph.mBearingY = aSlot->bitmap_top;
		aGlyph.mAdvance = std::max(1, static_cast<int>((aSlot->advance.x + 32) >> 6));
		aGlyph.mWidth = static_cast<int>(aBitmap.width);
		aGlyph.mHeight = static_cast<int>(aBitmap.rows);
		aGlyph.mFound = true;

		if (aGlyph.mWidth == 0 || aGlyph.mHeight == 0)
			return aGlyph;
		aGlyph.mImage = std::make_unique<MemoryImage>(theApp);
		aGlyph.mImage->Create(aGlyph.mWidth, aGlyph.mHeight);
		uint32_t* aBits = aGlyph.mImage->GetBits();
		for (int y = 0; y < aGlyph.mHeight; ++y)
		{
			const unsigned char* aRow = aBitmap.pitch >= 0 ?
				aBitmap.buffer + y * aBitmap.pitch :
				aBitmap.buffer + (aGlyph.mHeight - 1 - y) * -aBitmap.pitch;
			for (int x = 0; x < aGlyph.mWidth; ++x)
			{
				unsigned char anAlpha = 0;
				if (aBitmap.pixel_mode == FT_PIXEL_MODE_MONO)
					anAlpha = (aRow[x >> 3] & (0x80U >> (x & 7))) ? 255 : 0;
				else
					anAlpha = aRow[x];
				aBits[y * aGlyph.mWidth + x] =
					(static_cast<uint32_t>(anAlpha) << 24) | 0x00FFFFFFU;
			}
		}
		aGlyph.mImage->BitsChanged();
		return aGlyph;
	}
#else
	~Impl() = default;
#endif
};

TrueTypeFontFallback& TrueTypeFontFallback::Instance()
{
	// Keep cached GPU-backed glyph images alive until process exit without
	// depending on static destruction order relative to SexyAppBase.
	static TrueTypeFontFallback* aFallback = new TrueTypeFontFallback();
	return *aFallback;
}

TrueTypeFontFallback::TrueTypeFontFallback() :
	mImpl(new Impl())
{
}

TrueTypeFontFallback::~TrueTypeFontFallback()
{
	delete mImpl;
}

bool TrueTypeFontFallback::GetGlyph(SexyAppBase* theApp, char32_t theChar,
	int thePixelHeight, TrueTypeFallbackGlyph& theGlyph)
{
#ifdef PVZ_HAS_FREETYPE
	std::scoped_lock aLock(mImpl->mMutex);
	Impl::CachedGlyph& aCached = mImpl->LoadGlyph(theApp, theChar, thePixelHeight);
	if (!aCached.mFound)
		return false;
	theGlyph.mImage = aCached.mImage.get();
	theGlyph.mBearingX = aCached.mBearingX;
	theGlyph.mBearingY = aCached.mBearingY;
	theGlyph.mAdvance = aCached.mAdvance;
	theGlyph.mWidth = aCached.mWidth;
	theGlyph.mHeight = aCached.mHeight;
	return true;
#else
	(void)theApp;
	(void)theChar;
	(void)thePixelHeight;
	(void)theGlyph;
	return false;
#endif
}
