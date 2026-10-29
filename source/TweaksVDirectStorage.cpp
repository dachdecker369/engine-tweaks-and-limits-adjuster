// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - DirectStorage: the files GTA V Enhanced keeps locked in its folder.
//
// With DirectStorage on, the game asks DirectStorage to open every file in its folder at start-up
// (IDStorageFactory::OpenFile), and DirectStorage opens each for reading while letting others only read
// it (NtCreateFile, share R--) and keeps it open, so mods can no longer write their own files there and
// scripts can no longer be reloaded while the game runs.
//   UNLOCK_FILES    on by default: NtCreateFile is hooked at start-up, and an open of a file in the game
//                   folder that is not .rpf or .cache, asks for reading only and already lets others read
//                   gets write and delete sharing too before it goes on to Windows - whoever makes it, the
//                   game, DirectStorage or a mods loader. The file opens and reads as before, and other
//                   programs can write, rename and delete it while it is open; one that opens it for
//                   writing while sharing nothing still gets a sharing violation. Archives, write and
//                   exclusive opens, folders and everything outside the game folder are left as they are,
//                   and nothing is refused. Decided per call before it, on the stack: no memory allocation
//                   and no lock of TweaksV's. NtSetInformationFile is hooked too: a rename made inside the
//                   game that replaces a file in the game folder - which Windows refuses over a file somebody
//                   holds open, whatever the sharing - is made with POSIX semantics instead, on a copy of the
//                   caller's buffer, and made again as the caller made it where the file system refuses
//                   that; every other call goes on as made. A summary 10 seconds after the first open in the
//                   game folder, then every minute while the numbers change, counts the opens given the
//                   wider sharing, their files by type, the opens left as they were and the replace-renames.
//   DIAGNOSTIC_MODE the game's calls into DirectStorage (DStorageSetConfiguration, ...1,
//                   DStorageGetFactory and the factory's OpenFile, which TweaksV passes on unchanged) with
//                   the code that made them, the factory's functions and the configuration values, and every
//                   open (NtCreateFile) of a file or folder inside the game folder: the access and sharing
//                   asked for, the sharing UNLOCK_FILES gave it, the result and the code that asked for it,
//                   plus a summary every minute. A path outside the game folder is logged as a category and
//                   its file name only, and every kind of line that comes per call stops after 20,000.
// Written for TweaksV; the idea comes from alloc8or's DirectStorageFix.

#include "TweaksVDirectStorage.h"

#include <windows.h>
#include <winternl.h>
#include <intrin.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
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

