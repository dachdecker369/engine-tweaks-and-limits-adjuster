// TweaksV: carried over from HeapAdjusterEnhanced (Heap Adjuster/Game.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/HeapAdjusterEnhanced - based on citizenfx/gtav-heap-adjuster
// ((c) 2017 the CitizenFX Collective) as updated by Dilapidated; license: the project's LICENSE
// ("LGPLv2" + MIT text, see THIRD_PARTY_NOTICES.md).
// Modified for TweaksV: GTA V Enhanced only, namespace, TweaksV.ini/TweaksV.log,
// safety fixes. Every change is marked "TweaksV:".
#include "HeapAdjusterEnhanced_Game.h"
#include <string>			// TweaksV: <iostream> and <fstream> removed (the original included each twice) - unused

#include <cstdio>			// TweaksV: snprintf
#include "MemScanner.h"		// TweaksV: the shared scanner - was "pattern.h"
#include "TweaksVSettings.h" // TweaksV: HEAP_SIZE, DRY_RUN, DIAGNOSTIC_MODE
#include "TweaksVLog.h"     // TweaksV: TweaksV.log
#include "TweaksVPatch.h"   // TweaksV: WriteCode, the one code writer
#include "TweaksVSummary.h" // TweaksV: start-up summary

namespace HeapAdjusterEnhanced // TweaksV: one namespace per original plugin
{

int vanillaHeapSize;
int heapSize; // TweaksV: read in initializeGame() - was a global initialiser reading .\HeapAdjuster.ini
BYTE* loc;

void Logging()
{
	// TweaksV: was std::ofstream on "HeapAdjuster.log"; the same two lines now go to TweaksV.log
	TweaksV::WriteLog("HeapAdjusterEnhanced", "Vanilla Heap Size is: " + std::to_string(vanillaHeapSize));
	TweaksV::WriteLog("HeapAdjusterEnhanced", "Heap Size Adjusted to: " + std::to_string(heapSize));
}

void initializeGame()
{
	// TweaksV: the game edition check and the other edition's pattern are removed -
	// TweaksV only runs in GTA5_Enhanced.exe

	// TweaksV: moved here from the global initialiser; same section and key, from TweaksV's
	// settings table, which checks it (1..2047 MB; the default is 2000 MB, the original's was 1024)
	heapSize = TweaksV::GetNumber(TweaksV::kHeapSize);

	// TweaksV: was BytePattern (pattern.h) with the same bytes as "\xBA\x00\x00\x00\x00..." and the mask
	// "x????xxxxxxx????xxx"; the pattern is H-E1 of TweaksV's pattern table now
	BYTE* match = MemScanner::Find(TweaksV::Pattern("H-E1"));
	if (!match) { // TweaksV: never checked - a miss made loc = 0x1 and crashed below
		TweaksV::ReportFailure("HeapAdjusterEnhanced", "The heap size could not be changed: the game code it patches "
			"was not found (a game update may have moved it).");
		return;
	}
	loc = match + 1;

	if (loc[-1] != 0xBA) { // TweaksV: the target must be the imm32 of "mov edx, imm32"
		TweaksV::ReportFailure("HeapAdjusterEnhanced", "The heap size could not be changed: the game code found is "
			"not the expected instruction.");
		return;
	}

	vanillaHeapSize = *(int*)loc / 1024 / 1024;

	// TweaksV: the value found must be a plausible heap size before anything is written
	if (*(int*)loc <= 0 || *(int*)loc % (1024 * 1024) != 0) {
		char text[96];
		snprintf(text, sizeof(text), "unexpected value 0x%08X", static_cast<unsigned int>(*(int*)loc));
		TweaksV::ReportFailure("HeapAdjusterEnhanced", std::string("The heap size could not be changed: the game code "
			"found holds an ") + text + ".");
		return;
	}
	if (vanillaHeapSize > 1024) // TweaksV: another heap adjuster may have changed it already
		TweaksV::WriteLog("HeapAdjusterEnhanced", "WARNING: the heap size found (" + std::to_string(vanillaHeapSize) +
			" MB) is larger than the game's own - another heap adjuster may be active.");

	// TweaksV: clamp to [the game's own size, 2047 MB]; the game supports at most 2047 MB, the
	// setting's maximum
	const int maxHeapSize = TweaksV::kHeapSize.maxValue;
	const int requestedHeapSize = heapSize;
	if (heapSize < vanillaHeapSize)
		heapSize = vanillaHeapSize;
	if (heapSize > maxHeapSize)
		heapSize = maxHeapSize;
	if (heapSize != requestedHeapSize)
		TweaksV::WriteLog("HeapAdjusterEnhanced", "TweaksV.ini: HEAP_SIZE=" + std::to_string(requestedHeapSize) +
			" is outside " + std::to_string(vanillaHeapSize) + ".." + std::to_string(maxHeapSize) + " - using " +
			std::to_string(heapSize) + ".");

	// TweaksV: 64-bit math - heapSize * 1024 * 1024 overflowed int from 2048 up
	const long long newHeapBytes = (long long)heapSize * 1024 * 1024;

	if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN finds and checks everything but writes nothing
		TweaksV::WriteLog("HeapAdjusterEnhanced", "DRY_RUN: would change the heap size from " +
			std::to_string(vanillaHeapSize) + " MB to " + std::to_string(heapSize) + " MB.");
		return;
	}

	// TweaksV: was VirtualProtect + *(int*)loc = heapSize * 1024 * 1024 + VirtualProtect; TweaksV's one code
	// writer checks the protection, puts it back and logs the write with DIAGNOSTIC_MODE
	// default in 889.22 Enhanced was 648MiB,
	if (!TweaksV::WriteCode({ { loc, TweaksV::BytesOf((int)newHeapBytes), "heap size" } }, "HeapAdjusterEnhanced")) {
		TweaksV::ReportFailure("HeapAdjusterEnhanced", "The heap size could not be changed: the game code could not "
			"be made writable (details in TweaksV.log).");
		return;
	}

	Logging();
}

} // namespace HeapAdjusterEnhanced
