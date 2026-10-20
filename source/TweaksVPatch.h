// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the one place that patches the game's code: WriteCode writes bytes, RedirectCalls sends
// calls to TweaksV. Both are all or nothing, give every page its own protection back and honour
// DRY_RUN and DIAGNOSTIC_MODE, so no part of TweaksV has to. Plus small helpers for reading code.
// A hook on a function's start - RageOpenV's file functions, DirectStorage's OpenFile - is MinHook's,
// through HookFunctions and HookExport below (TweaksVDirectStorage.cpp still calls MinHook itself).
#pragma once

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace TweaksV
{
	// "RVA 0x1234" for an address inside GTA5_Enhanced.exe, "0x7FF6... (outside the game)"
	// otherwise - for log lines.
	std::string DescribeAddress(const void* address);

	// "0F 84 07 05" - for log lines.
	std::string HexBytes(const void* bytes, size_t size);

	// One write into the game: the bytes to put at address. what names it for the log, e.g.
	// "splash screen" or "modkit limit".
	struct CodeWrite
	{
		uint8_t* address;
		std::vector<uint8_t> bytes;
		std::string what;
	};

	// The bytes of a value as it lies in memory, for a CodeWrite.
	template <typename T> std::vector<uint8_t> BytesOf(const T& value)
	{
		std::vector<uint8_t> bytes(sizeof(T));
		memcpy(bytes.data(), &value, sizeof(T));
		return bytes;
	}

	// Writes every write, or none. Every page a write touches is made writable first (a page that is
	// writable already is left alone, and a page that cannot be changed stops everything); then all are
	// written, every page gets its own protection back and the instruction cache is flushed. False if
	// nothing could be written - logged under tag.
	// DRY_RUN: nothing is written; "DRY_RUN: would patch the <what>: <old> -> <new>" is logged for each.
	// DIAGNOSTIC_MODE: "DIAGNOSTIC: <what> at <address>: <old> -> <new>" is logged for each write.
	bool WriteCode(const std::vector<CodeWrite>& writes, const char* tag);

	// One call (E8 rel32) to send to function. original, if not null, gets the function the call led
	// to - set before the call is rewritten, so a hook that calls it is ready the moment it can run.
	struct CallRedirect
	{
		uint8_t* site;
		const void* function;
		std::string what;
		void** original;
	};

	// Redirects every call in redirects, together with the writes in alsoWrite, all or nothing. A call
	// reaches only +-2 GB, so each goes through a 16-byte jump stub near the game. The stubs get a block
	// of their own, written once and then read+execute only - never writable and executable at once -
	// and never changed again. False, with nothing changed, if a site is not a call, no memory near the
	// game can be had or WriteCode fails - logged under tag.
	bool RedirectCalls(const std::vector<CallRedirect>& redirects, std::vector<CodeWrite> alsoWrite, const char* tag);

	// The target of the 5-byte relative call (E8 rel32) at site, or nullptr if site is not one.
	const uint8_t* CallTarget(const void* site);

	// Where the rel32 at address leads - a jump's or call's rel32, or the disp32 of a rip-relative
	// operand that ends the instruction: the address right after it, plus the rel32.
	const uint8_t* Rel32Target(const void* address);

	// True if the code at address starts with a jump (E9, EB or FF 25) - the usual sign that
	// another mod has already hooked that function.
	bool StartsWithJump(const void* address);

	// True if address is where a function starts: the game's unwind data says so, or - for code
	// without unwind data - the byte before it is padding or a ret (CC, 90, C3). detail says which,
	// for the log.
	bool IsFunctionStart(const void* address, std::string& detail);

	// True if the size bytes at address are committed memory that can be read. A site that lies before or
	// after a pattern's match is read only after this check: the scan read the match alone.
	bool IsReadable(const void* address, size_t size);

	// ---- hooks on a function's start (MinHook)

	// One function to hook: its first bytes become a jump to hook, and original gets a trampoline that
	// runs those bytes and continues in the function. what names it for the log.
	struct FunctionHook
	{
		void* function;
		const void* hook;
		void** original;
		const char* what;
	};

	// Hooks every function in hooks, or none: the hooks are created and queued, then switched on
	// together, so the threads are frozen once and not once per hook. False, with the hooks made so
	// far removed again, if any step fails - logged under tag. MinHook is initialised on first use.
	bool HookFunctions(const std::vector<FunctionHook>& hooks, const char* tag);

	// Hooks an exported function of a loaded DLL (kernel32's GetSystemTimeAsFileTime) at once; target
	// gets the function's address, for DisableHook. False, logged under tag, if it failed.
	bool HookExport(const wchar_t* module, const char* function, const void* hook, void** original, void** target, const char* tag);

	// Switches a hook off again: the function's own bytes come back, and the trampoline stays for a
	// thread that is inside the hook right now. False, logged under tag, if MinHook refused.
	bool DisableHook(void* function, const char* tag);
}
