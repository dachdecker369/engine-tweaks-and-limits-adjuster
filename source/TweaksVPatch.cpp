// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the one place that patches the game's code (see TweaksVPatch.h).

#include "TweaksVPatch.h"

#include <cstdio>
#include <cstring>

#include "StubMemory.h"
#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVSettings.h"

namespace TweaksV
{
	namespace
	{
		constexpr DWORD kWritable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
		constexpr DWORD kExecutable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
		constexpr size_t kStubSize = 16;	// FF 25 00000000 <address of 8 bytes>, then 2 x CC

		struct ChangedPage
		{
			uint8_t* page;
			DWORD oldProtect;
		};

		size_t PageSize()
		{
			static const size_t size = []
			{
				SYSTEM_INFO info{};
				GetSystemInfo(&info);
				return static_cast<size_t>(info.dwPageSize ? info.dwPageSize : 0x1000);
			}();
			return size;
		}

		// Puts the protection of every changed page back, newest first.
		void Restore(const std::vector<ChangedPage>& changed)
		{
			for (size_t i = changed.size(); i-- > 0;)
			{
				DWORD unused = 0;
				VirtualProtect(changed[i].page, 1, changed[i].oldProtect, &unused);
			}
		}

		// Makes every page of [address, address + size) writable that is not yet, adding it to changed.
		// A page that is not committed memory, or a guard page, is never touched.
		bool MakePagesWritable(uint8_t* address, size_t size, std::vector<ChangedPage>& changed, const char* tag)
		{
			const uintptr_t pageSize = PageSize();
			const uintptr_t first = reinterpret_cast<uintptr_t>(address) & ~(pageSize - 1);
			const uintptr_t last = (reinterpret_cast<uintptr_t>(address) + size - 1) & ~(pageSize - 1);
			for (uintptr_t page = first; page <= last; page += pageSize)
			{
				MEMORY_BASIC_INFORMATION info{};
				if (VirtualQuery(reinterpret_cast<void*>(page), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
					(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
				{
					WriteLog(tag, "The game code at " + DescribeAddress(address) + " is not memory TweaksV may change.");
					return false;
				}
				if (info.Protect & kWritable)
					continue;
				const DWORD writable = (info.Protect & kExecutable) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
				DWORD old = 0;
				if (!VirtualProtect(reinterpret_cast<void*>(page), 1, writable, &old))
				{
					const DWORD error = GetLastError();
					WriteLog(tag, "The game code at " + DescribeAddress(address) + " could not be made writable (" +
						DescribeError(error) + ").");
					return false;
				}
				changed.push_back({ reinterpret_cast<uint8_t*>(page), old });
			}
			return true;
		}
	}

	std::string DescribeAddress(const void* address)
	{
		char text[64];
		if (IsInsideGameImage(address))
		{
			const auto base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
			snprintf(text, sizeof(text), "RVA 0x%llX",
				static_cast<unsigned long long>(static_cast<const uint8_t*>(address) - base));
		}
		else
		{
			snprintf(text, sizeof(text), "0x%llX (outside the game)",
				static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address)));
		}
		return text;
	}

	std::string HexBytes(const void* bytes, size_t size)
	{
		std::string text;
		const auto p = static_cast<const uint8_t*>(bytes);
		for (size_t i = 0; i < size; i++)
		{
			char byteText[4];
			snprintf(byteText, sizeof(byteText), i ? " %02X" : "%02X", p[i]);
			text += byteText;
		}
		return text;
	}

	bool WriteCode(const std::vector<CodeWrite>& writes, const char* tag)
	{
		for (const CodeWrite& write : writes)
			if (!write.address || write.bytes.empty())
			{
				WriteLog(tag, "A write of the " + write.what + " has no address or no bytes - nothing is written.");
				return false;
			}

		if (IsDryRun())
		{
			for (const CodeWrite& write : writes)
				WriteLog(tag, "DRY_RUN: would patch the " + write.what + ": " + HexBytes(write.address, write.bytes.size()) +
					" -> " + HexBytes(write.bytes.data(), write.bytes.size()));
			return true;
		}

		// 1. every page writable before anything is written
		std::vector<ChangedPage> changed;
		for (const CodeWrite& write : writes)
			if (!MakePagesWritable(write.address, write.bytes.size(), changed, tag))
			{
				Restore(changed);
				return false;
			}

		// 2. the writes, the old bytes kept for the log
		const bool diagnostic = IsDiagnosticMode();
		for (const CodeWrite& write : writes)
		{
			const std::string before = diagnostic ? HexBytes(write.address, write.bytes.size()) : std::string();
			memcpy(write.address, write.bytes.data(), write.bytes.size());
			if (diagnostic)
				WriteLog(tag, "DIAGNOSTIC: " + write.what + " at " + DescribeAddress(write.address) + ": " + before + " -> " +
					HexBytes(write.address, write.bytes.size()));
		}

		// 3. every page's own protection back, and the new code visible to the processor
		Restore(changed);
		for (const CodeWrite& write : writes)
			FlushInstructionCache(GetCurrentProcess(), write.address, write.bytes.size());
		return true;
	}

