// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the decal definitions limit (see TweaksVDecals.h).
//
// The game reads its decal definitions - the kinds of bullet holes, blood, scorch and tyre marks, liquids,
// graffiti and so on - from the decals.dat files into one static array of 260 entries of 80 bytes, with
// the number registered so far in a dword 16 bytes before it. Once the count is past 259 the parser
// refuses every further definition, silently: the files are read in order, so the definitions of the last
// files - add-on and mod content - go missing, and the decals that use them never appear.
//   DECALDEFS_SIZE   the new limit, from the game's own 260 up to 16384 (a definition's index is a uint16
//                    in the game's decal instances)
//   DIAGNOSTIC_MODE  the measurement: 10 seconds in and then every minute while it changes, the log says
//                    how many definitions are registered, against the limit and the game's own 260 - the
//                    number that says whether a load needs the larger array at all; nothing is hooked
// 22 instructions carry the limit as an imm32 - the parser's "full" check and 12 more compare an index
// against N-1, 9 use N as the index that means "no such definition" - and 31 "lea r64, [rip+disp32]"
// load the array; nothing else addresses it. The relocation core takes the array's address from the
// parser's lea (D-E-L2), finds the 22 by their patterns (one of them, the check the decal-placing
// functions make on the index they were given, seven times) and scans the game's code for every lea
// that loads the array, which must find exactly 31. What stays the part's own: the count, which must lie
// 16 bytes before the array; the decal manager's start-up loop, which zeroes the texture pointers of 26
// entries per pass and stops at its start offset + N x 80 - its end is written with the rest, to a whole
// number of passes below the new size (the entries it leaves are zero anyway); and the measurement.
// The count stays where it is: nothing reaches it through the array, nor the array through it.
// Written for TweaksV from its own findings about the game's code; the idea comes from CitizenFX's fix
// for FiveM and 0x-FADED's port of it to GTA V Legacy - none of their code was read or used.