#include "TweaksVConflicts.h"
#include "TweaksVFiles.h"
#include "TweaksVIni.h"
#include "TweaksVLock.h"
#include "TweaksVLog.h"
#include "TweaksVMeasure.h"
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
		constexpr size_t kMaxTableEntries = 100000;	// entries per table of the open summary; more are only counted
		constexpr size_t kMaxTypesShown = 15;		// types a summary lists; the rest are "others"
		constexpr size_t kMaxPathChars = 2047;		// characters of an OpenFile path TweaksV checks
		constexpr size_t kMaxRenameChars = 1024;	// characters of a rename's name, and of its file's path, TweaksV copies

		// The switches, read once: the hooks never read TweaksV.ini.
		bool g_unlockFiles = false;				// UNLOCK_FILES
		bool g_diagnostic = false;				// DIAGNOSTIC_MODE: the calls into DirectStorage and the opens are logged
		std::wstring g_gameFolder;				// without a trailing backslash
		std::wstring g_tempFolders[2];			// %TEMP% as Windows gives it and in its long form, without one either

		// This thread is inside one of the hooks below: whatever it opens there is left as it is and not logged.
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
			if (n <= kMaxLinesPerKind)
				return true;
			if (n == kMaxLinesPerKind + 1)
				WriteLog(kTag, prefix + std::to_string(kMaxLinesPerKind) + " lines of " + kind + " written - from now on they are only counted.");
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
			case 0xC0000003: name = " invalid information class"; break;
			case 0xC000000D: name = " invalid parameter"; break;
			case 0xC0000022: name = " access denied"; break;
			case 0xC0000033: name = " name invalid"; break;
			case 0xC0000034: name = " not found"; break;
			case 0xC0000035: name = " already exists"; break;
			case 0xC000003A: name = " path not found"; break;
			case 0xC0000043: name = " sharing violation"; break;
			case 0xC0000056: name = " delete pending"; break;
			case 0xC00000BA: name = " is a folder"; break;
			case 0xC00000BB: name = " not supported"; break;
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

		// ---- UNLOCK_FILES: the opens in the game folder (NtCreateFile), decided before the call

		using NtCreateFileFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
			PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
		NtCreateFileFn g_originalNtCreateFile = nullptr;

		// Access that writes or deletes: an open that asks for any of it is left as it is.
		constexpr ACCESS_MASK kWriteAccess = GENERIC_WRITE | GENERIC_ALL | MAXIMUM_ALLOWED | DELETE | FILE_WRITE_DATA |
			FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | WRITE_DAC | WRITE_OWNER;
		constexpr ULONG kWiderSharing = FILE_SHARE_WRITE | FILE_SHARE_DELETE;	// what UNLOCK_FILES adds

		// What an open is to UNLOCK_FILES. Every open the hook examines lands in exactly one count, whatever
		// its result, so the summary's figures add up; an open outside the game folder is not counted.
		enum OpenKind : uint8_t
		{
			kNotCounted,	// outside the game folder, or not examined: UNLOCK_FILES=0, or TweaksV's own open
			kWidened,		// a file other than .rpf and .cache, read-only, sharing reading: write and delete sharing added
			kArchive,		// an .rpf or .cache file
			kWriteOpen,		// asks for write or delete access
			kExclusive,		// does not share reading
			kDirectory,		// FILE_DIRECTORY_FILE, or the game folder itself
			kUnexamined,	// its path could not be examined: unreadable, by file ID, or relative to another folder
			kOpenKinds
		};
		std::atomic<uint64_t> g_openCounts[kOpenKinds];

		// The game folder's identity, for an open relative to a folder handle - a relative path given to
		// CreateFileW reaches NtCreateFile as a name relative to the current directory's handle: the volume's
		// serial number and the folder's file ID, FileIdInformation's 128 bits or, on a file system without
		// them, FileInternalInformation's 64 with FileFsVolumeInformation's serial number. Read once before the
		// hook is made; the hook compares the handle's with it, one query of Windows per open relative to a
		// folder handle (two on a file system without file IDs). These queries need no access to the handle -
		// the current directory's is opened for traversing only, which GetFileInformationByHandle refuses - and
		// leave the thread's last error alone.
		enum class IdentityKind : uint8_t { None, FileId, FileIndex };
		struct FolderIdentity
		{
			uint64_t volume = 0;
			uint8_t file[16]{};

			bool operator==(const FolderIdentity&) const = default;
		};
		IdentityKind g_identityKind = IdentityKind::None;	// None: the game folder's could not be read
		FolderIdentity g_gameFolderIdentity;
		using NtQueryFileFn = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
		NtQueryFileFn g_queryInformationFile = nullptr;			// ntdll's, read before the hook
		NtQueryFileFn g_queryVolumeInformationFile = nullptr;	// the same shape
		constexpr ULONG kFileInternalInformation = 6;
		constexpr ULONG kFileIdInformation = 59;
		constexpr ULONG kFileFsVolumeInformation = 1;
		constexpr NTSTATUS kBufferOverflow = static_cast<NTSTATUS>(0x80000005);	// a warning: the fixed part is there

		// The identity of what handle refers to, read the way kind says; false if Windows cannot give it.
		bool IdentityOf(HANDLE handle, IdentityKind kind, FolderIdentity& identity)
		{
			identity = {};
			IO_STATUS_BLOCK io{};
			if (kind == IdentityKind::FileId)
			{
				FILE_ID_INFO info{};	// FileIdInformation's layout
				if (!NT_SUCCESS(g_queryInformationFile(handle, &io, &info, sizeof(info), kFileIdInformation)))
					return false;
				identity.volume = info.VolumeSerialNumber;
				static_assert(sizeof(info.FileId.Identifier) == sizeof(identity.file));
				memcpy(identity.file, info.FileId.Identifier, sizeof(identity.file));
				return true;
			}
			if (kind == IdentityKind::FileIndex)
			{
				struct
				{
					LARGE_INTEGER created;
					ULONG serialNumber;
					ULONG labelLength;
					BOOLEAN supportsObjects;
					WCHAR label[34];	// a label has 32 characters at most
				} volume{};				// FILE_FS_VOLUME_INFORMATION's layout
				LARGE_INTEGER index{};	// FILE_INTERNAL_INFORMATION's
				const NTSTATUS volumeStatus = g_queryVolumeInformationFile(handle, &io, &volume, sizeof(volume), kFileFsVolumeInformation);
				if ((!NT_SUCCESS(volumeStatus) && volumeStatus != kBufferOverflow) ||
					!NT_SUCCESS(g_queryInformationFile(handle, &io, &index, sizeof(index), kFileInternalInformation)))
					return false;
				identity.volume = volume.serialNumber;
				memcpy(identity.file, &index.QuadPart, sizeof(index.QuadPart));
				return true;
			}
			return false;
		}

		constexpr wchar_t FoldAscii(wchar_t c)
		{
			return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c - L'A' + L'a') : c;
		}

		// a[0, n) and b[0, n) hold the same text, ASCII letters in any case
		bool SameFolded(const wchar_t* a, const wchar_t* b, size_t n)
		{
			for (size_t i = 0; i < n; i++)
				if (FoldAscii(a[i]) != FoldAscii(b[i]))
					return false;
			return true;
		}

		// The widened files by type, for the summary: a fixed table that the threads fill without a lock or an
		// allocation, in the order the types come. A type is the extension with its dot, in lower case, packed
		// into 8 bytes; one that does not fit - longer, or not plain ASCII - and every type past the table's end
		// counts as "others".
		constexpr uint64_t kNoExtension = 1;	// "(none)": a packed extension starts with its dot, so it is never 1
		struct TypeCount
		{
			std::atomic<uint64_t> type{ 0 };	// 0: free
			std::atomic<uint64_t> count{ 0 };
		};
		TypeCount g_types[kMaxTypesShown];
		std::atomic<uint64_t> g_otherTypes{ 0 };

		// extension[0, length), its dot included, packed as above: kNoExtension for none, 0 for one that does not fit
		uint64_t PackedType(const wchar_t* extension, size_t length)
		{
			if (length == 0)
				return kNoExtension;
			if (length > sizeof(uint64_t))
				return 0;
			uint64_t packed = 0;
			for (size_t i = 0; i < length; i++)
			{
				const wchar_t c = FoldAscii(extension[i]);
				if (c <= L' ' || c > L'~')
					return 0;
				packed |= static_cast<uint64_t>(c) << (8 * i);
			}
			return packed;
		}

		void CountType(uint64_t type)
		{
			if (type != 0)
				for (TypeCount& entry : g_types)
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
			g_otherTypes.fetch_add(1, std::memory_order_relaxed);
		}

		// ".txt 4, .asi 1, (none) 1, others 2"
		std::string TypesText()
		{
			std::string text;
			for (const TypeCount& entry : g_types)
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
			if (const uint64_t others = g_otherTypes.load())
				text += (text.empty() ? "" : ", ") + std::string("others ") + std::to_string(others);
			return text;
		}

		// The widened files, told apart for the summary by a hash of their path below the game folder: a fixed
		// table of hashes with open addressing that the threads fill without a lock or an allocation. It takes
		// kMaxFiles files, half its slots, so that a search stays short; the opens of more files are counted,
		// their files not told apart.
		constexpr size_t kFileSlots = 16384;		// a power of two
		constexpr uint64_t kMaxFiles = kFileSlots / 2;
		constexpr size_t kMaxProbes = 64;
		std::atomic<uint64_t> g_fileSlots[kFileSlots];	// a file's hash, 0: free
		std::atomic<uint64_t> g_files{ 0 };				// files told apart
		std::atomic<uint64_t> g_untrackedOpens{ 0 };	// widened opens of files the full table could not take

		enum class Tracked : uint8_t { Known, New, Full };

		Tracked TrackFile(uint64_t hash)
		{
			if (hash == 0)
				hash = 1;	// 0 marks a free slot
			for (size_t probe = 0; probe < kMaxProbes; probe++)
			{
				std::atomic<uint64_t>& slot = g_fileSlots[static_cast<size_t>(hash + probe) & (kFileSlots - 1)];
				uint64_t current = slot.load(std::memory_order_acquire);
				if (current == 0)
				{
					if (g_files.load(std::memory_order_relaxed) >= kMaxFiles)
						return Tracked::Full;
					if (slot.compare_exchange_strong(current, hash, std::memory_order_acq_rel))
					{
						g_files.fetch_add(1, std::memory_order_relaxed);
						return Tracked::New;
					}
					// another thread took the slot first: current is what it put there
				}
				if (current == hash)
					return Tracked::Known;
			}
			return Tracked::Full;
		}

		// What the part of a path below the game folder says: the hash that tells its file apart (FNV-1a over
		// its letters, ASCII in lower case, up to a stream), its type, and whether it is an archive. The
		// extension is the last name's, from its last '.' that is not the name's first letter, up to a ':' that
		// starts a stream - "x.rpf::$DATA" opens x.rpf.
		struct PathFacts
		{
			uint64_t hash = 0;
			uint64_t type = 0;
			bool archive = false;
		};

		void ReadPathFacts(const wchar_t* path, size_t length, PathFacts& facts)
		{
			size_t nameStart = length;
			while (nameStart > 0 && path[nameStart - 1] != L'\\')
				nameStart--;
			size_t nameEnd = nameStart;
			while (nameEnd < length && path[nameEnd] != L':')
				nameEnd++;
			size_t afterDot = nameEnd;
			while (afterDot > nameStart && path[afterDot - 1] != L'.')
				afterDot--;
			const bool hasExtension = afterDot > nameStart + 1;	// a '.' after the name's first letter
			const wchar_t* const extension = hasExtension ? path + afterDot - 1 : path;
			const size_t extensionLength = hasExtension ? nameEnd - afterDot + 1 : 0;
			facts.type = PackedType(extension, extensionLength);
			facts.archive = (extensionLength == 4 && SameFolded(extension, L".rpf", 4)) ||
				(extensionLength == 6 && SameFolded(extension, L".cache", 6));
			uint64_t hash = 14695981039346656037ull;
			for (size_t i = 0; i < nameEnd; i++)
			{
				hash ^= FoldAscii(path[i]);
				hash *= 1099511628211ull;
			}
			facts.hash = hash;
		}

		// The folder handle an NtCreateFile call's name is relative to, or nullptr; false if the
		// OBJECT_ATTRIBUTES cannot be read.
		bool ReadRoot(const OBJECT_ATTRIBUTES* attributes, HANDLE* root)
		{
			__try
			{
				if (!attributes)
					return false;
				*root = attributes->RootDirectory;
				return true;
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				return false;
			}
		}

		enum class Place : uint8_t { Outside, Folder, Inside, Unknown };

		// Where a full name lies, NtCreateFile's or a rename's: "\??\C:\..." - "\??\UNC\server\share\..." for a
		// game folder on a network share - compared with the game folder, letters in ASCII case, and every other
		// spelling (a device, \Device\...) outside: Inside, rest the part below the game folder, the game Folder
		// itself, or Outside. Reads name[0, length) only; the caller guards against a bad pointer.
		Place PlaceOfFullName(const wchar_t* name, size_t length, const wchar_t*& rest, size_t& restLength)
		{
			if (length < 4 || name[0] != L'\\' || name[1] != L'?' || name[2] != L'?' || name[3] != L'\\')
				return Place::Outside;
			name += 4;
			length -= 4;
			const wchar_t* folder = g_gameFolder.data();
			size_t n = g_gameFolder.size();
			if (n > 2 && folder[0] == L'\\' && folder[1] == L'\\')
			{
				if (length < 3 || !SameFolded(name, L"unc", 3))
					return Place::Outside;
				name += 3;	// "\server\share\...", compared with the folder from its second backslash
				length -= 3;
				folder += 1;
				n -= 1;
			}
			if (n == 0 || length < n || !SameFolded(name, folder, n))
				return Place::Outside;
			if (length == n || (length == n + 1 && name[n] == L'\\'))
				return Place::Folder;
			if (name[n] != L'\\')
				return Place::Outside;	// a folder whose name only starts with the game folder's
			rest = name + n + 1;
			restLength = length - n - 1;
			return Place::Inside;
		}

		// Where the name an NtCreateFile call asked for lies: Inside the game folder (facts filled), the game
		// Folder itself, Outside it, or Unknown - a name that cannot be read. Read on the caller's memory within
		// the length it gives; a bad pointer is caught. relative: the name is relative to the game folder (its
		// handle was checked); otherwise it is a full name (PlaceOfFullName).
		Place ExamineName(const OBJECT_ATTRIBUTES* attributes, bool relative, PathFacts& facts)
		{
			__try
			{
				const UNICODE_STRING* const object = attributes->ObjectName;
				if (!object)
					return Place::Unknown;
				const wchar_t* name = object->Buffer;
				size_t length = object->Length / sizeof(wchar_t);
				if (!name && length)
					return Place::Unknown;
				if (relative)
				{
					if (length == 0)
						return Place::Folder;	// the game folder itself, opened again
					if (name[0] == L'\\')
						return Place::Unknown;	// not a name relative to it
					ReadPathFacts(name, length, facts);
					return Place::Inside;
				}

				const wchar_t* rest = nullptr;
				size_t restLength = 0;
				const Place place = PlaceOfFullName(name, length, rest, restLength);
				if (place == Place::Inside)
					ReadPathFacts(rest, restLength, facts);
				return place;
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				return Place::Unknown;
			}
		}

		// UNLOCK_FILES' decision on one open, made before the call on the stack: no memory allocation, no lock
		// of TweaksV's, nothing that can throw. The name says where the open lies - for one relative to a folder
		// handle, that handle must be the game folder's - and then, in this order: a folder, an archive, write
		// access, sharing that leaves reading out; every other open in the game folder is given write and
		// delete sharing.
		struct OpenCheck
		{
			OpenKind kind = kNotCounted;
			PathFacts facts;	// for kWidened: the file's
		};

		OpenCheck CheckOpen(const OBJECT_ATTRIBUTES* attributes, ACCESS_MASK access, ULONG share, ULONG options)
		{
			OpenCheck check;
			HANDLE root = nullptr;
			if ((options & FILE_OPEN_BY_FILE_ID) || !ReadRoot(attributes, &root))
			{
				check.kind = kUnexamined;	// a name that is a file ID says nothing about the path
				return check;
			}
			if (root)
			{
				FolderIdentity identity;
				const bool gameFolder = g_identityKind != IdentityKind::None && IdentityOf(root, g_identityKind, identity) &&
					identity == g_gameFolderIdentity;
				if (!gameFolder)
				{
					check.kind = kUnexamined;	// relative to another folder: where it lies cannot be told
					return check;
				}
			}
			switch (ExamineName(attributes, root != nullptr, check.facts))
			{
			case Place::Outside:
				return check;
			case Place::Unknown:
				check.kind = kUnexamined;
				return check;
			case Place::Folder:
				check.kind = kDirectory;
				return check;
			case Place::Inside:
				break;
			}
			if (options & FILE_DIRECTORY_FILE)
				check.kind = kDirectory;
			else if (check.facts.archive)
				check.kind = kArchive;
			else if (access & kWriteAccess)
				check.kind = kWriteOpen;
			else if (!(share & FILE_SHARE_READ))
				check.kind = kExclusive;
			else
				check.kind = kWidened;
			return check;
		}

		void CountOpen(const OpenCheck& check)
		{
			g_openCounts[check.kind].fetch_add(1, std::memory_order_relaxed);
			if (check.kind != kWidened)
				return;
			switch (TrackFile(check.facts.hash))
			{
			case Tracked::New:
				CountType(check.facts.type);
				break;
			case Tracked::Full:
				g_untrackedOpens.fetch_add(1, std::memory_order_relaxed);
				break;
			case Tracked::Known:
				break;
			}
		}

		HANDLE g_firstCounted = nullptr;		// an event the hooks set at the first thing they count: the summary's clock
		std::atomic<bool> g_firstCountedSet{ false };

		// The first open or rename counted starts the UNLOCK_FILES summary's clock; its thread waits from the
		// start-up on. The caller's last error is kept.
		void StartSummaryClock()
		{
			if (g_firstCountedSet.load(std::memory_order_relaxed) || g_firstCountedSet.exchange(true) || !g_firstCounted)
				return;
			const DWORD lastError = GetLastError();
			SetEvent(g_firstCounted);
			SetLastError(lastError);
		}

		// ---- UNLOCK_FILES: the replace-renames into the game folder (NtSetInformationFile), turned before the call

		// Windows refuses the classic rename of a new file over one that somebody holds open, whatever the
		// sharing of the handles on it - the usual safe way to save a settings file. With POSIX semantics such a
		// rename goes through as long as every handle on the file shares deleting, which the opens above give
		// the game's: the holders go on reading the file they opened, and the name is the new file's. So a rename
		// made inside the game that replaces what may lie at its target in the game folder - FileRenameInformation
		// with ReplaceIfExists, or FileRenameInformationEx with replace-if-exists but without POSIX semantics - is
		// made as FileRenameInformationEx with both, the caller's other flags kept, on a copy of its buffer on the
		// stack. Where the target lies is told as for an open: a full name compared with the game folder, a name
		// relative to a folder handle by that folder's identity, and a bare name - a rename inside the source's own
		// folder - by the source's path, one query of Windows; every other rename goes on as made.
		using NtSetInformationFileFn = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
		NtSetInformationFileFn g_originalNtSetInformationFile = nullptr;
		constexpr ULONG kFileRenameInformation = 10;
		constexpr ULONG kFileRenameInformationEx = 65;
		constexpr ULONG kReplaceIfExists = 0x1;	// FILE_RENAME_REPLACE_IF_EXISTS
		constexpr ULONG kPosixSemantics = 0x2;		// FILE_RENAME_POSIX_SEMANTICS

		// FILE_RENAME_INFORMATION's layout, for both classes - FileRenameInformation's ReplaceIfExists is the
		// first byte of flags - with room for a name of kMaxRenameChars characters: the copy a rename is turned on.
		struct RenameCopy
		{
			ULONG flags;
			HANDLE root;			// the folder handle the name is relative to, or none
			ULONG nameLength;		// in bytes
			wchar_t name[kMaxRenameChars];
		};
		constexpr size_t kRenameNameOffset = 20;	// the name follows nameLength
		constexpr ULONG kRenameMinLength = 24;		// FILE_RENAME_INFORMATION's size: the shortest buffer Windows takes
		static_assert(offsetof(RenameCopy, root) == 8 && offsetof(RenameCopy, nameLength) == 16 &&
			offsetof(RenameCopy, name) == kRenameNameOffset);

		// What a file system that does not take FileRenameInformationEx, or its POSIX semantics, answers - a FAT
		// volume, a network share, a Windows older than 10 1607: the rename is then made again as the caller made it.
		bool Refused(NTSTATUS status)
		{
			switch (static_cast<uint32_t>(status))
			{
			case 0xC0000002:	// STATUS_NOT_IMPLEMENTED
			case 0xC0000003:	// STATUS_INVALID_INFO_CLASS
			case 0xC000000D:	// STATUS_INVALID_PARAMETER
			case 0xC0000010:	// STATUS_INVALID_DEVICE_REQUEST
			case 0xC00000BB:	// STATUS_NOT_SUPPORTED
				return true;
			default:
				return false;
			}
		}

