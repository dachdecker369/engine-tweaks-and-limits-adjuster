// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - DirectStorage: the files GTA V Enhanced keeps locked in its folder.
//
// With DirectStorage on, the game asks DirectStorage to open every file in its folder at start-up
// (IDStorageFactory::OpenFile, read-only, shared for reading only) and keeps them open, so mods can
// no longer write their own files there and scripts can no longer be reloaded while the game runs.
//   UNLOCK_FILES    on by default: in the game folder, DirectStorage may open only the .rpf and .cache
//                   files it reads; OpenFile refuses the folder's other files as not found, so they stay
//                   free. Files outside the game folder are never refused: the game reads its saves and
//                   settings (Documents\Rockstar Games\...\Profiles) through OpenFile, too. Whether a path
//                   is in the game folder is decided on its full path, as Windows makes it ("." and ".."
//                   collapsed), on the stack: no memory allocation and no lock of TweaksV's per call. A
//                   summary 10 seconds after the first call counts the files and lists the refused ones
//                   by type.
//   DIAGNOSTIC_MODE the game's calls into DirectStorage (DStorageSetConfiguration, ...1,
//                   DStorageGetFactory and the factory's OpenFile) with the code that made them, the
//                   factory's functions and the configuration values, and every open (NtCreateFile) of
//                   a file or folder inside the game folder: the access and sharing asked for, the
//                   result and the code that asked for it, plus a summary every minute. A path outside
//                   the game folder is logged as a category and its file name only, and every kind of
//                   line that comes per call stops after 20,000.
// Written for TweaksV; the idea comes from alloc8or's DirectStorageFix.

#include "TweaksVDirectStorage.h"

