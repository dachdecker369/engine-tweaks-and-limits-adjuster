// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the one log file, TweaksV.log (plan 5.7 and 4.7).

#include "TweaksVLog.h"

#include <windows.h>

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "BuildInfo.h"
#include "GameVersion.h"
#include "TweaksVIni.h"
#include "TweaksVSummary.h"

extern "C" IMAGE_DOS_HEADER __ImageBase; // this module (TweaksV.asi)

namespace TweaksV
{
	namespace
	{
		constexpr size_t kMaxLine = 2048;		// plan 5.7: 2 KB lines
		constexpr size_t kMaxPending = 1024;	// lines kept until the log is started

		SRWLOCK g_lock = SRWLOCK_INIT;
		HANDLE g_file = INVALID_HANDLE_VALUE;
		std::wstring g_path;					// the log file in use
		bool g_cleared = false;					// ClearLogFile() ran
		bool g_started = false;					// the header is written
		bool g_toFile = true;					// LOG_TO_FILE
		bool g_console = false;					// OpenConsole() succeeded
		std::vector<std::string> g_pending;

		std::string FormatLine(std::string_view tag, std::string_view text)
		{
			SYSTEMTIME now{};
			GetLocalTime(&now);
			char prefix[32];
			snprintf(prefix, sizeof(prefix), "%02u:%02u:%02u.%03u [", static_cast<unsigned>(now.wHour),
				static_cast<unsigned>(now.wMinute), static_cast<unsigned>(now.wSecond),
				static_cast<unsigned>(now.wMilliseconds));

			std::string line = prefix;
			line.append(tag);
			line += "] ";
			line.append(text);
			if (line.size() > kMaxLine - 2)
			{
				line.resize(kMaxLine - 5);
				line += "...";
			}
			return line;
		}

		// Caller holds g_lock.
		void WriteLineLocked(const std::string& line)
		{
			if (g_toFile && g_file != INVALID_HANDLE_VALUE)
			{
				const std::string data = line + "\r\n";
				DWORD written = 0;
				WriteFile(g_file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
			}
			if (g_console)
			{
				fputs(line.c_str(), stdout);
				fputc('\n', stdout);
				fflush(stdout);
			}
		}

		HANDLE CreateLogFile(const std::wstring& path)
		{
			return CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		}

		std::wstring ModulePath(HMODULE module)
		{
			std::wstring path(MAX_PATH, L'\0');
			for (;;)
			{
				const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
				if (length == 0)
					return L"?";
				if (length < path.size())
				{
					path.resize(length);
					return path;
				}
				path.resize(path.size() * 2);
			}
		}

		// "1.0.1158.13" or "unknown"
		std::string GameFileVersion()
		{
			FileVersion version{};
			if (!ReadCurrentProcessFileVersion(version))
				return "unknown";
			return std::to_string(version.Major) + "." + std::to_string(version.Minor) + "." +
				std::to_string(version.Build) + "." + std::to_string(version.Revision);
		}

		// "0x6870ABCD (2026-07-10 12:34:56 UTC)" from the exe's PE header.
		std::string GamePeTimestamp()
		{
			const auto base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
			const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE)
				return "unknown";
			const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE)
				return "unknown";

