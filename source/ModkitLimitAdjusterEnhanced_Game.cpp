// TweaksV: carried over from ModkitLimitAdjusterEnhanced (Game.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0) - based on the
// ModkitLimit Extender by Tanuki / 0x-FADED (MIT; its LICENSE notes a part adapted from FiveM).
// Modified for TweaksV: GTA V Enhanced only, namespace, TweaksV.ini/TweaksV.log,
// two-pass patching and safety fixes (B-MOD-01..06). Every change is marked "TweaksV:".
#include <windows.h>				// TweaksV: was #include "pch.h"
#include "MemScanner.h"				// TweaksV: the shared scanner
#include <cstdint>
#include <string_view>
#include <cassert>
#include "ModkitLimitAdjusterEnhanced_Game.h"
#include <fstream>
#include "ModkitLimitAdjusterEnhanced_Logging.h"
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVSettings.h"	// TweaksV: MODKITARRAY_SIZE, DRY_RUN, DIAGNOSTIC_MODE
#include "TweaksVPatch.h"	// TweaksV: DescribeAddress, WriteCode
#include "TweaksVSummary.h"	// TweaksV: start-up summary

namespace ModkitLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{

uint32_t VanillaModkitArray_Size;
uint32_t ModkitArray_Size;
void* newArray;

// TweaksV: set by FatalErrorExit(); initializeGame() stops at its next check (plan 5.8)
static bool fatalError = false;

// TweaksV: the new array always has the maximum size, whatever MODKITARRAY_SIZE says, so it
// can never be smaller than the game's own array (B-MOD-04). Modkit IDs are 16-bit.
static const uint32_t MaxModkitArray_Size = 65536;
static_assert(static_cast<int>(MaxModkitArray_Size) == TweaksV::kModkitArraySize.maxValue, "MODKITARRAY_SIZE's maximum must be the array's size"); // TweaksV

static const char* const kTag = "ModkitLimitAdjusterEnhanced"; // TweaksV: for the shared helpers' log lines

// TweaksV: every write goes through TweaksV::WriteCode (TweaksVPatch.h): everything is made writable
// before anything is written, so a change is made everywhere or nowhere, and every page gets its own
// protection back

void initializeGame()
{
	// TweaksV: ClearLog() removed - TweaksV.log is cleared once per start (plan 5.7)

	ModkitArray_Size = (uint32_t)TweaksV::GetNumber(TweaksV::kModkitArraySize); // TweaksV: from TweaksV's settings table, which checks it (was .\ModkitLimitAdjusterEnhanced.ini)
	// TweaksV: the game edition check is removed - TweaksV only runs in GTA5_Enhanced.exe (F-MOD-1/2)

	newArray = (void*)AllocateStubMemory(sizeof(uint16_t) * MaxModkitArray_Size, PAGE_READWRITE); // TweaksV: always the full array, read/write data - was the configured size, RWX (B-MOD-03/04)
	if (!newArray) { // TweaksV: was not checked (B-MOD-02)
		Logging("The new modkit array could not be allocated near the game.");
		FatalErrorExit();
		return;
	}

	// TweaksV: the other game edition's absolute-address relocation call is removed (F-MOD-3)

	AdjustModkitArrayLimit();
	if (fatalError) { // TweaksV: stop at the first failed step (B-MOD-05) - nothing points at the new array yet
		VirtualFree(newArray, 0, MEM_RELEASE);
		newArray = nullptr;
		return;
	}
	patchHardcodedArrayLimits();
	if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN changed nothing - give the unused array back
		VirtualFree(newArray, 0, MEM_RELEASE);
		newArray = nullptr;
		return;
	}
	if (fatalError) // TweaksV: the array was moved, but the game still uses its own limits - harmless
		return;

	// TweaksV: two log lines - the original wrote one string with "\n" in it
	Logging("Vanilla Modkit List Size is: " + std::to_string(VanillaModkitArray_Size));
	Logging("Modkit List Size Adjusted to: " + std::to_string(ModkitArray_Size));
}

