// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - small helpers for writing game code, shared by the merged plugins (plan 5.9 R1/R3).

#include "TweaksVPatch.h"

#include <cstdio>
#include <cstring>

#include "TweaksVConflicts.h"
#include "TweaksVLog.h"

namespace TweaksV
{
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

	bool MakeWritable(const std::vector<uint8_t*>& sites, size_t size, std::vector<DWORD>& oldProtects, const char* tag)
	{
		oldProtects.assign(sites.size(), 0);
		for (size_t i = 0; i < sites.size(); i++)
		{
			if (!VirtualProtect(sites[i], size, PAGE_EXECUTE_READWRITE, &oldProtects[i]))
			{
				const DWORD error = GetLastError();
				for (size_t j = i; j-- > 0;)	// newest first - see RestoreProtection
				{
					DWORD unused = 0;
					VirtualProtect(sites[j], size, oldProtects[j], &unused);
				}
				WriteLog(tag, "The game code at " + DescribeAddress(sites[i]) + " could not be made writable (" +
					DescribeError(error) + ").");
				return false;
			}
		}
		return true;
	}

	void RestoreProtection(const std::vector<uint8_t*>& sites, size_t size, const std::vector<DWORD>& oldProtects)
	{
		// Newest first: when two sites share a page, only the first one saw the page's real
		// protection (the second saw PAGE_EXECUTE_READWRITE), so the first must be restored last.
		for (size_t i = (sites.size() < oldProtects.size() ? sites.size() : oldProtects.size()); i-- > 0;)
		{
			DWORD unused = 0;
			VirtualProtect(sites[i], size, oldProtects[i], &unused);
			FlushInstructionCache(GetCurrentProcess(), sites[i], size);
		}
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
