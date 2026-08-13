/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "RollingLog.h"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <string>

namespace PvzMultiplayer
{
	namespace
	{
		std::mutex gRollingLogMutex;

		std::filesystem::path BackupPath(const std::filesystem::path& thePath,
			std::size_t theIndex)
		{
			return std::filesystem::path(thePath.string() + "." + std::to_string(theIndex));
		}

		bool Rotate(const std::filesystem::path& thePath, std::size_t theBackupCount)
		{
			std::error_code anError;
			if (theBackupCount == 0)
			{
				std::filesystem::remove(thePath, anError);
				return !anError;
			}

			std::filesystem::remove(BackupPath(thePath, theBackupCount), anError);
			if (anError)
				return false;
			for (std::size_t anIndex = theBackupCount; anIndex > 1; --anIndex)
			{
				const auto aSource = BackupPath(thePath, anIndex - 1);
				if (!std::filesystem::exists(aSource, anError))
				{
					if (anError)
						return false;
					continue;
				}
				std::filesystem::rename(aSource, BackupPath(thePath, anIndex), anError);
				if (anError)
					return false;
			}

			if (std::filesystem::exists(thePath, anError))
			{
				if (anError)
					return false;
				std::filesystem::rename(thePath, BackupPath(thePath, 1), anError);
				if (anError)
					return false;
			}
			return true;
		}
	}

	bool AppendRollingLog(const std::filesystem::path& thePath,
		std::string_view theRecord, RollingLogConfig theConfig)
	{
		if (thePath.empty() || theRecord.empty() || theConfig.mMaxFileBytes == 0)
			return false;

		std::scoped_lock aLock(gRollingLogMutex);
		std::error_code anError;
		if (!thePath.parent_path().empty())
		{
			std::filesystem::create_directories(thePath.parent_path(), anError);
			if (anError)
				return false;
		}

		std::uintmax_t aCurrentSize = 0;
		if (std::filesystem::exists(thePath, anError))
		{
			if (anError)
				return false;
			aCurrentSize = std::filesystem::file_size(thePath, anError);
			if (anError)
				return false;
		}

		if (aCurrentSize > 0 && (aCurrentSize >= theConfig.mMaxFileBytes ||
			theRecord.size() > theConfig.mMaxFileBytes -
				std::min<std::uintmax_t>(aCurrentSize, theConfig.mMaxFileBytes)))
		{
			if (!Rotate(thePath, theConfig.mBackupCount))
				return false;
		}

		// A single malformed/accidental giant record must not defeat the cap.
		if (theRecord.size() > theConfig.mMaxFileBytes)
			theRecord.remove_prefix(theRecord.size() - theConfig.mMaxFileBytes);

		std::ofstream aFile(thePath, std::ios::app | std::ios::binary);
		if (!aFile)
			return false;
		aFile.write(theRecord.data(), static_cast<std::streamsize>(theRecord.size()));
		return static_cast<bool>(aFile);
	}
}