	bool RedirectCalls(const std::vector<CallRedirect>& redirects, std::vector<CodeWrite> alsoWrite, const char* tag)
	{
		for (const CallRedirect& redirect : redirects)
			if (!CallTarget(redirect.site) || !redirect.function)
			{
				WriteLog(tag, "The " + redirect.what + " at " + DescribeAddress(redirect.site) + " is not a call - nothing is changed.");
				return false;
			}

		if (IsDryRun())
		{
			for (const CallRedirect& redirect : redirects)
				WriteLog(tag, "DRY_RUN: would redirect the " + redirect.what + " at " + DescribeAddress(redirect.site) + ".");
			return WriteCode(alsoWrite, tag);
		}

		if (redirects.empty())
			return WriteCode(alsoWrite, tag);

		// the stubs, in a block of their own near the game: written, then read+execute only
		const size_t blockSize = redirects.size() * kStubSize;
		uint8_t* const block = static_cast<uint8_t*>(AllocateStubMemory(blockSize, PAGE_READWRITE));
		if (!block)
		{
			WriteLog(tag, "No memory near the game could be had for the call stubs - nothing is changed.");
			return false;
		}
		for (size_t i = 0; i < redirects.size(); i++)
		{
			uint8_t* const stub = block + i * kStubSize;
			const uint8_t jump[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };	// jmp [rip+0]: the address right after
			memcpy(stub, jump, sizeof(jump));
			const uintptr_t function = reinterpret_cast<uintptr_t>(redirects[i].function);
			memcpy(stub + 6, &function, sizeof(function));
			memset(stub + 14, 0xCC, kStubSize - 14);

			const intptr_t distance = reinterpret_cast<intptr_t>(stub) - (reinterpret_cast<intptr_t>(redirects[i].site) + 5);
			if (distance < INT32_MIN || distance > INT32_MAX)
			{
				VirtualFree(block, 0, MEM_RELEASE);
				WriteLog(tag, "The call stubs are out of reach of the " + redirects[i].what + " - nothing is changed.");
				return false;
			}
			std::vector<uint8_t> call = { 0xE8 };
			const std::vector<uint8_t> rel = BytesOf(static_cast<int32_t>(distance));
			call.insert(call.end(), rel.begin(), rel.end());
			alsoWrite.push_back({ redirects[i].site, call, redirects[i].what });
		}
		DWORD old = 0;
		if (!VirtualProtect(block, blockSize, PAGE_EXECUTE_READ, &old))
		{
			const DWORD error = GetLastError();
			VirtualFree(block, 0, MEM_RELEASE);
			WriteLog(tag, "The call stubs could not be made read+execute (" + DescribeError(error) + ") - nothing is changed.");
			return false;
		}
		FlushInstructionCache(GetCurrentProcess(), block, blockSize);

		for (const CallRedirect& redirect : redirects)
			if (redirect.original)
				*redirect.original = const_cast<uint8_t*>(CallTarget(redirect.site));

		if (!WriteCode(alsoWrite, tag))
		{
			VirtualFree(block, 0, MEM_RELEASE);	// no call leads to it: WriteCode wrote nothing
			return false;
		}
		return true;
	}

	const uint8_t* CallTarget(const void* site)
	{
		const auto code = static_cast<const uint8_t*>(site);
		if (!code || code[0] != 0xE8)
			return nullptr;
		int32_t rel = 0;
		memcpy(&rel, code + 1, sizeof(rel));
		return code + 5 + rel;
	}

	const uint8_t* Rel32Target(const void* address)
	{
		const auto field = static_cast<const uint8_t*>(address);
		int32_t rel = 0;
		memcpy(&rel, field, sizeof(rel));
		return field + sizeof(rel) + rel;
	}

	bool StartsWithJump(const void* address)
	{
		const auto code = static_cast<const uint8_t*>(address);
		return code && (code[0] == 0xE9 || code[0] == 0xEB || (code[0] == 0xFF && code[1] == 0x25));
	}

	bool IsFunctionStart(const void* address, std::string& detail)
	{
		const auto pc = reinterpret_cast<DWORD64>(address);
		DWORD64 imageBase = 0;
		if (const PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(pc, &imageBase, nullptr))
		{
			const DWORD64 begin = imageBase + entry->BeginAddress;
			if (begin == pc)
			{
				detail = "a function start (unwind data)";
				return true;
			}
			detail = "inside the function at " + DescribeAddress(reinterpret_cast<const void*>(begin)) + " (unwind data)";
			return false;
		}

		const uint8_t before = static_cast<const uint8_t*>(address)[-1];
		char text[64];
		snprintf(text, sizeof(text), "no unwind data, byte before it %02X", before);
		detail = text;
		return before == 0xCC || before == 0x90 || before == 0xC3;
	}
}
