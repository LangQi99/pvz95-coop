/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace PvzMultiplayer
{
	struct RollingLogConfig
	{
		std::size_t mMaxFileBytes{4U * 1024U * 1024U};
		std::size_t mBackupCount{3};
	};

	// Appends one complete record and rotates path -> path.1 -> path.2, etc.
	// The active file and every backup are individually bounded by
	// mMaxFileBytes, so total storage is bounded by
	// mMaxFileBytes * (mBackupCount + 1).
	bool AppendRollingLog(const std::filesystem::path& thePath,
		std::string_view theRecord, RollingLogConfig theConfig = {});
}
