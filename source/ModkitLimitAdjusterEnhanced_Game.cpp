// TweaksV: carried over from ModkitLimitAdjusterEnhanced (Game.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0) - based on the
// ModkitLimit Extender by Tanuki / 0x-FADED (MIT; its LICENSE notes a part adapted from FiveM).
// Modified for TweaksV: GTA V Enhanced only, namespace, TweaksV.ini/TweaksV.log,
// two-pass patching and safety fixes. Every change is marked "TweaksV:".
#include <windows.h>				// TweaksV: was #include "pch.h"
#include "MemScanner.h"				// TweaksV: the shared scanner
#include <cstdint>
// TweaksV: <string_view>, <cassert> and <fstream> removed - unused
#include "ModkitLimitAdjusterEnhanced_Game.h"
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

// TweaksV: set by FatalErrorExit(); initializeGame() stops at its next check
static bool fatalError = false;

// TweaksV: the new array always has the maximum size, whatever MODKITARRAY_SIZE says, so it
// can never be smaller than the game's own array. Modkit IDs are 16-bit.
static const uint32_t MaxModkitArray_Size = 65536;
static_assert(static_cast<int>(MaxModkitArray_Size) == TweaksV::kModkitArraySize.maxValue, "MODKITARRAY_SIZE's maximum must be the array's size"); // TweaksV

static const char* const kTag = "ModkitLimitAdjusterEnhanced"; // TweaksV: for the shared helpers' log lines

// TweaksV: every write goes through TweaksV::WriteCode (TweaksVPatch.h): everything is made writable
// before anything is written, so a change is made everywhere or nowhere, and every page gets its own
// protection back

void initializeGame()
{
	// TweaksV: ClearLog() removed - TweaksV.log is cleared once per start

	ModkitArray_Size = (uint32_t)TweaksV::GetNumber(TweaksV::kModkitArraySize); // TweaksV: from TweaksV's settings table, which checks it (was .\ModkitLimitAdjusterEnhanced.ini)
	// TweaksV: the game edition check is removed - TweaksV only runs in GTA5_Enhanced.exe

	newArray = (void*)AllocateStubMemory(sizeof(uint16_t) * MaxModkitArray_Size, PAGE_READWRITE); // TweaksV: always the full array, read/write data - was the configured size, RWX
	if (!newArray) { // TweaksV: was not checked
		Logging("The new modkit array could not be allocated near the game.");
		FatalErrorExit();
		return;
	}

	// TweaksV: the other game edition's absolute-address relocation call is removed

	AdjustModkitArrayLimit();
	if (fatalError) { // TweaksV: stop at the first failed step - nothing points at the new array yet
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
	// written unless all were found and all agree
	std::vector<BYTE*> addresses;
	for (auto& entry : list) {
		auto address = MemScanner::Find(*entry.pattern);
		if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner
			Logging(std::string("A modkit limit was not found in the game code: ") + entry.pattern->label);
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
		else if (vanillaSize != VanillaModkitArray_Size) { // TweaksV: every limit must agree
			Logging("The modkit limits disagree about the game's own size: " + std::to_string(VanillaModkitArray_Size) +
				" and " + std::to_string(vanillaSize) + " (" + entry.pattern->label + ").");
			FatalErrorExit();
			return;
		}
		addresses.push_back(address);
	}
	if (VanillaModkitArray_Size == 0 || VanillaModkitArray_Size > MaxModkitArray_Size) { // TweaksV: the game's own size must be plausible
		Logging("The game's own modkit list size, " + std::to_string(VanillaModkitArray_Size) + ", is not plausible.");
		FatalErrorExit();
		return;
	}

	// TweaksV: clamp to [the game's own size, 65536] - smaller would shrink the game's limits
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

// TweaksV: the absolute-address relocation function is removed - only the other game edition used it

void RelocateRelativeModkitArray(void* newModkitArray, const std::vector<PatternPair>& list)
{
	// TweaksV: pass 1 - find and check every address before writing anything
	std::vector<BYTE*> addresses;
	void* oldAddress = nullptr;

	for (auto& entry : list)
	{
		auto address = MemScanner::Find(*entry.pattern);
		if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner
			Logging(std::string("A reference to the modkit array was not found in the game code: ") + entry.pattern->label);
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
		// moved it
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

	// TweaksV: only the GTA V Enhanced list is kept
	patternPairs = {
		// Data
		{ &TweaksV::Pattern("M-E-R1"), 5 },
		{ &TweaksV::Pattern("M-E-R2"), 5 },
		{ &TweaksV::Pattern("M-E-R3"), 7 },
		{ &TweaksV::Pattern("M-E-R4"), 8 },
		{ &TweaksV::Pattern("M-E-R5"), 3 },
		{ &TweaksV::Pattern("M-E-R6"), 5 }
	};

	RelocateRelativeModkitArray(newArray, patternPairs);
}

void patchHardcodedArrayLimits() {

	std::vector<PatternPairExtended> patternPairs;

	// TweaksV: only the GTA V Enhanced list is kept
	patternPairs = {
		// Data
		{ &TweaksV::Pattern("M-E-L1"), 8, 1, -1 },
		{ &TweaksV::Pattern("M-E-L2"), 15, 1, 3 },
		{ &TweaksV::Pattern("M-E-L3"), 16, 1, 3 },
		{ &TweaksV::Pattern("M-E-L4"), 15, 1, 3 },
		{ &TweaksV::Pattern("M-E-L5"), 15, 1, 3 },
		{ &TweaksV::Pattern("M-E-L6"), 2, 2, 0 },
		{ &TweaksV::Pattern("M-E-L7"), 10, 1, -1 },
		{ &TweaksV::Pattern("M-E-L8"), 10, 1, -1 }
	};

	OverrideModkitArraySize(ModkitArray_Size, patternPairs);
}

// TweaksV: FindPrevFreeRegion and AllocateStubMemory moved to StubMemory.cpp (one copy
// for every plugin); AllocateStubMemory gained the protect parameter used above

// TweaksV: kept under its original name, but it no longer shows a MessageBox or calls exit(1)
// inside DllMain: it logs, reports for the start-up summary and makes initializeGame()
// stop at its next check
void FatalErrorExit() {
	Logging("FATAL ERROR - ModkitLimitAdjusterEnhanced is skipped for this session");
	TweaksV::ReportFailure("ModkitLimitAdjusterEnhanced", "The modkit limit is not changed this session: a required "
		"step failed (details in TweaksV.log).");
	fatalError = true;
}

} // namespace ModkitLimitAdjusterEnhanced
