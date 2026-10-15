// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - small helpers for writing game code, shared by the merged plugins (plan 5.9 R1/R3).
#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace TweaksV
{
	// "RVA 0x1234" for an address inside GTA5_Enhanced.exe, "0x7FF6... (outside the game)"
	// otherwise - for log lines.
	std::string DescribeAddress(const void* address);

	// Makes every site (size bytes each) writable before anything is written, so a change is made
	// everywhere or nowhere. On failure it restores what it changed, logs the error under tag and
	// returns false.
	bool MakeWritable(const std::vector<uint8_t*>& sites, size_t size, std::vector<DWORD>& oldProtects, const char* tag);

	// Restores the protections saved by MakeWritable, newest first (sites may share a page), and
	// flushes the instruction cache.
	void RestoreProtection(const std::vector<uint8_t*>& sites, size_t size, const std::vector<DWORD>& oldProtects);

	// The target of the 5-byte relative call (E8 rel32) at site, or nullptr if site is not one.
	const uint8_t* CallTarget(const void* site);

	// True if the code at address starts with a jump (E9, EB or FF 25) - the usual sign that
	// another mod has already hooked that function (plan 4.4).
	bool StartsWithJump(const void* address);

	// True if address is where a function starts (plan 5.9 R9): the game's unwind data says so,
	// or - for code without unwind data - the byte before it is padding or a ret (CC, 90, C3).
	// detail says which, for the log.
	bool IsFunctionStart(const void* address, std::string& detail);
}