#ifdef TWEAKSV_TEST_BUILD
		// test builds only: TWEAKSV_TEST_NO_RENAME_EX=1 in the environment answers every rename turned as a file
		// system that refuses FileRenameInformationEx would, without asking Windows, so that the suite tests the
		// fallback
		bool g_testRefuseRenames = false;
#endif

		// Every replace-rename into the game folder lands in exactly one count, whatever its result: turned and
		// given POSIX semantics, made again as made where the file system refused them, or passed on as made, its
		// name longer than the copy takes. Every other rename is not counted.
		enum RenameKind : uint8_t
		{
			kRenameTurned,
			kRenameFallback,
			kRenameTooLong,
			kRenameKinds
		};
		std::atomic<uint64_t> g_renameCounts[kRenameKinds];
		std::atomic<uint64_t> g_renameNumber{ 0 };	// renames turned, for their lines
		std::atomic<uint64_t> g_renameLines{ 0 };	// rename lines written

		void CountRename(RenameKind kind)
		{
			g_renameCounts[kind].fetch_add(1, std::memory_order_relaxed);
			StartSummaryClock();
		}

		// The header of a rename's buffer, and its name when it fits the copy (copied), read within the length
		// the caller gives, under the guard against a bad pointer. False for a buffer that cannot be read or that
		// Windows refuses itself: shorter than FILE_RENAME_INFORMATION, a name empty, of odd length or longer
		// than the buffer.
		bool ReadRename(const void* information, ULONG length, RenameCopy& copy, bool& copied)
		{
			__try
			{
				if (!information || length < kRenameMinLength)
					return false;
				memcpy(&copy, information, kRenameNameOffset);
				const ULONG n = copy.nameLength;
				if (n == 0 || n % sizeof(wchar_t) != 0 || n > length - kRenameNameOffset)
					return false;
				copied = n <= sizeof(copy.name);
				if (copied)
					memcpy(copy.name, static_cast<const uint8_t*>(information) + kRenameNameOffset, n);
				return true;
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				return false;
			}
		}

		// Where a rename's target lies, by its name - read within its length, under the guard: Inside the game
		// folder, a Bare name - a rename inside the source's own folder, which the source's path decides - or
		// Outside: the game folder itself, another place, a stream's name (a ':' below the game folder), a bare
		// name with a '\', which Windows refuses, or a name that cannot be read. relative: the name is relative to
		// the game folder's handle. rest: the part of the name below the game folder.
		enum class Target : uint8_t { Outside, Inside, Bare };

		Target ExamineTarget(const wchar_t* name, size_t length, bool relative, const wchar_t*& rest, size_t& restLength)
		{
			__try
			{
				if (relative || name[0] != L'\\')
				{
					for (size_t i = 0; i < length; i++)
						if (name[i] == L':' || (name[i] == L'\\' && (i == 0 || !relative)))
							return Target::Outside;
					rest = name;
					restLength = length;
					return relative ? Target::Inside : Target::Bare;
				}
				if (PlaceOfFullName(name, length, rest, restLength) != Place::Inside)
					return Target::Outside;
				for (size_t i = 0; i < restLength; i++)
					if (rest[i] == L':')
						return Target::Outside;
				return Target::Inside;
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				return Target::Outside;
			}
		}

		// The game folder as Windows names a file object in it (NtQueryObject: "\Device\HarddiskVolume3\..."),
		// without a trailing backslash, read once before the hook is made; empty if it could not be read.
		std::wstring g_gameFolderPath;
		using NtQueryObjectFn = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
		NtQueryObjectFn g_queryObject = nullptr;	// ntdll's, read before the hook
		constexpr ULONG kObjectNameInformation = 1;

		// OBJECT_NAME_INFORMATION with room for a name of kMaxRenameChars characters
		struct ObjectName
		{
			UNICODE_STRING name;
			wchar_t text[kMaxRenameChars];
		};

		// For a bare name: the target lies in the source's folder, so below the game folder when the source - the
		// file being renamed - does. Its path as Windows names it, one query on the stack, compared with the game
		// folder's read the same way, letters in ASCII case; a stream (a ':' below the game folder) is no file in
		// it. folder: the part of the path below the game folder up to its last '\', for the line.
		bool SourceInGameFolder(HANDLE file, ObjectName& source, const wchar_t*& folder, size_t& folderLength)
		{
			const size_t n = g_gameFolderPath.size();
			if (n == 0 || !NT_SUCCESS(g_queryObject(file, kObjectNameInformation, &source, sizeof(source), nullptr)))
				return false;
			const wchar_t* const path = source.name.Buffer;
			const wchar_t* const end = source.text + std::size(source.text);
			const size_t length = source.name.Length / sizeof(wchar_t);
			if (!path || path < source.text || path > end || length > static_cast<size_t>(end - path))
				return false;
			if (length <= n + 1 || path[n] != L'\\' || !SameFolded(path, g_gameFolderPath.data(), n))
				return false;
			const wchar_t* const below = path + n + 1;
			const size_t belowLength = length - n - 1;
			size_t last = 0;	// after the last '\' below the game folder
			for (size_t i = 0; i < belowLength; i++)
			{
				if (below[i] == L':')
					return false;
				if (below[i] == L'\\')
					last = i + 1;
			}
			folder = below;
			folderLength = last ? last - 1 : 0;
			return true;
		}

		// UNLOCK_FILES' decision on one rename, made before the call on the stack: no memory allocation, no lock
		// of TweaksV's, nothing that can throw. A replace-rename into the game folder is Turned, its copy made
		// ready, or with a name longer than the copy takes TooLong; every other rename goes on AsMade.
		enum class RenameDecision : uint8_t { AsMade, Turned, TooLong };
		struct RenameCheck
		{
			RenameDecision decision = RenameDecision::AsMade;
			ULONG flags = 0;					// what the caller asked for: ReplaceIfExists's byte, or the flags
			ULONG length = 0;					// the copy's length for the turned call
			const char* form = "";				// how the target was named, for the line
			const wchar_t* folder = nullptr;	// a bare name's folder below the game folder, for the line
			size_t folderLength = 0;
			const wchar_t* name = nullptr;		// the target below the game folder - a bare name in that folder
			size_t nameLength = 0;
			RenameCopy copy;
			ObjectName source;					// a bare name's source as Windows names it
		};

		void CheckRename(HANDLE file, const void* information, ULONG length, ULONG informationClass, RenameCheck& check)
		{
			bool copied = false;
			if (!ReadRename(information, length, check.copy, copied))
				return;
			check.flags = check.copy.flags;
			const bool replace = informationClass == kFileRenameInformation ? (check.flags & 0xFF) != 0 :
				(check.flags & kReplaceIfExists) != 0 && (check.flags & kPosixSemantics) == 0;
			if (!replace)
				return;	// a rename that replaces nothing, or one with POSIX semantics already
			const HANDLE root = check.copy.root;
			if (root)
			{
				FolderIdentity identity;
				if (g_identityKind == IdentityKind::None || !IdentityOf(root, g_identityKind, identity) ||
					identity != g_gameFolderIdentity)
					return;	// relative to another folder: where it lies cannot be told
			}
			// the copy is examined when the name fits it, so that what is examined is what goes to Windows;
			// a longer name where the caller has it, only to be counted
			const wchar_t* const name = copied ? check.copy.name :
				reinterpret_cast<const wchar_t*>(static_cast<const uint8_t*>(information) + kRenameNameOffset);
			const Target target = ExamineTarget(name, check.copy.nameLength / sizeof(wchar_t), root != nullptr, check.name,
				check.nameLength);
			if (target == Target::Outside ||
				(target == Target::Bare && !SourceInGameFolder(file, check.source, check.folder, check.folderLength)))
				return;
			if (!copied)
			{
				check.decision = RenameDecision::TooLong;
				return;
			}
			check.form = target == Target::Bare ? "a bare name" : root ? "a name relative to the game folder" : "a full name";
			check.copy.flags = (informationClass == kFileRenameInformationEx ? check.flags : 0) | kReplaceIfExists | kPosixSemantics;
			check.length = std::max(kRenameMinLength, static_cast<ULONG>(kRenameNameOffset) + check.copy.nameLength);
			check.decision = RenameDecision::Turned;
		}

		// The rename turned - in test builds with TWEAKSV_TEST_NO_RENAME_EX=1 answered as a file system that
		// refuses it would, without asking Windows.
		NTSTATUS MakeTurnedRename(HANDLE file, PIO_STATUS_BLOCK ioStatus, RenameCheck& check)
		{
#ifdef TWEAKSV_TEST_BUILD
			if (g_testRefuseRenames)
				return static_cast<NTSTATUS>(0xC000000D);	// STATUS_INVALID_PARAMETER, FAT's answer
#endif
			return g_originalNtSetInformationFile(file, ioStatus, &check.copy, check.length, kFileRenameInformationEx);
		}

		// One line per rename turned (DIAGNOSTIC_MODE), 20,000 at most: its target below the game folder, what
		// Windows answered - where the file system refused POSIX semantics, and what the call as made gave - how
		// the caller asked and how it was made, and the code that made it.
		void LogRename(const RenameCheck& check, ULONG informationClass, NTSTATUS turned, bool refused, NTSTATUS status)
		{
			const uint64_t number = ++g_renameNumber;
			if (!Counted(g_renameLines, "DIAGNOSTIC: ", "renames turned"))
				return;
			std::wstring target;
			if (check.folderLength)
				target.assign(check.folder, check.folderLength).push_back(L'\\');
			target.append(check.name, check.nameLength);
			char asked[64], made[64];
			if (informationClass == kFileRenameInformation)
				snprintf(asked, sizeof(asked), "FileRenameInformation with ReplaceIfExists");
			else
				snprintf(asked, sizeof(asked), "FileRenameInformationEx, flags 0x%08lX", static_cast<unsigned long>(check.flags));
			snprintf(made, sizeof(made), "FileRenameInformationEx, flags 0x%08lX", static_cast<unsigned long>(check.copy.flags));
			const std::string result = refused ? "refused (" + StatusText(turned) + "), made again as made: " + StatusText(status) :
				StatusText(status);
			WriteLog(kTag, "rename #" + std::to_string(number) + ": " + ToUtf8(target) + " | " + result + " | " + asked + ", " +
				check.form + " -> " + made + " | from " + DescribeCallers(nullptr));
		}

		// Every open and every replace-rename counted: what the summary has to say
		uint64_t CountedEvents()
		{
			uint64_t total = 0;
			for (size_t kind = kWidened; kind < static_cast<size_t>(kOpenKinds); kind++)
				total += g_openCounts[kind].load();
			for (const std::atomic<uint64_t>& count : g_renameCounts)
				total += count.load();
			return total;
		}

		bool g_unlockSummaryWritten = false;	// only the measurement's thread uses it

		// The UNLOCK_FILES summary, 10 seconds after the first open or rename counted and then every minute while
		// the count changes. Each figure is read once, so the figures add up whatever comes meanwhile.
		void LogUnlockSummary(uint64_t)
		{
			uint64_t counts[kOpenKinds], renames[kRenameKinds];
			for (size_t kind = 0; kind < static_cast<size_t>(kOpenKinds); kind++)
				counts[kind] = g_openCounts[kind].load();
			for (size_t kind = 0; kind < static_cast<size_t>(kRenameKinds); kind++)
				renames[kind] = g_renameCounts[kind].load();
			const uint64_t files = g_files.load(), untracked = g_untrackedOpens.load();
			const std::string when = g_unlockSummaryWritten ? std::string("so far") :
				"in its first " + std::to_string(kFirstSummaryMs / 1000) + " seconds";
			g_unlockSummaryWritten = true;
			const std::string types = TypesText();
			WriteLog(kTag, "UNLOCK_FILES: " + when + ", " + std::to_string(counts[kWidened]) + " opens of " +
				(untracked ? "more than " : "") + std::to_string(files) + " files in the game folder other than .rpf and .cache "
				"were given write and delete sharing, so those files stay free for other programs" +
				(types.empty() ? std::string() : "; by type: " + types) + "; left as they were: " +
				std::to_string(counts[kArchive]) + " opens of archives, " + std::to_string(counts[kWriteOpen]) + " write opens, " +
				std::to_string(counts[kExclusive]) + " exclusive opens, " + std::to_string(counts[kDirectory]) + " directories, " +
				std::to_string(counts[kUnexamined]) + " whose path could not be examined; replace-renames into the game folder: " +
				std::to_string(renames[kRenameTurned]) + " given POSIX semantics, " + std::to_string(renames[kRenameFallback]) +
				" made again as the caller made them where the file system refused that, " + std::to_string(renames[kRenameTooLong]) +
				" passed on as made, their names longer than " + std::to_string(kMaxRenameChars) + " characters.");
		}

		constexpr Measurement kUnlockMeasurement = { "UNLOCK_FILES summary", kFirstSummaryMs, &CountedEvents, &LogUnlockSummary,
			&g_firstCounted };

		// ---- the file opens (DIAGNOSTIC_MODE)

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

		// share: the sharing the caller asked for; shareUsed: the sharing the open went to Windows with, wider
		// where UNLOCK_FILES widened it.
		void LogFileOpen(const OBJECT_ATTRIBUTES* attributes, ACCESS_MASK access, ULONG share, ULONG shareUsed,
			ULONG disposition, ULONG options, NTSTATUS status)
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
			if (status >= 0 && !folder && !(shareUsed & FILE_SHARE_WRITE))
				++g_noWriteSharing;
			std::string caller;
			const std::string callers = DescribeCallers(&caller);

			const std::wstring lower = Lower(relative);
			const std::wstring way = lower + L"|" + FromUtf8(caller) + L"|" + std::to_wstring(shareUsed & 7);
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
			if (line > kMaxLinesPerKind)
			{
				++g_notShown;
				if (line == kMaxLinesPerKind + 1)
					WriteLog(kTag, "DIAGNOSTIC: " + std::to_string(kMaxLinesPerKind) + " open lines written - from now on "
						"the opens are only counted (see the summaries).");
				return;
			}

			const std::string sharing = ShareText(share) + (shareUsed != share ? " widened to " + ShareText(shareUsed) : std::string());
			char details[160];
			snprintf(details, sizeof(details), " | access 0x%08lX, share %s, %s, options 0x%08lX%s | from ",
				static_cast<unsigned long>(access), sharing.c_str(), DispositionText(disposition),
				static_cast<unsigned long>(options), folder ? " (folder)" : "");
			WriteLog(kTag, "open #" + std::to_string(number) + ": " + ToUtf8(relative) + " | " + StatusText(status) +
				details + callers);
		}

		void LogSummary(uint64_t opens)
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

			std::string text = "summary: " + std::to_string(opens) + " opens of " + std::to_string(paths) +
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

		uint64_t Opens()
		{
			return g_opens.load();
		}

		// The open summary, a minute in and then every minute while the count changes.
		constexpr Measurement kMeasurement = { "DirectStorage open summary", kSummaryEveryMs, &Opens, &LogSummary };

		// ---- the hooks: both jobs on one NtCreateFile, and UNLOCK_FILES' replace-renames on NtSetInformationFile

		NTSTATUS NTAPI HookNtCreateFile(PHANDLE fileHandle, ACCESS_MASK access, POBJECT_ATTRIBUTES attributes,
			PIO_STATUS_BLOCK ioStatus, PLARGE_INTEGER allocationSize, ULONG fileAttributes, ULONG share,
			ULONG disposition, ULONG options, PVOID eaBuffer, ULONG eaLength)
		{
			// UNLOCK_FILES decides before the call; what TweaksV opens inside a hook of its own is left as it is
			OpenCheck check;
			if (g_unlockFiles && !t_inside)
				check = CheckOpen(attributes, access, share, options);
			const ULONG shareUsed = check.kind == kWidened ? share | kWiderSharing : share;
			const NTSTATUS status = g_originalNtCreateFile(fileHandle, access, attributes, ioStatus, allocationSize,
				fileAttributes, shareUsed, disposition, options, eaBuffer, eaLength);
			if (check.kind != kNotCounted)
			{
				CountOpen(check);
				StartSummaryClock();
			}
			// STATUS_ACCESS_VIOLATION: the caller's arguments could not even be read by the system
			if (g_diagnostic && status != static_cast<NTSTATUS>(STATUS_ACCESS_VIOLATION))
				AfterCall([&] { LogFileOpen(attributes, access, share, shareUsed, disposition, options, status); });
			return status;
		}

		NTSTATUS NTAPI HookNtSetInformationFile(HANDLE file, PIO_STATUS_BLOCK ioStatus, PVOID information, ULONG length,
			ULONG informationClass)
		{
			// every other class goes on at once, and so does what TweaksV renames inside a hook of its own
			if ((informationClass != kFileRenameInformation && informationClass != kFileRenameInformationEx) || t_inside)
				return g_originalNtSetInformationFile(file, ioStatus, information, length, informationClass);
			RenameCheck check;
			CheckRename(file, information, length, informationClass, check);
			if (check.decision != RenameDecision::Turned)
			{
				const NTSTATUS status = g_originalNtSetInformationFile(file, ioStatus, information, length, informationClass);
				if (check.decision == RenameDecision::TooLong)
					CountRename(kRenameTooLong);
				return status;
			}
			const NTSTATUS turned = MakeTurnedRename(file, ioStatus, check);
			// the file system refused the class or the flag: the call as the caller made it, and its result
			const bool refused = Refused(turned);
			const NTSTATUS status = refused ? g_originalNtSetInformationFile(file, ioStatus, information, length, informationClass) :
				turned;
			CountRename(refused ? kRenameFallback : kRenameTurned);
			if (g_diagnostic)
				AfterCall([&] { LogRename(check, informationClass, turned, refused, status); });
			return status;
		}

		// ---- the factory's OpenFile (DIAGNOSTIC_MODE): every call logged, none changed

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
		std::atomic<uint64_t> g_archives{ 0 };		// .rpf and .cache files in the game folder
		std::atomic<uint64_t> g_otherInside{ 0 };	// the folder's other files
		std::atomic<uint64_t> g_outside{ 0 };		// paths outside the game folder
		std::atomic<uint64_t> g_unreadable{ 0 };	// paths that could not be read or made full
		std::atomic<uint64_t> g_openFileLines{ 0 };	// OpenFile lines written

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

		// An OpenFile path as DirectStorage opens it, worked out on the stack - no memory allocation, nothing
		// that can throw: copied (at most kMaxPathChars characters), '/' made '\' in the copy only (the game's
		// path is passed on as it is), then made full - a "\\?\UNC\server\share" path, the prefix in any case,
		// as "\\server\share", any other "\\?\" path without the prefix and as it is, as Windows takes it,
		// every other path through GetFullPathNameW, which makes it absolute from the current directory, as
		// DirectStorage does, and collapses "." and "..".
		struct OpenFilePath
		{
			wchar_t full[kMaxPathChars + 1];
			size_t length = 0;
			bool readable = false;	// false: it could not be read or made full
			bool inside = false;	// in the game folder
			bool archive = false;	// .rpf or .cache

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
			PathFacts facts;
			ReadPathFacts(result.full, result.length, facts);
			result.archive = facts.archive;
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

		void LogOpenFileSummary()
		{
			// each figure read once, and the calls their sum, so the figures add up whatever comes meanwhile
			const uint64_t archives = g_archives.load(), otherInside = g_otherInside.load(), outside = g_outside.load(),
				unreadable = g_unreadable.load();
			const uint64_t calls = archives + otherInside + outside + unreadable;
			WriteLog(kTag, "OpenFile: in its first " + std::to_string(kOpenFileSummaryMs / 1000) + " seconds the game asked "
				"DirectStorage for " + std::to_string(calls) + " files: " + std::to_string(archives) + " .rpf and .cache files, " +
				std::to_string(otherInside) + " other files in the game folder and " + std::to_string(outside) + " outside it" +
				(unreadable ? ", " + std::to_string(unreadable) + " with an unreadable path" : std::string()) +
				" - all of them passed on to DirectStorage.");
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
			FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
		}

		// Every call, 20,000 lines at most; the name is made only for a line that is written.
		void LogOpenFile(uint64_t number, const OpenFilePath& path, HRESULT result)
		{
			if (!Counted(g_openFileLines, "DIAGNOSTIC: ", "OpenFile calls"))
				return;
			std::string text = "OpenFile #" + std::to_string(number) + ": " + LogName(path) + " -> " + HResultText(result);
			if (number == 1)
				text += ", from " + DescribeCallers(nullptr);
			WriteLog(kTag, text);
		}

		HRESULT STDMETHODCALLTYPE HookOpenFile(void* factory, const WCHAR* path, REFIID riid, void** ppv)
		{
			OpenFilePath checked;
			ReadOpenFilePath(path, checked);
			const HRESULT result = g_originalOpenFile(factory, path, riid, ppv);	// passed on unchanged

			const uint64_t number = ++g_openFileCalls;
			if (!checked.readable)
				++g_unreadable;
			else if (!checked.inside)
				++g_outside;
			else if (checked.archive)
				++g_archives;
			else
				++g_otherInside;
			AfterCall([&]
			{
				if (number == 1)	// the summary 10 seconds after the first call; a thread that cannot start is logged
					StartModuleThread(OpenFileSummaryThread, "OpenFile summary");
				LogOpenFile(number, checked, result);
			});
			return result;
		}

		// The OpenFile hook is a diagnostic aid: what keeps it from being made is a log line.
		void FailOpenFile(const std::string& why)
		{
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
			else
				WriteLog(kTag, "DIAGNOSTIC: the game's OpenFile calls are logged (OpenFile at " + DescribeCode(target) + " hooked).");
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
				StartModuleThread(InterfaceCheckThread, "DirectStorage interface check");	// one that cannot start is logged
				return;
			}

			// made before the attempt, so that logging an exception in it needs no new text
			const std::string unexpected = "DIAGNOSTIC: OpenFile is not logged: OpenFile could not be hooked (unexpected exception).";
			try
			{
				HookOpenFileOf(ppv);
			}
			catch (...)
			{
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

		// Where the factory's first 8 functions are (IUnknown's 3, then its own)
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
				WriteLog(kTag, "the game asked for the DirectStorage factory (call #" + std::to_string(++g_factoryCalls) +
					", interface " + GuidText(riid) + ") -> " + HResultText(result) + ", from " + DescribeCallers(nullptr));
				if (SUCCEEDED(result) && ppv)
				{
					LogFactoryFunctions(ppv);
					HookFactoryOpenFile(riid, ppv);
				}
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

		// A factory the game has not asked for 30 seconds after TweaksV started may have been asked for before
		// TweaksV was loaded: the calls are not logged then. A diagnostic line - nothing depends on the factory.
		DWORD WINAPI FactoryCheckThread(LPVOID module)
		{
			Sleep(kFactoryCheckMs);
			try
			{
				if (!g_factoryAsked.load())
					WriteLog(kTag, "DIAGNOSTIC: the game has not asked for its DirectStorage factory since TweaksV started.");
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
			for (const wchar_t* folder : kLoaderFolders)
			{
				const std::wstring path = g_gameFolder + folder + L"\\DirectStorageFix.asi";
				if (FileExists(path))
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

		// The game folder's identity (FolderIdentity), read before the hook is made. False, with Windows'
		// error, if it cannot be read: no open relative to a folder handle is then known to lie in the game
		// folder.
		bool ReadGameFolderIdentity(DWORD& error)
		{
			if (const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"))
			{
				g_queryInformationFile = reinterpret_cast<NtQueryFileFn>(GetProcAddress(ntdll, "NtQueryInformationFile"));
				g_queryVolumeInformationFile = reinterpret_cast<NtQueryFileFn>(GetProcAddress(ntdll, "NtQueryVolumeInformationFile"));
			}
			if (!g_queryInformationFile || !g_queryVolumeInformationFile)
			{
				error = ERROR_PROC_NOT_FOUND;
				return false;
			}
			const HANDLE folder = CreateFileW(g_gameFolder.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE |
				FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
			if (folder == INVALID_HANDLE_VALUE)
			{
				error = GetLastError();
				return false;
			}
			IdentityKind first = IdentityKind::FileId;
#ifdef TWEAKSV_TEST_BUILD
			// test builds only: TWEAKSV_TEST_FILE_INDEX=1 in the environment reads it as on a file system without
			// file IDs, so that the suite tests that way too
			wchar_t fileIndex[4]{};
			if (GetEnvironmentVariableW(L"TWEAKSV_TEST_FILE_INDEX", fileIndex, 4) == 1 && fileIndex[0] == L'1')
			{
				first = IdentityKind::FileIndex;
				WriteLog(kTag, "TEST BUILD: the game folder's identity is read without FileIdInformation (TWEAKSV_TEST_FILE_INDEX=1).");
			}
#endif
			for (const IdentityKind kind : { IdentityKind::FileId, IdentityKind::FileIndex })
				if (kind >= first && IdentityOf(folder, kind, g_gameFolderIdentity))
				{
					g_identityKind = kind;
					break;
				}
			CloseHandle(folder);
			error = ERROR_NOT_SUPPORTED;	// the file system gives neither
			return g_identityKind != IdentityKind::None;
		}

		// The game folder as Windows names a file object in it (g_gameFolderPath), read before the rename hook is
		// made, the same way the hook reads a source's path. False, with Windows' error, if it cannot be read: no
		// rename by a bare name is then known to lie in the game folder.
		bool ReadGameFolderPath(DWORD& error)
		{
			using StatusToErrorFn = ULONG(NTAPI*)(NTSTATUS);
			StatusToErrorFn statusToError = nullptr;
			if (const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"))
			{
				g_queryObject = reinterpret_cast<NtQueryObjectFn>(GetProcAddress(ntdll, "NtQueryObject"));
				statusToError = reinterpret_cast<StatusToErrorFn>(GetProcAddress(ntdll, "RtlNtStatusToDosError"));
			}
			if (!g_queryObject || !statusToError)
			{
				error = ERROR_PROC_NOT_FOUND;
				return false;
			}
			const HANDLE folder = CreateFileW(g_gameFolder.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE |
				FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
			if (folder == INVALID_HANDLE_VALUE)
			{
				error = GetLastError();
				return false;
			}
			std::vector<uint8_t> buffer(sizeof(UNICODE_STRING) + 65536);	// room for the longest name Windows gives
			const NTSTATUS status = g_queryObject(folder, kObjectNameInformation, buffer.data(), static_cast<ULONG>(buffer.size()), nullptr);
			CloseHandle(folder);
			if (!NT_SUCCESS(status))
			{
				error = statusToError(status);
				return false;
			}
			const auto* const name = reinterpret_cast<const UNICODE_STRING*>(buffer.data());
			if (name->Buffer)
				g_gameFolderPath.assign(name->Buffer, name->Length / sizeof(wchar_t));
			while (!g_gameFolderPath.empty() && g_gameFolderPath.back() == L'\\')
				g_gameFolderPath.pop_back();
			error = ERROR_NOT_SUPPORTED;	// a folder without a name
			return !g_gameFolderPath.empty();
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
			{
				WriteLog(kTag, "DRY_RUN: would hook NtCreateFile so that the game folder's other files are opened with write and "
					"delete sharing (UNLOCK_FILES).");
				WriteLog(kTag, "DRY_RUN: would hook NtSetInformationFile so that a rename inside the game that replaces a file in the "
					"game folder gets POSIX semantics (UNLOCK_FILES).");
			}
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

		// DIAGNOSTIC_MODE: the calls into DirectStorage - the factory's OpenFile is hooked when the game creates the
		// factory (HookFactoryOpenFile). Diagnostic aids: what cannot be hooked is logged.
		if (diagnostic && !loaded)
			WriteLog(kTag, "dstorage.dll is not loaded, so the game does not use DirectStorage here: there are no DirectStorage "
				"calls to log.");
		else if (diagnostic)
		{
			const bool factory = InstallHook(L"dstorage.dll", "DStorageGetFactory", reinterpret_cast<LPVOID>(&HookGetFactory),
				reinterpret_cast<LPVOID*>(&g_originalGetFactory));
			int hooked = factory ? 1 : 0;
			hooked += InstallHook(L"dstorage.dll", g_configurationFunctions[0].name, reinterpret_cast<LPVOID>(&HookSetConfiguration<0>),
				reinterpret_cast<LPVOID*>(&g_configurationFunctions[0].original)) ? 1 : 0;
			hooked += InstallHook(L"dstorage.dll", g_configurationFunctions[1].name, reinterpret_cast<LPVOID>(&HookSetConfiguration<1>),
				reinterpret_cast<LPVOID*>(&g_configurationFunctions[1].original)) ? 1 : 0;
			WriteLog(kTag, "DIAGNOSTIC: the game's DirectStorage calls are logged (" + std::to_string(hooked) + " of 3 functions hooked).");
			if (factory)
				StartModuleThread(FactoryCheckThread, "DirectStorage factory check");	// one that cannot start is logged
		}

		// One NtCreateFile hook for both jobs, made whatever loaded DirectStorage: UNLOCK_FILES frees the files
		// whoever opens them, and turns the replace-renames into the game folder on a hook of NtSetInformationFile.
		// First what the hooks use from their first call on: the game folder's identity and path and the event
		// they set at the first thing they count.
		DWORD identityError = 0, pathError = 0;
		const bool identity = unlock && ReadGameFolderIdentity(identityError);
		const bool path = unlock && ReadGameFolderPath(pathError);
		if (unlock)
			g_firstCounted = CreateEventW(nullptr, TRUE, FALSE, nullptr);	// null: the summary's clock starts at once
#ifdef TWEAKSV_TEST_BUILD
		wchar_t refuseRenames[4]{};
		if (unlock && GetEnvironmentVariableW(L"TWEAKSV_TEST_NO_RENAME_EX", refuseRenames, 4) == 1 && refuseRenames[0] == L'1')
		{
			g_testRefuseRenames = true;
			WriteLog(kTag, "TEST BUILD: every rename turned is answered as on a file system that refuses POSIX semantics, without "
				"asking Windows (TWEAKSV_TEST_NO_RENAME_EX=1).");
		}
#endif
		const bool created = InstallHook(L"ntdll.dll", "NtCreateFile", reinterpret_cast<LPVOID>(&HookNtCreateFile),
			reinterpret_cast<LPVOID*>(&g_originalNtCreateFile));
		if (unlock && created)
			WriteLog(kTag, "UNLOCK_FILES: NtCreateFile is hooked - a read-only open of a file in the game folder other than .rpf "
				"and .cache that shares reading gets write and delete sharing too, so those files stay free for other programs.");
		else if (unlock)
			ReportFailure(kTag, "The files in the game folder are not unlocked this session: NtCreateFile could not be hooked "
				"(details in TweaksV.log).");
		const bool renamed = unlock && InstallHook(L"ntdll.dll", "NtSetInformationFile",
			reinterpret_cast<LPVOID>(&HookNtSetInformationFile), reinterpret_cast<LPVOID*>(&g_originalNtSetInformationFile));
		if (unlock && renamed)
			WriteLog(kTag, "UNLOCK_FILES: NtSetInformationFile is hooked - a rename inside the game that replaces a file in the "
				"game folder gets POSIX semantics, so it goes through while the game holds that file open.");
		else if (unlock)
			ReportFailure(kTag, "The replace-renames into the game folder are not given POSIX semantics this session: "
				"NtSetInformationFile could not be hooked (details in TweaksV.log).");
		if (unlock && (created || renamed))
		{
			StartMeasurement<kUnlockMeasurement>();	// the summary; one that cannot start is logged
			if (!identity)
				ReportFailure(kTag, "Files in the game folder opened by a relative path are not unlocked this session: the game "
					"folder's file ID could not be read (" + DescribeError(identityError) + ").");
			if (renamed && !path)
				ReportFailure(kTag, "Replace-renames in the game folder by a bare name are not given POSIX semantics this session: "
					"the game folder's path could not be read (" + DescribeError(pathError) + ").");
		}
		if (diagnostic && created)
		{
			WriteLog(kTag, "DIAGNOSTIC: every open of a file or folder in " + ToUtf8(g_gameFolder) + " is logged, " +
				std::to_string(kLinesPerWay) + " lines per file, caller and sharing; a summary follows every minute.");
			StartMeasurement<kMeasurement>();	// one that cannot start is logged
		}
	}
}