void OverrideModkitArraySize(uint32_t newSize, const std::vector<PatternPairExtended>& list) {

	// TweaksV: pass 1 - find every limit and decode the game's own size from each; nothing is
	// written unless all were found and all agree (plan 5.9 R1/R2)
	std::vector<BYTE*> addresses;
	for (auto& entry : list) {
		auto address = MemScanner::FindPatternBmh(entry.pattern);
		if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner (B-MOD-05)
			Logging("A modkit limit was not found in the game code: " + entry.pattern);
			FatalErrorExit();
			return;
		}
		address += entry.offset;
		const uint32_t vanillaSize = (uint32_t)((*(uint32_t*)address - entry.addModifier) / entry.mulModifier); // TweaksV: decoded for every entry
		if (TweaksV::IsDiagnosticMode())
			Logging("DIAGNOSTIC: limit at " + TweaksV::DescribeAddress(address) + " holds " + std::to_string(*(uint32_t*)address) +
				" -> game size " + std::to_string(vanillaSize));
		if (VanillaModkitArray_Size == 0) {
			VanillaModkitArray_Size = vanillaSize;
		}
		else if (vanillaSize != VanillaModkitArray_Size) { // TweaksV: every limit must agree (plan 5.9 R2)
			Logging("The modkit limits disagree about the game's own size: " + std::to_string(VanillaModkitArray_Size) +
				" and " + std::to_string(vanillaSize) + " (" + entry.pattern + ").");
			FatalErrorExit();
			return;
		}
		addresses.push_back(address);
	}
	if (VanillaModkitArray_Size == 0 || VanillaModkitArray_Size > MaxModkitArray_Size) { // TweaksV: plausible (plan 5.9 R2)
		Logging("The game's own modkit list size, " + std::to_string(VanillaModkitArray_Size) + ", is not plausible.");
		FatalErrorExit();
		return;
	}

	// TweaksV: clamp to [the game's own size, 65536] - smaller would shrink the game's limits (B-MOD-04)
	const uint32_t requestedSize = newSize;
	if (newSize < VanillaModkitArray_Size)
		newSize = VanillaModkitArray_Size;
	if (newSize > MaxModkitArray_Size)
		newSize = MaxModkitArray_Size;
	if (newSize != requestedSize)
		Logging("TweaksV.ini: MODKITARRAY_SIZE=" + std::to_string(requestedSize) + " is outside " +
			std::to_string(VanillaModkitArray_Size) + ".." + std::to_string(MaxModkitArray_Size) + " - using " + std::to_string(newSize) + ".");
	ModkitArray_Size = newSize; // TweaksV: the size actually used, for the log

	if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN stops after pass 1
		Logging("DRY_RUN: would change " + std::to_string(addresses.size()) + " modkit limits from " +
			std::to_string(VanillaModkitArray_Size) + " to " + std::to_string(newSize) + ".");
		return;
	}

	// TweaksV: pass 2 - the original write, for every address found above, through TweaksV's one code
	// writer: all limits change or none
	std::vector<TweaksV::CodeWrite> writes;
	for (size_t i = 0; i < addresses.size(); i++) {
		auto address = addresses[i];
		auto& entry = list[i];
		writes.push_back({ address, TweaksV::BytesOf((uint32_t)((newSize * entry.mulModifier) + entry.addModifier)), "modkit limit" });
	}
	if (!TweaksV::WriteCode(writes, kTag))
		FatalErrorExit();
}

// TweaksV: the absolute-address relocation function is removed - only the other game edition used it (F-MOD-4)

