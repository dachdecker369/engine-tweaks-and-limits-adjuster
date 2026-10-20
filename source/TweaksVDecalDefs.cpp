// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the decal definitions limit (see TweaksVDecalDefs.h).
//
// The game reads its decal definitions - the kinds of bullet holes, blood, scorch and tyre marks, liquids,
// graffiti and so on - from the decals.dat files into one static array of 260 entries of 80 bytes, with
// the number registered so far in a dword 16 bytes before it. Once the count is past 259 the parser
// refuses every further definition, silently: the files are read in order, so the definitions of the last
// files - add-on and mod content - go missing, and the decals that use them never appear.
//   DECALDEFS_SIZE  the new limit, from the game's own 260 up to 16384 (a definition's index is a uint16
//                   in the game's decal instances)
//   LOG_DECALDEFS   testing: 10 seconds in and then every minute while it changes, the log says how many
//                   definitions are registered, against the limit and the game's own 260 - the number
//                   that says whether a load needs the larger array at all; nothing is hooked for it
// 23 instructions carry the limit as an imm32: the parser's "full" check and 12 more compare an index
// against N-1, 9 use N as the index that means "no such definition", and the decal manager's start-up
// loop, which zeroes the texture pointers of 26 entries per pass, stops at its start offset + N x 80.
// 31 "lea r64, [rip+disp32]" load the array - every one its first byte - and nothing else addresses it.
// The parser's two sites give the count's address, N and the array's address (16 bytes after the count);
// the start-up loop's two sites its start offset, its pass and its end; every other constant site is
// found by its own pattern - one of them, the check the decal-placing functions make on the index they
// were given, seven times - and must hold N-1 or N; and the game's code is scanned for every lea that
// loads the array, which must find exactly 31. Then, all or nothing, the 31 are pointed at the new array
// and the 23 constants set to its size (the loop end to a whole number of passes below it: the entries
// it leaves are zero anyway). The count stays where it is: nothing reaches it through the array, nor the
// array through it.
// Written for TweaksV from its own findings about the game's code; the idea comes from CitizenFX's fix
// for FiveM and 0x-FADED's port of it to GTA V Legacy - none of their code was read or used.