			const DWORD stamp = nt->FileHeader.TimeDateStamp;
			char text[64];
			const time_t seconds = static_cast<time_t>(stamp);
			tm utc{};
			if (gmtime_s(&utc, &seconds) == 0)
			{
				char date[32];
				strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &utc);
				snprintf(text, sizeof(text), "0x%08lX (%s UTC)", static_cast<unsigned long>(stamp), date);
			}
			else
			{
				snprintf(text, sizeof(text), "0x%08lX", static_cast<unsigned long>(stamp));
			}
			return text;
		}

		std::string FolderState(const std::wstring& folder)
		{
			const DWORD attributes = GetFileAttributesW(folder.c_str());
			const bool exists = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
			return ToUtf8(folder) + (exists ? " (present)" : " (not present)");
		}

		const char* OnOff(bool value)
		{
			return value ? "on" : "off";
		}

		std::string Switch(const char* section, const char* key, bool defaultValue)
		{
			return std::string(key) + "=" + (GetIniBool(section, key, defaultValue) ? "1" : "0");
		}
	}

	void WriteLog(std::string_view tag, std::string_view text)
	{
		const std::string line = FormatLine(tag, text);

		AcquireSRWLockExclusive(&g_lock);
		if (!g_started)
		{
			if (g_pending.size() < kMaxPending)
				g_pending.push_back(line);
		}
		else
		{
			WriteLineLocked(line);
		}
		ReleaseSRWLockExclusive(&g_lock);
	}

	void ClearLogFile()
	{
		const bool toFile = GetIniBool("TWEAKSV", "LOG_TO_FILE", true);
		const std::wstring primary = GetGameFolder() + L"\\TweaksV.log";

		AcquireSRWLockExclusive(&g_lock);
		g_toFile = toFile;
		if (g_file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(g_file);
			g_file = INVALID_HANDLE_VALUE;
		}
		g_path.clear();

		DWORD primaryError = ERROR_SUCCESS;
		std::wstring fallback;
		if (g_toFile)
		{
			g_file = CreateLogFile(primary);
			if (g_file != INVALID_HANDLE_VALUE)
			{
				g_path = primary;
			}
			else
			{
				// Game folder read-only (plan 4.7): use %LOCALAPPDATA%\TweaksV instead.
				primaryError = GetLastError();
				const std::wstring folder = GetFallbackFolder();
				if (!folder.empty())
				{
					CreateDirectoryW(folder.c_str(), nullptr);
					fallback = folder + L"\\TweaksV.log";
					g_file = CreateLogFile(fallback);
					if (g_file != INVALID_HANDLE_VALUE)
						g_path = fallback;
				}
			}
		}
		g_cleared = true;
		const bool usingFallback = g_toFile && !fallback.empty() && g_path == fallback;
		const bool noFile = g_toFile && g_file == INVALID_HANDLE_VALUE;
		ReleaseSRWLockExclusive(&g_lock);

		// Written after releasing the lock: WriteLog and ReportFailure take it. A
		// read-only game folder is normal (e.g. under C:\Program Files), so the fallback
		// is only logged; failing in both places is a problem.
		if (usingFallback)
			WriteLog("TweaksV", "The game folder is read-only (" + DescribeError(primaryError) +
				"), so TweaksV.log is written to " + ToUtf8(fallback) + ".");
		else if (noFile)
			ReportFailure("TweaksV", "TweaksV.log could not be created in the game folder or in %LOCALAPPDATA%\\TweaksV (" +
				DescribeError(primaryError) + "). Nothing is logged to a file this session.");
	}

	void WriteLogHeader()
	{
		// Gather everything first: the INI readers may themselves log.
		const std::wstring game = GetGameFolder();
		std::vector<std::string> header;
		header.push_back(std::string("TweaksV ") + TWEAKSV_VERSION + " (git " +
			(TWEAKSV_GIT_HASH[0] ? TWEAKSV_GIT_HASH : "unknown") + ")");
		header.push_back("TweaksV.asi: " + ToUtf8(ModulePath(reinterpret_cast<HMODULE>(&__ImageBase))));
		header.push_back("Game: " + ToUtf8(ModulePath(nullptr)) + ", file version " + GameFileVersion() +
			", PE timestamp " + GamePeTimestamp());
		header.push_back("TweaksV.ini: " + DescribeIniLocation());
		header.push_back("TweaksV.log: " + ToUtf8(GetLogPathW()));
		header.push_back("mods folder: " + FolderState(game + L"\\mods"));
		header.push_back("newmods folder: " + FolderState(game + L"\\newmods"));
		header.push_back(std::string("Features: HeapAdjusterEnhanced ") + OnOff(IsFeatureOn("HEAP_SETTINGS")) +
			", ModkitLimitAdjusterEnhanced " + OnOff(IsFeatureOn("MODKIT_SETTINGS")) +
			", PackfileLimitAdjusterEnhanced " + OnOff(IsFeatureOn("PACKFILE_SETTINGS")) +
			", RageOpenV " + OnOff(IsFeatureOn("RAGEOPENV_SETTINGS")) +
			", StraightToStoryMode " + OnOff(IsFeatureOn("STRAIGHTTOSTORYMODE_SETTINGS")) +
			", DirectStorage " + OnOff(IsFeatureOn("DIRECTSTORAGE_SETTINGS")));
		header.push_back("Sizes: HEAP_SIZE=" + GetIniString("HEAP_SETTINGS", "HEAP_SIZE", "2000") +
			", MODKITARRAY_SIZE=" + GetIniString("MODKIT_SETTINGS", "MODKITARRAY_SIZE", "65536") +
			", PACKFILEARRAY_SIZE=" + GetIniString("PACKFILE_SETTINGS", "PACKFILEARRAY_SIZE", "13000"));
		header.push_back("RageOpenV: " + Switch("RAGEOPENV_SETTINGS", "MODS_FOLDER", true) + " " +
			Switch("RAGEOPENV_SETTINGS", "OPEN_RPF", true) + " " +
			Switch("RAGEOPENV_SETTINGS", "SKIP_RPF_CACHE_CHECK", true) + " " +
			Switch("RAGEOPENV_SETTINGS", "NEWMODS", false) + " " +
			Switch("RAGEOPENV_SETTINGS", "LOG_INFO", false) + " " +
			Switch("RAGEOPENV_SETTINGS", "LOG_MODS", false) + " " +
			Switch("RAGEOPENV_SETTINGS", "LOG_DEVICE", false));
		header.push_back("StraightToStoryMode: " + Switch("STRAIGHTTOSTORYMODE_SETTINGS", "SKIP_SPLASH_SCREEN", true) + " " +
			Switch("STRAIGHTTOSTORYMODE_SETTINGS", "SKIP_LEGAL_SCREEN", true) + " " +
			Switch("STRAIGHTTOSTORYMODE_SETTINGS", "SKIP_LANDING_PAGE", true));
		header.push_back("DirectStorage: " + Switch("DIRECTSTORAGE_SETTINGS", "LOG_CALLS", false) + " " +
			Switch("DIRECTSTORAGE_SETTINGS", "LOG_FILE_OPENS", false));
		header.push_back("TweaksV: " + Switch("TWEAKSV", "LOG_TO_FILE", true) + " " +
			Switch("TWEAKSV", "CONSOLE", false) + " " +
			Switch("TWEAKSV", "SHOW_ERROR_POPUPS", true) + " " +
			Switch("TWEAKSV", "CHECK_FOR_CONFLICTS", true) + " " +
			Switch("TWEAKSV", "DIAGNOSTIC_MODE", false) + " " +
			Switch("TWEAKSV", "DRY_RUN", false));
#ifdef TWEAKSV_TEST_BUILD
		header.push_back("TEST BUILD - not for release. TEST_BREAK_PATTERN=" +
			GetIniString("TWEAKSV", "TEST_BREAK_PATTERN", "(none)"));
#endif

		AcquireSRWLockExclusive(&g_lock);
		for (const auto& line : header)
			WriteLineLocked(FormatLine("TweaksV", line));
		for (const auto& line : g_pending)
			WriteLineLocked(line);
		g_pending.clear();
		g_pending.shrink_to_fit();
		g_started = true;
		ReleaseSRWLockExclusive(&g_lock);
	}

	void CloseLog()
	{
		// Never wait here: at process exit another thread may have died holding the lock.
		if (!TryAcquireSRWLockExclusive(&g_lock))
			return;
		if (g_file != INVALID_HANDLE_VALUE)
		{
			FlushFileBuffers(g_file);
			CloseHandle(g_file);
			g_file = INVALID_HANDLE_VALUE;
		}
		ReleaseSRWLockExclusive(&g_lock);
	}

	void OpenConsole()
	{
		// RageOpenV's code (main.cpp:56-64), moved here (plan 5.5 step 5).
		if (!AllocConsole() && GetConsoleWindow() == nullptr)
		{
			const DWORD error = GetLastError();
			WriteLog("TweaksV", "The console could not be opened (" + DescribeError(error) + ").");
			return;
		}

		FILE* unused = nullptr;
		freopen_s(&unused, "CONIN$", "r", stdin);
		freopen_s(&unused, "CONOUT$", "w", stdout);
		freopen_s(&unused, "CONOUT$", "w", stderr);
		SetConsoleTitleW(L"TweaksV");

		AcquireSRWLockExclusive(&g_lock);
		g_console = true;
		ReleaseSRWLockExclusive(&g_lock);
		WriteLog("TweaksV", "Console opened (CONSOLE=1).");
	}

	std::wstring GetLogPathW()
	{
		AcquireSRWLockShared(&g_lock);
		std::wstring path = g_path;
		const bool cleared = g_cleared;
		const bool toFile = g_toFile;
		ReleaseSRWLockShared(&g_lock);
		if (!cleared)
			return GetGameFolder() + L"\\TweaksV.log";
		return toFile ? path : std::wstring(L"(off - LOG_TO_FILE=0)");
	}

	std::string ToUtf8(std::wstring_view text)
	{
		if (text.empty())
			return {};
		const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		if (size <= 0)
			return {};
		std::string result(static_cast<size_t>(size), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
		return result;
	}

	std::wstring FromUtf8(std::string_view text)
	{
		if (text.empty())
			return {};
		const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
		if (size <= 0)
			return {};
		std::wstring result(static_cast<size_t>(size), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
		return result;
	}

	std::string DescribeError(DWORD error)
	{
		// A short table instead of FormatMessage, which may load resource DLLs - not
		// something to do under the loader lock.
		const char* text = nullptr;
		switch (error)
		{
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:		text = "path not found"; break;
		case ERROR_ACCESS_DENIED:		text = "access denied"; break;
		case ERROR_SHARING_VIOLATION:	text = "file in use"; break;
		case ERROR_WRITE_PROTECT:		text = "write-protected"; break;
		case ERROR_DISK_FULL:
		case ERROR_HANDLE_DISK_FULL:	text = "disk full"; break;
		default: break;
		}
		std::string result = "Windows error " + std::to_string(error);
		if (text)
			result += std::string(", ") + text;
		return result;
	}
}