void RelocateRelativeModkitArray(void* newModkitArray, const std::vector<PatternPair>& list)
{
	// TweaksV: pass 1 - find and check every address before writing anything (plan 5.9 R1)
	std::vector<BYTE*> addresses;
	void* oldAddress = nullptr;

	for (auto& entry : list)
	{
		auto address = MemScanner::FindPatternBmh(entry.pattern);
		if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner (B-MOD-05)
			Logging("A reference to the modkit array was not found in the game code: " + entry.pattern);
			FatalErrorExit();
			return;
		}
		address += entry.offset;
		if (!oldAddress)
		{
			oldAddress = *(int32_t*)address + address + 4;
		}

		auto curTarget = *(int32_t*)address + address + 4;;
		if (TweaksV::IsDiagnosticMode()) // TweaksV
			Logging("DIAGNOSTIC: reference at " + TweaksV::DescribeAddress(address) + " -> " + TweaksV::DescribeAddress(curTarget));
		// TweaksV: was assert(curTarget == oldAddress), which Release builds leave out - now a real
		// check, and the array must still be inside the game; otherwise another mod has already
		// moved it (B-MOD-01, plan 5.9 R6)
		if (curTarget != oldAddress || !TweaksV::IsInsideGameImage(curTarget))
		{
			Logging("The references to the modkit array do not all point to the game's own array: " +
				TweaksV::DescribeAddress(oldAddress) + " and " + TweaksV::DescribeAddress(curTarget) + " (another mod may have moved it already).");
			FatalErrorExit();
			return;
		}
		addresses.push_back(address);
	}

	if (TweaksV::IsDryRun()) // TweaksV: DRY_RUN stops after pass 1
	{
		Logging("DRY_RUN: would move the modkit array (" + std::to_string(addresses.size()) + " references).");
		return;
	}

	// TweaksV: pass 2 - the original write, for every address found above, through TweaksV's one code
	// writer: the array moves everywhere or nowhere
	std::vector<TweaksV::CodeWrite> writes;
	for (auto address : addresses)
	{
		writes.push_back({ address, TweaksV::BytesOf(static_cast<int32_t>((intptr_t)newModkitArray - (intptr_t)address - 4)),
			"modkit array reference" });
	}
	if (!TweaksV::WriteCode(writes, kTag))
		FatalErrorExit();
}

void AdjustModkitArrayLimit()
{
	std::vector<PatternPair> patternPairs;

	// TweaksV: only the GTA V Enhanced list is kept (F-MOD-5)
	patternPairs = {
		// Data
		{ "31 c9 48 8d 05 ? ? ? ? eb", 5 },
		{ "89 c2 48 8d 0d ? ? ? ? eb", 5 },
		{ "41 0f b7 c2 48 8d 0d ? ? ? ? 0f b7 04 41", 7 },
		{ "57 48 83 ec ? 48 8d 0d ? ? ? ? 41 b8", 8 },
		{ "48 8d 3d ? ? ? ? 0f b7 3c 47", 3 },
		{ "77 ? 48 8d 15 ? ? ? ? 0f b7 14 42", 5 }
	};

	RelocateRelativeModkitArray(newArray, patternPairs);
}

void patchHardcodedArrayLimits() {

	std::vector<PatternPairExtended> patternPairs;

	// TweaksV: only the GTA V Enhanced list is kept (F-MOD-6)
	patternPairs = {
		// Data
		{ "0f b7 c0 66 ba ? ? 3d ? ? ? ? 77", 8, 1, -1 },
		{ "48 83 c5 ? 48 83 c0 ? 48 83 c7", 15, 1, 3 },
		{ "74 ? 48 83 c5 ? 48 83 c3 ? 48 83 c0 ? 48 3d", 16, 1, 3 },
		{ "48 83 c7 ? 48 83 c2 ? 48 83 c5 ? 48 81 fd", 15, 1, 3 },
		{ "48 83 c7 ? 48 83 c2 ? 48 83 c6 ? 48 81 fe", 15, 1, 3 },
		{ "41 b8 ? ? ? ? b2 ? e8 ? ? ? ? 66 0f 76 c0", 2, 2, 0 },
		{ "0f b7 41 ? 66 bf ? ? 48 3d", 10, 1, -1 },
		{ "41 0f b7 ca 66 b8 ? ? 81 f9", 10, 1, -1 }
	};

	OverrideModkitArraySize(ModkitArray_Size, patternPairs);
}

// TweaksV: FindPrevFreeRegion and AllocateStubMemory moved to StubMemory.cpp (one copy
// for every plugin); AllocateStubMemory gained the protect parameter used above

// TweaksV: kept under its original name, but it no longer shows a MessageBox or calls exit(1)
// inside DllMain (plan 5.8): it logs, reports for the start-up summary and makes initializeGame()
// stop at its next check
void FatalErrorExit() {
	Logging("FATAL ERROR - ModkitLimitAdjusterEnhanced is skipped for this session");
	TweaksV::ReportFailure("ModkitLimitAdjusterEnhanced", "The modkit limit is not changed this session: a required "
		"step failed (details in TweaksV.log).");
	fatalError = true;
}

} // namespace ModkitLimitAdjusterEnhanced
