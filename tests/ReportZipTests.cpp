/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ZipWriter.h"

#include <zlib.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
	[[noreturn]] void Fail(const std::string& theMessage)
	{
		std::cerr << theMessage << '\n';
		std::exit(1);
	}

	uint16_t ReadU16(const std::vector<uint8_t>& theBytes, size_t& theOffset)
	{
		if (theOffset + 2 > theBytes.size())
			Fail("truncated ZIP field");
		uint16_t aValue = static_cast<uint16_t>(theBytes[theOffset]) |
			(static_cast<uint16_t>(theBytes[theOffset + 1]) << 8);
		theOffset += 2;
		return aValue;
	}

	uint32_t ReadU32(const std::vector<uint8_t>& theBytes, size_t& theOffset)
	{
		if (theOffset + 4 > theBytes.size())
			Fail("truncated ZIP field");
		uint32_t aValue = 0;
		for (int aShift = 0; aShift < 32; aShift += 8)
			aValue |= static_cast<uint32_t>(theBytes[theOffset++]) << aShift;
		return aValue;
	}

	std::string ReadEntry(const std::vector<uint8_t>& theArchive, size_t& theOffset,
		const std::string& theExpectedName)
	{
		if (ReadU32(theArchive, theOffset) != 0x04034B50)
			Fail("missing local ZIP header");
		ReadU16(theArchive, theOffset); // version
		if ((ReadU16(theArchive, theOffset) & 0x0800) == 0)
			Fail("ZIP entry is not marked UTF-8");
		if (ReadU16(theArchive, theOffset) != 8)
			Fail("ZIP entry is not deflated");
		ReadU16(theArchive, theOffset); ReadU16(theArchive, theOffset); // date/time
		ReadU32(theArchive, theOffset); // CRC
		uint32_t aCompressedSize = ReadU32(theArchive, theOffset);
		uint32_t anOriginalSize = ReadU32(theArchive, theOffset);
		uint16_t aNameSize = ReadU16(theArchive, theOffset);
		uint16_t anExtraSize = ReadU16(theArchive, theOffset);
		if (theOffset + aNameSize + anExtraSize + aCompressedSize > theArchive.size())
			Fail("truncated ZIP entry");
		std::string aName(reinterpret_cast<const char*>(theArchive.data() + theOffset), aNameSize);
		theOffset += aNameSize + anExtraSize;
		if (aName != theExpectedName)
			Fail("unexpected ZIP entry name");

		std::string anOutput(anOriginalSize, '\0');
		z_stream aStream{};
		if (inflateInit2(&aStream, -MAX_WBITS) != Z_OK)
			Fail("could not initialize inflater");
		aStream.next_in = const_cast<Bytef*>(
			reinterpret_cast<const Bytef*>(theArchive.data() + theOffset));
		aStream.avail_in = aCompressedSize;
		aStream.next_out = reinterpret_cast<Bytef*>(anOutput.data());
		aStream.avail_out = anOriginalSize;
		int aResult = inflate(&aStream, Z_FINISH);
		inflateEnd(&aStream);
		if (aResult != Z_STREAM_END || aStream.total_out != anOriginalSize)
			Fail("ZIP entry did not inflate");
		theOffset += aCompressedSize;
		return anOutput;
	}
}

int main()
{
	auto aUnique = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::filesystem::path aDirectory = std::filesystem::temp_directory_path() /
		("pvz95-report-test-" + std::to_string(aUnique));
	std::filesystem::create_directories(aDirectory);
	std::filesystem::path aFirst = aDirectory / "lan-sync.log";
	std::filesystem::path aSecond = aDirectory / "lan-sync.log.1";
	{
		std::ofstream(aFirst, std::ios::binary) << "current log\nline two\n";
		std::ofstream(aSecond, std::ios::binary) << std::string(10000, 'A');
	}

	std::filesystem::path anArchivePath = aDirectory / "report.zip";
	std::string anError;
	if (!PvzReport::WriteZipArchive(anArchivePath,
		{{aFirst, "logs/lan-sync.log"}, {aSecond, "logs/lan-sync.log.1"}}, anError))
		Fail("ZIP creation failed: " + anError);

	std::ifstream anArchiveFile(anArchivePath, std::ios::binary);
	std::vector<uint8_t> anArchive((std::istreambuf_iterator<char>(anArchiveFile)), {});
	size_t anOffset = 0;
	if (ReadEntry(anArchive, anOffset, "logs/lan-sync.log") != "current log\nline two\n")
		Fail("first ZIP entry contents differ");
	if (ReadEntry(anArchive, anOffset, "logs/lan-sync.log.1") != std::string(10000, 'A'))
		Fail("second ZIP entry contents differ");
	if (ReadU32(anArchive, anOffset) != 0x02014B50)
		Fail("missing central ZIP directory");

	std::error_code anErrorCode;
	std::filesystem::remove_all(aDirectory, anErrorCode);
	std::cout << "PvZ 95 report ZIP tests passed\n";
	return 0;
}
