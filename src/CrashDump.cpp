/*
 * Copyright (C) 2026 LangQi99
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "CrashDump.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

#include <cwchar>
#include <filesystem>
#include <iterator>
#include <string>

namespace
{
	std::wstring gDumpDirectory;

	LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* theException)
	{
		if (gDumpDirectory.empty())
			return EXCEPTION_CONTINUE_SEARCH;
		SYSTEMTIME aTime{};
		GetLocalTime(&aTime);
		wchar_t aFileName[160]{};
		swprintf_s(aFileName, L"\\pvz95-coop-%04u%02u%02u-%02u%02u%02u-pid%lu.dmp",
			aTime.wYear, aTime.wMonth, aTime.wDay, aTime.wHour, aTime.wMinute, aTime.wSecond,
			static_cast<unsigned long>(GetCurrentProcessId()));
		std::wstring aPath = gDumpDirectory + aFileName;
		HANDLE aFile = CreateFileW(aPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (aFile != INVALID_HANDLE_VALUE)
		{
			MINIDUMP_EXCEPTION_INFORMATION anExceptionInfo{};
			anExceptionInfo.ThreadId = GetCurrentThreadId();
			anExceptionInfo.ExceptionPointers = theException;
			anExceptionInfo.ClientPointers = FALSE;
			MINIDUMP_TYPE aType = static_cast<MINIDUMP_TYPE>(
				MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), aFile, aType,
				&anExceptionInfo, nullptr, nullptr);
			CloseHandle(aFile);
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

void InstallCrashDumpHandler()
{
	wchar_t aLocalAppData[32768]{};
	DWORD aLength = GetEnvironmentVariableW(L"LOCALAPPDATA", aLocalAppData,
		static_cast<DWORD>(std::size(aLocalAppData)));
	if (aLength == 0 || aLength >= std::size(aLocalAppData))
		return;
	std::filesystem::path aDirectory = std::filesystem::path(aLocalAppData) /
		"io.github.wszqkzqk" / "PvZPortable" / "crash-dumps";
	std::error_code anError;
	std::filesystem::create_directories(aDirectory, anError);
	if (anError)
		return;
	gDumpDirectory = aDirectory.wstring();
	SetUnhandledExceptionFilter(WriteCrashDump);
}

#else

void InstallCrashDumpHandler()
{
}

#endif
