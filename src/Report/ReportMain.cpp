/*
 * Copyright (C) 2026 LangQi99
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "ProjectVersion.h"
#include "ZipWriter.h"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace
{
	constexpr uintmax_t MAX_DUMP_BYTES = 128ULL * 1024ULL * 1024ULL;
	constexpr uintmax_t MAX_ALL_DUMP_BYTES = 256ULL * 1024ULL * 1024ULL;
	constexpr size_t MAX_DUMP_COUNT = 8;
	constexpr auto MAX_DUMP_AGE = std::chrono::hours(24 * 30);

	struct DumpFile
	{
		std::filesystem::path mPath;
		std::filesystem::file_time_type mModified;
		uintmax_t mSize{};
	};

	std::filesystem::path GetExecutableDirectory()
	{
		std::wstring aPath(32768, L'\0');
		DWORD aLength = GetModuleFileNameW(nullptr, aPath.data(), static_cast<DWORD>(aPath.size()));
		if (aLength == 0 || aLength >= aPath.size())
			return std::filesystem::current_path();
		aPath.resize(aLength);
		return std::filesystem::path(aPath).parent_path();
	}

	std::filesystem::path GetEnvironmentPath(const wchar_t* theName)
	{
		DWORD aLength = GetEnvironmentVariableW(theName, nullptr, 0);
		if (aLength == 0)
			return {};
		std::wstring aValue(aLength, L'\0');
		GetEnvironmentVariableW(theName, aValue.data(), aLength);
		if (!aValue.empty() && aValue.back() == L'\0')
			aValue.pop_back();
		return aValue;
	}

	std::wstring Lower(std::wstring theValue)
	{
		std::transform(theValue.begin(), theValue.end(), theValue.begin(),
			[](wchar_t theCharacter) { return static_cast<wchar_t>(towlower(theCharacter)); });
		return theValue;
	}

	bool IsLanLog(const std::filesystem::path& thePath)
	{
		std::wstring aName = Lower(thePath.filename().wstring());
		if (aName == L"lan-sync.log")
			return true;
		constexpr std::wstring_view aPrefix = L"lan-sync.log.";
		if (!aName.starts_with(aPrefix) || aName.size() == aPrefix.size())
			return false;
		return std::all_of(aName.begin() + static_cast<std::ptrdiff_t>(aPrefix.size()),
			aName.end(), [](wchar_t theCharacter) { return iswdigit(theCharacter) != 0; });
	}

	bool IsDumpExtension(const std::filesystem::path& thePath)
	{
		std::wstring anExtension = Lower(thePath.extension().wstring());
		return anExtension == L".dmp" || anExtension == L".mdmp" || anExtension == L".hdmp";
	}

	bool LooksLikePvzDump(const std::filesystem::path& thePath)
	{
		std::wstring aPath = Lower(thePath.wstring());
		return aPath.find(L"pvz95") != std::wstring::npos ||
			aPath.find(L"pvzportable") != std::wstring::npos ||
			aPath.find(L"plantsvszombies") != std::wstring::npos;
	}

	void AddLog(const std::filesystem::path& thePath,
		std::vector<std::filesystem::path>& theLogs, std::set<std::wstring>& theSeen)
	{
		std::error_code anError;
		auto aCanonical = std::filesystem::weakly_canonical(thePath, anError);
		if (!anError && theSeen.insert(L"log:" + Lower(aCanonical.wstring())).second)
			theLogs.push_back(std::move(aCanonical));
	}

	void AddDump(const std::filesystem::path& thePath, bool theAllowAnyDump,
		std::vector<DumpFile>& theDumps, std::set<std::wstring>& theSeen)
	{
		if (!IsDumpExtension(thePath) || (!theAllowAnyDump && !LooksLikePvzDump(thePath)))
			return;
		std::error_code anError;
		auto aCanonical = std::filesystem::weakly_canonical(thePath, anError);
		if (anError || !theSeen.insert(L"dump:" + Lower(aCanonical.wstring())).second)
			return;
		auto aSize = std::filesystem::file_size(aCanonical, anError);
		if (anError || aSize == 0 || aSize > MAX_DUMP_BYTES)
			return;
		auto aModified = std::filesystem::last_write_time(aCanonical, anError);
		if (anError || std::filesystem::file_time_type::clock::now() - aModified > MAX_DUMP_AGE)
			return;
		theDumps.push_back({std::move(aCanonical), aModified, aSize});
	}

	void CollectPath(const std::filesystem::path& thePath, bool theRecursive, bool theAllowAnyDump,
		std::vector<std::filesystem::path>& theLogs, std::vector<DumpFile>& theDumps,
		std::set<std::wstring>& theSeen)
	{
		std::error_code anError;
		if (std::filesystem::is_regular_file(thePath, anError))
		{
			if (!anError && IsLanLog(thePath))
				AddLog(thePath, theLogs, theSeen);
			else if (!anError)
				AddDump(thePath, theAllowAnyDump, theDumps, theSeen);
			return;
		}
		if (!std::filesystem::is_directory(thePath, anError) || anError)
			return;

		if (!theRecursive)
		{
			for (const auto& anEntry : std::filesystem::directory_iterator(thePath,
				std::filesystem::directory_options::skip_permission_denied, anError))
			{
				if (anError)
					break;
				if (!anEntry.is_regular_file(anError) || anError)
					continue;
				if (IsLanLog(anEntry.path()))
					AddLog(anEntry.path(), theLogs, theSeen);
				else
					AddDump(anEntry.path(), theAllowAnyDump, theDumps, theSeen);
			}
			return;
		}

		for (std::filesystem::recursive_directory_iterator anIterator(thePath,
			std::filesystem::directory_options::skip_permission_denied, anError), anEnd;
			anIterator != anEnd; anIterator.increment(anError))
		{
			if (anError)
			{
				anError.clear();
				continue;
			}
			if (!anIterator->is_regular_file(anError) || anError)
				continue;
			if (IsLanLog(anIterator->path()))
				AddLog(anIterator->path(), theLogs, theSeen);
			else
				AddDump(anIterator->path(), theAllowAnyDump, theDumps, theSeen);
		}
	}

	void KeepNewestDumps(std::vector<DumpFile>& theDumps)
	{
		std::sort(theDumps.begin(), theDumps.end(), [](const DumpFile& theLeft, const DumpFile& theRight)
			{ return theLeft.mModified > theRight.mModified; });
		uintmax_t aTotal = 0;
		size_t aKeepCount = 0;
		for (; aKeepCount < theDumps.size() && aKeepCount < MAX_DUMP_COUNT; ++aKeepCount)
		{
			if (aTotal + theDumps[aKeepCount].mSize > MAX_ALL_DUMP_BYTES)
				break;
			aTotal += theDumps[aKeepCount].mSize;
		}
		theDumps.resize(aKeepCount);
	}

	std::filesystem::path GetManagedDumpDirectory()
	{
		auto aLocalAppData = GetEnvironmentPath(L"LOCALAPPDATA");
		if (aLocalAppData.empty())
			aLocalAppData = GetEnvironmentPath(L"APPDATA");
		return aLocalAppData.empty() ? std::filesystem::path{} :
			aLocalAppData / "io.github.wszqkzqk" / "PvZPortable" / "crash-dumps";
	}

	bool EnableFutureWerDumps(const std::filesystem::path& theDumpDirectory)
	{
		if (theDumpDirectory.empty())
			return false;
		std::error_code anError;
		std::filesystem::create_directories(theDumpDirectory, anError);
		if (anError)
			return false;

		HKEY aKey = nullptr;
		constexpr wchar_t aKeyName[] =
			L"Software\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\pvz95-coop.exe";
		if (RegCreateKeyExW(HKEY_CURRENT_USER, aKeyName, 0, nullptr, 0, KEY_SET_VALUE,
			nullptr, &aKey, nullptr) != ERROR_SUCCESS)
			return false;
		std::wstring aFolder = theDumpDirectory.wstring();
		DWORD aDumpType = 1;
		DWORD aDumpCount = 5;
		bool aSuccess = RegSetValueExW(aKey, L"DumpFolder", 0, REG_SZ,
			reinterpret_cast<const BYTE*>(aFolder.c_str()),
			static_cast<DWORD>((aFolder.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS &&
			RegSetValueExW(aKey, L"DumpType", 0, REG_DWORD,
				reinterpret_cast<const BYTE*>(&aDumpType), sizeof(aDumpType)) == ERROR_SUCCESS &&
			RegSetValueExW(aKey, L"DumpCount", 0, REG_DWORD,
				reinterpret_cast<const BYTE*>(&aDumpCount), sizeof(aDumpCount)) == ERROR_SUCCESS;
		RegCloseKey(aKey);
		return aSuccess;
	}

	std::string Timestamp(bool theFileName)
	{
		auto aNow = std::chrono::system_clock::now();
		std::time_t aTime = std::chrono::system_clock::to_time_t(aNow);
		std::tm aLocal{};
		localtime_s(&aLocal, &aTime);
		std::ostringstream aStream;
		aStream << std::put_time(&aLocal, theFileName ? "%Y%m%d-%H%M%S" : "%Y-%m-%d %H:%M:%S %z");
		return aStream.str();
	}

	std::filesystem::path UniqueOutputPath(const std::filesystem::path& theDirectory)
	{
		std::filesystem::path aBase = theDirectory / ("pvz95-report-" + Timestamp(true) + ".zip");
		std::error_code anError;
		if (!std::filesystem::exists(aBase, anError))
			return aBase;
		for (int anIndex = 2; anIndex < 1000; ++anIndex)
		{
			auto aCandidate = aBase.parent_path() /
				(aBase.stem().string() + "-" + std::to_string(anIndex) + ".zip");
			if (!std::filesystem::exists(aCandidate, anError))
				return aCandidate;
		}
		return aBase;
	}

	std::filesystem::path MakeManifest(const std::vector<std::filesystem::path>& theLogs,
		const std::vector<DumpFile>& theDumps, bool theFutureDumpsEnabled)
	{
		std::filesystem::path aTemp = GetEnvironmentPath(L"TEMP");
		if (aTemp.empty())
			aTemp = std::filesystem::temp_directory_path();
		aTemp /= "pvz95-report-info-" + std::to_string(GetCurrentProcessId()) + ".txt";
		std::ofstream aFile(aTemp, std::ios::binary | std::ios::trunc);
		aFile << "PvZ 95 Co-op diagnostic report\r\n"
			<< "Game/report version: " << PVZP_VERSION << "\r\n"
			<< "Report created: " << Timestamp(false) << "\r\n"
			<< "Platform: Windows " << (sizeof(void*) == 8 ? "x64" : "x86") << "\r\n"
			<< "Log files: " << theLogs.size() << "\r\n"
			<< "Crash dumps: " << theDumps.size() << "\r\n"
			<< "Future Windows crash dumps enabled: " << (theFutureDumpsEnabled ? "yes" : "no") << "\r\n";
		if (!theDumps.empty())
		{
			aFile << "\r\nIncluded crash dumps:\r\n";
			for (const auto& aDump : theDumps)
				aFile << "- " << aDump.mPath.filename().string() << " (" << aDump.mSize << " bytes)\r\n";
		}
		aFile << "\r\nThe archive contains PvZ 95 Co-op LAN logs and relevant recent crash dumps only.\r\n"
			<< "LAN logs can contain player names and network addresses. Crash dumps may contain\r\n"
			<< "fragments of process memory; share this archive only with trusted maintainers.\r\n";
		return aFile ? aTemp : std::filesystem::path{};
	}

	void ShowError(const std::wstring& theMessage)
	{
		MessageBoxW(nullptr, theMessage.c_str(), L"PvZ 95 Co-op Report", MB_OK | MB_ICONERROR);
	}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	std::filesystem::path anExeDirectory = GetExecutableDirectory();
	// path, recursive, accept any dump (the latter is only true for user-supplied paths).
	std::vector<std::tuple<std::filesystem::path, bool, bool>> aSearchPaths;

	int anArgumentCount = 0;
	LPWSTR* anArguments = CommandLineToArgvW(GetCommandLineW(), &anArgumentCount);
	for (int anIndex = 1; anArguments && anIndex < anArgumentCount; ++anIndex)
		aSearchPaths.emplace_back(std::filesystem::path(anArguments[anIndex]), true, true);
	if (anArguments)
		LocalFree(anArguments);

	std::filesystem::path aManagedDumpDirectory = GetManagedDumpDirectory();
	bool aFutureDumpsEnabled = EnableFutureWerDumps(aManagedDumpDirectory);
	if (aSearchPaths.empty())
	{
		aSearchPaths.emplace_back(anExeDirectory, false, false);
		aSearchPaths.emplace_back(anExeDirectory / "savedata", false, false);
		aSearchPaths.emplace_back(std::filesystem::current_path(), false, false);
		aSearchPaths.emplace_back(std::filesystem::current_path() / "pvz95-data", false, false);
		std::filesystem::path anAppData = GetEnvironmentPath(L"APPDATA");
		if (!anAppData.empty())
			aSearchPaths.emplace_back(anAppData / "io.github.wszqkzqk" / "PvZPortable", false, false);
		if (!aManagedDumpDirectory.empty())
			aSearchPaths.emplace_back(aManagedDumpDirectory, false, false);
		std::filesystem::path aLocalAppData = GetEnvironmentPath(L"LOCALAPPDATA");
		if (!aLocalAppData.empty())
		{
			aSearchPaths.emplace_back(aLocalAppData / "CrashDumps", false, false);
			aSearchPaths.emplace_back(aLocalAppData / "Microsoft" / "Windows" / "WER" / "ReportArchive", true, false);
			aSearchPaths.emplace_back(aLocalAppData / "Microsoft" / "Windows" / "WER" / "ReportQueue", true, false);
		}
	}

	std::vector<std::filesystem::path> aLogs;
	std::vector<DumpFile> aDumps;
	std::set<std::wstring> aSeen;
	for (const auto& [aPath, aRecursive, anExplicit] : aSearchPaths)
		CollectPath(aPath, aRecursive, anExplicit, aLogs, aDumps, aSeen);
	std::sort(aLogs.begin(), aLogs.end());
	KeepNewestDumps(aDumps);
	if (aLogs.empty() && aDumps.empty())
	{
		ShowError(L"没有找到联机日志或崩溃转储。\n\n已启用后续崩溃转储；请复现一次闪退，"
			L"然后再次运行 report.exe。也可以把日志或 .dmp 文件拖到它上面。");
		return 1;
	}

	std::filesystem::path aManifest = MakeManifest(aLogs, aDumps, aFutureDumpsEnabled);
	if (aManifest.empty())
	{
		ShowError(L"无法创建报告信息文件。");
		return 1;
	}

	std::vector<PvzReport::ZipSource> aSources{{aManifest, "report-info.txt"}};
	std::map<std::wstring, size_t> aLogSets;
	for (const auto& aLog : aLogs)
	{
		auto [aSet, anInserted] = aLogSets.emplace(aLog.parent_path().wstring(), aLogSets.size() + 1);
		aSources.push_back({aLog, "logs/set-" + std::to_string(aSet->second) + "/" + aLog.filename().string()});
	}
	for (size_t anIndex = 0; anIndex < aDumps.size(); ++anIndex)
		aSources.push_back({aDumps[anIndex].mPath, "crash-dumps/" + std::to_string(anIndex + 1) + "-" +
			aDumps[anIndex].mPath.filename().string()});

	std::filesystem::path anOutput = UniqueOutputPath(anExeDirectory);
	std::string anError;
	bool aSuccess = PvzReport::WriteZipArchive(anOutput, aSources, anError);
	if (!aSuccess)
	{
		std::filesystem::path aDesktop = GetEnvironmentPath(L"USERPROFILE") / "Desktop";
		std::error_code aDesktopError;
		if (std::filesystem::is_directory(aDesktop, aDesktopError))
		{
			anOutput = UniqueOutputPath(aDesktop);
			aSuccess = PvzReport::WriteZipArchive(anOutput, aSources, anError);
		}
	}
	std::error_code aRemoveError;
	std::filesystem::remove(aManifest, aRemoveError);
	if (!aSuccess)
	{
		std::wstring aWideError(anError.begin(), anError.end());
		ShowError(L"诊断报告打包失败：\n" + aWideError);
		return 1;
	}

	std::wstring aMessage = L"诊断报告已打包：" + std::to_wstring(aLogs.size()) + L" 个日志，" +
		std::to_wstring(aDumps.size()) + L" 个崩溃转储。\n\n" + anOutput.wstring();
	if (aDumps.empty())
		aMessage += L"\n\n本次没有找到旧转储；已经为下一次闪退启用自动转储。请复现后再运行一次。";
	else
		aMessage += L"\n\n转储可能包含进程内存片段，请只发给可信的维护者。";
	MessageBoxW(nullptr, aMessage.c_str(), L"PvZ 95 Co-op Report", MB_OK | MB_ICONINFORMATION);
	std::wstring aParameter = L"/select,\"" + anOutput.wstring() + L"\"";
	ShellExecuteW(nullptr, L"open", L"explorer.exe", aParameter.c_str(), nullptr, SW_SHOWNORMAL);
	return 0;
}