#include "TweaksVDecalDefs.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "MemScanner.h"
#include "StubMemory.h"
#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "DecalDefs";

		constexpr uint32_t kEntrySize = 80;				// one decal definition
		constexpr uint32_t kMaxDefs = 16384;			// the new array always has this many entries, whatever DECALDEFS_SIZE says
		static_assert(static_cast<int>(kMaxDefs) == kDecalDefsSize.maxValue, "TweaksV: DECALDEFS_SIZE's maximum must be the array's size");
		constexpr size_t kArrayReferences = 31;			// the lea instructions that load the array - every one of them, on 1.0.1158.16
		constexpr size_t kConstantSites = 23;			// the instructions that carry the limit
		constexpr size_t kLeaSize = 7;					// REX.W, 8D, ModRM, disp32
		constexpr size_t kCountToArray = 16;			// the array lies this many bytes after its count
		constexpr DWORD kFirstSummaryMs = 10000;		// LOG_DECALDEFS: the first summary, then one every minute
		constexpr DWORD kSummaryEveryMs = 60000;
		constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
			PAGE_EXECUTE_WRITECOPY;

		// What a constant site holds: N-1 (the last valid index, which an index is compared against), N (the
		// index that means "no such definition") or the end of the start-up loop (its start offset + N x 80).
		enum class Holds { NMinus1, N, LoopEnd };

		// One imm32 of the limit in a pattern's match: the opcode bytes of the instruction that carries it
		// (checked before the imm32 is read), where the instruction lies in the match, where its imm32 lies.
		struct ConstantSite
		{
			const char* opcode;
			size_t opcodeSize;
			size_t offset;
			size_t immediateOffset;
			Holds holds;
		};

		// A pattern and the constant sites in its match. matches: how often the pattern must match - once,
		// except the check that seven of the decal-placing functions make on the index they were given.
		struct ConstantAnchor
		{
			const GamePattern* pattern;
			size_t matches;
			size_t siteCount;
			ConstantSite sites[3];
		};

		constexpr ConstantAnchor kConstantAnchors[] = {
			// FindById: "mov dword ptr [rdx], N" (the first index: none yet) and "cmp dword ptr [rdx], N" twice
			{ &Pattern("D-E-L3"), 1, 3, { { "\xC7\x02", 2, 5, 7, Holds::N }, { "\x81\x3A", 2, 0x51, 0x53, Holds::N },
				{ "\x81\x3A", 2, 0x60, 0x62, Holds::N } } },
			// PickVariant: "mov eax, N" (its result for none) and "cmp ecx, N-1"
			{ &Pattern("D-E-L4"), 1, 2, { { "\xB8", 1, 0, 1, Holds::N }, { "\x81\xF9", 2, 5, 7, Holds::NMinus1 } } },
			// the texture override: "mov r11d, N" twice (no index yet), then "cmp r11d, N", "cmp ebp, N" and "cmp r11d, N"
			{ &Pattern("D-E-L5"), 1, 1, { { "\x41\xBB", 2, 3, 5, Holds::N } } },
			{ &Pattern("D-E-L6"), 1, 1, { { "\x41\xBB", 2, 0, 2, Holds::N } } },
			{ &Pattern("D-E-L7"), 1, 1, { { "\x41\x81\xFB", 3, 0, 3, Holds::N } } },
			{ &Pattern("D-E-L8"), 1, 1, { { "\x81\xFD", 2, 0, 2, Holds::N } } },
			{ &Pattern("D-E-L9"), 1, 1, { { "\x41\x81\xFB", 3, 0, 3, Holds::N } } },
			// the decal-placing functions: "cmp r32, N-1" on the index PickVariant gave them, then jg or ja
			{ &Pattern("D-E-L12"), 7, 1, { { "\x3D", 1, 5, 6, Holds::NMinus1 } } },
			{ &Pattern("D-E-L13"), 1, 1, { { "\x41\x81\xF8", 3, 3, 6, Holds::NMinus1 } } },
			{ &Pattern("D-E-L14"), 1, 1, { { "\x81\xFE", 2, 2, 4, Holds::NMinus1 } } },
			{ &Pattern("D-E-L15"), 1, 1, { { "\x3D", 1, 5, 6, Holds::NMinus1 } } },
			{ &Pattern("D-E-L16"), 1, 1, { { "\x81\xFB", 2, 4, 6, Holds::NMinus1 } } },
		};

		// The parser's check (D-E-L1) and the loop end (D-E-L11) are the other two of the 23.
		consteval size_t AnchoredSites()
		{
			size_t sites = 0;
			for (const ConstantAnchor& anchor : kConstantAnchors)
				sites += anchor.matches * anchor.siteCount;
			return sites;
		}
		static_assert(AnchoredSites() + 2 == kConstantSites, "TweaksV: kConstantAnchors must cover all but two of the constant sites");

		// A constant site that passed its checks: the imm32 to write and what it holds.
		struct ConstantWrite
		{
			uint8_t* immediate;
			Holds holds;
		};

		// Logs why and reports for the start-up summary; the caller then leaves the game as it is.
		void Fail(const std::string& why)
		{
			WriteLog(kTag, why);
			ReportFailure(kTag, "The decal definitions limit is not raised this session: a required step failed (details in TweaksV.log).");
		}

		uint32_t Imm32At(const uint8_t* address)
		{
			uint32_t value = 0;
			memcpy(&value, address, sizeof(value));
			return value;
		}

		// True if the size bytes at address are committed, readable memory. A site that lies past the end
		// of a pattern's match is read only after this check: the match's own bytes were read by the scan,
		// the bytes after it were not.
		bool IsReadable(const void* address, size_t size)
		{
			const auto begin = reinterpret_cast<uintptr_t>(address);
			for (uintptr_t at = begin; at < begin + size;)
			{
				MEMORY_BASIC_INFORMATION info{};
				if (VirtualQuery(reinterpret_cast<const void*>(at), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
					(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !(info.Protect & kReadable))
					return false;
				const uintptr_t next = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
				if (next <= at)
					return false;
				at = next;
			}
			return true;
		}

		const char* Describe(Holds holds)
		{
			return holds == Holds::NMinus1 ? "N-1" : holds == Holds::N ? "N" : "loop end";
		}

		// ---- LOG_DECALDEFS

		const volatile uint32_t* g_count = nullptr;	// the game's count of registered definitions
		uint32_t g_capacity = 0;					// the array in use: the new one, or the game's own with DRY_RUN
		uint32_t g_vanillaSize = 0;					// the game's own array

		void LogSummary(uint32_t registered)
		{
			std::string line = "LOG_DECALDEFS: " + std::to_string(registered) + " decal definitions registered (the game's own array holds " +
				std::to_string(g_vanillaSize) + (g_capacity == g_vanillaSize ? std::string(" and is in use") : ", this one " + std::to_string(g_capacity)) + ")";
			if (registered > g_vanillaSize)
				line += " - the game's own array would have dropped " + std::to_string(registered - g_vanillaSize) + " of them";
			if (registered >= g_capacity)
				line += " - it is full: any further definitions were dropped";
			WriteLog(kTag, line + ".");
		}

		// The first summary 10 seconds in, then one a minute while the number changes. The count is the
		// game's own dword, read as it is: nothing is hooked.
		DWORD WINAPI SummaryThread(LPVOID)
		{
			uint32_t last = UINT32_MAX;
			for (DWORD wait = kFirstSummaryMs;; wait = kSummaryEveryMs)
			{
				Sleep(wait);
				const uint32_t registered = *g_count;
				if (registered == last)
					continue;
				last = registered;
				try
				{
					LogSummary(registered);
				}
				catch (...)
				{
				}
			}
		}

		void StartSummaryThread()
		{
			const HANDLE thread = CreateThread(nullptr, 0, SummaryThread, nullptr, 0, nullptr);
			if (thread)
				CloseHandle(thread);
			else
				ReportFailure(kTag, "LOG_DECALDEFS: the summary thread could not be started (" + DescribeError(GetLastError()) +
					") - the number of registered definitions is never logged.");
		}
	}

	void InitDecalDefs()
	{
		const bool logDefs = IsOn(kLogDecalDefs);

		// 1. the parser's "full" check: the count's address from the mov before it, N from its compare
		uint8_t* const fullCheck = MemScanner::Find(Pattern("D-E-L1"));
		if (!fullCheck)
		{
			Fail("The parser's check of the decal definitions limit was not found in the game code: D-E-L1");
			return;
		}
		const uint8_t* const count = Rel32Target(fullCheck + 2);			// mov esi, dword ptr [rip+disp32]
		uint8_t* const fullCheckImmediate = fullCheck + 9;					// cmp rsi, imm32: the last valid index
		const uint32_t lastIndex = Imm32At(fullCheckImmediate);
		const uint32_t gameSize = lastIndex + 1;
		if (IsDiagnosticMode())
			WriteLog(kTag, "DIAGNOSTIC: limit at " + DescribeAddress(fullCheckImmediate) + " holds " + std::to_string(lastIndex) +
				" (N-1, D-E-L1): the game's own array holds " + std::to_string(gameSize) + " definitions");
		if (gameSize == 0 || gameSize > kMaxDefs)
		{
			Fail("The game's own decal definitions limit, " + std::to_string(gameSize) + ", is not plausible.");
			return;
		}

		// 2. the parser's lea: the array, which lies 16 bytes after the count, both inside the game
		const uint8_t* const parserLea = MemScanner::Find(Pattern("D-E-L2"));
		if (!parserLea)
		{
			Fail("The parser's reference to the decal definitions array was not found in the game code: D-E-L2");
			return;
		}
		const uint8_t* const array = Rel32Target(parserLea + 3);			// lea r13, [rip+disp32]
		if (!IsInsideGameImage(count) || !IsInsideGameImage(array))
		{
			Fail("The decal definitions array or its count is not inside the game: the array at " + DescribeAddress(array) +
				", the count at " + DescribeAddress(count) + " (another mod may have moved the array already).");
			return;
		}
		if (array != count + kCountToArray)
		{
			Fail("The decal definitions array at " + DescribeAddress(array) + " does not lie " + std::to_string(kCountToArray) +
				" bytes after its count at " + DescribeAddress(count) + " (another mod may have moved it already).");
			return;
		}
		if (IsDiagnosticMode())
			WriteLog(kTag, "DIAGNOSTIC: the decal definitions array is at " + DescribeAddress(array) + " - " + std::to_string(gameSize) +
				" entries of " + std::to_string(kEntrySize) + " bytes; its count is at " + DescribeAddress(count));

		// 3. the decal manager's start-up loop: its start offset and its lea of the array, the bytes it
		// clears per pass, and its end, which must be the start offset + N x 80
		const uint8_t* const loopStart = MemScanner::Find(Pattern("D-E-L10"));
		if (!loopStart)
		{
			Fail("The start of the decal manager's start-up loop was not found in the game code: D-E-L10");
			return;
		}
		const uint32_t startOffset = Imm32At(loopStart + 1);				// mov eax, imm32
		const uint8_t* const loopLea = Rel32Target(loopStart + 8);			// lea rcx, [rip+disp32]
		if (loopLea != array)
		{
			Fail("The decal manager's start-up loop at " + DescribeAddress(loopStart + 5) + " loads " + DescribeAddress(loopLea) +
				", not the decal definitions array (another mod may have moved it already).");
			return;
		}
		uint8_t* const loopEnd = MemScanner::Find(Pattern("D-E-L11"));
		if (!loopEnd)
		{
			Fail("The end of the decal manager's start-up loop was not found in the game code: D-E-L11");
			return;
		}
		const uint32_t passBytes = Imm32At(loopEnd + 2);					// add rax, imm32
		uint8_t* const loopEndImmediate = loopEnd + 8;						// cmp rax, imm32
		const uint32_t endValue = Imm32At(loopEndImmediate);
		if (passBytes == 0 || passBytes % kEntrySize != 0)
		{
			Fail("The decal manager's start-up loop clears " + std::to_string(passBytes) + " bytes per pass, not a whole number of definitions of " +
				std::to_string(kEntrySize) + " bytes.");
			return;
		}
		const uint32_t entriesPerPass = passBytes / kEntrySize;
		const uint64_t expectedEnd = static_cast<uint64_t>(startOffset) + static_cast<uint64_t>(gameSize) * kEntrySize;
		if (IsDiagnosticMode())
			WriteLog(kTag, "DIAGNOSTIC: limit at " + DescribeAddress(loopEndImmediate) + " holds " + std::to_string(endValue) +
				" (loop end, D-E-L11): the start-up loop clears " + std::to_string(entriesPerPass) + " definitions per pass from offset " +
				std::to_string(startOffset));
		if (endValue != expectedEnd)
		{
			Fail("The end of the decal manager's start-up loop at " + DescribeAddress(loopEndImmediate) + " holds " + std::to_string(endValue) +
				", not its start offset " + std::to_string(startOffset) + " + " + std::to_string(gameSize) + " x " + std::to_string(kEntrySize) + ".");
			return;
		}

		// 4. every other constant site, by its pattern: each must be the expected instruction and hold N-1 or N
		std::vector<ConstantWrite> constants;
		constants.push_back({ fullCheckImmediate, Holds::NMinus1 });
		constants.push_back({ loopEndImmediate, Holds::LoopEnd });
		for (const ConstantAnchor& anchor : kConstantAnchors)
		{
			const std::vector<uint8_t*> matches = MemScanner::FindAll(*anchor.pattern, anchor.matches + 1);
			if (matches.size() != anchor.matches)
			{
				Fail(matches.empty()
					? std::string("A site of the decal definitions limit was not found in the game code: ") + anchor.pattern->label
					: "The game code matches a site of the decal definitions limit " + std::to_string(matches.size()) +
						(matches.size() > anchor.matches ? " or more" : "") + " times, not the expected " + std::to_string(anchor.matches) +
						" (" + anchor.pattern->label + ").");
				return;
			}
			for (uint8_t* match : matches)
			{
				for (size_t i = 0; i < anchor.siteCount; i++)
				{
					const ConstantSite& site = anchor.sites[i];
					uint8_t* const instruction = match + site.offset;
					uint8_t* const immediate = match + site.immediateOffset;
					if (!IsReadable(instruction, site.immediateOffset - site.offset + sizeof(uint32_t)))
					{
						Fail("The decal definitions limit at " + DescribeAddress(instruction) + " is not readable memory (" + anchor.pattern->label + ").");
						return;
					}
					if (memcmp(instruction, site.opcode, site.opcodeSize) != 0)
					{
						Fail("The decal definitions limit at " + DescribeAddress(instruction) + " is not the expected instruction: " +
							HexBytes(instruction, site.opcodeSize) + " (" + anchor.pattern->label + ").");
						return;
					}
					const uint32_t value = Imm32At(immediate);
					const uint32_t expected = site.holds == Holds::NMinus1 ? gameSize - 1 : gameSize;
					if (IsDiagnosticMode())
						WriteLog(kTag, "DIAGNOSTIC: limit at " + DescribeAddress(immediate) + " holds " + std::to_string(value) + " (" +
							Describe(site.holds) + ", " + anchor.pattern->label + ")");
					if (value != expected)
					{
						Fail("The decal definitions limit at " + DescribeAddress(immediate) + " holds " + std::to_string(value) + ", not the " +
							std::to_string(expected) + " the parser's check implies (" + anchor.pattern->label + ").");
						return;
					}
					constants.push_back({ immediate, site.holds });
				}
			}
		}

		// 5. every lea that loads the array: the game has exactly kArrayReferences of them - fewer means
		// another mod has changed some, more that the game has changed - each with a 48 or 4C prefix
		const std::vector<uint8_t*> references = MemScanner::FindRipRelativeLeas(array, kArrayReferences + 1);
		if (IsDiagnosticMode())
			for (const uint8_t* reference : references)
				WriteLog(kTag, "DIAGNOSTIC: the array is loaded at " + DescribeAddress(reference));
		if (references.size() != kArrayReferences)
		{
			Fail("The game code loads the decal definitions array in " + std::to_string(references.size()) +
				(references.size() > kArrayReferences ? " or more" : "") + " places, not the expected " + std::to_string(kArrayReferences) +
				" (another mod may have moved some of them, or the game has changed).");
			return;
		}
		for (const uint8_t* reference : references)
			if (reference[0] != 0x48 && reference[0] != 0x4C)
			{
				Fail("The load of the decal definitions array at " + DescribeAddress(reference) + " is not a \"lea r64, [rip+disp32]\" with a 48 or 4C "
					"prefix: " + HexBytes(reference, kLeaSize) + ".");
				return;
			}

		// 6. the size to use: [the game's own size, kMaxDefs] - smaller would shrink the game's array. The
		// loop end goes to a whole number of passes below it: the entries it leaves are zero anyway.
		const uint32_t requested = static_cast<uint32_t>(GetNumber(kDecalDefsSize));
		uint32_t size = requested;
		if (size < gameSize)
			size = gameSize;
		if (size > kMaxDefs)
			size = kMaxDefs;
		if (size != requested)
			WriteLog(kTag, "TweaksV.ini: DECALDEFS_SIZE=" + std::to_string(requested) + " is outside " + std::to_string(gameSize) + ".." +
				std::to_string(kMaxDefs) + " - using " + std::to_string(size) + ".");
		const uint64_t newEnd = static_cast<uint64_t>(startOffset) + static_cast<uint64_t>(kEntrySize) * (size - size % entriesPerPass);
		if (newEnd > UINT32_MAX)
		{
			Fail("The new end of the decal manager's start-up loop, " + std::to_string(newEnd) + ", does not fit its imm32.");
			return;
		}

		if (logDefs)
		{
			g_count = reinterpret_cast<const volatile uint32_t*>(count);
			g_vanillaSize = gameSize;
			g_capacity = IsDryRun() ? gameSize : size;
		}

		if (IsDryRun())	// everything found and checked, nothing written
		{
			WriteLog(kTag, "DRY_RUN: would move the decal definitions array (" + std::to_string(references.size()) +
				" references) and raise its limit from " + std::to_string(gameSize) + " to " + std::to_string(size) + " (" +
				std::to_string(constants.size()) + " sites).");
			if (logDefs)
			{
				WriteLog(kTag, "LOG_DECALDEFS: the number of registered decal definitions is logged after 10 seconds, then every minute "
					"while it changes (DRY_RUN: the game's own array is in use).");
				StartSummaryThread();
			}
			return;
		}

		// 7. the new array: always the full kMaxDefs entries, read/write data near the game (a lea reaches
		// +-2 GB), zeroed by Windows - which is the array's initial state
		uint8_t* const newArray = static_cast<uint8_t*>(AllocateStubMemory(static_cast<size_t>(kEntrySize) * kMaxDefs, PAGE_READWRITE));
		if (!newArray)
		{
			Fail("The new decal definitions array could not be allocated near the game.");
			return;
		}

		// 8. one write, all or nothing: the 31 references to the new array, the 23 constants to its size
		std::vector<CodeWrite> writes;
		for (uint8_t* reference : references)
		{
			uint8_t* const rel32 = reference + kLeaSize - sizeof(int32_t);
			const intptr_t distance = newArray - (rel32 + sizeof(int32_t));
			if (distance < INT32_MIN || distance > INT32_MAX)
			{
				VirtualFree(newArray, 0, MEM_RELEASE);
				Fail("The new decal definitions array is out of reach of the game code at " + DescribeAddress(reference) + ".");
				return;
			}
			writes.push_back({ rel32, BytesOf(static_cast<int32_t>(distance)), "decal definitions array reference" });
		}
		for (const ConstantWrite& constant : constants)
		{
			const uint32_t value = constant.holds == Holds::NMinus1 ? size - 1 : constant.holds == Holds::N ? size : static_cast<uint32_t>(newEnd);
			writes.push_back({ constant.immediate, BytesOf(value), constant.holds == Holds::LoopEnd ? "decal definitions loop end" : "decal definitions limit" });
		}
		if (!WriteCode(writes, kTag))
		{
			VirtualFree(newArray, 0, MEM_RELEASE);	// nothing points at it: nothing was written
			Fail("The game code could not be changed.");
			return;
		}

		WriteLog(kTag, "Decal definitions array moved and its limit raised from " + std::to_string(gameSize) + " to " + std::to_string(size) +
			" entries.");
		if (logDefs)
		{
			WriteLog(kTag, "LOG_DECALDEFS: the number of registered decal definitions is logged after 10 seconds, then every minute while it changes.");
			StartSummaryThread();
		}
	}
}
