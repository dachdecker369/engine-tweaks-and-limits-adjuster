// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the one log file, TweaksV.log.

#include "TweaksVLog.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "BuildInfo.h"
#include "GameVersion.h"
#include "TweaksVFiles.h"
#include "TweaksVIni.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

extern "C" IMAGE_DOS_HEADER __ImageBase; // this module (TweaksV.asi)

namespace TweaksV
{
	namespace
	{
		constexpr size_t kMaxLine = 2048;		// the longest line written: 2 KB
		constexpr size_t kMaxPending = 1024;	// lines kept until the log is started
		constexpr uint64_t kMaxLogBytes = 64ull * 1024 * 1024;	// TweaksV.log grows to 64 MB at most

		SRWLOCK g_lock = SRWLOCK_INIT;
		HANDLE g_file = INVALID_HANDLE_VALUE;
		std::wstring g_path;					// the log file in use
		bool g_cleared = false;					// ClearLogFile() ran
		bool g_started = false;					// the header is written
		bool g_toFile = true;					// LOG
		bool g_console = false;					// OpenConsole() succeeded
		std::vector<std::string> g_pending;
		uint64_t g_logLimit = kMaxLogBytes;		// test builds can lower it (TEST_LOG_LIMIT_KB)
		uint64_t g_logBytes = 0;				// written to the file this session
		bool g_logFull = false;					// the limit was reached: nothing more goes to the file

		// A folder as it appears in log lines: UTF-8, no separator at the end. Empty if it is too short
		// to stand for a folder of its own - never a bare drive such as "C:".
		std::string AsLogFolder(std::wstring_view folder)
		{
			std::string utf8 = ToUtf8(folder);
			while (!utf8.empty() && (utf8.back() == '\\' || utf8.back() == '/'))
				utf8.pop_back();
			return utf8.size() >= 4 ? utf8 : std::string();
		}

		// The user's profile folder (USERPROFILE, e.g. C:\Users\Name) as it appears in log lines, so
		// that it can be written as %USERPROFILE%: logs get posted in public, and the folder is
		// usually named after the Windows user. Its short form as well (C:\Users\NAME~1), which %TEMP%
		// often has. Empty forms are left out; worked out once.
		const std::vector<std::string>& ProfileFolders()
		{
			static const std::vector<std::string> folders = []
			{
				std::vector<std::string> forms;
				wchar_t buffer[MAX_PATH]{};
				const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", buffer, MAX_PATH);
				if (length == 0 || length >= MAX_PATH)
					return forms;
				const std::string full = AsLogFolder(std::wstring_view(buffer, length));
				if (full.empty())
					return forms;
				forms.push_back(full);
				wchar_t shortBuffer[MAX_PATH]{};
				const DWORD shortLength = GetShortPathNameW(buffer, shortBuffer, MAX_PATH);
				if (shortLength > 0 && shortLength < MAX_PATH)
				{
					const std::string shortForm = AsLogFolder(std::wstring_view(shortBuffer, shortLength));
					if (!shortForm.empty() && _stricmp(shortForm.c_str(), full.c_str()) != 0)
						forms.push_back(shortForm);
				}
				return forms;
			}();
			return folders;
		}

		// Writes every mention of the profile folder in line as %USERPROFILE%, whatever its case. Only a
		// whole folder name counts: it must be followed by a separator or end the line, so that the
		// profile C:\Users\Al does not turn C:\Users\Alex into nonsense.
		void HideProfileFolder(std::string& line)
		{
			for (const std::string& folder : ProfileFolders())
			{
				for (size_t at = 0; at + folder.size() <= line.size(); ++at)
				{
					const size_t end = at + folder.size();
					if ((end == line.size() || line[end] == '\\' || line[end] == '/') &&
						_strnicmp(line.c_str() + at, folder.c_str(), folder.size()) == 0)
					{
						line.replace(at, folder.size(), "%USERPROFILE%");
						at += sizeof("%USERPROFILE%") - 2;
					}
				}
			}
		}

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
			HideProfileFolder(line);
			if (line.size() > kMaxLine - 2)
			{
				line.resize(kMaxLine - 5);
				line += "...";
			}
			return line;
		}

		// "64 MB" or, for a limit that is not whole megabytes (test builds), "16 KB".
		std::string DescribeLogLimit()
		{
			const uint64_t mb = 1024 * 1024;
			return g_logLimit % mb == 0 ? std::to_string(g_logLimit / mb) + " MB" : std::to_string(g_logLimit / 1024) + " KB";
		}

