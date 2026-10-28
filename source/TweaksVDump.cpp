// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - TEST_DUMP_CODE and TEST_DUMP_SCRIPT, test builds only (see TweaksVDump.h).

#include "TweaksVDump.h"

#ifdef TWEAKSV_TEST_BUILD

#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "TweaksVFiles.h"
#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVScripts.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "TweaksV";	// the switches are TweaksV's own, not a part's
		constexpr char kMagic[8] = { 'T', 'W', 'K', 'V', 'D', 'U', 'M', 'P' };
		constexpr uint32_t kFormatVersion = 1;
		constexpr unsigned long kMaxSeconds = 3600;
		constexpr WORD kMaxSections = 96;				// as many as Windows loads
		constexpr uint64_t kMaxBytes = 1ull << 30;		// 1 GB for the image and for a copy - the game is about 90 MB
		constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
			PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

		// One copy of the game: the whole file, and what the log says about it.
		struct Copy
		{
			std::vector<uint8_t> file;
			std::string saved;		// " HEADERS .text ..." - the sections in it
			std::string leftOut;	// the sections the game writes to, not saved
			uint64_t unreadable = 0;
		};

		struct Part
		{
			std::string name;
			uint32_t rva;
			uint32_t size;
			uint32_t characteristics;
		};

		unsigned long g_seconds = 0;	// set once, before the thread starts

		// A section's name for the log and the file; a byte that cannot be printed becomes '?'.
		std::string SectionName(const BYTE (&raw)[IMAGE_SIZEOF_SHORT_NAME])
		{
			std::string name;
			for (BYTE c : raw)
			{
				if (!c)
					break;
				name += (c < 0x20 || c > 0x7E) ? '?' : static_cast<char>(c);
			}
			return name;
		}

		// Copies size bytes of the game from begin to out: what can be read as it is, the rest as zeros.
		// Returns how many bytes could not be read. A guard page is never touched.
		uint64_t CopyRange(const uint8_t* begin, size_t size, uint8_t* out)
		{
			uint64_t unreadable = 0;
			for (size_t done = 0; done < size;)
			{
				const uint8_t* at = begin + done;
				size_t take = size - done;
				bool readable = false;
				MEMORY_BASIC_INFORMATION info{};
				if (VirtualQuery(at, &info, sizeof(info)) == sizeof(info))
				{
					const uint8_t* regionEnd = static_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize;
					if (regionEnd > at)
						take = std::min<size_t>(take, static_cast<size_t>(regionEnd - at));
					readable = info.State == MEM_COMMIT && (info.Protect & kReadable) && !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS));
				}
				if (readable)
					memcpy(out + done, at, take);
				else
				{
					memset(out + done, 0, take);
					unreadable += take;
				}
				done += take;
			}
			return unreadable;
		}

		// The game's NT headers in memory, read only where they lie inside its image; nullptr if they are not
		// those of a program Windows loads.
		const IMAGE_NT_HEADERS* NtHeaders(const uint8_t* base, uint64_t imageSize)
		{
			const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			const bool dosOk = imageSize >= sizeof(IMAGE_DOS_HEADER) && imageSize <= kMaxBytes && dos->e_magic == IMAGE_DOS_SIGNATURE &&
				dos->e_lfanew > 0 && static_cast<uint64_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS) <= imageSize;
			const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + (dosOk ? dos->e_lfanew : 0));
			return dosOk && nt->Signature == IMAGE_NT_SIGNATURE ? nt : nullptr;
		}

		// A copy of the game as it is now - or nullptr, reported, if its headers in memory are not what
		// Windows loads. May throw std::bad_alloc.
		std::unique_ptr<Copy> TakeCopy(uint32_t secondsAfterStart)
		{
			const HMODULE module = GetModuleHandleW(nullptr);
			MODULEINFO image{};
			if (!module || !GetModuleInformation(GetCurrentProcess(), module, &image, sizeof(image)))
			{
				ReportFailure(kTag, "TEST_DUMP_CODE: the game was not found in memory - its code is not saved.");
				return nullptr;
			}
			const auto base = static_cast<const uint8_t*>(image.lpBaseOfDll);
			const uint64_t imageSize = image.SizeOfImage;

			// the headers are read only where they lie inside the image
			const IMAGE_NT_HEADERS* const nt = NtHeaders(base, imageSize);
			const IMAGE_SECTION_HEADER* sections = nt ? IMAGE_FIRST_SECTION(nt) : nullptr;
			const WORD count = nt ? nt->FileHeader.NumberOfSections : 0;
			if (!nt || count == 0 || count > kMaxSections || reinterpret_cast<const uint8_t*>(sections + count) > base + imageSize)
			{
				ReportFailure(kTag, "TEST_DUMP_CODE: the game's headers in memory are not those of a program Windows "
					"loads - its code is not saved.");
				return nullptr;
			}

			auto copy = std::make_unique<Copy>();
			std::vector<Part> parts;
			const uint64_t headers = std::min<uint64_t>(nt->OptionalHeader.SizeOfHeaders, imageSize);
			if (headers)
				parts.push_back({ "HEADERS", 0, static_cast<uint32_t>(headers), 0 });
			for (WORD i = 0; i < count; i++)
			{
				const IMAGE_SECTION_HEADER& section = sections[i];
				const std::string name = SectionName(section.Name);
				if ((section.Characteristics & IMAGE_SCN_MEM_WRITE) && !(section.Characteristics & IMAGE_SCN_MEM_EXECUTE))
				{
					copy->leftOut += " " + name;	// the game's data: never saved
					continue;
				}
				const uint64_t length = section.Misc.VirtualSize ? section.Misc.VirtualSize : section.SizeOfRawData;
				if (section.VirtualAddress >= imageSize || length == 0)
					continue;
				parts.push_back({ name, section.VirtualAddress,
					static_cast<uint32_t>(std::min<uint64_t>(length, imageSize - section.VirtualAddress)), section.Characteristics });
			}

			uint64_t total = sizeof(DumpHeader) + parts.size() * sizeof(DumpSection);
			for (const Part& part : parts)
				total += part.size;
			if (total > kMaxBytes)
			{
				ReportFailure(kTag, "TEST_DUMP_CODE: the game's sections add up to more than 1 GB - its code is not saved.");
				return nullptr;
			}
			copy->file.resize(static_cast<size_t>(total));

			DumpHeader header{};
			memcpy(header.magic, kMagic, sizeof(kMagic));
			header.formatVersion = kFormatVersion;
			header.sectionCount = static_cast<uint32_t>(parts.size());
			header.imageBase = reinterpret_cast<uintptr_t>(base);
			header.imageSize = static_cast<uint32_t>(imageSize);
			header.timeDateStamp = nt->FileHeader.TimeDateStamp;
			header.secondsAfterStart = secondsAfterStart;
			memcpy(copy->file.data(), &header, sizeof(header));

			uint64_t at = sizeof(DumpHeader) + parts.size() * sizeof(DumpSection);
			for (size_t i = 0; i < parts.size(); i++)
			{
				const Part& part = parts[i];
				DumpSection entry{};
				memcpy(entry.name, part.name.data(), std::min<size_t>(part.name.size(), sizeof(entry.name)));
				entry.rva = part.rva;
				entry.size = part.size;
				entry.characteristics = part.characteristics;
				entry.fileOffset = at;
				const uint64_t unreadable = CopyRange(base + part.rva, part.size, copy->file.data() + at);
				entry.unreadable = static_cast<uint32_t>(unreadable);
				memcpy(copy->file.data() + sizeof(DumpHeader) + i * sizeof(DumpSection), &entry, sizeof(entry));
				copy->unreadable += unreadable;
				copy->saved += " " + part.name;
				at += part.size;
			}
			return copy;
		}

		// Writes bytes to %LOCALAPPDATA%\TweaksV\name, the folder created if need be and a file of that name
		// replaced; reported, what naming the switch and the file, if it cannot be. True once written whole.
		bool SaveFile(const std::string& what, const wchar_t* name, const std::vector<uint8_t>& bytes)
		{
			const std::wstring folder = GetFallbackFolder();
			if (folder.empty())
			{
				ReportFailure(kTag, what + " was not saved: LOCALAPPDATA is not set.");
				return false;
			}
			CreateDirectoryW(folder.c_str(), nullptr);
			const OwnFile file = OpenOwnFile(folder, name, FileFolder::Own, OPEN_ALWAYS, true);
			if (file.handle == INVALID_HANDLE_VALUE)
			{
				ReportFailure(kTag, what + " was not saved: " + file.Reason() + ".");
				return false;
			}

			DWORD error = ERROR_SUCCESS;
			for (size_t done = 0; error == ERROR_SUCCESS && done < bytes.size();)
			{
				const DWORD chunk = static_cast<DWORD>(std::min<size_t>(bytes.size() - done, 16u << 20));
				DWORD wrote = 0;
				if (!WriteFile(file.handle, bytes.data() + done, chunk, &wrote, nullptr))
					error = GetLastError();
				else if (wrote != chunk)
					error = ERROR_DISK_FULL;
				done += wrote;
			}
			CloseHandle(file.handle);
			if (error != ERROR_SUCCESS)
			{
				ReportFailure(kTag, what + " is incomplete: it could not be written to the end (" + DescribeError(error) + ").");
				return false;
			}
			return true;
		}

		// Writes a copy to %LOCALAPPDATA%\TweaksV\name; reported if it cannot be.
		void SaveCopy(const Copy& copy, const wchar_t* name, uint32_t secondsAfterStart)
		{
			if (!SaveFile("TEST_DUMP_CODE: " + ToUtf8(name), name, copy.file))
				return;

			const std::wstring folder = GetFallbackFolder();
			char size[32];
			snprintf(size, sizeof(size), "%.1f MB", static_cast<double>(copy.file.size()) / (1024.0 * 1024.0));
			WriteLog(kTag, "TEST BUILD: " + ToUtf8(name) + " saved, the game as it was " + std::to_string(secondsAfterStart) +
				" s after TweaksV started" + (secondsAfterStart ? std::string() : std::string(", before anything was patched")) + ": " +
				size + " in " + ToUtf8(folder) + " - sections" + copy.saved + "; " +
				(copy.unreadable ? std::to_string(copy.unreadable) + " bytes unreadable, saved as zeros" : std::string("none unreadable")) +
				"; not saved (the game writes to them):" + (copy.leftOut.empty() ? std::string(" none") : copy.leftOut) + ".");
		}

		DWORD WINAPI CopyThread(void* parameter)
		{
			std::unique_ptr<Copy> first(static_cast<Copy*>(parameter));
			try
			{
				SaveCopy(*first, L"GameCode-start.bin", 0);
				first.reset();	// not kept while waiting
				Sleep(g_seconds * 1000);
				if (const std::unique_ptr<Copy> later = TakeCopy(g_seconds))
					SaveCopy(*later, L"GameCode-later.bin", g_seconds);
			}
			catch (...)
			{
				ReportFailure(kTag, "TEST_DUMP_CODE: not enough memory for a copy of the game's code - not saved.");
			}
			return 0;
		}

		// ---- TEST_DUMP_SCRIPT

		constexpr char kScriptMagic[8] = { 'T', 'W', 'K', 'V', 'S', 'C', 'R', 'P' };
		constexpr uint32_t kScriptFormatVersion = 1;
		constexpr size_t kMaxScriptName = 63;
		static_assert(Scripts::kNameRead == kMaxScriptName + 1, "TweaksV: a program's name is read within the longest name and its zero");
		constexpr uint32_t kMaxScriptCopies = 16;			// a session

		// Set before the hook can call: the name asked for, in lower case, and its hash as the game makes it
		std::string g_script;
		uint32_t g_scriptHash = 0;
		uint64_t g_imageBase = 0;
		uint32_t g_imageSize = 0;
		uint32_t g_timeDateStamp = 0;
		// On the game's thread that places the programs
		std::atomic<uint32_t> g_placements{ 0 };	// the program's placements, saved or not
		std::atomic<bool> g_firstPlacement{ true };
		std::atomic<bool> g_halfMatchLogged{ false };
		std::atomic<bool> g_unreadableLogged{ false };
		std::atomic<bool> g_capLogged{ false };

		// One copy in the making, from before the game resolves the natives to after.
		struct ScriptCopy
		{
			ScriptDumpHeader header{};
			std::vector<uint8_t> file;	// the whole file; the header goes in last
			std::wstring name;			// Script-<name>-<n>.bin
		};

		std::string Hex(uint32_t value)
		{
			char text[16];
			snprintf(text, sizeof(text), "0x%08X", value);
			return text;
		}

		std::string Ordinal(uint32_t n)
		{
			const uint32_t lastTwo = n % 100;
			const char* const suffix = lastTwo >= 11 && lastTwo <= 13 ? "th" : n % 10 == 1 ? "st" : n % 10 == 2 ? "nd" : n % 10 == 3 ? "rd" : "th";
			return std::to_string(n) + suffix;
		}

		// "1 page", "2 pages"
		std::string Counted(uint32_t n, const char* thing)
		{
			return std::to_string(n) + " " + thing + (n == 1 ? "" : "s");
		}

		// True if a name Scripts::ReadName read is the one asked for, A to Z taken as a to z.
		bool IsAskedFor(const char (&name)[Scripts::kNameRead])
		{
			const size_t length = strnlen(name, Scripts::kNameRead);
			if (length != g_script.size())
				return false;
			for (size_t i = 0; i < length; i++)
				if ((name[i] >= 'A' && name[i] <= 'Z' ? name[i] - 'A' + 'a' : name[i]) != g_script[i])
					return false;
			return true;
		}

		// size bytes from address to out where they can be read, as zeros where not; the bytes that could not.
		uint32_t CopyBlock(const uint8_t* address, size_t size, uint8_t* out)
		{
			if (size == 0)
				return 0;
			if (IsReadable(address, size))
			{
				memcpy(out, address, size);
				return 0;
			}
			memset(out, 0, size);
			return static_cast<uint32_t>(size);
		}

		// length bytes from the pages the page table at table lists, page after page, to out, each page as
		// CopyBlock copies it; the bytes that could not be read.
		uint32_t CopyPages(const uint8_t* table, uint32_t length, uint8_t* out)
		{
			const uint32_t pages = Scripts::PageCount(length);
			if (pages == 0)
				return 0;
			if (!IsReadable(table, size_t{ pages } * sizeof(void*)))
			{
				memset(out, 0, length);
				return length;
			}
			uint32_t unreadable = 0;
			for (uint32_t page = 0; page < pages; page++)
				unreadable += CopyBlock(Scripts::Field<const uint8_t*>(table, size_t{ page } * sizeof(void*)), Scripts::PageLength(length, page),
					out + size_t{ page } * Scripts::kPageSize);
			return unreadable;
		}

		// Before the game resolves a program's natives: the program asked for gets its copy started - the file
		// allocated and the native table copied while it still holds the hashes. A program with only the name
		// or only the hash is logged once; the first program of all is logged, whatever its name.
		void* BeforeResolution(uint8_t* program, uint32_t placed)
		{
			if (!IsReadable(program, Scripts::kProgramSize))
			{
				if (!g_unreadableLogged.exchange(true))
					WriteLog(kTag, "TEST BUILD: TEST_DUMP_SCRIPT: a script program at " + DescribeAddress(program) + " cannot be read, so it "
						"is not looked at (logged once).");
				return nullptr;
			}
			const uint32_t hash = Scripts::Field<uint32_t>(program, Scripts::kNameHashOffset);
			char name[Scripts::kNameRead];
			const bool named = Scripts::ReadName(Scripts::Field<const char*>(program, Scripts::kNameOffset), name);
			const auto shown = [&] { return named ? Scripts::Printable(name) : std::string("(a name that cannot be read)"); };
			if (g_firstPlacement.exchange(false))
				WriteLog(kTag, "TEST BUILD: TEST_DUMP_SCRIPT: the first script program the game placed is " + shown() + " (hash " + Hex(hash) +
					"), on thread " + std::to_string(GetCurrentThreadId()) + " - the hook runs.");

			const bool hashMatches = hash == g_scriptHash;
			const bool nameMatches = named && IsAskedFor(name);
			if (!hashMatches && !nameMatches)
				return nullptr;
			if (!hashMatches || !nameMatches)
			{
				if (!g_halfMatchLogged.exchange(true))
					WriteLog(kTag, "TEST BUILD: TEST_DUMP_SCRIPT: a script program " + (hashMatches ? "has the hash of " + g_script + ", " +
						Hex(g_scriptHash) + ", but is named " + shown() : "is named " + shown() + " but has the hash " + Hex(hash) + ", not " +
						Hex(g_scriptHash)) + " - it is not saved (logged once).");
				return nullptr;
			}

			const uint32_t placement = g_placements.fetch_add(1) + 1;
			if (placement > kMaxScriptCopies)
			{
				if (!g_capLogged.exchange(true))
					WriteLog(kTag, "TEST BUILD: " + std::to_string(kMaxScriptCopies) + " copies of " + g_script + " saved - from now on its "
						"placements are only counted.");
				return nullptr;
			}
			const std::wstring file = L"Script-" + FromUtf8(g_script) + L"-" + std::to_wstring(placement) + L".bin";
			const std::string what = "TEST_DUMP_SCRIPT: " + ToUtf8(file);
			const uint32_t codeLength = Scripts::Field<uint32_t>(program, Scripts::kCodeLengthOffset);
			const uint32_t nativeCount = Scripts::Field<uint32_t>(program, Scripts::kNativeCountOffset);
			const uint32_t stringLength = Scripts::Field<uint32_t>(program, Scripts::kStringLengthOffset);
			std::string beyond;
			if (codeLength > Scripts::kMaxLength)
				beyond = "its code length, " + std::to_string(codeLength) + " bytes, is beyond 16 MB.";
			else if (nativeCount > Scripts::kMaxNatives)
				beyond = "its native count, " + std::to_string(nativeCount) + ", is beyond " + std::to_string(Scripts::kMaxNatives) + ".";
			else if (stringLength > Scripts::kMaxLength)
				beyond = "its string length, " + std::to_string(stringLength) + " bytes, is beyond 16 MB.";
			if (!beyond.empty())
			{
				ReportFailure(kTag, what + " was not saved: " + beyond);
				return nullptr;
			}

			try
			{
				auto copy = std::make_unique<ScriptCopy>();
				copy->name = file;
				ScriptDumpHeader& header = copy->header;
				memcpy(header.magic, kScriptMagic, sizeof(kScriptMagic));
				header.formatVersion = kScriptFormatVersion;
				header.headerSize = sizeof(ScriptDumpHeader);
				memcpy(header.name, name, sizeof(header.name));
				header.nameHash = hash;
				header.codeLength = codeLength;
				header.nativeCount = nativeCount;
				header.stringLength = stringLength;
				header.placement = placement;
				header.threadId = GetCurrentThreadId();
				header.programsPlaced = placed;
				header.imageBase = g_imageBase;
				header.imageSize = g_imageSize;
				header.timeDateStamp = g_timeDateStamp;
				const uint64_t natives = uint64_t{ nativeCount } * sizeof(uint64_t);
				header.programOffset = sizeof(ScriptDumpHeader);
				header.codeOffset = header.programOffset + Scripts::kProgramSize;
				header.nativesBeforeOffset = header.codeOffset + codeLength;
				header.nativesAfterOffset = header.nativesBeforeOffset + natives;
				header.stringsOffset = header.nativesAfterOffset + natives;
				copy->file.resize(static_cast<size_t>(header.stringsOffset + stringLength));
				header.unreadable = CopyBlock(Scripts::Field<const uint8_t*>(program, Scripts::kNativesOffset), static_cast<size_t>(natives),
					copy->file.data() + header.nativesBeforeOffset);
				return copy.release();
			}
			catch (...)
			{
				ReportFailure(kTag, what + " was not saved: not enough memory for a copy of " + g_script + ".");
				return nullptr;
			}
		}

		// After the game has resolved the natives: the rest of the copy - the program object, its code, the native
		// table as the game left it, its strings - and the file.
		void AfterResolution(uint8_t* program, uint8_t resolved, void* context)
		{
			const std::unique_ptr<ScriptCopy> copy(static_cast<ScriptCopy*>(context));
			if (!copy)
				return;
			ScriptDumpHeader& header = copy->header;
			uint8_t* const file = copy->file.data();
			const bool readable = IsReadable(program, Scripts::kProgramSize);
			const auto pointer = [&](size_t offset) { return readable ? Scripts::Field<const uint8_t*>(program, offset) : nullptr; };
			header.resolved = resolved;
			header.unreadable += CopyBlock(program, Scripts::kProgramSize, file + header.programOffset);
			header.unreadable += CopyPages(pointer(Scripts::kCodePagesOffset), header.codeLength, file + header.codeOffset);
			header.unreadable += CopyBlock(pointer(Scripts::kNativesOffset), static_cast<size_t>(header.nativesAfterOffset - header.nativesBeforeOffset),
				file + header.nativesAfterOffset);
			header.unreadable += CopyPages(pointer(Scripts::kStringPagesOffset), header.stringLength, file + header.stringsOffset);
			memcpy(file, &header, sizeof(header));

			try
			{
				if (!SaveFile("TEST_DUMP_SCRIPT: " + ToUtf8(copy->name), copy->name.c_str(), copy->file))
					return;
				WriteLog(kTag, "TEST BUILD: " + ToUtf8(copy->name) + " saved in " + ToUtf8(GetFallbackFolder()) + ": " + g_script + " placed the " +
					Ordinal(header.placement) + " time, as the game's " + Ordinal(header.programsPlaced) + " script program, on thread " +
					std::to_string(header.threadId) + " - code " + Counted(header.codeLength, "byte") + " in " +
					Counted(Scripts::PageCount(header.codeLength), "page") + ", " + Counted(header.nativeCount, "native") + ", " +
					(resolved == 1 ? "every one resolved" : "not every one resolved") + ", strings " + Counted(header.stringLength, "byte") + "; " +
					(header.unreadable ? Counted(header.unreadable, "byte") + " unreadable, saved as zeros" : std::string("none unreadable")) + ".");
			}
			catch (...)
			{
				ReportFailure(kTag, "TEST_DUMP_SCRIPT: " + ToUtf8(copy->name) + " was not saved: not enough memory.");
			}
		}

		constexpr Scripts::User kScriptDumper = { kTag, &BeforeResolution, &AfterResolution };
	}

	void StartCodeDump()
	{
		const std::string& value = GetText(kTestDumpCode);
		if (value.empty())
			return;
		char* end = nullptr;
		const unsigned long seconds = strtoul(value.c_str(), &end, 10);
		if (value[0] < '0' || value[0] > '9' || *end != '\0' || seconds < 1 || seconds > kMaxSeconds)
		{
			ReportFailure(kTag, "TweaksV.ini: [TWEAKSV] TEST_DUMP_CODE=" + value + " is not a number of seconds from 1 to " +
				std::to_string(kMaxSeconds) + " - the game's code is not saved.");
			return;
		}
		g_seconds = seconds;
		try
		{
			std::unique_ptr<Copy> first = TakeCopy(0);	// now: before any part of TweaksV changes the game
			if (!first)
				return;
			// DllMain does not wait for the thread: it writes this copy, waits and takes the second
			const HANDLE thread = CreateThread(nullptr, 0, CopyThread, first.get(), 0, nullptr);
			if (!thread)
			{
				ReportFailure(kTag, "TEST_DUMP_CODE: its thread could not be started (" + DescribeError(GetLastError()) +
					") - the game's code is not saved.");
				return;
			}
			static_cast<void>(first.release());	// the thread owns it now
			CloseHandle(thread);
		}
		catch (...)
		{
			ReportFailure(kTag, "TEST_DUMP_CODE: not enough memory for a copy of the game's code - not saved.");
		}
	}

	void StartScriptDump()
	{
		const std::string& value = GetText(kTestDumpScript);
		if (value.empty())
			return;
		bool isName = value.size() <= kMaxScriptName;
		for (const char c : value)
			isName = isName && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
		if (!isName)
		{
			ReportFailure(kTag, "TweaksV.ini: [TWEAKSV] TEST_DUMP_SCRIPT=" + value + " is not a script's name of 1 to " +
				std::to_string(kMaxScriptName) + " letters, digits and _ - no script program is saved.");
			return;
		}
		const std::wstring folder = GetFallbackFolder();
		if (folder.empty())
		{
			ReportFailure(kTag, "TEST_DUMP_SCRIPT: LOCALAPPDATA is not set - no script program is saved.");
			return;
		}

		// what the hook needs, before it can run: the name and its hash, and the game for the files' headers
		g_script = value;
		for (char& c : g_script)
			c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
		g_scriptHash = Scripts::NameHash(g_script);
		const HMODULE module = GetModuleHandleW(nullptr);
		MODULEINFO image{};
		if (module && GetModuleInformation(GetCurrentProcess(), module, &image, sizeof(image)))
		{
			g_imageBase = reinterpret_cast<uintptr_t>(image.lpBaseOfDll);
			g_imageSize = image.SizeOfImage;
			if (const IMAGE_NT_HEADERS* const nt = NtHeaders(static_cast<const uint8_t*>(image.lpBaseOfDll), image.SizeOfImage))
				g_timeDateStamp = nt->FileHeader.TimeDateStamp;
		}

		std::string reason;
		if (!Scripts::AddUser(kScriptDumper, reason))
		{
			ReportFailure(kTag, "TEST_DUMP_SCRIPT: the script program " + g_script + " is not saved this session: " + reason);
			return;
		}
		if (IsDryRun())
			WriteLog(kTag, "TEST BUILD: TEST_DUMP_SCRIPT=" + value + ": DRY_RUN - the call is not redirected, so no script program is saved.");
		else
			WriteLog(kTag, "TEST BUILD: TEST_DUMP_SCRIPT=" + value + ": the script program " + g_script + " (hash " + Hex(g_scriptHash) +
				") is saved each time the game places it, at most " + std::to_string(kMaxScriptCopies) + " times, as Script-" + g_script +
				"-<n>.bin in " + ToUtf8(folder) + ".");
	}
}

#endif
