// TweaksV: carried over from HeapAdjusterEnhanced (Heap Adjuster/Game.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/HeapAdjusterEnhanced - based on citizenfx/gtav-heap-adjuster
// ((c) 2017 the CitizenFX Collective) as updated by Dilapidated; license: the project's LICENSE
// ("LGPLv2" + MIT text, see THIRD_PARTY_NOTICES.md).
// Modified for TweaksV: GTA V Enhanced only, namespace, TweaksV.ini/TweaksV.log,
// safety fixes (B-HEAP-01..05). Every change is marked "TweaksV:".
#include "HeapAdjusterEnhanced_Game.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iostream>
#include <fstream>

#include "pattern.h"
#include "TweaksVIni.h"     // TweaksV: TweaksV.ini, DRY_RUN, DIAGNOSTIC_MODE
#include "TweaksVLog.h"     // TweaksV: TweaksV.log
#include "TweaksVSummary.h" // TweaksV: start-up summary

namespace HeapAdjusterEnhanced // TweaksV: one namespace per original plugin
{

int vanillaHeapSize;
int heapSize; // TweaksV: read in initializeGame() - was a global initialiser reading .\HeapAdjuster.ini (B-HEAP-05)
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
	// TweaksV only runs in GTA5_Enhanced.exe (F-HEAP-1/2)

	// TweaksV: moved here from the global initialiser; same section and key, read
	// from TweaksV.ini (B-HEAP-05); the default is 2000 MB (the original's was 1024)
	heapSize = GetPrivateProfileInt(L"HEAP_SETTINGS", L"HEAP_SIZE", 2000, TweaksV::GetIniPathW());
	if (heapSize <= 0) { // TweaksV: 0, negative or not a number (B-HEAP-03)
		TweaksV::WriteLog("HeapAdjusterEnhanced", "TweaksV.ini: HEAP_SIZE is not a valid size - using the default, 2000.");
		heapSize = 2000;
	}

	BytePattern pattern((BYTE*)"\xBA\x00\x00\x00\x00\x45\x31\xC0\x41\xB1\x01\xE8\x00\x00\x00\x00\x48\x8D\x0D", "x????xxxxxxx????xxx");
	if (!pattern.bSuccess) { // TweaksV: never checked - a miss made loc = 0x1 and crashed below (B-HEAP-01)
		TweaksV::ReportFailure("HeapAdjusterEnhanced", "The heap size could not be changed: the game code it patches "
			"was not found (a game update may have moved it).");
		return;
	}
	loc = pattern.get(1);

	if (loc[-1] != 0xBA) { // TweaksV: the target must be the imm32 of "mov edx, imm32" (B-HEAP-04)
		TweaksV::ReportFailure("HeapAdjusterEnhanced", "The heap size could not be changed: the game code found is "
			"not the expected instruction.");
		return;
	}

	vanillaHeapSize = *(int*)loc / 1024 / 1024;

	// TweaksV: the value found must be a plausible heap size before anything is written (plan 5.9 R2)
	if (*(int*)loc <= 0 || *(int*)loc % (1024 * 1024) != 0) {
		char text[96];
		snprintf(text, sizeof(text), "unexpected value 0x%08X", static_cast<unsigned int>(*(int*)loc));
		TweaksV::ReportFailure("HeapAdjusterEnhanced", std::string("The heap size could not be changed: the game code "
			"found holds an ") + text + ".");
		return;
	}
	if (vanillaHeapSize > 1024) // TweaksV: plan 4.4 - another heap adjuster may have changed it already
		TweaksV::WriteLog("HeapAdjusterEnhanced", "WARNING: the heap size found (" + std::to_string(vanillaHeapSize) +
			" MB) is larger than the game's own - another heap adjuster may be active.");

	// TweaksV: clamp to [the game's own size, 2047 MB]; the game supports at most 2047 MB (B-HEAP-02/03)
	const int requestedHeapSize = heapSize;
	if (heapSize < vanillaHeapSize)
		heapSize = vanillaHeapSize;
	if (heapSize > 2047)
		heapSize = 2047;
	if (heapSize != requestedHeapSize)
		TweaksV::WriteLog("HeapAdjusterEnhanced", "TweaksV.ini: HEAP_SIZE=" + std::to_string(requestedHeapSize) +
			" is outside " + std::to_string(vanillaHeapSize) + "..2047 - using " + std::to_string(heapSize) + ".");

	// TweaksV: 64-bit math - heapSize * 1024 * 1024 overflowed int from 2048 up (B-HEAP-02)
	const long long newHeapBytes = (long long)heapSize * 1024 * 1024;

	if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN finds and checks everything but writes nothing
		TweaksV::WriteLog("HeapAdjusterEnhanced", "DRY_RUN: would change the heap size from " +
			std::to_string(vanillaHeapSize) + " MB to " + std::to_string(heapSize) + " MB.");
		return;
	}

	DWORD oldProtect;

	if (!VirtualProtect(loc, 4, PAGE_EXECUTE_READWRITE, &oldProtect)) { // TweaksV: checked (plan 5.9 R3)
		const DWORD error = GetLastError();
		TweaksV::ReportFailure("HeapAdjusterEnhanced", "The heap size could not be changed: the game code could not "
			"be made writable (" + TweaksV::DescribeError(error) + ").");
		return;
	}

	*(int*)loc = (int)newHeapBytes; // TweaksV: was heapSize * 1024 * 1024
	// default in 889.22 Enhanced was 648MiB,

	VirtualProtect(loc, 4, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), loc, 4); // TweaksV: game code was changed (plan 5.9 R3)

	if (TweaksV::IsDiagnosticMode()) { // TweaksV: DIAGNOSTIC_MODE logs every write
		char text[160];
		snprintf(text, sizeof(text), "DIAGNOSTIC: wrote 0x%08X (%d MB) over 0x%08X (%d MB) at RVA 0x%llX.",
			static_cast<unsigned int>(*(int*)loc), heapSize, static_cast<unsigned int>(vanillaHeapSize) * 1024u * 1024u,
			vanillaHeapSize, (unsigned long long)(loc - (BYTE*)GetModuleHandleW(nullptr)));
		TweaksV::WriteLog("HeapAdjusterEnhanced", text);
	}

	Logging();
}

} // namespace HeapAdjusterEnhanced
