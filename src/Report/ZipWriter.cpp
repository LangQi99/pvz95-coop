/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ZipWriter.h"

#include <zlib.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>
#include <system_error>

namespace PvzReport
{
	namespace
	{
		struct CentralEntry
		{
			std::string mName;
			uint32_t mCrc{};
			uint32_t mCompressedSize{};
			uint32_t mOriginalSize{};
			uint32_t mLocalOffset{};
		};

		void WriteU16(std::ostream& theStream, uint16_t theValue)
		{
			std::array<char, 2> aBytes{
				static_cast<char>(theValue), static_cast<char>(theValue >> 8)};
			theStream.write(aBytes.data(), aBytes.size());
		}

		void WriteU32(std::ostream& theStream, uint32_t theValue)
		{
			std::array<char, 4> aBytes{
				static_cast<char>(theValue), static_cast<char>(theValue >> 8),
				static_cast<char>(theValue >> 16), static_cast<char>(theValue >> 24)};
			theStream.write(aBytes.data(), aBytes.size());
		}

		bool ReadFile(const std::filesystem::path& thePath, std::vector<uint8_t>& theBytes)
		{
			std::error_code anError;
			auto aSize = std::filesystem::file_size(thePath, anError);
			if (anError || aSize > std::numeric_limits<uint32_t>::max())
				return false;
			theBytes.resize(static_cast<size_t>(aSize));
			std::ifstream aFile(thePath, std::ios::binary);
			if (!aFile)
				return false;
			if (!theBytes.empty())
				aFile.read(reinterpret_cast<char*>(theBytes.data()),
					static_cast<std::streamsize>(theBytes.size()));
			return static_cast<bool>(aFile) || theBytes.empty();
		}

		bool Deflate(std::span<const uint8_t> theInput, std::vector<uint8_t>& theOutput)
		{
			z_stream aStream{};
			if (deflateInit2(&aStream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
				8, Z_DEFAULT_STRATEGY) != Z_OK)
				return false;

			theOutput.resize(static_cast<size_t>(deflateBound(&aStream,
				static_cast<uLong>(theInput.size()))));
			aStream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(theInput.data()));
			aStream.avail_in = static_cast<uInt>(theInput.size());
			aStream.next_out = reinterpret_cast<Bytef*>(theOutput.data());
			aStream.avail_out = static_cast<uInt>(theOutput.size());
			int aResult = deflate(&aStream, Z_FINISH);
			if (aResult == Z_STREAM_END)
				theOutput.resize(aStream.total_out);
			deflateEnd(&aStream);
			return aResult == Z_STREAM_END;
		}

