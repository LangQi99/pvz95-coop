/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

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
#include <vector>

namespace
{
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

	bool IsLanLog(const std::filesystem::path& thePath)
	{
		std::wstring aName = thePath.filename().wstring();
		std::transform(aName.begin(), aName.end(), aName.begin(), towlower);
		if (aName == L"lan-sync.log")
			return true;
		constexpr std::wstring_view aPrefix = L"lan-sync.log.";
		if (!aName.starts_with(aPrefix) || aName.size() == aPrefix.size())
			return false;
		return std::all_of(aName.begin() + static_cast<std::ptrdiff_t>(aPrefix.size()),
			aName.end(), [](wchar_t theCharacter) { return iswdigit(theCharacter) != 0; });
	}

	void CollectPath(const std::filesystem::path& thePath,
		std::vector<std::filesystem::path>& theLogs, std::set<std::wstring>& theSeen)
	{
		std::error_code anError;
		if (std::filesystem::is_regular_file(thePath, anError))
		{
			if (!anError && IsLanLog(thePath))
			{
				auto aCanonical = std::filesystem::weakly_canonical(thePath, anError);
				if (!anError && theSeen.insert(aCanonical.wstring()).second)
					theLogs.push_back(aCanonical);
			}
			return;
		}
		if (!std::filesystem::is_directory(thePath, anError) || anError)
			return;
		for (const auto& anEntry : std::filesystem::directory_iterator(thePath,
			std::filesystem::directory_options::skip_permission_denied, anError))
		{
			if (anError)
				break;
			if (anEntry.is_regular_file(anError) && !anError && IsLanLog(anEntry.path()))
				CollectPath(anEntry.path(), theLogs, theSeen);
		}
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

	std::filesystem::path MakeManifest(const std::vector<std::filesystem::path>& theLogs)
	{
		std::filesystem::path aTemp = GetEnvironmentPath(L"TEMP");
		if (aTemp.empty())
			aTemp = std::filesystem::temp_directory_path();
		aTemp /= "pvz95-report-info-" + std::to_string(GetCurrentProcessId()) + ".txt";
		std::ofstream aFile(aTemp, std::ios::binary | std::ios::trunc);
		aFile << "PvZ 95 Co-op diagnostic report\r\n"
			<< "Game version: " << PVZP_VERSION << "\r\n"
			<< "Report created: " << Timestamp(false) << "\r\n"
			<< "Platform: Windows " << (sizeof(void*) == 8 ? "x64" : "x86") << "\r\n"
			<< "Log files: " << theLogs.size() << "\r\n\r\n"
			<< "The archive intentionally contains only LAN logs and this manifest.\r\n"
			<< "LAN logs can contain player names and network addresses.\r\n";
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
	std::vector<std::filesystem::path> aSearchPaths;

	int anArgumentCount = 0;
	LPWSTR* anArguments = CommandLineToArgvW(GetCommandLineW(), &anArgumentCount);
	for (int anIndex = 1; anArguments && anIndex < anArgumentCount; ++anIndex)
		aSearchPaths.emplace_back(anArguments[anIndex]);
	if (anArguments)
		LocalFree(anArguments);

	if (aSearchPaths.empty())
	{
		aSearchPaths.push_back(anExeDirectory);
		aSearchPaths.push_back(anExeDirectory / "savedata");
		aSearchPaths.push_back(std::filesystem::current_path());
		aSearchPaths.push_back(std::filesystem::current_path() / "pvz95-data");
		std::filesystem::path anAppData = GetEnvironmentPath(L"APPDATA");
		if (!anAppData.empty())
			aSearchPaths.push_back(anAppData / "io.github.wszqkzqk" / "PvZPortable");
	}

	std::vector<std::filesystem::path> aLogs;
	std::set<std::wstring> aSeen;
	for (const auto& aPath : aSearchPaths)
		CollectPath(aPath, aLogs, aSeen);
	std::sort(aLogs.begin(), aLogs.end());
	if (aLogs.empty())
	{
		ShowError(L"没有找到 lan-sync.log。\n\n请把日志文件或包含日志的存档目录拖到 report.exe 上重试。");
		return 1;
	}

	std::filesystem::path aManifest = MakeManifest(aLogs);
	if (aManifest.empty())
	{
		ShowError(L"无法创建报告信息文件。");
		return 1;
	}

	std::vector<PvzReport::ZipSource> aSources;
	std::map<std::wstring, size_t> aLogSets;
	for (size_t anIndex = 0; anIndex < aLogs.size(); ++anIndex)
	{
		auto [aSet, anInserted] = aLogSets.emplace(aLogs[anIndex].parent_path().wstring(),
			aLogSets.size() + 1);
		aSources.push_back({aLogs[anIndex], "logs/set-" + std::to_string(aSet->second) + "/" +
			aLogs[anIndex].filename().string()});
	}
	// Put the human-readable summary first when the ZIP is opened.
	// Insertion here is harmless because WriteZipArchive preserves entry order.
	aSources.insert(aSources.begin(), {aManifest, "report-info.txt"});

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
		ShowError(L"日志打包失败：\n" + aWideError);
		return 1;
	}

	std::wstring aMessage = L"日志已打包完成，共 " + std::to_wstring(aLogs.size()) +
		L" 个文件：\n\n" + anOutput.wstring() + L"\n\n可以直接把这个 ZIP 发到交流群。";
	MessageBoxW(nullptr, aMessage.c_str(), L"PvZ 95 Co-op Report", MB_OK | MB_ICONINFORMATION);
	std::wstring aParameter = L"/select,\"" + anOutput.wstring() + L"\"";
	ShellExecuteW(nullptr, L"open", L"explorer.exe", aParameter.c_str(), nullptr, SW_SHOWNORMAL);
	return 0;
}
