// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - DirectStorage: the files GTA V Enhanced keeps locked in its folder.
//
// With DirectStorage on, the game asks DirectStorage to open every file in its folder at start-up
// (IDStorageFactory::OpenFile, read-only, shared for reading only) and keeps them open, so mods can
// no longer write their own files there and scripts can no longer be reloaded while the game runs.
//   UNLOCK_FILES    on by default: in the game folder, DirectStorage may open only the .rpf and .cache
//                   files it reads; OpenFile refuses the folder's other files as not found, so they stay
//                   free. Files outside the game folder are never refused: the game reads its saves and
//                   settings (Documents\Rockstar Games\...\Profiles) through OpenFile, too.
//   LOG_CALLS       testing: the game's calls into DirectStorage (DStorageSetConfiguration, ...1,
//                   DStorageGetFactory and the factory's OpenFile) and the code that made them
//   LOG_FILE_OPENS  testing: every open (NtCreateFile) of a file or folder inside the game folder:
//                   the access and sharing asked for, the result and the code that asked for it, plus
//                   a summary every minute
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

#include "MinHook.h"
#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVSummary.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;	// TweaksV.asi itself

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "DirectStorage";
		constexpr const char* kSection = "DIRECTSTORAGE_SETTINGS";

		constexpr int kLinesPerWay = 2;			// lines per file, caller and sharing; more opens are counted
		constexpr uint64_t kMaxLines = 20000;	// open lines in all; then the opens are only counted
		constexpr DWORD kSummaryEveryMs = 60000;

		// The switches, read once: the hooks never read TweaksV.ini.
		bool g_unlockFiles = false;				// UNLOCK_FILES
		bool g_logCalls = false;				// LOG_CALLS
		bool g_diagnostic = false;				// DIAGNOSTIC_MODE
		std::wstring g_gameFolder;				// without a trailing backslash

		// This thread is inside one of the hooks below: whatever it opens there is not logged.
		thread_local bool t_inside = false;

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

			AcquireSRWLockShared(&g_moduleLock);
			const auto known = g_moduleNames.find(base);
			std::string name = known != g_moduleNames.end() ? known->second : std::string();
			ReleaseSRWLockShared(&g_moduleLock);
			if (!name.empty())
				return name;

			wchar_t path[MAX_PATH]{};
			const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
			std::wstring_view file(path, length < MAX_PATH ? length : 0);
			const size_t slash = file.find_last_of(L"\\/");
			if (slash != std::wstring_view::npos)
				file.remove_prefix(slash + 1);
			name = file.empty() ? std::string("?") : ToUtf8(file);

			AcquireSRWLockExclusive(&g_moduleLock);
			g_moduleNames.emplace(base, name);
			ReleaseSRWLockExclusive(&g_moduleLock);
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

		// ---- LOG_FILE_OPENS

		using NtCreateFileFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
			PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
		NtCreateFileFn g_originalNtCreateFile = nullptr;

		std::atomic<uint64_t> g_opens{ 0 };				// opens inside the game folder
		std::atomic<uint64_t> g_lines{ 0 };				// open lines written
		std::atomic<uint64_t> g_notShown{ 0 };			// opens that were only counted
		std::atomic<uint64_t> g_noWriteSharing{ 0 };	// files opened while others may not write them
		SRWLOCK g_statsLock = SRWLOCK_INIT;
		std::unordered_map<std::wstring, int> g_ways;	// "path|caller|sharing" -> opens
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

		// The part of path below the game folder ("x64a.rpf", "update\update.rpf"), "." for the game
		// folder itself; false if path is not inside the game folder.
		bool InGameFolder(std::wstring_view path, std::wstring_view& relative)
		{
			const size_t n = g_gameFolder.size();
			if (n == 0 || path.size() < n || _wcsnicmp(path.data(), g_gameFolder.c_str(), n) != 0)
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
			AcquireSRWLockExclusive(&g_statsLock);
			const int seen = ++g_ways[way];
			g_paths.insert(lower);
			++g_byCaller[caller];
			++g_byType[TypeOf(lower, folder)];
			ReleaseSRWLockExclusive(&g_statsLock);

			if (seen > kLinesPerWay)
			{
				++g_notShown;
				return;
			}
			const uint64_t line = ++g_lines;
			if (line > kMaxLines)
			{
				++g_notShown;
				if (line == kMaxLines + 1)
					WriteLog(kTag, "LOG_FILE_OPENS: " + std::to_string(kMaxLines) + " open lines written - from now on "
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
			AcquireSRWLockShared(&g_statsLock);
			const size_t paths = g_paths.size();
			std::vector<std::pair<std::string, uint64_t>> callers(g_byCaller.begin(), g_byCaller.end());
			std::vector<std::pair<std::wstring, uint64_t>> types(g_byType.begin(), g_byType.end());
			ReleaseSRWLockShared(&g_statsLock);

			const auto byCount = [](const auto& a, const auto& b) { return a.second > b.second; };
			std::sort(callers.begin(), callers.end(), byCount);
			std::sort(types.begin(), types.end(), byCount);

			std::string text = "summary: " + std::to_string(g_opens.load()) + " opens of " + std::to_string(paths) +
				" files and folders in the game folder (" + std::to_string(g_notShown.load()) + " not shown above); " +
				std::to_string(g_noWriteSharing.load()) + " file opens kept others from writing the file | by caller:";
			for (size_t i = 0; i < callers.size(); i++)
				text += (i ? ", " : " ") + callers[i].first + " " + std::to_string(callers[i].second);
			text += " | by type:";
			uint64_t others = 0;
			for (size_t i = 0; i < types.size(); i++)
			{
				if (i < 15)
					text += (i ? ", " : " ") + ToUtf8(types[i].first) + " " + std::to_string(types[i].second);
				else
					others += types[i].second;
			}
			if (others)
				text += ", others " + std::to_string(others);
			WriteLog(kTag, text);
		}

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
			if (!t_inside && status != static_cast<NTSTATUS>(STATUS_ACCESS_VIOLATION))
			{
				const DWORD lastError = GetLastError();
				t_inside = true;
				try
				{
					LogFileOpen(attributes, access, share, disposition, options, status);
				}
				catch (...)	// nothing may throw into the game
				{
				}
				t_inside = false;
				SetLastError(lastError);
			}
			return status;
		}

		// ---- UNLOCK_FILES: the factory's OpenFile

		// IDStorageFactory, whose function [4] is OpenFile (after IUnknown's 3 and CreateQueue)
		constexpr GUID kFactoryId = { 0x6924EA0C, 0xC3CD, 0x4826, { 0xB1, 0x0A, 0xF6, 0x4F, 0x4E, 0xD9, 0x27, 0xC1 } };
		constexpr size_t kOpenFileSlot = 4;
		constexpr DWORD kOpenFileSummaryMs = 10000;	// the game opens its files right after creating the factory

		using OpenFileFn = HRESULT(STDMETHODCALLTYPE*)(void* factory, const WCHAR* path, REFIID riid, void** ppv);
		OpenFileFn g_originalOpenFile = nullptr;
		SRWLOCK g_openFileLock = SRWLOCK_INIT;
		bool g_openFileHookTried = false;				// under g_openFileLock
		std::atomic<uint64_t> g_openFileCalls{ 0 };
		std::atomic<uint64_t> g_archives{ 0 };			// .rpf and .cache paths
		std::atomic<uint64_t> g_refused{ 0 };
		std::atomic<uint64_t> g_outside{ 0 };			// paths outside the game folder: never refused
		std::atomic<uint64_t> g_unreadable{ 0 };		// paths that could not be read: passed on unchecked
		std::atomic<bool> g_openFileSummaryDone{ false };

		struct ExclusiveLock
		{
			SRWLOCK& lock;
			explicit ExclusiveLock(SRWLOCK& l) : lock(l) { AcquireSRWLockExclusive(&lock); }
			~ExclusiveLock() { ReleaseSRWLockExclusive(&lock); }
			ExclusiveLock(const ExclusiveLock&) = delete;
			ExclusiveLock& operator=(const ExclusiveLock&) = delete;
		};

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

		// The full path of an OpenFile path (backslashes only): without a "\\?\" prefix, and a relative
		// path made absolute from the current directory, as DirectStorage does. Empty if that fails.
		std::wstring FullOpenFilePath(std::wstring_view path)
		{
			if (path.substr(0, 4) == L"\\\\?\\")
				path.remove_prefix(4);
			const bool absolute = (path.size() >= 3 && path[1] == L':' && path[2] == L'\\') || path.substr(0, 2) == L"\\\\";
			if (absolute)
				return std::wstring(path);
			wchar_t buffer[2048];
			const std::wstring relative(path);
			const DWORD n = GetFullPathNameW(relative.c_str(), static_cast<DWORD>(std::size(buffer)), buffer, nullptr);
			return n == 0 || n >= std::size(buffer) ? std::wstring() : std::wstring(buffer, n);
		}

		// "x64a.rpf" for a path inside the game folder, the full path otherwise
		std::string ShortPath(std::wstring_view path)
		{
			const std::wstring full = FullOpenFilePath(path);
			std::wstring_view relative;
			return ToUtf8(!full.empty() && InGameFolder(full, relative) ? relative : (full.empty() ? path : std::wstring_view(full)));
		}

		void LogOpenFileSummary()
		{
			const uint64_t calls = g_openFileCalls.load();
			const uint64_t archives = g_archives.load();
			const uint64_t outside = g_outside.load();
			const uint64_t unreadable = g_unreadable.load();
			const std::string seconds = std::to_string(kOpenFileSummaryMs / 1000);
			const std::string unread = unreadable ? ", " + std::to_string(unreadable) + " with an unreadable path passed on" : "";
			if (g_unlockFiles)
				WriteLog(kTag, "UNLOCK_FILES: in its first " + seconds + " seconds the game asked DirectStorage for " +
					std::to_string(calls) + " files: " + std::to_string(archives) + " .rpf and .cache files were opened, " +
					std::to_string(g_refused.load()) + " other files in the game folder refused, so they stay free for other " +
					"programs" + (outside ? ", " + std::to_string(outside) + " outside the game folder opened as usual" : "") + unread + ".");
			else
				WriteLog(kTag, "OpenFile: in its first " + seconds + " seconds the game asked DirectStorage for " +
					std::to_string(calls) + " files: " + std::to_string(archives) + " .rpf and .cache files, " +
					std::to_string(calls - archives - outside - unreadable) + " other files in the game folder and " +
					std::to_string(outside) + " outside it" + unread + " (UNLOCK_FILES=0 - all of them opened).");
		}

		DWORD WINAPI OpenFileSummaryThread(LPVOID)
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
			return 0;
		}

		// LOG_CALLS: every call; otherwise only the refusals after the summary, which are rare.
		void LogOpenFile(uint64_t number, std::wstring_view path, bool readable, bool refused, HRESULT result)
		{
			const std::string name = readable ? ShortPath(path) : std::string("(unreadable path)");
			if (g_logCalls)
			{
				std::string text = "OpenFile #" + std::to_string(number) + ": " + name + " -> " +
					(refused ? std::string("refused (in the game folder, not .rpf or .cache)") : HResultText(result));
				if (number == 1)
					text += ", from " + DescribeCallers(nullptr);
				WriteLog(kTag, text);
			}
			else if (refused && g_openFileSummaryDone.load())
			{
				WriteLog(kTag, "UNLOCK_FILES: refused " + name + " (in the game folder, not .rpf or .cache).");
			}
		}

		HRESULT STDMETHODCALLTYPE HookOpenFile(void* factory, const WCHAR* path, REFIID riid, void** ppv)
		{
			wchar_t copy[2048];
			size_t length = 0;
			bool complete = false;
			const bool readable = path && CopyPathSafely(path, copy, std::size(copy), &length, &complete) && complete;
			if (readable)
				for (size_t i = 0; i < length; i++)
					if (copy[i] == L'/')
						copy[i] = L'\\';	// only in TweaksV's copy: the game's path is passed on as it is
			const bool archive = readable && IsArchive({ copy, length });
			std::wstring_view relative;
			const std::wstring full = readable ? FullOpenFilePath({ copy, length }) : std::wstring();
			const bool inside = !full.empty() && InGameFolder(full, relative);
			const bool refuse = g_unlockFiles && inside && !archive;	// never outside the game folder: saves, settings

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
			if (archive)
				++g_archives;
			if (refuse)
				++g_refused;
			if (!readable)
				++g_unreadable;
			else if (!inside)
				++g_outside;
			if (!t_inside)
			{
				const DWORD lastError = GetLastError();
				t_inside = true;
				try
				{
					if (number == 1)
					{
						const HANDLE thread = CreateThread(nullptr, 0, OpenFileSummaryThread, nullptr, 0, nullptr);
						if (thread)
							CloseHandle(thread);
					}
					LogOpenFile(number, readable ? std::wstring_view(copy, length) : std::wstring_view(), readable, refuse, result);
				}
				catch (...)	// nothing may throw into the game
				{
				}
				t_inside = false;
				SetLastError(lastError);
			}
			return result;
		}

		// Hooks the factory's OpenFile, once: when the game has created the factory (riid, *ppv) and
		// before it gets it back, so the hook sees its first OpenFile already.
		void HookFactoryOpenFile(REFIID riid, void** ppv)
		{
			const ExclusiveLock lock(g_openFileLock);	// a second caller waits until OpenFile is hooked
			// with UNLOCK_FILES a failure is a problem for the start-up summary; for LOG_CALLS alone a log line
			const auto fail = [](const std::string& why)
			{
				if (g_unlockFiles)
					ReportFailure(kTag, "The files in the game folder are not unlocked this session: " + why + ".");
				else
					WriteLog(kTag, "LOG_CALLS: OpenFile is not logged: " + why + ".");
			};
			if (!IsEqualGUID(riid, kFactoryId))
			{
				static bool reported = false;	// under g_openFileLock
				if (!reported)
					fail("the game asked for another DirectStorage interface, " + GuidText(riid) + ", whose functions TweaksV does not know");
				reported = true;
				return;
			}
			if (g_openFileHookTried)
				return;
			g_openFileHookTried = true;

			void* factory = nullptr;
			void* table = nullptr;
			void* target = nullptr;
			if (!CopyMemorySafely(&factory, ppv, sizeof(factory)) || !factory ||
				!CopyMemorySafely(&table, factory, sizeof(table)) || !table ||
				!CopyMemorySafely(&target, static_cast<void**>(table) + kOpenFileSlot, sizeof(target)) || !target)
			{
				fail("the factory's function table could not be read");
				return;
			}
			uintptr_t base = 0;
			const std::string module = ModuleName(target, base);
			if (_stricmp(module.c_str(), "dstoragecore.dll") != 0 && _stricmp(module.c_str(), "dstorage.dll") != 0)
			{
				fail("the factory's OpenFile is not in DirectStorage's DLLs (" + DescribeCode(target) + ")");
				return;
			}

			MH_STATUS status = MH_CreateHook(target, reinterpret_cast<LPVOID>(&HookOpenFile), reinterpret_cast<LPVOID*>(&g_originalOpenFile));
			if (status == MH_OK)
			{
				status = MH_EnableHook(target);
				if (status != MH_OK)
					MH_RemoveHook(target);
			}
			if (status != MH_OK)
				fail("OpenFile (" + DescribeCode(target) + ") could not be hooked (MinHook: " + MH_StatusToString(status) + ")");
			else if (g_unlockFiles)
				WriteLog(kTag, "UNLOCK_FILES: from now on DirectStorage may open only the .rpf and .cache files in the game "
					"folder (OpenFile at " + DescribeCode(target) + " hooked); files outside it are not touched.");
			else
				WriteLog(kTag, "LOG_CALLS: the game's OpenFile calls are logged (OpenFile at " + DescribeCode(target) +
					" hooked); UNLOCK_FILES=0, so every file is opened.");
		}

		// ---- LOG_CALLS

		using GetFactoryFn = HRESULT(WINAPI*)(REFIID riid, void** ppv);
		using SetConfigurationFn = HRESULT(WINAPI*)(const void* configuration);
		GetFactoryFn g_originalGetFactory = nullptr;
		SetConfigurationFn g_originalSetConfiguration = nullptr;
		SetConfigurationFn g_originalSetConfiguration1 = nullptr;
		std::atomic<int> g_factoryCalls{ 0 };

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
			const HRESULT result = g_originalGetFactory(riid, ppv);
			if (!t_inside)
			{
				const DWORD lastError = GetLastError();
				t_inside = true;
				try
				{
					if (g_logCalls)
					{
						WriteLog(kTag, "the game asked for the DirectStorage factory (call #" + std::to_string(++g_factoryCalls) +
							", interface " + GuidText(riid) + ") -> " + HResultText(result) + ", from " + DescribeCallers(nullptr));
						if (g_diagnostic && SUCCEEDED(result) && ppv)
							LogFactoryFunctions(ppv);
					}
					if (SUCCEEDED(result) && ppv)
						HookFactoryOpenFile(riid, ppv);
				}
				catch (...)
				{
				}
				t_inside = false;
				SetLastError(lastError);
			}
			return result;
		}

		// rawSize: the bytes logged in DIAGNOSTIC_MODE - the size of DSTORAGE_CONFIGURATION (7 values)
		// or DSTORAGE_CONFIGURATION1 (8 values)
		void LogSetConfiguration(const char* function, const void* configuration, size_t rawSize, HRESULT result)
		{
			WriteLog(kTag, std::string("the game set the DirectStorage configuration (") + function + ") -> " +
				HResultText(result) + ", from " + DescribeCallers(nullptr));
			uint32_t values[8]{};
			if (!g_diagnostic || !configuration || rawSize > sizeof(values) || !CopyMemorySafely(values, configuration, rawSize))
				return;
			std::string text = std::string("DIAGNOSTIC: ") + function + " values:";
			for (size_t i = 0; i < rawSize / sizeof(values[0]); i++)
			{
				char value[16];
				snprintf(value, sizeof(value), " %08X", values[i]);
				text += value;
			}
			WriteLog(kTag, text);
		}

		HRESULT WINAPI HookSetConfiguration(const void* configuration)
		{
			const HRESULT result = g_originalSetConfiguration(configuration);
			if (!t_inside)
			{
				const DWORD lastError = GetLastError();
				t_inside = true;
				try
				{
					LogSetConfiguration("DStorageSetConfiguration", configuration, 7 * sizeof(uint32_t), result);
				}
				catch (...)
				{
				}
				t_inside = false;
				SetLastError(lastError);
			}
			return result;
		}

		HRESULT WINAPI HookSetConfiguration1(const void* configuration)
		{
			const HRESULT result = g_originalSetConfiguration1(configuration);
			if (!t_inside)
			{
				const DWORD lastError = GetLastError();
				t_inside = true;
				try
				{
					LogSetConfiguration("DStorageSetConfiguration1", configuration, 8 * sizeof(uint32_t), result);
				}
				catch (...)
				{
				}
				t_inside = false;
				SetLastError(lastError);
			}
			return result;
		}

		// ---- installing

		bool InstallHook(const wchar_t* module, const char* function, LPVOID detour, LPVOID* original)
		{
			LPVOID target = nullptr;
			MH_STATUS status = MH_CreateHookApiEx(module, function, detour, original, &target);
			if (status == MH_OK)
			{
				status = MH_EnableHook(target);
				if (status != MH_OK)
					MH_RemoveHook(target);
			}
			if (status != MH_OK)
			{
				WriteLog(kTag, std::string(function) + " could not be hooked (MinHook: " + MH_StatusToString(status) + ").");
				return false;
			}
			return true;
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
	}

	void InitDirectStorage()
	{
		const bool unlock = GetIniBool(kSection, "UNLOCK_FILES", true);
		const bool logCalls = GetIniBool(kSection, "LOG_CALLS", false);
		const bool logOpens = GetIniBool(kSection, "LOG_FILE_OPENS", false);
		if (!unlock && !logCalls && !logOpens)
		{
			WriteLog(kTag, "UNLOCK_FILES=0, LOG_CALLS=0 and LOG_FILE_OPENS=0 - nothing to do.");
			return;
		}

		g_unlockFiles = unlock;
		g_logCalls = logCalls;
		g_diagnostic = IsDiagnosticMode();
		g_gameFolder = GetGameFolder();
		const bool loaded = GetModuleHandleW(L"dstorage.dll") != nullptr;
		WriteLog(kTag, "dstorage.dll " + FileVersionOf(g_gameFolder + L"\\dstorage.dll") + ", dstoragecore.dll " +
			FileVersionOf(g_gameFolder + L"\\dstoragecore.dll") + "; dstorage.dll is " + (loaded ? "loaded" : "not loaded") + ".");
		const std::wstring fix = FindDirectStorageFix();	// only with CHECK_FOR_CONFLICTS=0: otherwise this part is off
		if (!fix.empty())
			WriteLog(kTag, "Note: " + ToUtf8(fix) + " is installed as well - it changes DirectStorage's file opens, too.");

		if (IsDryRun())
		{
			if (unlock)
				WriteLog(kTag, "DRY_RUN: would let DirectStorage open only .rpf and .cache files (UNLOCK_FILES).");
			if (logCalls)
				WriteLog(kTag, "DRY_RUN: would hook dstorage.dll to log the game's DirectStorage calls.");
			if (logOpens)
				WriteLog(kTag, "DRY_RUN: would hook NtCreateFile to log the opens in the game folder.");
			return;
		}

		const MH_STATUS init = MH_Initialize();
		if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED)
		{
			WriteLog(kTag, std::string("MinHook could not be initialised (") + MH_StatusToString(init) + ") - nothing is unlocked or logged.");
			return;
		}

		// before the hooks: they use these from the first call on
		g_game = RangeOf(GetModuleHandleW(nullptr));
		g_self = RangeOf(reinterpret_cast<HMODULE>(&__ImageBase));
		g_ntdll = RangeOf(GetModuleHandleW(L"ntdll.dll"));
		g_kernelBase = RangeOf(GetModuleHandleW(L"KernelBase.dll"));
		g_kernel32 = RangeOf(GetModuleHandleW(L"kernel32.dll"));

		if (unlock || logCalls)
		{
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
				if (logCalls)
				{
					int hooked = factory ? 1 : 0;
					hooked += InstallHook(L"dstorage.dll", "DStorageSetConfiguration", reinterpret_cast<LPVOID>(&HookSetConfiguration),
						reinterpret_cast<LPVOID*>(&g_originalSetConfiguration)) ? 1 : 0;
					hooked += InstallHook(L"dstorage.dll", "DStorageSetConfiguration1", reinterpret_cast<LPVOID>(&HookSetConfiguration1),
						reinterpret_cast<LPVOID*>(&g_originalSetConfiguration1)) ? 1 : 0;
					WriteLog(kTag, "LOG_CALLS: the game's DirectStorage calls are logged (" + std::to_string(hooked) +
						" of 3 functions hooked).");
				}
				if (unlock && factory)
					WriteLog(kTag, "UNLOCK_FILES: when the game creates its DirectStorage factory, OpenFile is hooked so that "
						"in the game folder only .rpf and .cache files can be opened.");
				else if (unlock)
					ReportFailure(kTag, "The files in the game folder are not unlocked this session: DStorageGetFactory could "
						"not be hooked (details in TweaksV.log).");
			}
		}

		if (logOpens && InstallHook(L"ntdll.dll", "NtCreateFile", reinterpret_cast<LPVOID>(&HookNtCreateFile),
			reinterpret_cast<LPVOID*>(&g_originalNtCreateFile)))
		{
			WriteLog(kTag, "LOG_FILE_OPENS: every open of a file or folder in " + ToUtf8(g_gameFolder) + " is logged, " +
				std::to_string(kLinesPerWay) + " lines per file, caller and sharing; a summary follows every minute.");
			const HANDLE thread = CreateThread(nullptr, 0, SummaryThread, nullptr, 0, nullptr);
			if (thread)
				CloseHandle(thread);
		}
	}
}
