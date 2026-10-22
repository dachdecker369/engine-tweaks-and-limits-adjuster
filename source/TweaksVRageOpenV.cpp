// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - RageOpenV (see TweaksVRageOpenV.h).
//
// Three jobs, each with its own switch, each done all or nothing and left undone alone when something
// is not as expected, plus the console patch:
//   MODS_FOLDER           the game's eight file functions - OpenBulk, GetFileTime, GetFileSize and
//                         GetAttributes of its two file devices (R-E-F1..F8) - are hooked, so that a
//                         path that exists under mods\ (in the game's current directory, normally the
//                         game folder) is served from there. The folder is read once at start-up into a
//                         list, and every call looks its path up in that list and does nothing else: no
//                         file system call, no memory allocation, no lock. Both spellings of a path
//                         (slashes, case) match, a path with ".." or one that is not relative is never
//                         redirected, and a file the folder does not have reaches the game's own
//                         function untouched. A mods file that cannot be opened is never replaced by
//                         the game's own: OpenBulk gives the game that failure, as the game's function
//                         would for any file it cannot open, logged and reported once. Without a mods
//                         folder, or an empty one, nothing is hooked.
//   OPEN_RPF              five calls of the archive code (R-E-E1..E5: FindEncryption, DecryptHeader,
//                         DecryptHeader2 twice, ParseHeader) are redirected through call stubs, so that
//                         an archive whose header says OPEN is read without decryption. The encryption
//                         tag is kept per thread and used up by the parse it was found for, so archives
//                         opened at the same time never mix, and an encrypted archive's entry table is
//                         never walked.
//   SKIP_RPF_CACHE_CHECK  the function the RPF cache check calls (R-E-K1) returns at once: C3 90.
//   CONSOLE=1             the call that would hide the console (R-E-C1) becomes NOPs.
//   DIAGNOSTIC_MODE       every site and write; then every file request, every file served from mods\
//                         and every archive header read - each kind 20,000 lines at most, then only
//                         counted - and a summary 10 seconds in, then every minute while the numbers
//                         change.
// The game's file system is not up in DllMain, so the part hooks GetSystemTimeAsFileTime and does its
// work when the game first asks for the time; then it shows the start-up summary and switches that
// hook off again - or, if the game has not asked 30 seconds in, a thread says so, shows the summary and
// switches the hook off. A file function another mod has already hooked is hooked on top and chains
// through - for the four found by a pattern inside them (GetFileTime and GetFileSize of both devices):
// a hook on OpenBulk or GetAttributes changes the bytes their patterns match at the function's start,
// so the pattern is not found and the mods folder is not used. A call another mod has redirected, and
// a cache check another mod has skipped or hooked, are left alone.
// Written for TweaksV; the patterns, the archive facts and the idea come from RageOpenV by
// Chiheb-Bacha, which builds on ClosedIV by martonp96 (CREDITS.md).