#include "TweaksVDecals.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVRelocation.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "Decals";

		constexpr uint32_t kEntrySize = 80;				// one decal definition
		constexpr uint32_t kMaxDefs = 16384;			// the new array always has this many entries, whatever DECALDEFS_SIZE says
		static_assert(kMaxDefs == static_cast<uint32_t>(kDecalDefsSize.maxValue), "TweaksV: DECALDEFS_SIZE's maximum must be the array's size");
		constexpr size_t kArrayReferences = 31;			// the lea instructions that load the array - every one of them, on 1.0.1158.16
		constexpr size_t kCountToArray = 16;			// the array lies this many bytes after its count
		constexpr char kLeaPrefixes[] = "\x48\x4C";		// the REX bytes the game's loads of the array start with
		constexpr DWORD kFirstSummaryMs = 10000;		// DIAGNOSTIC_MODE: the first summary, then one every minute
		constexpr DWORD kSummaryEveryMs = 60000;

		// The anchors the part reads itself, by their place in the table.
		enum : size_t { kParserCheck, kParserLea, kLoopStart, kLoopEnd };

		constexpr Anchor kAnchors[] = {
			// the parser's "full" check: "mov esi, [count]" (its disp32 at +2 is the part's) and "cmp rsi, N-1"
			Limit(Pattern("D-E-L1"), Imm32(6, "\x48\x81\xFE", 3, 9, 1, -1)),
			// the parser's lea of the array
			Reference(Pattern("D-E-L2"), Rel32(3, "\x4C\x8D\x2D", 3)),
			// the decal manager's start-up loop: its start (the offset at +1, a lea of the array at +5) and
			// its end (the bytes one pass clears at +2, the end at +8) - the part's own
			Site(Pattern("D-E-L10")),
			Site(Pattern("D-E-L11")),
			// FindById: "mov dword ptr [rdx], N" (the first index: none yet) and "cmp dword ptr [rdx], N" twice
			Limits(Pattern("D-E-L3"), 1, Imm32(5, "\xC7\x02", 2, 7, 1, 0), Imm32(0x51, "\x81\x3A", 2, 0x53, 1, 0), Imm32(0x60, "\x81\x3A", 2, 0x62, 1, 0)),
			// PickVariant: "mov eax, N" (its result for none) and "cmp ecx, N-1"
			Limits(Pattern("D-E-L4"), 1, Imm32(0, "\xB8", 1, 1, 1, 0), Imm32(5, "\x81\xF9", 2, 7, 1, -1)),
			// the texture override: "mov r11d, N" twice (no index yet), then "cmp r11d, N", "cmp ebp, N" and "cmp r11d, N"
			Limit(Pattern("D-E-L5"), Imm32(3, "\x41\xBB", 2, 5, 1, 0)),
			Limit(Pattern("D-E-L6"), Imm32(0, "\x41\xBB", 2, 2, 1, 0)),
			Limit(Pattern("D-E-L7"), Imm32(0, "\x41\x81\xFB", 3, 3, 1, 0)),
			Limit(Pattern("D-E-L8"), Imm32(0, "\x81\xFD", 2, 2, 1, 0)),
			Limit(Pattern("D-E-L9"), Imm32(0, "\x41\x81\xFB", 3, 3, 1, 0)),
			// the decal-placing functions: "cmp r32, N-1" on the index PickVariant gave them, then jg or ja -
			// seven of them share one check
			Repeated(Pattern("D-E-L12"), 7, Imm32(5, "\x3D", 1, 6, 1, -1)),
			Limit(Pattern("D-E-L13"), Imm32(3, "\x41\x81\xF8", 3, 6, 1, -1)),
			Limit(Pattern("D-E-L14"), Imm32(2, "\x81\xFE", 2, 4, 1, -1)),
			Limit(Pattern("D-E-L15"), Imm32(5, "\x3D", 1, 6, 1, -1)),
			Limit(Pattern("D-E-L16"), Imm32(4, "\x81\xFB", 2, 6, 1, -1)),
		};

		constexpr RelocationSpec kSpec = { kTag, "decal definitions array", "decal definitions limit", "The decal definitions limit is not raised",
			kEntrySize, kMaxDefs, &kDecalDefsSize, kAnchors, kArrayReferences, kLeaPrefixes, 2 };

		// ---- the measurement (DIAGNOSTIC_MODE)

		const volatile uint32_t* g_count = nullptr;	// the game's count of registered definitions
		uint32_t g_capacity = 0;					// the array in use: the new one, or the game's own with DRY_RUN
		uint32_t g_vanillaSize = 0;					// the game's own array

		void LogSummary(uint32_t registered)
		{
			std::string line = "DIAGNOSTIC: " + std::to_string(registered) + " decal definitions registered (the game's own array holds " +
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
				ReportFailure(kTag, "DIAGNOSTIC: the summary thread could not be started (" + DescribeError(GetLastError()) +
					") - the number of registered definitions is never logged.");
		}

		uint32_t Imm32At(const uint8_t* address)
		{
			uint32_t value = 0;
			memcpy(&value, address, sizeof(value));
			return value;
		}
	}

	void InitDecals()
	{
		// 1. the array, the 22 constant sites and the 31 loads, on the core
		RelocationPlan plan;
		if (!FindRelocation(kSpec, plan))
			return;
		const bool diagnostic = IsDiagnosticMode();

		// 2. the count: the mov of the parser's check reads it, and the array lies 16 bytes after it
		const uint8_t* const count = Rel32Target(plan.matches[kParserCheck][0] + 2);
		if (!IsInsideGameImage(count))
		{
			RelocationFailed(kSpec, "the count of registered definitions is not inside the game: " + DescribeAddress(count) + ".");
			return;
		}
		if (plan.array != count + kCountToArray)
		{
			RelocationFailed(kSpec, "the decal definitions array at " + DescribeAddress(plan.array) + " does not lie " + std::to_string(kCountToArray) +
				" bytes after its count at " + DescribeAddress(count) + " (another mod may have moved it already).");
			return;
		}
		if (diagnostic)
			WriteLog(kTag, "DIAGNOSTIC: the count of registered definitions is at " + DescribeAddress(count));

		// 3. the start-up loop: it loads the array, clears a whole number of definitions per pass and ends
		// at its start offset + N x 80
		const uint8_t* const loopStart = plan.matches[kLoopStart][0];
		const uint32_t startOffset = Imm32At(loopStart + 1);				// mov eax, imm32
		const uint8_t* const loopLea = Rel32Target(loopStart + 8);			// lea rcx, [rip+disp32]
		if (loopLea != plan.array)
		{
			RelocationFailed(kSpec, "the decal manager's start-up loop at " + DescribeAddress(loopStart + 5) + " loads " + DescribeAddress(loopLea) +
				", not the decal definitions array (another mod may have moved it already).");
			return;
		}
		uint8_t* const loopEnd = plan.matches[kLoopEnd][0];
		const uint32_t passBytes = Imm32At(loopEnd + 2);					// add rax, imm32
		uint8_t* const loopEndImmediate = loopEnd + 8;						// cmp rax, imm32
		const uint32_t endValue = Imm32At(loopEndImmediate);
		if (passBytes == 0 || passBytes % kEntrySize != 0)
		{
			RelocationFailed(kSpec, "the decal manager's start-up loop clears " + std::to_string(passBytes) + " bytes per pass, not a whole number of "
				"definitions of " + std::to_string(kEntrySize) + " bytes.");
			return;
		}
		const uint32_t entriesPerPass = passBytes / kEntrySize;
		const uint64_t expectedEnd = static_cast<uint64_t>(startOffset) + static_cast<uint64_t>(plan.gameSize) * kEntrySize;
		if (diagnostic)
			WriteLog(kTag, "DIAGNOSTIC: loop end at " + DescribeAddress(loopEndImmediate) + " holds " + std::to_string(endValue) +
				" (D-E-L11): the start-up loop clears " + std::to_string(entriesPerPass) + " definitions per pass from offset " + std::to_string(startOffset));
		if (endValue != expectedEnd)
		{
			RelocationFailed(kSpec, "the end of the decal manager's start-up loop at " + DescribeAddress(loopEndImmediate) + " holds " +
				std::to_string(endValue) + ", not its start offset " + std::to_string(startOffset) + " + " + std::to_string(plan.gameSize) + " x " +
				std::to_string(kEntrySize) + ".");
			return;
		}
		// the new end: a whole number of passes below the new size
		const uint64_t newEnd = static_cast<uint64_t>(startOffset) + static_cast<uint64_t>(kEntrySize) * (plan.size - plan.size % entriesPerPass);
		if (newEnd > UINT32_MAX)
		{
			RelocationFailed(kSpec, "the new end of the decal manager's start-up loop, " + std::to_string(newEnd) + ", does not fit its imm32.");
			return;
		}

		// 4. the measurement reads the count of the array in use
		if (diagnostic)
		{
			g_count = reinterpret_cast<const volatile uint32_t*>(count);
			g_vanillaSize = plan.gameSize;
			g_capacity = IsDryRun() ? plan.gameSize : plan.size;
		}

		// 5. one write, all or nothing: the 31 references, the 22 limits and the loop end
		const std::vector<CodeWrite> loopWrite = { { loopEndImmediate, BytesOf(static_cast<uint32_t>(newEnd)), "decal definitions array loop end" } };
		if (!ApplyRelocation(kSpec, plan, loopWrite, {}, " and the end of its start-up loop"))
			return;
		if (!diagnostic)
			return;
		WriteLog(kTag, std::string("DIAGNOSTIC: the number of registered decal definitions is logged after 10 seconds, then every minute while it changes") +
			(IsDryRun() ? " (DRY_RUN: the game's own array is in use)." : "."));
		StartSummaryThread();
	}
}
