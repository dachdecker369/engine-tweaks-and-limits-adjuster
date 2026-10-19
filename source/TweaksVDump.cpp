// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - TEST_DUMP_CODE, test builds only (see TweaksVDump.h).

#include "TweaksVDump.h"

#ifdef TWEAKSV_TEST_BUILD

#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "TweaksVFiles.h"
#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
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

		// A copy of the game as it is now - or nullptr, reported, if its headers in memory are not what
		// Windows loads. May throw std::bad_alloc.
		std::unique_ptr<Copy> TakeCopy(uint32_t secondsAfterStart)
		{
			const HMODULE module = GetModuleHandleW(nullptr);
			MODULEINFO image{};
			if (!module || !GetModuleInformation(GetCurrentProcess(), module, &image, sizeof(image)))
			{
				ReportFailure("TweaksV", "TEST_DUMP_CODE: the game was not found in memory - its code is not saved.");
				return nullptr;
			}
			const auto base = static_cast<const uint8_t*>(image.lpBaseOfDll);
			const uint64_t imageSize = image.SizeOfImage;

			// the headers are read only where they lie inside the image
			const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			const bool dosOk = imageSize >= sizeof(IMAGE_DOS_HEADER) && imageSize <= kMaxBytes && dos->e_magic == IMAGE_DOS_SIGNATURE &&
				dos->e_lfanew > 0 && static_cast<uint64_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS) <= imageSize;
			const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + (dosOk ? dos->e_lfanew : 0));
			const IMAGE_SECTION_HEADER* sections = dosOk ? IMAGE_FIRST_SECTION(nt) : nullptr;
			const WORD count = dosOk ? nt->FileHeader.NumberOfSections : 0;
			if (!dosOk || nt->Signature != IMAGE_NT_SIGNATURE || count == 0 || count > kMaxSections ||
				reinterpret_cast<const uint8_t*>(sections + count) > base + imageSize)
			{
				ReportFailure("TweaksV", "TEST_DUMP_CODE: the game's headers in memory are not those of a program Windows "
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
				ReportFailure("TweaksV", "TEST_DUMP_CODE: the game's sections add up to more than 1 GB - its code is not saved.");
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

		// Writes a copy to %LOCALAPPDATA%\TweaksV\name; reported if it cannot be.
		void SaveCopy(const Copy& copy, const wchar_t* name, uint32_t secondsAfterStart)
		{
			const std::string what = "TEST_DUMP_CODE: " + ToUtf8(name);
			const std::wstring folder = GetFallbackFolder();
			if (folder.empty())
			{
				ReportFailure("TweaksV", what + " was not saved: LOCALAPPDATA is not set.");
				return;
			}
			CreateDirectoryW(folder.c_str(), nullptr);
			const OwnFile file = OpenOwnFile(folder, name, FileFolder::Own, OPEN_ALWAYS, true);
			if (file.handle == INVALID_HANDLE_VALUE)
			{
				ReportFailure("TweaksV", what + " was not saved: " + file.Reason() + ".");
				return;
			}

			DWORD error = ERROR_SUCCESS;
			for (size_t done = 0; error == ERROR_SUCCESS && done < copy.file.size();)
			{
				const DWORD chunk = static_cast<DWORD>(std::min<size_t>(copy.file.size() - done, 16u << 20));
				DWORD wrote = 0;
				if (!WriteFile(file.handle, copy.file.data() + done, chunk, &wrote, nullptr))
					error = GetLastError();
				else if (wrote != chunk)
					error = ERROR_DISK_FULL;
				done += wrote;
			}
			CloseHandle(file.handle);
			if (error != ERROR_SUCCESS)
			{
				ReportFailure("TweaksV", what + " is incomplete: it could not be written to the end (" + DescribeError(error) + ").");
				return;
			}

			char size[32];
			snprintf(size, sizeof(size), "%.1f MB", static_cast<double>(copy.file.size()) / (1024.0 * 1024.0));
			WriteLog("TweaksV", "TEST BUILD: " + ToUtf8(name) + " saved, the game as it was " + std::to_string(secondsAfterStart) +
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
				ReportFailure("TweaksV", "TEST_DUMP_CODE: not enough memory for a copy of the game's code - not saved.");
			}
			return 0;
		}
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
			ReportFailure("TweaksV", "TweaksV.ini: [TWEAKSV] TEST_DUMP_CODE=" + value + " is not a number of seconds from 1 to " +
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
				ReportFailure("TweaksV", "TEST_DUMP_CODE: its thread could not be started (" + DescribeError(GetLastError()) +
					") - the game's code is not saved.");
				return;
			}
			static_cast<void>(first.release());	// the thread owns it now
			CloseHandle(thread);
		}
		catch (...)
		{
			ReportFailure("TweaksV", "TEST_DUMP_CODE: not enough memory for a copy of the game's code - not saved.");
		}
	}
}

#endif