		// Caller holds g_lock. Once the file holds g_logLimit bytes, a last line says so and nothing more
		// goes to the file; the console still gets every line.
		void WriteLineLocked(const std::string& line)
		{
			constexpr size_t kLastLineRoom = 256;	// kept free for the line that says the limit is reached
			if (g_toFile && g_file != INVALID_HANDLE_VALUE && !g_logFull)
			{
				std::string data = line + "\r\n";
				if (g_logBytes + data.size() + kLastLineRoom > g_logLimit)
				{
					data = FormatLine("TweaksV", "TweaksV.log has reached its limit of " + DescribeLogLimit() +
						" - nothing more is written to it this session.") + "\r\n";
					g_logFull = true;
				}
				DWORD written = 0;
				if (WriteFile(g_file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr))
					g_logBytes += written;
			}
			if (g_console)
			{
				fputs(line.c_str(), stdout);
				fputc('\n', stdout);
				fflush(stdout);
			}
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

		// "0x6870ABCD (YYYY-MM-DD hh:mm:ss UTC)" from the exe's PE header.
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
		const bool toFile = IsOn(kLog);
		const std::wstring primaryPath = GetGameFolder() + L"\\TweaksV.log";

#ifdef TWEAKSV_TEST_BUILD
		// test builds only: TEST_LOG_LIMIT_KB under [TWEAKSV] lowers the limit, so the tests can reach it
		const std::string& testLimit = GetText(kTestLogLimitKb);
		const unsigned long testLimitKb = testLimit.empty() ? 0 : strtoul(testLimit.c_str(), nullptr, 10);
#endif

		AcquireSRWLockExclusive(&g_lock);
		g_toFile = toFile;
		if (g_file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(g_file);
			g_file = INVALID_HANDLE_VALUE;
		}
		g_path.clear();
		g_logBytes = 0;
		g_logFull = false;
#ifdef TWEAKSV_TEST_BUILD
		if (testLimitKb >= 1 && testLimitKb <= 65536)
			g_logLimit = static_cast<uint64_t>(testLimitKb) * 1024;
#endif

		// Only a TweaksV.log that is what its path says is written (TweaksVFiles.h): first in the game
		// folder, else - the game folder read-only, a link there, the file in use - in %LOCALAPPDATA%\TweaksV.
		OwnFile primary{}, fallback{};
		std::wstring fallbackPath;
		if (g_toFile)
		{
			primary = OpenOwnFile(GetGameFolder(), L"TweaksV.log", FileFolder::Game, OPEN_ALWAYS, true);
			if (primary.handle != INVALID_HANDLE_VALUE)
			{
				g_file = primary.handle;
				g_path = primaryPath;
			}
			else
			{
				const std::wstring folder = GetFallbackFolder();
				if (!folder.empty())
				{
					CreateDirectoryW(folder.c_str(), nullptr);
					fallbackPath = folder + L"\\TweaksV.log";
					fallback = OpenOwnFile(folder, L"TweaksV.log", FileFolder::Own, OPEN_ALWAYS, true);
					if (fallback.handle != INVALID_HANDLE_VALUE)
					{
						g_file = fallback.handle;
						g_path = fallbackPath;
					}
				}
			}
		}
		g_cleared = true;
		const bool usingFallback = g_toFile && !fallbackPath.empty() && g_path == fallbackPath;
		const bool noFile = g_toFile && g_file == INVALID_HANDLE_VALUE;
		ReleaseSRWLockExclusive(&g_lock);

		// Written after releasing the lock: WriteLog and ReportFailure take it. A read-only game folder
		// is normal (e.g. under C:\Program Files), so that fallback is only logged; any other reason - a
		// link where the log belongs, the file in use - and no log at all are problems.
		if (usingFallback && primary.ReadOnly())
			WriteLog("TweaksV", "The game folder is read-only (" + DescribeError(primary.error) +
				"), so TweaksV.log is written to " + ToUtf8(fallbackPath) + ".");
		else if (usingFallback)
			ReportFailure("TweaksV", "The TweaksV.log in the game folder is not used: " + primary.Reason() +
				". TweaksV.log is written to " + ToUtf8(fallbackPath) + " instead.");
		else if (noFile)
			ReportFailure("TweaksV", "TweaksV.log could not be written in the game folder (" + primary.Reason() + ") or in "
				"%LOCALAPPDATA%\\TweaksV (" + (fallbackPath.empty() ? std::string("LOCALAPPDATA is not set") : fallback.Reason()) +
				"). Nothing is logged to a file this session.");
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
		for (std::string& line : DescribeSettings())	// which parts are on, then every setting's value
			header.push_back(std::move(line));

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
		// Carried over from RageOpenV by Chiheb-Bacha (GPL-3.0): its DllMain opened the console this way.
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
		return toFile ? path : std::wstring(L"(off - LOG=0)");
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
		case ERROR_ENVVAR_NOT_FOUND:	text = "environment variable not set"; break;
		default: break;
		}
		std::string result = "Windows error " + std::to_string(error);
		if (text)
			result += std::string(", ") + text;
		return result;
	}
}