#include "TweaksVRageOpenV.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "TweaksVConflicts.h"
#include "TweaksVFiles.h"
#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVPatterns.h"
#include "TweaksVScanner.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "RageOpenV";

		bool g_diagnostic = false;		// DIAGNOSTIC_MODE, read once: the hooks never read the settings
		bool g_filesHooked = false;
		bool g_archivesHooked = false;

		template <typename F> const void* Code(F* function) { return reinterpret_cast<const void*>(function); }
		template <typename F> void** Original(F*& variable) { return reinterpret_cast<void**>(&variable); }

		// ---- the diagnostic lines: each kind capped, then only counted; a summary every minute

		constexpr uint64_t kMaxLinesPerKind = 20000;
		constexpr DWORD kFirstSummaryMs = 10000;
		constexpr DWORD kSummaryEveryMs = 60000;
		constexpr size_t kMaxPathText = 259;		// characters of a game path (MAX_PATH less the terminator)

		struct EventKind
		{
			const char* name;						// "file requests"
			bool diagnostic = true;					// its lines come only with DIAGNOSTIC_MODE
			std::atomic<uint64_t> count{ 0 };
		};
		EventKind g_requests{ "file requests" };
		EventKind g_served{ "files served from the mods folder" };
		EventKind g_notOpened{ "mods files that could not be opened", false };
		EventKind g_archives{ "archive headers" };
		EventKind g_unprepared{ "OPEN archives left as they are", false };
		std::atomic<uint64_t> g_openArchives{ 0 };
		std::atomic<bool> g_notOpenedReported{ false };		// the first mods file that cannot be opened is a problem
		std::atomic<bool> g_unpreparedReported{ false };	// so is the first OPEN archive left as it is

		// Counts the event; true if it still gets a line of its own. The first event past the cap says so.
		bool Counted(EventKind& kind)
		{
			const uint64_t n = kind.count.fetch_add(1, std::memory_order_relaxed) + 1;
			if (n <= kMaxLinesPerKind)
				return true;
			if (n == kMaxLinesPerKind + 1)
				WriteLog(kTag, std::string(kind.diagnostic ? "DIAGNOSTIC: " : "") + std::to_string(kMaxLinesPerKind) + " lines of " + kind.name +
					" written - from now on they are only counted" + (g_diagnostic ? "; the summary every minute has the numbers." : "."));
			return false;
		}

		// A path as the game passed it, for a log line: at most kMaxPathText bytes in the ANSI code page
		// the game asks with, written as UTF-8 as every line is - so that %USERPROFILE% is replaced in it too.
		std::string PathText(const char* path)
		{
			if (!path)
				return "(null)";
			const int length = static_cast<int>(strnlen(path, kMaxPathText));
			wchar_t wide[kMaxPathText];
			const int converted = length > 0 ? MultiByteToWideChar(CP_ACP, 0, path, length, wide, static_cast<int>(kMaxPathText)) : 0;
			return converted > 0 ? ToUtf8(std::wstring_view(wide, static_cast<size_t>(converted))) : std::string(path, static_cast<size_t>(length));
		}

		// The lines from the hooks: nothing may throw back into the game.
		void LogRequest(const char* function, const char* path)
		{
			try
			{
				if (Counted(g_requests))
					WriteLog(kTag, std::string("DIAGNOSTIC: ") + function + " asked for " + PathText(path));
			}
			catch (...)
			{
			}
		}

		void LogServed(const char* function, const char* modsPath)
		{
			try
			{
				if (Counted(g_served))
					WriteLog(kTag, std::string("DIAGNOSTIC: ") + function + " served " + PathText(modsPath));
			}
			catch (...)
			{
			}
		}

		void LogArchive(const char* name, int32_t entries, bool open)
		{
			try
			{
				if (Counted(g_archives))
					WriteLog(kTag, "DIAGNOSTIC: archive " + PathText(name) + ": " + std::to_string(entries) + " entries, " +
						(open ? "OPEN - read without decryption" : "encrypted"));
			}
			catch (...)
			{
			}
		}

		// The mods files that could not be opened and the OPEN archives left as they are only when there are any.
		void LogSummary()
		{
			const uint64_t notOpened = g_notOpened.count.load(), unprepared = g_unprepared.count.load();
			WriteLog(kTag, "DIAGNOSTIC: " + std::to_string(g_requests.count.load()) + " file requests so far, " +
				std::to_string(g_served.count.load()) + " of them served from the mods folder" +
				(notOpened ? " and " + std::to_string(notOpened) + " whose mods file could not be opened" : std::string()) + "; " +
				std::to_string(g_archives.count.load()) + " archive headers read, " + std::to_string(g_openArchives.load()) + " of them OPEN" +
				(unprepared ? " (" + std::to_string(unprepared) + " left as they are)" : std::string()) + ".");
		}

		// The first summary 10 seconds in, then one a minute while the numbers change.
		DWORD WINAPI SummaryThread(LPVOID)
		{
			uint64_t last = UINT64_MAX;
			for (DWORD wait = kFirstSummaryMs;; wait = kSummaryEveryMs)
			{
				Sleep(wait);
				const uint64_t now = g_requests.count.load() + g_served.count.load() + g_notOpened.count.load() + g_archives.count.load() +
					g_openArchives.load() + g_unprepared.count.load();
				if (now == last)
					continue;
				last = now;
				try
				{
					LogSummary();
				}
				catch (...)
				{
				}
			}
		}

		// ---- the mods folder: its list, read once, and the eight file functions

		constexpr size_t kMaxModsEntries = 200000;	// files and folders the list holds at most
		constexpr size_t kMaxModsDepth = 32;		// folders deep
		constexpr size_t kMaxModsPath = kMaxPathText;
		constexpr char kModsPrefix[] = "mods/";
		constexpr size_t kModsPrefixSize = sizeof(kModsPrefix) - 1;
		constexpr size_t kDevices = 2;				// the game's two file devices, each with the four functions
		constexpr size_t kFunctionsPerDevice = 4;

		struct PathHash
		{
			using is_transparent = void;
			size_t operator()(std::string_view path) const noexcept { return std::hash<std::string_view>{}(path); }
		};

		// Every file and folder under mods\, as the list spells it: relative, lower case, '/' between the
		// names. Filled once, before the hooks go on; never changed afterwards. Allocated once and never
		// freed - a deliberate leak at the end of the process: the hooks stay on until then, and a list
		// destroyed with TweaksV's static objects at DLL_PROCESS_DETACH could still be looked up.
		using ModsList = std::unordered_set<std::string, PathHash, std::equal_to<>>;
		ModsList* g_modsList = nullptr;

		// The list's spelling of a game path: lower case (ASCII), '/' between the names, a doubled '/'
		// inside collapsed. False for an empty path, one longer than kMaxModsPath, and one that is not
		// relative - it starts with a separator or a drive ("C:"): only a relative path names a file of
		// the game folder.
		bool ListSpelling(const char* path, char (&out)[kMaxModsPath + 1], size_t& length)
		{
			length = 0;
			if (path[0] == '\\' || path[0] == '/' || (path[0] != '\0' && path[1] == ':'))
				return false;
			for (const char* p = path; *p; p++)
			{
				char c = *p == '\\' ? '/' : *p;
				if (c == '/' && length > 0 && out[length - 1] == '/')
					continue;
				if (c >= 'A' && c <= 'Z')
					c = static_cast<char>(c - 'A' + 'a');
				if (length == kMaxModsPath)
					return false;
				out[length++] = c;
			}
			out[length] = '\0';
			return length > 0;
		}

		// What reading the mods folder found: the entries in the list; the names left out, the two reasons
		// apart; the folders not read - at the depth limit, or that could not be listed, the first one's
		// Windows error kept; whether the folder itself could not be listed, or the list filled up; and the
		// ANSI code page the names were spelled in.
		struct ModsFolderRead
		{
			size_t entries = 0;
			size_t notConvertible = 0;			// names the ANSI code page cannot hold
			size_t tooLong = 0;					// names whose path in the list would be longer than kMaxModsPath bytes
			size_t tooDeep = 0;					// folders at the depth limit: listed, what they hold not read
			size_t unreadable = 0;				// folders that could not be listed
			DWORD firstError = ERROR_SUCCESS;	// why the first of them, or the mods folder itself, could not be
			bool rootUnreadable = false;
			bool full = false;					// the list stopped at kMaxModsEntries
			UINT codePage = 0;
		};

		// Reads the mods folder - in the current directory, where the game's own file functions resolve
		// "mods/..." too - into g_modsList. A name the ANSI code page cannot hold, or longer than the
		// limit, is left out and counted: the game, which asks with ANSI paths, could not ask for it either.
		// A UTF-8 code page (65001) takes neither WC_NO_BEST_FIT_CHARS nor a default character - the call
		// would fail for every name - so its names are converted as UTF-8. A folder at the depth limit, and
		// one that cannot be listed, is counted and the reading goes on; only the entry limit ends it.
		ModsFolderRead ReadModsFolder()
		{
			struct Folder { std::wstring path; std::string relative; size_t depth; };
			std::vector<Folder> todo{ { L"mods", std::string(), 1 } };
			ModsFolderRead read;
			read.codePage = GetACP();
			const bool utf8 = read.codePage == CP_UTF8;
			if (!g_modsList)
				g_modsList = new ModsList();
			while (!todo.empty() && !read.full)
			{
				const Folder folder = std::move(todo.back());
				todo.pop_back();
				WIN32_FIND_DATAW found{};
				const HANDLE find = FindFirstFileExW((folder.path + L"\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch,
					nullptr, FIND_FIRST_EX_LARGE_FETCH);
				if (find == INVALID_HANDLE_VALUE)
				{
					const DWORD error = GetLastError();
					if (error == ERROR_FILE_NOT_FOUND)	// nothing in it, not even "." - a drive's root behind a junction
						continue;
					if (folder.relative.empty())
					{
						read.rootUnreadable = true;
						read.firstError = error;
						break;
					}
					if (read.unreadable++ == 0)
						read.firstError = error;
					if (read.unreadable <= kMaxLinesPerKind)
						WriteLog(kTag, "The folder " + ToUtf8(folder.path) + " could not be read (" + DescribeError(error) + ") - its files are not used.");
					continue;
				}
				do
				{
					if (wcscmp(found.cFileName, L".") == 0 || wcscmp(found.cFileName, L"..") == 0)
						continue;
					char name[kMaxModsPath + 1];
					BOOL lossy = FALSE;
					const int size = utf8
						? WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, found.cFileName, -1, name, sizeof(name), nullptr, nullptr)
						: WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, found.cFileName, -1, name, sizeof(name), nullptr, &lossy);
					if (size <= 1 || lossy)
					{
						if (size == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
							read.tooLong++;
						else
							read.notConvertible++;
						continue;
					}
					std::string relative = folder.relative.empty() ? std::string(name) : folder.relative + "/" + name;
					if (relative.size() > kMaxModsPath)
					{
						read.tooLong++;
						continue;
					}
					for (char& c : relative)
						if (c >= 'A' && c <= 'Z')
							c = static_cast<char>(c - 'A' + 'a');
					if (g_modsList->size() >= kMaxModsEntries)
					{
						read.full = true;
						break;
					}
					const bool isFolder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
					if (isFolder && folder.depth >= kMaxModsDepth)
						read.tooDeep++;	// listed; what it holds is not read
					else if (isFolder)
						todo.push_back({ folder.path + L"\\" + found.cFileName, relative, folder.depth + 1 });
					g_modsList->insert(std::move(relative));
				} while (FindNextFileW(find, &found));
				FindClose(find);
			}
			read.entries = g_modsList->size();
			return read;
		}

		// "1 folder", "2 folders"
		std::string Folders(size_t count)
		{
			return std::to_string(count) + (count == 1 ? " folder" : " folders");
		}

		// The line for the names left out of the list, each reason with its count.
		std::string LeftOutLine(const ModsFolderRead& read)
		{
			std::string line = "Names in the mods folder left out of the list:";
			if (read.notConvertible)
				line += " " + std::to_string(read.notConvertible) + " that the ANSI code page (" + std::to_string(read.codePage) +
					") cannot hold - the game asks with paths in that code page";
			if (read.notConvertible && read.tooLong)
				line += ";";
			if (read.tooLong)
				line += " " + std::to_string(read.tooLong) + " whose path is longer than " + std::to_string(kMaxModsPath) + " bytes";
			return line + ".";
		}

		// The path to hand the game's function for a file or folder the mods folder has: "mods/" and the
		// path in the game's own spelling. False if the mods folder does not have it.
		bool ModsPath(const char* path, char (&out)[kMaxModsPath + kModsPrefixSize + 1])
		{
			char spelling[kMaxModsPath + 1];
			size_t length = 0;
			if (!path || !g_modsList || !ListSpelling(path, spelling, length) || !g_modsList->contains(std::string_view(spelling, length)))
				return false;
			const size_t original = strnlen(path, kMaxModsPath + 1);
			if (original > kMaxModsPath)
				return false;
			memcpy(out, kModsPrefix, kModsPrefixSize);
			memcpy(out + kModsPrefixSize, path, original);
			out[kModsPrefixSize + original] = '\0';
			return true;
		}

		// The game's functions: OpenBulk, and the three that take a path and return 8 bytes in rax - a
		// FILETIME, a size, attributes.
		using OpenBulkFunction = HANDLE (*)(void* device, const char* path, int64_t* offset);
		using PathFunction = uint64_t (*)(void* device, const char* path);

		constexpr const char* kFunctionNames[kDevices][kFunctionsPerDevice] = {
			{ "OpenBulk 1", "GetFileTime 1", "GetFileSize 1", "GetAttributes 1" },
			{ "OpenBulk 2", "GetFileTime 2", "GetFileSize 2", "GetAttributes 2" } };
		OpenBulkFunction g_originalOpenBulk[kDevices] = {};
		PathFunction g_originalPath[kDevices][kFunctionsPerDevice - 1] = {};	// GetFileTime, GetFileSize, GetAttributes

		// The mods file could not be opened, and the game gets that failure, not its own file: a line for
		// each (capped), and the first of a session is reported - the player's file is not used.
		void NotOpened(const char* function, const char* modsPath)
		{
			try
			{
				if (Counted(g_notOpened))
					WriteLog(kTag, std::string(function) + " could not open " + PathText(modsPath) +
						" - the game gets that failure, not its own file.");
				if (!g_notOpenedReported.load(std::memory_order_relaxed) && !g_notOpenedReported.exchange(true))
					ReportFailure(kTag, "A file in the mods folder could not be opened - the game gets that failure, not its own file: " +
						PathText(modsPath) + ".");
			}
			catch (...)
			{
			}
		}

		// One hook per device for OpenBulk: a file the mods folder has is opened from there, and what the
		// game's function returns for it - a failure too - is what the game gets. A file served is logged once
		// it is open; the game's last error is kept around either line.
		template <size_t Device> HANDLE OpenBulkHook(void* device, const char* path, int64_t* offset)
		{
			if (g_diagnostic)
				LogRequest(kFunctionNames[Device][0], path);
			char modsPath[kMaxModsPath + kModsPrefixSize + 1];
			if (!ModsPath(path, modsPath))
				return g_originalOpenBulk[Device](device, path, offset);
			const HANDLE result = g_originalOpenBulk[Device](device, modsPath, offset);
			const bool opened = result && result != INVALID_HANDLE_VALUE;
			if (opened && !g_diagnostic)
				return result;
			const DWORD lastError = GetLastError();
			if (opened)
				LogServed(kFunctionNames[Device][0], modsPath);
			else
				NotOpened(kFunctionNames[Device][0], modsPath);
			SetLastError(lastError);
			return result;
		}

		// One hook per device and function for GetFileTime, GetFileSize and GetAttributes: a file the
		// mods folder has is answered for from there.
		template <size_t Device, size_t Function> uint64_t PathHook(void* device, const char* path)
		{
			if (g_diagnostic)
				LogRequest(kFunctionNames[Device][Function + 1], path);
			char modsPath[kMaxModsPath + kModsPrefixSize + 1];
			if (ModsPath(path, modsPath))
			{
				if (g_diagnostic)
					LogServed(kFunctionNames[Device][Function + 1], modsPath);
				return g_originalPath[Device][Function](device, modsPath);
			}
			return g_originalPath[Device][Function](device, path);
		}

		void HookModsFolder()
		{
			if (!IsOn(kModsFolder))
			{
				WriteLog(kTag, "MODS_FOLDER=0 - files in the mods folder are not used.");
				return;
			}
			const DWORD attributes = GetFileAttributesW(L"mods");
			const bool haveFolder = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			if (!haveFolder && !g_diagnostic)
			{
				WriteLog(kTag, "No mods folder in the current directory - the file functions are not hooked.");
				return;
			}
			const auto fail = [](const std::string& why) { ReportFailure(kTag, "Files in the mods folder are not used this session: " + why); };

			// 1. the eight functions, found and checked before anything is hooked: two per device by a
			// pattern at their start, two by a pattern inside them, a fixed distance before the match
			struct FileFunction { const GamePattern* pattern; ptrdiff_t start; const void* hook; void** original; };
			const FileFunction functions[kDevices][kFunctionsPerDevice] = {
				{ { &Pattern("R-E-F1"), 0, Code(&OpenBulkHook<0>), Original(g_originalOpenBulk[0]) },
				  { &Pattern("R-E-F2"), -0x10D, Code(&PathHook<0, 0>), Original(g_originalPath[0][0]) },
				  { &Pattern("R-E-F3"), -0x10D, Code(&PathHook<0, 1>), Original(g_originalPath[0][1]) },
				  { &Pattern("R-E-F4"), 0, Code(&PathHook<0, 2>), Original(g_originalPath[0][2]) } },
				{ { &Pattern("R-E-F5"), 0, Code(&OpenBulkHook<1>), Original(g_originalOpenBulk[1]) },
				  { &Pattern("R-E-F6"), -0x15D, Code(&PathHook<1, 0>), Original(g_originalPath[1][0]) },
				  { &Pattern("R-E-F7"), -0x15D, Code(&PathHook<1, 1>), Original(g_originalPath[1][1]) },
				  { &Pattern("R-E-F8"), 0, Code(&PathHook<1, 2>), Original(g_originalPath[1][2]) } } };
			std::vector<FunctionHook> hooks;
			for (size_t device = 0; device < kDevices; device++)
				for (size_t i = 0; i < kFunctionsPerDevice; i++)
				{
					const FileFunction& function = functions[device][i];
					const char* const name = kFunctionNames[device][i];
					const std::string label = function.pattern->label;
					uint8_t* const match = Scanner::Find(*function.pattern);
					if (!match)
					{
						fail(label + " was not found (" + name + ").");
						return;
					}
					uint8_t* const start = match + function.start;
					std::string detail;
					if (!IsInsideGameImage(start))
					{
						fail(std::string(name) + " is not inside the game: " + DescribeAddress(start) + " (" + label + ").");
						return;
					}
					if (!IsReadable(start, 2))	// the two bytes StartsWithJump reads; IsFunctionStart checks the byte before itself
					{
						fail(std::string(name) + " at " + DescribeAddress(start) + " is not readable memory (" + label + ").");
						return;
					}
					if (function.start != 0 && !IsFunctionStart(start, detail))
					{
						fail(std::string(name) + " at " + DescribeAddress(start) + " is not a function start (" + detail + ").");
						return;
					}
					if (StartsWithJump(start))
						WriteLog(kTag, std::string(name) + " at " + DescribeAddress(start) + " already starts with a jump - another mod may have "
							"hooked it; the hook goes on top and chains through.");
					if (g_diagnostic)
						WriteLog(kTag, "DIAGNOSTIC: " + std::string(name) + " at " + DescribeAddress(start) +
							(function.start != 0 ? " - " + detail : std::string()));
					hooks.push_back({ start, function.hook, function.original, name });
				}
			if (!haveFolder)
			{
				WriteLog(kTag, "No mods folder in the current directory - the patterns were checked (DIAGNOSTIC_MODE), the file functions "
					"are not hooked.");
				return;
			}
			if (IsDryRun())
			{
				WriteLog(kTag, "DRY_RUN: would hook the 8 file functions for the mods folder.");
				return;
			}

			// 2. the folder, read once before the hooks go on
			const ModsFolderRead read = ReadModsFolder();
			if (read.rootUnreadable)
			{
				fail("the mods folder could not be read (" + DescribeError(read.firstError) + ").");
				return;
			}
			WriteLog(kTag, "The mods folder has " + std::to_string(read.entries) + " files and folders.");
			const size_t leftOut = read.notConvertible + read.tooLong;
			if (leftOut)
				WriteLog(kTag, LeftOutLine(read));
			const auto partly = [](const std::string& why) { ReportFailure(kTag, "Only part of the mods folder is used this session: " + why); };
			if (read.full)
				partly("the list stopped at " + std::to_string(kMaxModsEntries) + " files and folders.");
			if (read.tooDeep)
				partly(Folders(read.tooDeep) + " deeper than " + std::to_string(kMaxModsDepth) + " levels " + (read.tooDeep == 1 ? "was" : "were") +
					" skipped.");
			if (read.unreadable)
				partly(Folders(read.unreadable) + " could not be read (" + DescribeError(read.firstError) + ").");
			if (read.entries == 0)	// nothing to serve: the hooks would only cost
			{
				if (leftOut)
					fail("none of the " + std::to_string(leftOut) + " names in the mods folder can be used (see the line above).");
				else
					WriteLog(kTag, "The mods folder is empty - the file functions are not hooked.");
				return;
			}

			// 3. the eight hooks, all or nothing
			if (!HookFunctions(hooks, kTag))
			{
				fail("the file functions could not be hooked (details in TweaksV.log).");
				return;
			}
			if (g_diagnostic)
				for (const FunctionHook& hook : hooks)
					WriteLog(kTag, "DIAGNOSTIC: " + std::string(hook.what) + " hooked at " + DescribeAddress(hook.function) + ".");
			WriteLog(kTag, "Files in the mods folder are used (8 file functions hooked).");
			g_filesHooked = true;
		}

		// ---- OPEN archives: the five redirected calls

		constexpr uint32_t kOpenTag = 0x4E45504F;			// "OPEN": the encryption field of an archive that is not encrypted
		constexpr uint32_t kNgMask = 0x0FF00000;			// FindEncryption answers yes to an NG-encrypted archive: these bits...
		constexpr uint32_t kNgTag = 0x0FE00000;				// ...hold this
		constexpr uint32_t kEntryMark = 0x0FEFFFFF;			// what an OPEN archive's named binary entries and its current offset get
		constexpr int32_t kMaxArchiveEntries = 0x100000;	// more means the header was not read right
		constexpr size_t kPackfileShift = 8;				// Enhanced's fiPackfile has 8 bytes more before these fields
		constexpr size_t kEntryTableField = 0x20;			// char*: the entry table
		constexpr size_t kFileCountField = 0x28;			// int32: entries in it
		constexpr size_t kCurrentOffsetField = 0xB4;		// int32: the current file offset
		constexpr size_t kEntrySize = 16;					// every RPF7 entry
		constexpr size_t kEntryNameField = 0;				// uint16: a binary entry's name offset
		constexpr size_t kEntryTypeField = 4;				// uint32: kFolderType, or kResourceBit set, else a binary entry
		constexpr size_t kEntryEncryptedField = 12;			// uint32: a binary entry's encryption
		constexpr uint32_t kFolderType = 0x7FFFFF00;
		constexpr uint32_t kResourceBit = 0x80000000;
		constexpr size_t kArchiveCalls = 5;
		constexpr size_t kFirstDecryptHeader2 = 2;			// the two DecryptHeader2 calls in kCalls below...
		constexpr size_t kSecondDecryptHeader2 = 3;			// ...share one original, so they must lead to the same function
		constexpr size_t kCallSize = 5;						// "call rel32"

		thread_local uint32_t t_encryption = 0;				// what FindEncryption saw last on this thread, for the hooks that follow;
															// ParseHeader uses it up
		void (*g_originalDecryptHeader)(uint32_t salt, char* table, int size) = nullptr;
		void (*g_originalDecryptHeader2)(uint32_t encryption, uint32_t salt, char* header, int size) = nullptr;
		bool (*g_originalParseHeader)(void* packfile, const char* name, bool readHeader, void* customHeader) = nullptr;

		template <typename T> T Field(const void* base, size_t offset)
		{
			T value;
			memcpy(&value, static_cast<const uint8_t*>(base) + offset, sizeof(T));
			return value;
		}

		template <typename T> void SetField(void* base, size_t offset, T value)
		{
			memcpy(static_cast<uint8_t*>(base) + offset, &value, sizeof(T));
		}

		// Replaces the game's FindEncryption: keeps the tag for this thread and answers whether the
		// archive is NG-encrypted.
		bool FindEncryptionHook(uint32_t encryption)
		{
			t_encryption = encryption;
			return (encryption & kNgMask) == kNgTag;
		}

		// An OPEN archive's entry table is not decrypted.
		void DecryptHeaderHook(uint32_t salt, char* table, int size)
		{
			if (t_encryption != kOpenTag)
				g_originalDecryptHeader(salt, table, size);
		}

		void DecryptHeader2Hook(uint32_t encryption, uint32_t salt, char* header, int size)
		{
			if (encryption != kOpenTag)
				g_originalDecryptHeader2(encryption, salt, header, size);
		}

		// Once the game has read an archive's header: an OPEN archive's named binary entries and its
		// current offset are marked as the game expects them; any other archive is left as it is, and so
		// is an OPEN one whose entry table is implausible - logged (capped) and, the first time in a
		// session, reported. Can throw (a log line).
		void PrepareArchive(void* packfile, const char* name, bool open)
		{
			uint8_t* const fields = static_cast<uint8_t*>(packfile) + kPackfileShift;
			char* const table = Field<char*>(fields, kEntryTableField);
			const int32_t count = Field<int32_t>(fields, kFileCountField);
			if (g_diagnostic)
				LogArchive(name, count, open);
			if (!open)
				return;
			g_openArchives.fetch_add(1, std::memory_order_relaxed);
			if (!table || count < 0 || count > kMaxArchiveEntries)
			{
				if (Counted(g_unprepared))
					WriteLog(kTag, "The OPEN archive " + PathText(name) + " has an implausible entry table (" + std::to_string(count) +
						" entries) - left as it is.");
				if (!g_unpreparedReported.load(std::memory_order_relaxed) && !g_unpreparedReported.exchange(true))
					ReportFailure(kTag, "Unencrypted (OPEN) RPF archive " + PathText(name) + " could not be prepared: its entry table is "
						"implausible (" + std::to_string(count) + " entries) - its files may not load.");
				return;
			}
			for (int32_t i = 0; i < count; i++)
			{
				uint8_t* const entry = reinterpret_cast<uint8_t*>(table) + static_cast<size_t>(i) * kEntrySize;
				const uint32_t type = Field<uint32_t>(entry, kEntryTypeField);
				if (type == kFolderType || (type & kResourceBit) != 0)
					continue;
				if (Field<uint16_t>(entry, kEntryNameField) > 0 && Field<uint32_t>(entry, kEntryEncryptedField) != 0)
					SetField<uint32_t>(entry, kEntryEncryptedField, kEntryMark);
			}
			SetField<int32_t>(fields, kCurrentOffsetField, static_cast<int32_t>(kEntryMark));
		}

		// The game's parser, then the archive prepared. The tag is used up here, on every exit, so that only
		// a FindEncryption made for this archive counts (the game's ParseHeader makes that call before it
		// can succeed); the game's last error stands, whatever a log line does.
		bool ParseHeaderHook(void* packfile, const char* name, bool readHeader, void* customHeader)
		{
			const bool parsed = g_originalParseHeader(packfile, name, readHeader, customHeader);
			const uint32_t encryption = t_encryption;
			t_encryption = 0;
			if (!parsed || !packfile)
				return parsed;
			const DWORD lastError = GetLastError();
			try
			{
				PrepareArchive(packfile, name, encryption == kOpenTag);
			}
			catch (...)
			{
			}
			SetLastError(lastError);
			return parsed;
		}

		void HookArchives()
		{
			if (!IsOn(kOpenRpf))
			{
				WriteLog(kTag, "OPEN_RPF=0 - unencrypted (OPEN) RPF archives are not enabled.");
				return;
			}
			const auto fail = [](const std::string& why) { ReportFailure(kTag, "Unencrypted (OPEN) RPF archives cannot be loaded this session: " + why); };

			// 1. the five calls, found and checked before anything is written: each a call into the game
			struct ArchiveCall { const char* name; const GamePattern* pattern; size_t offset; const void* hook; void** original; };
			const ArchiveCall kCalls[kArchiveCalls] = {
				{ "FindEncryption call", &Pattern("R-E-E1"), 0, Code(&FindEncryptionHook), nullptr },
				{ "DecryptHeader call", &Pattern("R-E-E2"), 0, Code(&DecryptHeaderHook), Original(g_originalDecryptHeader) },
				{ "first DecryptHeader2 call", &Pattern("R-E-E3"), 0, Code(&DecryptHeader2Hook), Original(g_originalDecryptHeader2) },
				{ "second DecryptHeader2 call", &Pattern("R-E-E4"), 0, Code(&DecryptHeader2Hook), nullptr },
				{ "ParseHeader call", &Pattern("R-E-E5"), 19, Code(&ParseHeaderHook), Original(g_originalParseHeader) } };
			std::vector<CallRedirect> redirects;
			const uint8_t* targets[kArchiveCalls] = {};
			for (size_t i = 0; i < kArchiveCalls; i++)
			{
				const ArchiveCall& call = kCalls[i];
				const std::string label = call.pattern->label;
				uint8_t* const match = Scanner::Find(*call.pattern);
				if (!match)
				{
					fail(label + " was not found (the " + call.name + ").");
					return;
				}
				uint8_t* const site = match + call.offset;
				const uint8_t* const target = IsReadable(site, kCallSize) ? CallTarget(site) : nullptr;
				if (!target || !IsInsideGameImage(target))
				{
					fail("the " + std::string(call.name) + " at " + DescribeAddress(site) + " is not a call into the game (another mod may have "
						"hooked it already).");
					return;
				}
				if (g_diagnostic)
					WriteLog(kTag, "DIAGNOSTIC: the " + std::string(call.name) + " at " + DescribeAddress(site) + " -> " + DescribeAddress(target));
				targets[i] = target;
				redirects.push_back({ site, call.hook, call.name, call.original });
			}
			if (targets[kFirstDecryptHeader2] != targets[kSecondDecryptHeader2])
			{
				fail("the two DecryptHeader2 calls lead to different functions (" + DescribeAddress(targets[kFirstDecryptHeader2]) + " and " +
					DescribeAddress(targets[kSecondDecryptHeader2]) + ").");
				return;
			}
			if (IsDryRun())
			{
				WriteLog(kTag, "DRY_RUN: would redirect the 5 RPF encryption calls.");
				return;
			}

			// 2. the five calls, all or nothing, each original set before its call can reach a hook
			if (!RedirectCalls(redirects, {}, kTag))
			{
				fail("the calls could not be redirected (details in TweaksV.log).");
				return;
			}
			WriteLog(kTag, "Unencrypted (OPEN) RPF archives can be loaded (5 calls redirected).");
			g_archivesHooked = true;
		}

		// ---- the RPF cache check and the console

		void SkipCacheCheck()
		{
			if (!IsOn(kSkipRpfCacheCheck))
			{
				WriteLog(kTag, "SKIP_RPF_CACHE_CHECK=0 - the RPF cache check is not skipped.");
				return;
			}
			const auto fail = [](const std::string& why) { ReportFailure(kTag, "The RPF cache check is not skipped this session: " + why); };
			uint8_t* const call = Scanner::Find(Pattern("R-E-K1"));
			if (!call)
			{
				fail("R-E-K1 was not found.");
				return;
			}
			const uint8_t* const function = CallTarget(call);
			if (!function || !IsInsideGameImage(function) || !IsReadable(function, 2))
			{
				fail("the call at " + DescribeAddress(call) + " is not a call into the game (another mod may have hooked it already).");
				return;
			}
			std::string detail;
			if (!IsFunctionStart(function, detail))
			{
				fail("its function at " + DescribeAddress(function) + " is not a function start (" + detail + ").");
				return;
			}
			if (function[0] == 0xC3)
			{
				WriteLog(kTag, "The RPF cache check is already skipped (another mod did it) - left as it is.");
				return;
			}
			if (StartsWithJump(function))
			{
				WriteLog(kTag, "The RPF cache check's function is already hooked by another mod - left as it is.");
				return;
			}
			if (g_diagnostic)
				WriteLog(kTag, "DIAGNOSTIC: the RPF cache check's function is at " + DescribeAddress(function));
			const std::string before = HexBytes(function, 2);
			if (!WriteCode({ { const_cast<uint8_t*>(function), { 0xC3, 0x90 }, "RPF cache check" } }, kTag))
			{
				fail("the game code could not be changed (details in TweaksV.log).");
				return;
			}
			if (!IsDryRun())
				WriteLog(kTag, "RPF cache check patched: " + before + " -> C3 90.");
		}

		// With CONSOLE=1 the game must not hide the console TweaksV opened: the call that would is NOPed.
		// Not a problem for the start-up summary when it cannot be: the console is a testing aid.
		void KeepConsole()
		{
			if (!IsOn(kConsole))
				return;
			constexpr size_t kCallOffset = 16;
			constexpr size_t kCallSizeIndirect = 6;	// "call [rip+disp32]": FF 15 disp32
			uint8_t* const match = Scanner::Find(Pattern("R-E-C1"));
			if (!match)
			{
				WriteLog(kTag, "Console: R-E-C1 was not found - the game may hide the console.");
				return;
			}
			uint8_t* const call = match + kCallOffset;
			const std::vector<uint8_t> nops(kCallSizeIndirect, 0x90);
			const bool readable = IsReadable(call, kCallSizeIndirect);
			if (readable && memcmp(call, nops.data(), kCallSizeIndirect) == 0)
			{
				WriteLog(kTag, "Console hide call already patched (another mod did it) - left as it is.");
				return;
			}
			if (!readable || call[0] != 0xFF || call[1] != 0x15)
			{
				WriteLog(kTag, "Console: unexpected bytes at " + DescribeAddress(call) + " - left as they are, so the game may hide the console.");
				return;
			}
			const std::string before = HexBytes(call, kCallSizeIndirect);
			if (!WriteCode({ { call, nops, "console hide call" } }, kTag))
				WriteLog(kTag, "Console: the game code could not be changed - the game may hide the console.");
			else if (!IsDryRun())
				WriteLog(kTag, "Console hide call patched: " + before + " -> 90 90 90 90 90 90.");
		}

		// ---- the start-up, at the game's first time request

		std::atomic<bool> g_started{ false };
		void (WINAPI* g_originalGetSystemTimeAsFileTime)(LPFILETIME time) = nullptr;
		void* g_timeFunction = nullptr;		// GetSystemTimeAsFileTime itself, to switch the hook off again

		// Whether two folders are the same one: by their final paths, every link followed, so that two
		// spellings of a folder (a short name, a junction) are one - or by their text, without a separator
		// at the end, when a final path cannot be read.
		bool SameFolder(const std::wstring& a, const std::wstring& b)
		{
			const std::wstring finalA = FinalPathOf(a), finalB = FinalPathOf(b);
			if (!finalA.empty() && !finalB.empty())
				return _wcsicmp(finalA.c_str(), finalB.c_str()) == 0;
			const auto trimmed = [](std::wstring folder)
			{
				while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/'))
					folder.pop_back();
				return folder;
			};
			return _wcsicmp(trimmed(a).c_str(), trimmed(b).c_str()) == 0;
		}

		// The mods folder is looked for in the current directory, where the game's own file functions
		// resolve "mods/..." too. When that is not the game folder the log says so, and with MODS_FOLDER=1 a
		// mods folder in the game folder, which the game then does not use, is a problem.
		void CheckCurrentDirectory()
		{
			const DWORD size = GetCurrentDirectoryW(0, nullptr);	// the size Windows needs: a long directory too
			if (size == 0)
				return;
			std::wstring here(size, L'\0');
			const DWORD length = GetCurrentDirectoryW(size, here.data());
			if (length == 0 || length >= size)
				return;
			here.resize(length);
			const std::wstring& game = GetGameFolder();
			if (SameFolder(here, game))
				return;
			WriteLog(kTag, "The current directory is " + ToUtf8(here) + ", not the game folder - the mods folder is looked for there.");
			const DWORD attributes = GetFileAttributesW((game + L"\\mods").c_str());
			if (IsOn(kModsFolder) && attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				ReportFailure(kTag, "Files in the mods folder are not used this session: the game runs in " + ToUtf8(here) +
					", not in the game folder.");
		}

		void StartUp()
		{
			CheckCurrentDirectory();
			KeepConsole();
			SkipCacheCheck();
			HookModsFolder();
			HookArchives();
			if (!g_diagnostic || !(g_filesHooked || g_archivesHooked))
				return;
			WriteLog(kTag, "DIAGNOSTIC: every file request, every file served from the mods folder and every archive header read are logged, " +
				std::to_string(kMaxLinesPerKind) + " lines per kind at most; a summary follows after 10 seconds, then every minute while the "
				"numbers change.");
			const HANDLE thread = CreateThread(nullptr, 0, SummaryThread, nullptr, 0, nullptr);
			if (thread)
				CloseHandle(thread);
			else
				ReportFailure(kTag, "DIAGNOSTIC: the summary thread could not be started (" + DescribeError(GetLastError()) +
					") - the requests are counted but never summed up.");
		}

		// If the game never asks for the time through the hooked function, the start-up never runs, and with
		// it the start-up summary of every part: 30 seconds in, this thread - which holds a reference to
		// TweaksV.asi - claims the start-up, says so, shows the summary and switches the hook off.
		constexpr DWORD kStartUpWaitMs = 30000;

		DWORD WINAPI StartUpCheckThread(LPVOID module)
		{
			Sleep(kStartUpWaitMs);
			if (!g_started.load(std::memory_order_relaxed) && !g_started.exchange(true))
			{
				try	// nothing may leave a thread of this module
				{
					ReportFailure(kTag, "RageOpenV's start-up never ran: the game did not ask for the time through GetSystemTimeAsFileTime - the "
						"mods folder, OPEN archives and the cache check skip are off this session.");
					ShowStartupSummaryLater();
					DisableHook(g_timeFunction, kTag);
				}
				catch (...)
				{
				}
			}
			FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
		}

		// Runs the start-up once, on whichever thread first asks for the time; the claim is made before
		// the start-up runs, because the start-up can ask for the time itself.
		void WINAPI GetSystemTimeAsFileTimeHook(LPFILETIME time)
		{
			if (!g_started.load(std::memory_order_relaxed) && !g_started.exchange(true))
			{
				// nothing may leave this hook: it runs on one of the game's threads
				try
				{
					try
					{
						StartUp();
					}
					catch (...)
					{
						ReportFailure(kTag, "Unexpected exception during the start-up - the rest of RageOpenV is skipped this session.");
					}
					// one start-up summary for what DllMain and this start-up found - from a worker thread,
					// because this hook must not hold up the game's thread
					ShowStartupSummaryLater();
					// the hook's one job is done: the game's later calls go straight to Windows again
					DisableHook(g_timeFunction, kTag);
				}
				catch (...)
				{
				}
			}
			g_originalGetSystemTimeAsFileTime(time);
		}
	}

	bool InitRageOpenV()
	{
		g_diagnostic = IsDiagnosticMode();
		if (!HookExport(L"kernel32.dll", "GetSystemTimeAsFileTime", Code(&GetSystemTimeAsFileTimeHook),
			Original(g_originalGetSystemTimeAsFileTime), &g_timeFunction, kTag))
		{
			ReportFailure(kTag, "RageOpenV could not start: the GetSystemTimeAsFileTime hook failed (details in TweaksV.log). The mods folder, "
				"OPEN archives and the cache check skip are off this session.");
			return false;
		}
		StartModuleThread(StartUpCheckThread, "RageOpenV start-up check");	// one that cannot start is logged
		return true;
	}
}
