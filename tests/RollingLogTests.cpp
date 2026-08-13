/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/RollingLog.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
	[[noreturn]] void Fail(const std::string& theMessage)
	{
		std::cerr << theMessage << '\n';
		std::exit(1);
	}

	std::string Read(const std::filesystem::path& thePath)
	{
		std::ifstream aFile(thePath, std::ios::binary);
		return {std::istreambuf_iterator<char>(aFile), std::istreambuf_iterator<char>()};
	}
}

int main()
{
	using namespace PvzMultiplayer;
	const auto aUnique = std::to_string(
		std::chrono::steady_clock::now().time_since_epoch().count());
	const auto aDirectory = std::filesystem::temp_directory_path() /
		("pvz95-rolling-log-" + aUnique);
	const auto aLogPath = aDirectory / "lan-sync.log";
	std::filesystem::create_directories(aDirectory);

	RollingLogConfig aConfig{24, 2};
	if (!AppendRollingLog(aLogPath, "first-record\n", aConfig) ||
		!AppendRollingLog(aLogPath, "second-record\n", aConfig))
		Fail("could not append initial rolling-log records");
	if (Read(aLogPath) != "second-record\n" || Read(aLogPath.string() + ".1") != "first-record\n")
		Fail("rolling log did not rotate the active file");

	if (!AppendRollingLog(aLogPath, "third-record\n", aConfig) ||
		!AppendRollingLog(aLogPath, "fourth-record\n", aConfig))
		Fail("could not append backup rotation records");
	if (Read(aLogPath) != "fourth-record\n" || Read(aLogPath.string() + ".1") != "third-record\n" ||
		Read(aLogPath.string() + ".2") != "second-record\n")
		Fail("rolling log did not keep the newest bounded backup set");
	if (std::filesystem::exists(aLogPath.string() + ".3"))
		Fail("rolling log exceeded the configured backup count");

	if (!AppendRollingLog(aLogPath, std::string(40, 'x'), aConfig) ||
		std::filesystem::file_size(aLogPath) != aConfig.mMaxFileBytes)
		Fail("rolling log did not bound an oversized record");

	std::error_code anError;
	std::filesystem::remove_all(aDirectory, anError);
	std::cout << "PvZ 95 rolling log tests passed\n";
	return 0;
}