		bool IsSafeArchiveName(const std::string& theName)
		{
			return !theName.empty() && theName.size() <= std::numeric_limits<uint16_t>::max() &&
				theName.front() != '/' && theName.find("..") == std::string::npos &&
				theName.find('\\') == std::string::npos;
		}
	}

	bool WriteZipArchive(const std::filesystem::path& theOutputPath,
		const std::vector<ZipSource>& theSources, std::string& theError)
	{
		theError.clear();
		if (theSources.empty() || theSources.size() > std::numeric_limits<uint16_t>::max())
		{
			theError = "No files to archive.";
			return false;
		}

		std::filesystem::path aTemporaryPath = theOutputPath;
		aTemporaryPath += ".tmp";
		std::ofstream anOutput(aTemporaryPath, std::ios::binary | std::ios::trunc);
		if (!anOutput)
		{
			theError = "Could not create the ZIP file.";
			return false;
		}

		std::vector<CentralEntry> aCentralEntries;
		for (const ZipSource& aSource : theSources)
		{
			if (!IsSafeArchiveName(aSource.mArchiveName))
			{
				theError = "Unsafe ZIP entry name: " + aSource.mArchiveName;
				break;
			}
			std::vector<uint8_t> anOriginal;
			std::vector<uint8_t> aCompressed;
			if (!ReadFile(aSource.mSourcePath, anOriginal) || !Deflate(anOriginal, aCompressed) ||
				aCompressed.size() > std::numeric_limits<uint32_t>::max())
			{
				theError = "Could not read or compress: " + aSource.mSourcePath.string();
				break;
			}
			auto anOffset = anOutput.tellp();
			if (anOffset < 0 || static_cast<uint64_t>(static_cast<std::streamoff>(anOffset)) >
				std::numeric_limits<uint32_t>::max())
			{
				theError = "The ZIP archive is too large.";
				break;
			}

			uLong aCrc = crc32(0L, Z_NULL, 0);
			aCrc = crc32(aCrc, reinterpret_cast<const Bytef*>(anOriginal.data()),
				static_cast<uInt>(anOriginal.size()));
			CentralEntry anEntry{aSource.mArchiveName, static_cast<uint32_t>(aCrc),
				static_cast<uint32_t>(aCompressed.size()), static_cast<uint32_t>(anOriginal.size()),
				static_cast<uint32_t>(static_cast<std::streamoff>(anOffset))};

			WriteU32(anOutput, 0x04034B50);
			WriteU16(anOutput, 20);
			WriteU16(anOutput, 0x0800); // UTF-8 entry names.
			WriteU16(anOutput, 8);      // Deflate.
			WriteU16(anOutput, 0); WriteU16(anOutput, 0);
			WriteU32(anOutput, anEntry.mCrc);
			WriteU32(anOutput, anEntry.mCompressedSize);
			WriteU32(anOutput, anEntry.mOriginalSize);
			WriteU16(anOutput, static_cast<uint16_t>(anEntry.mName.size()));
			WriteU16(anOutput, 0);
			anOutput.write(anEntry.mName.data(), static_cast<std::streamsize>(anEntry.mName.size()));
			anOutput.write(reinterpret_cast<const char*>(aCompressed.data()),
				static_cast<std::streamsize>(aCompressed.size()));
			if (!anOutput)
			{
				theError = "Could not write the ZIP file.";
				break;
			}
			aCentralEntries.push_back(std::move(anEntry));
		}

		if (theError.empty())
		{
			auto aCentralOffsetValue = anOutput.tellp();
			if (aCentralOffsetValue < 0 || static_cast<uint64_t>(
				static_cast<std::streamoff>(aCentralOffsetValue)) > std::numeric_limits<uint32_t>::max())
			{
				theError = "The ZIP archive is too large.";
			}
			uint32_t aCentralOffset = static_cast<uint32_t>(
				static_cast<std::streamoff>(aCentralOffsetValue));
			for (const CentralEntry& anEntry : aCentralEntries)
			{
				WriteU32(anOutput, 0x02014B50);
				WriteU16(anOutput, 20); WriteU16(anOutput, 20);
				WriteU16(anOutput, 0x0800); WriteU16(anOutput, 8);
				WriteU16(anOutput, 0); WriteU16(anOutput, 0);
				WriteU32(anOutput, anEntry.mCrc);
				WriteU32(anOutput, anEntry.mCompressedSize);
				WriteU32(anOutput, anEntry.mOriginalSize);
				WriteU16(anOutput, static_cast<uint16_t>(anEntry.mName.size()));
				WriteU16(anOutput, 0); WriteU16(anOutput, 0);
				WriteU16(anOutput, 0); WriteU16(anOutput, 0); WriteU32(anOutput, 0);
				WriteU32(anOutput, anEntry.mLocalOffset);
				anOutput.write(anEntry.mName.data(), static_cast<std::streamsize>(anEntry.mName.size()));
			}
			auto aCentralEndValue = anOutput.tellp();
			if (aCentralEndValue < 0 || static_cast<uint64_t>(
				static_cast<std::streamoff>(aCentralEndValue)) >
				std::numeric_limits<uint32_t>::max())
				theError = "The ZIP archive is too large.";
			uint32_t aCentralSize = static_cast<uint32_t>(
				static_cast<std::streamoff>(aCentralEndValue)) - aCentralOffset;
			WriteU32(anOutput, 0x06054B50);
			WriteU16(anOutput, 0); WriteU16(anOutput, 0);
			WriteU16(anOutput, static_cast<uint16_t>(aCentralEntries.size()));
			WriteU16(anOutput, static_cast<uint16_t>(aCentralEntries.size()));
			WriteU32(anOutput, aCentralSize); WriteU32(anOutput, aCentralOffset); WriteU16(anOutput, 0);
			if (!anOutput)
				theError = "Could not finish the ZIP file.";
		}

		anOutput.close();
		std::error_code anError;
		if (!theError.empty())
		{
			std::filesystem::remove(aTemporaryPath, anError);
			return false;
		}
		std::filesystem::rename(aTemporaryPath, theOutputPath, anError);
		if (anError)
		{
			std::filesystem::remove(aTemporaryPath, anError);
			theError = "Could not move the completed ZIP into place.";
			return false;
		}
		return true;
	}
}