#include <windows.h>
#include <winternl.h>
#include <intrin.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "TweaksVIni.h"
#include "TweaksVLock.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;	// TweaksV.asi itself

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "DirectStorage";

		constexpr uint64_t kLinesPerWay = 2;		// open lines per file, caller and sharing; more opens are counted
		constexpr uint64_t kMaxLines = 20000;		// lines of one kind; then the events are only counted
		constexpr size_t kMaxTableEntries = 100000;	// entries per table of the open summary; more are only counted
		constexpr size_t kMaxTypesShown = 15;		// types a summary lists; the rest are "others"
		constexpr size_t kMaxPathChars = 2047;		// characters of an OpenFile path TweaksV checks
		constexpr DWORD kSummaryEveryMs = 60000;

		// The switches, read once: the hooks never read TweaksV.ini.
		bool g_unlockFiles = false;				// UNLOCK_FILES
		bool g_diagnostic = false;				// DIAGNOSTIC_MODE: the calls into DirectStorage and the opens are logged
		std::wstring g_gameFolder;				// without a trailing backslash
		std::wstring g_tempFolders[2];			// %TEMP% as Windows gives it and in its long form, without one either

		// This thread is inside one of the hooks below: whatever it opens there is not logged.
		thread_local bool t_inside = false;

		// What a hook does after the game's call - its lines, a hook of its own: not on a thread that is
		// inside a hook already, with the game's last error kept and every exception caught, so that
		// nothing throws into the game.
		template <typename Work> void AfterCall(Work&& work)
		{
			if (t_inside)
				return;
			const DWORD lastError = GetLastError();
			t_inside = true;
			try
			{
				work();
			}
			catch (...)
			{
			}
			t_inside = false;
			SetLastError(lastError);
		}

		// Counts a line of a kind that comes per call; true while the kind still gets lines of its own.
		// The first one past the cap says in its place that from now on they are only counted.
		bool Counted(std::atomic<uint64_t>& lines, const char* prefix, const char* kind)
		{
			const uint64_t n = lines.fetch_add(1, std::memory_order_relaxed) + 1;
			if (n <= kMaxLines)
				return true;
			if (n == kMaxLines + 1)
				WriteLog(kTag, prefix + std::to_string(kMaxLines) + " lines of " + kind + " written - from now on they are only counted.");
			return false;
		}

		// ---- describing code addresses

		struct ModuleRange
		{
			uintptr_t base = 0;
			uintptr_t end = 0;

			bool Contains(const void* p) const
			{
				const auto a = reinterpret_cast<uintptr_t>(p);
				return a >= base && a < end;
			}
		};

		ModuleRange g_game, g_self, g_ntdll, g_kernelBase, g_kernel32;
		SRWLOCK g_moduleLock = SRWLOCK_INIT;
		std::map<uintptr_t, std::string> g_moduleNames;	// start of a module -> its file name

		ModuleRange RangeOf(HMODULE module)
		{
			ModuleRange range;
			const auto base = reinterpret_cast<const uint8_t*>(module);
			if (!base)
				return range;
			const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			if (dos->e_magic != IMAGE_DOS_SIGNATURE)
				return range;
			const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE)
				return range;
			range.base = reinterpret_cast<uintptr_t>(base);
			range.end = range.base + nt->OptionalHeader.SizeOfImage;
			return range;
		}

		// "dstoragecore.dll" for an address inside that module, whose start goes to base; "" if the
		// address is in no module.
		std::string ModuleName(const void* address, uintptr_t& base)
		{
			HMODULE module = nullptr;
			if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				static_cast<LPCWSTR>(address), &module) || !module)
				return {};
			base = reinterpret_cast<uintptr_t>(module);

			{
				SharedLock lock(g_moduleLock);
				const auto known = g_moduleNames.find(base);
				if (known != g_moduleNames.end())
					return known->second;
			}

			wchar_t path[MAX_PATH]{};
			const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
			std::wstring_view file(path, length < MAX_PATH ? length : 0);
			const size_t slash = file.find_last_of(L"\\/");
			if (slash != std::wstring_view::npos)
				file.remove_prefix(slash + 1);
			const std::string name = file.empty() ? std::string("?") : ToUtf8(file);

			{
				ExclusiveLock lock(g_moduleLock);
				g_moduleNames.emplace(base, name);
			}
			return name;
		}

		// "RVA 0x1234" in the game, "dstoragecore.dll+0x1234 (function +0x1200)" in the DirectStorage
		// DLLs, "name.dll+0x1234" in another module, "0x7FF6..." outside every module.
		std::string DescribeCode(const void* address)
		{
			char text[96];
			const auto a = reinterpret_cast<uintptr_t>(address);
			if (g_game.Contains(address))
			{
				snprintf(text, sizeof(text), "RVA 0x%llX", static_cast<unsigned long long>(a - g_game.base));
				return text;
			}

			uintptr_t base = 0;
			const std::string name = ModuleName(address, base);
			if (name.empty())
			{
				snprintf(text, sizeof(text), "0x%llX", static_cast<unsigned long long>(a));
				return text;
			}
			snprintf(text, sizeof(text), "+0x%llX", static_cast<unsigned long long>(a - base));
			std::string result = name + text;
			if (_stricmp(name.c_str(), "dstorage.dll") == 0 || _stricmp(name.c_str(), "dstoragecore.dll") == 0)
			{
				DWORD64 imageBase = 0;
				const PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(a, &imageBase, nullptr);
				if (function && imageBase == base)
				{
					snprintf(text, sizeof(text), " (function +0x%lX)", static_cast<unsigned long>(function->BeginAddress));
					result += text;
				}
			}
			return result;
		}

		bool IsSystemCode(const void* address)
		{
			return g_ntdll.Contains(address) || g_kernelBase.Contains(address) || g_kernel32.Contains(address);
		}

		// The code that called: the first three frames of the call stack that are neither this hook
		// (TweaksV's frames at the top) nor ntdll, KernelBase or kernel32, most direct caller first:
		// "a <- b <- c". firstModule gets the module of the first one ("the game", "dstoragecore.dll").
		std::string DescribeCallers(std::string* firstModule)
		{
			void* frames[32];
			const WORD count = RtlCaptureStackBackTrace(0, static_cast<DWORD>(std::size(frames)), frames, nullptr);
			std::string text;
			int shown = 0;
			bool pastHook = false;
			for (WORD i = 0; i < count && shown < 3; i++)
			{
				if (!pastHook)
				{
					if (g_self.Contains(frames[i]))
						continue;
					pastHook = true;
				}
				if (IsSystemCode(frames[i]))
					continue;
				if (shown == 0 && firstModule)
				{
					uintptr_t base = 0;
					*firstModule = g_game.Contains(frames[i]) ? std::string("the game") : ModuleName(frames[i], base);
					if (firstModule->empty())
						*firstModule = "no module";
				}
				text += (shown ? " <- " : "") + DescribeCode(frames[i]);
				shown++;
			}
			if (shown == 0)
			{
				if (firstModule)
					*firstModule = "unknown";
				return "unknown";
			}
			return text;
		}

		// ---- reading the callers' memory: a bad pointer must not crash the game in TweaksV

		bool CopyMemorySafely(void* target, const void* source, size_t size)
		{
			__try
			{
				memcpy(target, source, size);
				return true;
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				return false;
			}
		}

		// The name an NtCreateFile call asked for (cut to capacity - 1 characters) and the folder
		// handle it is relative to, if any. False if the OBJECT_ATTRIBUTES cannot be read.
		bool CopyObjectName(const OBJECT_ATTRIBUTES* attributes, wchar_t* buffer, size_t capacity, size_t* length, HANDLE* root)
		{
			__try
			{
				if (!attributes || !attributes->ObjectName || !attributes->ObjectName->Buffer)
					return false;
				size_t count = attributes->ObjectName->Length / sizeof(wchar_t);
				if (count >= capacity)
					count = capacity - 1;
				memcpy(buffer, attributes->ObjectName->Buffer, count * sizeof(wchar_t));
				buffer[count] = L'\0';
				*length = count;
				*root = attributes->RootDirectory;
				return true;
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				return false;
			}
		}

		// ---- text helpers

		std::string HResultText(HRESULT result)
		{
			if (result == S_OK)
				return "ok";
			char text[16];
			snprintf(text, sizeof(text), "0x%08lX", static_cast<unsigned long>(result));
			return text;
		}

		std::string StatusText(NTSTATUS status)
		{
			if (status >= 0)
				return "ok";
			const char* name = "";
			switch (static_cast<uint32_t>(status))
			{
			case 0xC0000022: name = " access denied"; break;
			case 0xC0000034: name = " not found"; break;
			case 0xC0000035: name = " already exists"; break;
			case 0xC000003A: name = " path not found"; break;
			case 0xC0000043: name = " sharing violation"; break;
			case 0xC0000056: name = " delete pending"; break;
			case 0xC00000BA: name = " is a folder"; break;
			case 0xC0000103: name = " not a folder"; break;
			}
			char text[48];
			snprintf(text, sizeof(text), "0x%08lX%s", static_cast<unsigned long>(status), name);
			return text;
		}

		// "R--" = others may only read the file while it is open, "RWD" = read, write and delete.
		std::string ShareText(ULONG share)
		{
			std::string text = "---";
			if (share & FILE_SHARE_READ)
				text[0] = 'R';
			if (share & FILE_SHARE_WRITE)
				text[1] = 'W';
			if (share & FILE_SHARE_DELETE)
				text[2] = 'D';
			return text;
		}

		const char* DispositionText(ULONG disposition)
		{
			switch (disposition)
			{
			case FILE_SUPERSEDE: return "supersede";
			case FILE_OPEN: return "open";
			case FILE_CREATE: return "create";
			case FILE_OPEN_IF: return "open-if";
			case FILE_OVERWRITE: return "overwrite";
			case FILE_OVERWRITE_IF: return "overwrite-if";
			default: return "?";
			}
		}

		std::string GuidText(const GUID& guid)
		{
			char text[48];
			unsigned bytes[8];
			for (size_t i = 0; i < std::size(bytes); i++)
				bytes[i] = guid.Data4[i];
			snprintf(text, sizeof(text), "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
				static_cast<unsigned long>(guid.Data1), static_cast<unsigned>(guid.Data2), static_cast<unsigned>(guid.Data3),
				bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
			return text;
		}

		std::wstring Lower(std::wstring_view text)
		{
			std::wstring lower(text);
			for (wchar_t& c : lower)
				c = static_cast<wchar_t>(std::towlower(c));
			return lower;
		}

		// "1.2.2311.1405", "not found" or "unknown version"
		std::string FileVersionOf(const std::wstring& path)
		{
			DWORD unused = 0;
			const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &unused);
			if (size == 0)
				return GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES ? "not found" : "unknown version";
			std::vector<uint8_t> data(size);
			VS_FIXEDFILEINFO* info = nullptr;
			UINT length = 0;
			if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()) ||
				!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) || !info || length < sizeof(*info))
				return "unknown version";
			return std::to_string(HIWORD(info->dwFileVersionMS)) + "." + std::to_string(LOWORD(info->dwFileVersionMS)) + "." +
				std::to_string(HIWORD(info->dwFileVersionLS)) + "." + std::to_string(LOWORD(info->dwFileVersionLS));
		}

		// ---- the folders a path is checked against

		// The part of path below folder ("x64a.rpf", "update\update.rpf"), "." for the folder itself; false
		// if path is not inside folder (given without a trailing backslash) or folder is empty. Letters in
		// any case.
		bool InFolder(std::wstring_view path, std::wstring_view folder, std::wstring_view& relative)
		{
			const size_t n = folder.size();
			if (n == 0 || path.size() < n || _wcsnicmp(path.data(), folder.data(), n) != 0)
				return false;
			if (path.size() == n || (path.size() == n + 1 && path[n] == L'\\'))
			{
				relative = L".";
				return true;
			}
			if (path[n] != L'\\')
				return false;
			relative = path.substr(n + 1);
			return true;
		}

		bool InGameFolder(std::wstring_view path, std::wstring_view& relative)
		{
			return InFolder(path, g_gameFolder, relative);
		}

		bool InTempFolder(std::wstring_view path)
		{
			std::wstring_view relative;
			return InFolder(path, g_tempFolders[0], relative) || InFolder(path, g_tempFolders[1], relative);
		}

		// \\server\share\... - not a device path (\\.\, \\?\)
		bool IsNetworkPath(std::wstring_view path)
		{
			return path.size() > 2 && path[0] == L'\\' && path[1] == L'\\' && path[2] != L'.' && path[2] != L'?';
		}

		// ---- the file opens (DIAGNOSTIC_MODE)

		using NtCreateFileFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
			PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
		NtCreateFileFn g_originalNtCreateFile = nullptr;

		std::atomic<uint64_t> g_opens{ 0 };				// opens inside the game folder
		std::atomic<uint64_t> g_lines{ 0 };				// open lines written
		std::atomic<uint64_t> g_notShown{ 0 };			// opens that were only counted
		std::atomic<uint64_t> g_untracked{ 0 };			// opens a full table could not take
		std::atomic<uint64_t> g_noWriteSharing{ 0 };	// files opened while others may not write them
		std::atomic<bool> g_tablesFull{ false };		// said once
		SRWLOCK g_statsLock = SRWLOCK_INIT;
		// the summary's tables, kMaxTableEntries entries each at most
		std::unordered_map<std::wstring, uint64_t> g_ways;	// "path|caller|sharing" -> opens
		std::unordered_set<std::wstring> g_paths;		// every path opened, lower case
		std::map<std::string, uint64_t> g_byCaller;
		std::map<std::wstring, uint64_t> g_byType;		// ".rpf", "(folder)", "(none)"

		// The DOS path an NtCreateFile call opens: "\??\C:\x" becomes "C:\x", and a name relative to a
		// folder handle is put after that folder's path. Empty for everything else (devices, pipes).
		std::wstring FullPath(const wchar_t* name, size_t length, HANDLE root)
		{
			const std::wstring_view view(name, length);
			if (!root)
				return view.substr(0, 4) == L"\\??\\" ? std::wstring(view.substr(4)) : std::wstring();

			wchar_t folder[MAX_PATH * 2];
			const DWORD n = GetFinalPathNameByHandleW(root, folder, static_cast<DWORD>(std::size(folder)),
				FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
			if (n == 0 || n >= std::size(folder))
				return {};
			std::wstring path(folder, n);
			if (path.rfind(L"\\\\?\\UNC\\", 0) == 0)
				return {};
			if (path.rfind(L"\\\\?\\", 0) == 0)
				path.erase(0, 4);
			if (!view.empty())
			{
				if (path.empty() || path.back() != L'\\')
					path += L'\\';
				path.append(view);
			}
			return path;
		}

		std::wstring TypeOf(std::wstring_view lowerPath, bool folder)
		{
			if (folder)
				return L"(folder)";
			const size_t slash = lowerPath.find_last_of(L'\\');
			const std::wstring_view name = slash == std::wstring_view::npos ? lowerPath : lowerPath.substr(slash + 1);
			const size_t dot = name.find_last_of(L'.');
			if (dot == std::wstring_view::npos || dot == 0)
				return L"(none)";
			return std::wstring(name.substr(dot));
		}

		// Counts key in a table of the summary and returns its count; 0 for a new key the full table cannot take.
		template <typename Table, typename Key> uint64_t CountIn(Table& table, const Key& key)
		{
			const auto known = table.find(key);
			if (known != table.end())
				return ++known->second;
			if (table.size() >= kMaxTableEntries)
				return 0;
			table.emplace(key, 1);
			return 1;
		}

		void LogFileOpen(const OBJECT_ATTRIBUTES* attributes, ACCESS_MASK access, ULONG share, ULONG disposition,
			ULONG options, NTSTATUS status)
		{
			wchar_t name[1024];
			size_t length = 0;
			HANDLE root = nullptr;
			if (!CopyObjectName(attributes, name, std::size(name), &length, &root))
				return;
			const std::wstring path = FullPath(name, length, root);
			std::wstring_view relative;
			if (path.empty() || !InGameFolder(path, relative))
				return;

			const uint64_t number = ++g_opens;
			const bool folder = (options & FILE_DIRECTORY_FILE) != 0;
			if (status >= 0 && !folder && !(share & FILE_SHARE_WRITE))
				++g_noWriteSharing;
			std::string caller;
			const std::string callers = DescribeCallers(&caller);

			const std::wstring lower = Lower(relative);
			const std::wstring way = lower + L"|" + FromUtf8(caller) + L"|" + std::to_wstring(share & 7);
			const std::wstring type = TypeOf(lower, folder);
			uint64_t seen = 0;	// 0: the table of the ways of opening is full and this one is new
			bool tracked = true;
			{
				ExclusiveLock lock(g_statsLock);
				seen = CountIn(g_ways, way);
				const bool pathTracked = g_paths.contains(lower) || (g_paths.size() < kMaxTableEntries && g_paths.insert(lower).second);
				const bool callerTracked = CountIn(g_byCaller, caller) != 0;
				const bool typeTracked = CountIn(g_byType, type) != 0;
				tracked = seen != 0 && pathTracked && callerTracked && typeTracked;
			}
			if (!tracked)
			{
				++g_untracked;
				if (!g_tablesFull.exchange(true))
					WriteLog(kTag, "DIAGNOSTIC: a table of the open summary is full (" + std::to_string(kMaxTableEntries) +
						" entries) - the opens it cannot take are only counted (see the summaries).");
			}

			if (seen == 0 || seen > kLinesPerWay)
			{
				++g_notShown;
				return;
			}
			const uint64_t line = ++g_lines;
			if (line > kMaxLines)
			{
				++g_notShown;
				if (line == kMaxLines + 1)
					WriteLog(kTag, "DIAGNOSTIC: " + std::to_string(kMaxLines) + " open lines written - from now on "
						"the opens are only counted (see the summaries).");
				return;
			}

			char details[128];
			snprintf(details, sizeof(details), " | access 0x%08lX, share %s, %s, options 0x%08lX%s | from ",
				static_cast<unsigned long>(access), ShareText(share).c_str(), DispositionText(disposition),
				static_cast<unsigned long>(options), folder ? " (folder)" : "");
			WriteLog(kTag, "open #" + std::to_string(number) + ": " + ToUtf8(relative) + " | " + StatusText(status) +
				details + callers);
		}

		void LogSummary()
		{
			size_t paths = 0;
			std::vector<std::pair<std::string, uint64_t>> callers;
			std::vector<std::pair<std::wstring, uint64_t>> types;
			{
				SharedLock lock(g_statsLock);
				paths = g_paths.size();
				callers.assign(g_byCaller.begin(), g_byCaller.end());
				types.assign(g_byType.begin(), g_byType.end());
			}

			const auto byCount = [](const auto& a, const auto& b) { return a.second > b.second; };
			std::sort(callers.begin(), callers.end(), byCount);
			std::sort(types.begin(), types.end(), byCount);

			std::string text = "summary: " + std::to_string(g_opens.load()) + " opens of " + std::to_string(paths) +
				" files and folders in the game folder (" + std::to_string(g_notShown.load()) + " not shown above";
			if (const uint64_t untracked = g_untracked.load())
				text += ", " + std::to_string(untracked) + " not tracked: " + std::to_string(kMaxTableEntries) + " entries per table at most";
			text += "); " + std::to_string(g_noWriteSharing.load()) + " file opens kept others from writing the file | by caller:";
			for (size_t i = 0; i < callers.size(); i++)
				text += (i ? ", " : " ") + callers[i].first + " " + std::to_string(callers[i].second);
			text += " | by type:";
			uint64_t others = 0;
			for (size_t i = 0; i < types.size(); i++)
			{
				if (i < kMaxTypesShown)
					text += (i ? ", " : " ") + ToUtf8(types[i].first) + " " + std::to_string(types[i].second);
				else
					others += types[i].second;
			}
			if (others)
				text += ", others " + std::to_string(others);
			WriteLog(kTag, text);
		}

		// The open summary, every minute while the count changes. The thread keeps its reference to
		// TweaksV.asi: it never ends.
		DWORD WINAPI SummaryThread(LPVOID)
		{
			uint64_t last = 0;
			for (;;)
			{
				Sleep(kSummaryEveryMs);
				const uint64_t opens = g_opens.load();
				if (opens == last)
					continue;
				last = opens;
				try
				{
					LogSummary();
				}
				catch (...)
				{
				}
			}
		}

		NTSTATUS NTAPI HookNtCreateFile(PHANDLE fileHandle, ACCESS_MASK access, POBJECT_ATTRIBUTES attributes,
			PIO_STATUS_BLOCK ioStatus, PLARGE_INTEGER allocationSize, ULONG fileAttributes, ULONG share,
			ULONG disposition, ULONG options, PVOID eaBuffer, ULONG eaLength)
		{
			const NTSTATUS status = g_originalNtCreateFile(fileHandle, access, attributes, ioStatus, allocationSize,
				fileAttributes, share, disposition, options, eaBuffer, eaLength);
			// STATUS_ACCESS_VIOLATION: the caller's arguments could not even be read by the system
			if (status != static_cast<NTSTATUS>(STATUS_ACCESS_VIOLATION))
				AfterCall([&] { LogFileOpen(attributes, access, share, disposition, options, status); });
			return status;
		}

		// ---- UNLOCK_FILES: the factory's OpenFile

		// IDStorageFactory, whose function [4] is OpenFile (after IUnknown's 3 and CreateQueue)
		constexpr GUID kFactoryId = { 0x6924EA0C, 0xC3CD, 0x4826, { 0xB1, 0x0A, 0xF6, 0x4F, 0x4E, 0xD9, 0x27, 0xC1 } };
		constexpr size_t kOpenFileSlot = 4;
		constexpr DWORD kOpenFileSummaryMs = 10000;	// the game opens its files right after creating the factory
		constexpr DWORD kInterfaceCheckMs = 10000;	// another interface asked for first: the time the known one has to follow
		constexpr DWORD kFactoryCheckMs = 30000;	// no factory asked for by then: TweaksV may have been loaded too late

		using OpenFileFn = HRESULT(STDMETHODCALLTYPE*)(void* factory, const WCHAR* path, REFIID riid, void** ppv);
		OpenFileFn g_originalOpenFile = nullptr;
		SRWLOCK g_openFileLock = SRWLOCK_INIT;
		bool g_openFileHookTried = false;		// under g_openFileLock: OpenFile's hook was tried, made or not
		bool g_interfaceCheckStarted = false;	// under g_openFileLock: another interface came before it
		GUID g_otherInterface{};				// under g_openFileLock: that interface
		bool g_otherInterfaceLogged = false;	// under g_openFileLock
		std::atomic<bool> g_factoryAsked{ false };
		std::atomic<uint64_t> g_openFileCalls{ 0 };
		// each OpenFile call in exactly one of these, so that the summary's figures add up
		std::atomic<uint64_t> g_archives{ 0 };		// .rpf and .cache files in the game folder: opened
		std::atomic<uint64_t> g_refused{ 0 };		// the folder's other files, with UNLOCK_FILES: refused
		std::atomic<uint64_t> g_otherInside{ 0 };	// the folder's other files, with UNLOCK_FILES=0: opened
		std::atomic<uint64_t> g_outside{ 0 };		// paths outside the game folder: never refused
		std::atomic<uint64_t> g_unreadable{ 0 };	// paths that could not be read or made full: passed on unchecked
		std::atomic<uint64_t> g_openFileLines{ 0 };	// DIAGNOSTIC_MODE: OpenFile lines written
		std::atomic<uint64_t> g_lateLines{ 0 };		// lines of refusals after the summary written
		std::atomic<bool> g_openFileSummaryDone{ false };

		// True for a path that ends in .rpf or .cache, in any case: the files DirectStorage reads.
		bool IsArchive(std::wstring_view path)
		{
			const size_t slash = path.find_last_of(L"\\/");
			const std::wstring_view name = slash == std::wstring_view::npos ? path : path.substr(slash + 1);
			const size_t dot = name.find_last_of(L'.');
			if (dot == std::wstring_view::npos)
				return false;
			const std::wstring_view extension = name.substr(dot);
			return (extension.size() == 4 && _wcsnicmp(extension.data(), L".rpf", 4) == 0) ||
				(extension.size() == 6 && _wcsnicmp(extension.data(), L".cache", 6) == 0);
		}

		// Copies a path the game passed (at most capacity - 1 characters); complete is false if it was
		// longer. False if it cannot be read.
		bool CopyPathSafely(const wchar_t* path, wchar_t* buffer, size_t capacity, size_t* length, bool* complete)
		{
			__try
			{
				size_t n = 0;
				while (n + 1 < capacity && path[n] != L'\0')
				{
					buffer[n] = path[n];
					n++;
				}
				buffer[n] = L'\0';
				*length = n;
				*complete = path[n] == L'\0';
				return true;
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				return false;
			}
		}

		// An OpenFile path as DirectStorage opens it, worked out on the stack before the game's call - no
		// memory allocation, nothing that can throw: copied (at most kMaxPathChars characters), '/' made '\'
		// in the copy only (the game's path is passed on as it is), then made full - a "\\?\UNC\server\share"
		// path, the prefix in any case, as "\\server\share", any other "\\?\" path without the prefix and as
		// it is, as Windows takes it, every other path through GetFullPathNameW, which makes it absolute from
		// the current directory, as DirectStorage does, and collapses "." and "..".
		struct OpenFilePath
		{
			wchar_t full[kMaxPathChars + 1];
			size_t length = 0;
			bool readable = false;	// false: it could not be read or made full, and is passed on unchecked
			bool inside = false;	// in the game folder
			bool archive = false;	// ends in .rpf or .cache

			std::wstring_view Full() const { return { full, length }; }
		};

		void ReadOpenFilePath(const wchar_t* path, OpenFilePath& result)
		{
			wchar_t copy[kMaxPathChars + 1];
			size_t length = 0;
			bool complete = false;
			if (!path || !CopyPathSafely(path, copy, std::size(copy), &length, &complete) || !complete || length > kMaxPathChars)
				return;
			for (size_t i = 0; i < length; i++)
				if (copy[i] == L'/')
					copy[i] = L'\\';
			if (length >= 8 && _wcsnicmp(copy, L"\\\\?\\UNC\\", 8) == 0)
			{
				result.full[0] = L'\\';
				result.full[1] = L'\\';
				wmemcpy(result.full + 2, copy + 8, length - 8);
				result.length = length - 6;
			}
			else if (length >= 4 && wmemcmp(copy, L"\\\\?\\", 4) == 0)
			{
				wmemcpy(result.full, copy + 4, length - 4);
				result.length = length - 4;
			}
			else
			{
				const DWORD n = GetFullPathNameW(copy, static_cast<DWORD>(std::size(result.full)), result.full, nullptr);
				if (n == 0 || n >= std::size(result.full))
					return;
				result.length = n;
			}
			result.full[result.length] = L'\0';
			std::wstring_view relative;
			result.readable = true;
			result.inside = InGameFolder(result.Full(), relative);
			result.archive = IsArchive(result.Full());
		}

		// The name of an OpenFile path in a line: below the game folder for a path inside it ("x64a.rpf"),
		// a category and the file name only for any other - "(outside the game folder) SGTA50000", "(temp)
		// cloud_tweaksv.dat", "(network) cloud.dat" - so that no other folder, a profile's say, reaches the log.
		std::string LogName(const OpenFilePath& path)
		{
			if (!path.readable)
				return "(unreadable path)";
			const std::wstring_view full = path.Full();
			std::wstring_view relative;
			if (InGameFolder(full, relative))
				return ToUtf8(relative);
			const size_t slash = full.find_last_of(L'\\');
			const std::wstring_view name = slash == std::wstring_view::npos ? full : full.substr(slash + 1);
			const char* const category = IsNetworkPath(full) ? "(network) " : InTempFolder(full) ? "(temp) " : "(outside the game folder) ";
			return category + ToUtf8(name);
		}

		// The refused files by type, for the summary: a fixed table that the game's threads fill without a
		// lock or an allocation, in the order the types come. A type is the extension with its dot, in lower
		// case, packed into 8 bytes; one that does not fit - longer, or not plain ASCII - and every type past
		// the table's end counts as "others".
		constexpr uint64_t kNoExtension = 1;	// "(none)": a packed extension starts with its dot, so it is never 1
		struct TypeCount
		{
			std::atomic<uint64_t> type{ 0 };	// 0: free
			std::atomic<uint64_t> count{ 0 };
		};
		TypeCount g_refusedTypes[kMaxTypesShown];
		std::atomic<uint64_t> g_refusedOthers{ 0 };

		uint64_t PackedType(std::wstring_view path)
		{
			const size_t slash = path.find_last_of(L'\\');
			const std::wstring_view name = slash == std::wstring_view::npos ? path : path.substr(slash + 1);
			const size_t dot = name.find_last_of(L'.');
			if (dot == std::wstring_view::npos || dot == 0)
				return kNoExtension;
			const std::wstring_view extension = name.substr(dot);
			if (extension.size() > sizeof(uint64_t))
				return 0;
			uint64_t packed = 0;
			for (size_t i = 0; i < extension.size(); i++)
			{
				wchar_t c = extension[i];
				if (c >= L'A' && c <= L'Z')
					c = static_cast<wchar_t>(c - L'A' + L'a');
				if (c <= L' ' || c > L'~')
					return 0;
				packed |= static_cast<uint64_t>(c) << (8 * i);
			}
			return packed;
		}

		void CountRefusedType(uint64_t type)
		{
			if (type != 0)
				for (TypeCount& entry : g_refusedTypes)
				{
					uint64_t current = entry.type.load(std::memory_order_acquire);
					if (current == 0 && entry.type.compare_exchange_strong(current, type, std::memory_order_acq_rel))
						current = type;	// taken for this type; on a race, current is what the other thread put there
					if (current == type)
					{
						entry.count.fetch_add(1, std::memory_order_relaxed);
						return;
					}
				}
			g_refusedOthers.fetch_add(1, std::memory_order_relaxed);
		}

		// ".txt 4, .asi 1, (none) 1, others 2"
		std::string RefusedTypesText()
		{
			std::string text;
			for (const TypeCount& entry : g_refusedTypes)
			{
				const uint64_t type = entry.type.load(std::memory_order_acquire), count = entry.count.load();
				if (type == 0 || count == 0)
					continue;
				std::string name = "(none)";
				if (type != kNoExtension)
				{
					name.clear();
					for (uint64_t rest = type; rest; rest >>= 8)
						name += static_cast<char>(rest & 0xFF);
				}
				text += (text.empty() ? "" : ", ") + name + " " + std::to_string(count);
			}
			if (const uint64_t others = g_refusedOthers.load())
				text += (text.empty() ? "" : ", ") + std::string("others ") + std::to_string(others);
			return text;
		}

		void LogOpenFileSummary()
		{
			// each figure read once, and the calls their sum, so the figures add up whatever comes meanwhile
			const uint64_t archives = g_archives.load(), refused = g_refused.load(), otherInside = g_otherInside.load(),
				outside = g_outside.load(), unreadable = g_unreadable.load();
			const uint64_t calls = archives + refused + otherInside + outside + unreadable;
			const std::string seconds = std::to_string(kOpenFileSummaryMs / 1000);
			const std::string unread = unreadable ? ", " + std::to_string(unreadable) + " with an unreadable path passed on" : "";
			if (g_unlockFiles)
				WriteLog(kTag, "UNLOCK_FILES: in its first " + seconds + " seconds the game asked DirectStorage for " +
					std::to_string(calls) + " files: " + std::to_string(archives) + " .rpf and .cache files were opened, " +
					std::to_string(refused) + " other files in the game folder refused, so they stay free for other programs" +
					(outside ? ", " + std::to_string(outside) + " outside the game folder opened as usual" : "") + unread +
					(refused ? "; refused by type: " + RefusedTypesText() : "") + ".");
			else
				WriteLog(kTag, "OpenFile: in its first " + seconds + " seconds the game asked DirectStorage for " +
					std::to_string(calls) + " files: " + std::to_string(archives) + " .rpf and .cache files, " +
					std::to_string(otherInside) + " other files in the game folder and " + std::to_string(outside) +
					" outside it" + unread + " (UNLOCK_FILES=0 - all of them opened).");
		}

		DWORD WINAPI OpenFileSummaryThread(LPVOID module)
		{
			Sleep(kOpenFileSummaryMs);
			try
			{
				LogOpenFileSummary();
			}
			catch (...)
			{
			}
			g_openFileSummaryDone = true;
			FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
		}

		// The summary 10 seconds after the first call. Without it every later refusal gets a line of its own,
		// and with UNLOCK_FILES the part's status line is missing - a problem.
		void StartOpenFileSummary()
		{
			if (StartModuleThread(OpenFileSummaryThread, "OpenFile summary"))
				return;
			g_openFileSummaryDone = true;
			if (g_unlockFiles)
				ReportFailure(kTag, "The UNLOCK_FILES summary is not written this session: its thread could not be started (details in "
					"TweaksV.log).");
		}

		// DIAGNOSTIC_MODE: every call; otherwise only the refusals after the summary. Each kind stops after
		// kMaxLines lines; the name is made only for a line that is written.
		void LogOpenFile(uint64_t number, const OpenFilePath& path, bool refused, HRESULT result)
		{
			if (g_diagnostic)
			{
				if (!Counted(g_openFileLines, "DIAGNOSTIC: ", "OpenFile calls"))
					return;
				std::string text = "OpenFile #" + std::to_string(number) + ": " + LogName(path) + " -> " +
					(refused ? std::string("refused (in the game folder, not .rpf or .cache)") : HResultText(result));
				if (number == 1)
					text += ", from " + DescribeCallers(nullptr);
				WriteLog(kTag, text);
			}
			else if (refused && g_openFileSummaryDone.load() && Counted(g_lateLines, "UNLOCK_FILES: ", "files refused after the summary"))
			{
				WriteLog(kTag, "UNLOCK_FILES: refused " + LogName(path) + " (in the game folder, not .rpf or .cache).");
			}
		}

		HRESULT STDMETHODCALLTYPE HookOpenFile(void* factory, const WCHAR* path, REFIID riid, void** ppv)
		{
			OpenFilePath checked;
			ReadOpenFilePath(path, checked);
			const bool refuse = g_unlockFiles && checked.inside && !checked.archive;	// never outside the game folder: saves, settings

			HRESULT result = S_OK;
			if (refuse)
			{
				if (ppv)
					*ppv = nullptr;
				result = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);	// as for a file that is not there
			}
			else
			{
				result = g_originalOpenFile(factory, path, riid, ppv);
			}

			const uint64_t number = ++g_openFileCalls;
			if (!checked.readable)
				++g_unreadable;
			else if (!checked.inside)
				++g_outside;
			else if (checked.archive)
				++g_archives;
			else if (refuse)
			{
				++g_refused;
				CountRefusedType(PackedType(checked.Full()));
			}
			else
				++g_otherInside;
			AfterCall([&]
			{
				if (number == 1)
					StartOpenFileSummary();
				LogOpenFile(number, checked, refuse, result);
			});
			return result;
		}

		// A failure of the OpenFile hook: with UNLOCK_FILES a problem, for DIAGNOSTIC_MODE alone a log line.
		void FailOpenFile(const std::string& why)
		{
			if (g_unlockFiles)
				ReportFailure(kTag, "The files in the game folder are not unlocked this session: " + why + ".");
			else
				WriteLog(kTag, "DIAGNOSTIC: OpenFile is not logged: " + why + ".");
		}

		// Under g_openFileLock: another interface next to the one TweaksV knows, said once a session.
		void LogOtherInterface(const GUID& riid)
		{
			if (g_otherInterfaceLogged)
				return;
			g_otherInterfaceLogged = true;
			WriteLog(kTag, "The game also asked for another DirectStorage interface, " + GuidText(riid) +
				", whose functions TweaksV does not know - left as it is.");
		}

		// The game asked for another interface before the one TweaksV knows: 10 seconds later, a hook made or
		// tried means the other one is a line; none means the game asked only for that one.
		DWORD WINAPI InterfaceCheckThread(LPVOID module)
		{
			Sleep(kInterfaceCheckMs);
			try
			{
				const ExclusiveLock lock(g_openFileLock);
				if (g_openFileHookTried)
					LogOtherInterface(g_otherInterface);
				else
					FailOpenFile("the game asked only for another DirectStorage interface, " + GuidText(g_otherInterface) +
						", whose functions TweaksV does not know");
			}
			catch (...)
			{
			}
			FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
		}

		// The factory's function table read and its function [4], OpenFile, hooked; it must lie in
		// DirectStorage's DLLs.
		void HookOpenFileOf(void** ppv)
		{
			void* factory = nullptr;
			void* table = nullptr;
			void* target = nullptr;
			if (!CopyMemorySafely(&factory, ppv, sizeof(factory)) || !factory ||
				!CopyMemorySafely(&table, factory, sizeof(table)) || !table ||
				!CopyMemorySafely(&target, static_cast<void**>(table) + kOpenFileSlot, sizeof(target)) || !target)
			{
				FailOpenFile("the factory's function table could not be read");
				return;
			}
			uintptr_t base = 0;
			const std::string module = ModuleName(target, base);
			if (_stricmp(module.c_str(), "dstoragecore.dll") != 0 && _stricmp(module.c_str(), "dstorage.dll") != 0)
			{
				FailOpenFile("the factory's OpenFile is not in DirectStorage's DLLs (" + DescribeCode(target) + ")");
				return;
			}

			if (!HookFunctions({ { target, reinterpret_cast<const void*>(&HookOpenFile), reinterpret_cast<void**>(&g_originalOpenFile), "OpenFile" } }, kTag))
				FailOpenFile("OpenFile (" + DescribeCode(target) + ") could not be hooked (details in TweaksV.log)");
			else if (g_unlockFiles)
				WriteLog(kTag, "UNLOCK_FILES: from now on DirectStorage may open only the .rpf and .cache files in the game "
					"folder (OpenFile at " + DescribeCode(target) + " hooked); files outside it are not touched.");
			else
				WriteLog(kTag, "DIAGNOSTIC: the game's OpenFile calls are logged (OpenFile at " + DescribeCode(target) +
					" hooked); UNLOCK_FILES=0, so every file is opened.");
		}

		// The game got a factory for riid. For the interface TweaksV knows, OpenFile is hooked once, before the
		// game gets the factory back, so the hook sees its first OpenFile already; another interface asked for
		// before that is checked again 10 seconds later, one asked for afterwards is a line.
		void HookFactoryOpenFile(REFIID riid, void** ppv)
		{
			const ExclusiveLock lock(g_openFileLock);	// a second caller waits until OpenFile is hooked
			const bool known = IsEqualGUID(riid, kFactoryId) != 0;
			if (g_openFileHookTried)
			{
				if (!known)
					LogOtherInterface(riid);
				return;
			}
			if (!known)
			{
				if (g_interfaceCheckStarted)
					return;
				g_interfaceCheckStarted = true;
				g_otherInterface = riid;
				if (!StartModuleThread(InterfaceCheckThread, "DirectStorage interface check") && g_unlockFiles)
					ReportFailure(kTag, "The check of the other DirectStorage interface the game asked for, " + GuidText(riid) +
						", is not done this session: its thread could not be started (details in TweaksV.log).");
				return;
			}

			// made before the attempt, so that reporting an exception in it needs no new text
			const std::string unexpected = g_unlockFiles
				? "The files in the game folder are not unlocked this session: OpenFile could not be hooked (unexpected exception)."
				: "DIAGNOSTIC: OpenFile is not logged: OpenFile could not be hooked (unexpected exception).";
			try
			{
				HookOpenFileOf(ppv);
			}
			catch (...)
			{
				if (g_unlockFiles)
					ReportFailure(kTag, unexpected);
				else
					WriteLog(kTag, unexpected);
			}
			g_openFileHookTried = true;
		}

		// ---- the factory and the configuration (DIAGNOSTIC_MODE): the calls into DirectStorage

		using GetFactoryFn = HRESULT(WINAPI*)(REFIID riid, void** ppv);
		using SetConfigurationFn = HRESULT(WINAPI*)(const void* configuration);
		GetFactoryFn g_originalGetFactory = nullptr;
		std::atomic<int> g_factoryCalls{ 0 };

		// The two configuration functions, one hook for both: as many values are logged as the
		// configuration has - DSTORAGE_CONFIGURATION 7, DSTORAGE_CONFIGURATION1 8.
		struct ConfigurationFunction
		{
			const char* name;
			size_t values;
			SetConfigurationFn original;
		};
		ConfigurationFunction g_configurationFunctions[] = { { "DStorageSetConfiguration", 7, nullptr },
			{ "DStorageSetConfiguration1", 8, nullptr } };

		// DIAGNOSTIC_MODE: where the factory's first 8 functions are (IUnknown's 3, then its own)
		void LogFactoryFunctions(void** ppv)
		{
			void* factory = nullptr;
			void* table = nullptr;
			void* functions[8]{};
			if (!CopyMemorySafely(&factory, ppv, sizeof(factory)) || !factory ||
				!CopyMemorySafely(&table, factory, sizeof(table)) || !table ||
				!CopyMemorySafely(functions, table, sizeof(functions)))
			{
				WriteLog(kTag, "DIAGNOSTIC: the factory's function table could not be read.");
				return;
			}
			std::string text = "DIAGNOSTIC: the factory's first 8 functions:";
			for (size_t i = 0; i < std::size(functions); i++)
				text += " [" + std::to_string(i) + "] " + DescribeCode(functions[i]);
			WriteLog(kTag, text);
		}

		HRESULT WINAPI HookGetFactory(REFIID riid, void** ppv)
		{
			g_factoryAsked.store(true, std::memory_order_relaxed);
			const HRESULT result = g_originalGetFactory(riid, ppv);
			AfterCall([&]
			{
				if (g_diagnostic)
				{
					WriteLog(kTag, "the game asked for the DirectStorage factory (call #" + std::to_string(++g_factoryCalls) +
						", interface " + GuidText(riid) + ") -> " + HResultText(result) + ", from " + DescribeCallers(nullptr));
					if (SUCCEEDED(result) && ppv)
						LogFactoryFunctions(ppv);
				}
				if (SUCCEEDED(result) && ppv)
					HookFactoryOpenFile(riid, ppv);
			});
			return result;
		}

		// The call, and with it the values the configuration holds (count of them).
		void LogSetConfiguration(const char* function, const void* configuration, size_t count, HRESULT result)
		{
			WriteLog(kTag, std::string("the game set the DirectStorage configuration (") + function + ") -> " +
				HResultText(result) + ", from " + DescribeCallers(nullptr));
			uint32_t values[8]{};
			if (!configuration || count > std::size(values) || !CopyMemorySafely(values, configuration, count * sizeof(values[0])))
				return;
			std::string text = std::string("DIAGNOSTIC: ") + function + " values:";
			for (size_t i = 0; i < count; i++)
			{
				char value[16];
				snprintf(value, sizeof(value), " %08X", values[i]);
				text += value;
			}
			WriteLog(kTag, text);
		}

		template <size_t Index> HRESULT WINAPI HookSetConfiguration(const void* configuration)
		{
			const ConfigurationFunction& function = g_configurationFunctions[Index];
			const HRESULT result = function.original(configuration);
			AfterCall([&] { LogSetConfiguration(function.name, configuration, function.values, result); });
			return result;
		}

		// UNLOCK_FILES: a factory the game has not asked for 30 seconds after TweaksV started may have been
		// asked for before TweaksV was loaded - a log line, not a problem.
		DWORD WINAPI FactoryCheckThread(LPVOID module)
		{
			Sleep(kFactoryCheckMs);
			try
			{
				if (!g_factoryAsked.load())
					WriteLog(kTag, "The game has not asked for its DirectStorage factory since TweaksV started - nothing is unlocked; TweaksV "
						"may have been loaded too late.");
			}
			catch (...)
			{
			}
			FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
		}

		// ---- installing

		bool InstallHook(const wchar_t* module, const char* function, LPVOID detour, LPVOID* original)
		{
			return HookExport(module, function, detour, original, nullptr, kTag);
		}

		// DirectStorageFix.asi in a folder an ASI loader loads from, or empty
		std::wstring FindDirectStorageFix()
		{
			for (const wchar_t* folder : { L"", L"\\scripts", L"\\plugins" })
			{
				const std::wstring path = g_gameFolder + folder + L"\\DirectStorageFix.asi";
				const DWORD attributes = GetFileAttributesW(path.c_str());
				if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY))
					return path;
			}
			return {};
		}

		// %TEMP%, for the category of a path in a line: as GetTempPathW gives it and in its long form, each
		// without its trailing backslash; empty where Windows cannot give it.
		void ReadTempFolders()
		{
			wchar_t temp[MAX_PATH + 1];
			const DWORD length = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
			if (length == 0 || length >= std::size(temp))
				return;
			std::wstring folder(temp, length);
			while (!folder.empty() && folder.back() == L'\\')
				folder.pop_back();
			g_tempFolders[0] = folder;
			wchar_t longForm[MAX_PATH * 2];
			const DWORD longLength = GetLongPathNameW(folder.c_str(), longForm, static_cast<DWORD>(std::size(longForm)));
			if (longLength > 0 && longLength < std::size(longForm))
				g_tempFolders[1].assign(longForm, longLength);
		}
	}

	void InitDirectStorage()
	{
		const bool unlock = IsOn(kUnlockFiles);
		const bool diagnostic = IsDiagnosticMode();	// the calls into DirectStorage and the file opens are logged
		if (!unlock && !diagnostic)
		{
			WriteLog(kTag, "UNLOCK_FILES=0 and DIAGNOSTIC_MODE=0 - nothing to do.");
			return;
		}

		g_unlockFiles = unlock;
		g_diagnostic = diagnostic;
		g_gameFolder = GetGameFolder();
		ReadTempFolders();
		const bool loaded = GetModuleHandleW(L"dstorage.dll") != nullptr;
		WriteLog(kTag, "dstorage.dll " + FileVersionOf(g_gameFolder + L"\\dstorage.dll") + ", dstoragecore.dll " +
			FileVersionOf(g_gameFolder + L"\\dstoragecore.dll") + "; dstorage.dll is " + (loaded ? "loaded" : "not loaded") + ".");
		const std::wstring fix = FindDirectStorageFix();	// only with CONFLICT_CHECK=0: otherwise this part is off
		if (!fix.empty())
			WriteLog(kTag, "Note: " + ToUtf8(fix) + " is installed as well - it changes DirectStorage's file opens, too.");

		if (IsDryRun())
		{
			if (unlock)
				WriteLog(kTag, "DRY_RUN: would let DirectStorage open only .rpf and .cache files (UNLOCK_FILES).");
			if (diagnostic)
			{
				WriteLog(kTag, "DRY_RUN: would hook dstorage.dll to log the game's DirectStorage calls.");
				WriteLog(kTag, "DRY_RUN: would hook NtCreateFile to log the opens in the game folder.");
			}
			return;
		}

		// before the hooks: they use these from the first call on
		g_game = RangeOf(GetModuleHandleW(nullptr));
		g_self = RangeOf(reinterpret_cast<HMODULE>(&__ImageBase));
		g_ntdll = RangeOf(GetModuleHandleW(L"ntdll.dll"));
		g_kernelBase = RangeOf(GetModuleHandleW(L"KernelBase.dll"));
		g_kernel32 = RangeOf(GetModuleHandleW(L"kernel32.dll"));

		if (!loaded)
		{
			WriteLog(kTag, std::string("dstorage.dll is not loaded, so the game does not use DirectStorage here: there is ") +
				"nothing to " + (unlock ? "unlock." : "log."));
		}
		else
		{
			// the factory's OpenFile is hooked when the game creates the factory (HookFactoryOpenFile)
			const bool factory = InstallHook(L"dstorage.dll", "DStorageGetFactory", reinterpret_cast<LPVOID>(&HookGetFactory),
				reinterpret_cast<LPVOID*>(&g_originalGetFactory));
			if (diagnostic)
			{
				int hooked = factory ? 1 : 0;
				hooked += InstallHook(L"dstorage.dll", g_configurationFunctions[0].name, reinterpret_cast<LPVOID>(&HookSetConfiguration<0>),
					reinterpret_cast<LPVOID*>(&g_configurationFunctions[0].original)) ? 1 : 0;
				hooked += InstallHook(L"dstorage.dll", g_configurationFunctions[1].name, reinterpret_cast<LPVOID>(&HookSetConfiguration<1>),
					reinterpret_cast<LPVOID*>(&g_configurationFunctions[1].original)) ? 1 : 0;
				WriteLog(kTag, "DIAGNOSTIC: the game's DirectStorage calls are logged (" + std::to_string(hooked) +
					" of 3 functions hooked).");
			}
			if (unlock && factory)
			{
				WriteLog(kTag, "UNLOCK_FILES: when the game creates its DirectStorage factory, OpenFile is hooked so that "
					"in the game folder only .rpf and .cache files can be opened.");
				StartModuleThread(FactoryCheckThread, "DirectStorage factory check");	// one that cannot start is logged
			}
			else if (unlock)
				ReportFailure(kTag, "The files in the game folder are not unlocked this session: DStorageGetFactory could "
					"not be hooked (details in TweaksV.log).");
		}

		if (diagnostic && InstallHook(L"ntdll.dll", "NtCreateFile", reinterpret_cast<LPVOID>(&HookNtCreateFile),
			reinterpret_cast<LPVOID*>(&g_originalNtCreateFile)))
		{
			WriteLog(kTag, "DIAGNOSTIC: every open of a file or folder in " + ToUtf8(g_gameFolder) + " is logged, " +
				std::to_string(kLinesPerWay) + " lines per file, caller and sharing; a summary follows every minute.");
			StartModuleThread(SummaryThread, "DirectStorage open summary");	// one that cannot start is logged
		}
	}
}
