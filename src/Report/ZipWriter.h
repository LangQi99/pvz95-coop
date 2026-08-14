/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace PvzReport
{
	struct ZipSource
	{
		std::filesystem::path mSourcePath;
		std::string mArchiveName;
	};

	bool WriteZipArchive(const std::filesystem::path& theOutputPath,
		const std::vector<ZipSource>& theSources, std::string& theError);
}
