// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the heap adjuster (see TweaksVHeapAdjuster.h).
//
// The game sizes its memory heap with one immediate in its start-up code: the imm32 of a "mov edx,
// imm32" (H-E1 matches the instruction; the size, in bytes, is at +1). HEAP_SIZE in [HEAP_ADJUSTER]
// sets it in MB, from the size the game holds up to 2047, the most whole MB that imm32 holds as a
// positive int32; a value below the game's own is raised to it and reported. The instruction and the
// value it holds are checked before the one write. A heap that is not the game's own - 1024 MB or more,
// up to the 4095 MB the imm32 holds when the mov zero-extends it - was set by another heap adjuster and
// is left as it is, logged: what another mod has already changed, TweaksV does not change again
// (README, "Other mods").
// Written for TweaksV; the pattern and the idea come from HeapAdjusterEnhanced by Chiheb-Bacha, which
// builds on the CitizenFX Collective's heap adjuster as updated by Dilapidated (CREDITS.md).

#include "TweaksVHeapAdjuster.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVScanner.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "HeapAdjuster";

		constexpr uint32_t kMegabyte = 1024 * 1024;
		constexpr int kMaxHeapMb = 2047;		// the most whole MB the game's imm32 holds as a positive int32
		static_assert(kMaxHeapMb == kHeapSize.maxValue, "TweaksV: HEAP_SIZE's maximum must be the imm32's");
		constexpr int kAdjustedFromMb = 1024;	// a heap of this size or more is not the game's own: another adjuster set it

		// Logs why for the start-up summary; the caller then leaves the game as it is.
		void Fail(const std::string& why)
		{
			ReportFailure(kTag, "The heap size is not changed this session: " + why);
		}
	}

	void InitHeapAdjuster()
	{
		// 1. the site: the imm32 of "mov edx, imm32" (BA imm32), one byte into the match
		uint8_t* const match = Scanner::Find(Pattern("H-E1"));
		if (!match)
		{
			Fail("H-E1 was not found.");
			return;
		}
		uint8_t* const site = match + 1;
		if (match[0] != 0xBA)
		{
			Fail("the game code found is not the expected instruction (" + HexBytes(match, 5) + ").");
			return;
		}

		// 2. the size the game holds: a whole number of MB - up to 4095, as the mov zero-extends its imm32;
		// 1024 MB or more is another heap adjuster's, whatever it is
		uint32_t bytes = 0;
		memcpy(&bytes, site, sizeof(bytes));
		if (bytes == 0 || bytes % kMegabyte != 0)
		{
			char text[32];
			snprintf(text, sizeof(text), "0x%08X", bytes);
			Fail(std::string("the game code found holds an unexpected value (") + text + ").");
			return;
		}
		const int found = static_cast<int>(bytes / kMegabyte);
		if (IsDiagnosticMode())
			WriteLog(kTag, "DIAGNOSTIC: heap size at " + DescribeAddress(site) + " holds " + std::to_string(found) + " MB");
		if (found >= kAdjustedFromMb)
		{
			WriteLog(kTag, "The heap size is not changed this session: another heap adjuster already set it to " + std::to_string(found) +
				" MB - left as it is.");
			return;
		}

		// 3. the size to use: [the size found, 2047 MB] - smaller would shrink the heap; a value outside is
		// reported, so that the start-up summary names the size really used
		const int requested = GetNumber(kHeapSize);
		int size = requested;
		if (size < found)
			size = found;
		if (size > kMaxHeapMb)
			size = kMaxHeapMb;
		if (size != requested)
			ReportFailure(kTag, "TweaksV.ini: HEAP_SIZE=" + std::to_string(requested) + " is outside " + std::to_string(found) + ".." +
				std::to_string(kMaxHeapMb) + " - using " + std::to_string(size) + ".");
		if (size == found)
		{
			WriteLog(kTag, "Heap size is " + std::to_string(found) + " MB already (HEAP_SIZE=" + std::to_string(requested) + ") - nothing to patch.");
			return;
		}

		// 4. one write; the arithmetic is 64-bit, as 2047 MB only just fits the imm32
		const uint32_t newBytes = static_cast<uint32_t>(static_cast<uint64_t>(size) * kMegabyte);
		const std::string change = std::to_string(found) + " MB -> " + std::to_string(size) + " MB";
		if (!WriteCode({ { site, BytesOf(newBytes), "heap size (" + change + ")" } }, kTag))
		{
			Fail("the game code could not be made writable (details in TweaksV.log).");
			return;
		}
		if (!IsDryRun())
			WriteLog(kTag, "Heap size patched: " + change + ".");
	}
}
