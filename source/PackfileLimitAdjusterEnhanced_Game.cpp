// TweaksV: carried over from PackfileLimitAdjusterEnhanced (Game.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0) - based on the
// original Packfile Limit Adjuster by alloc8or.
// Modified for TweaksV: GTA V Enhanced only, namespace, TweaksV.ini/TweaksV.log,
// the array is moved before the limits are raised, two-pass patching and safety fixes.
// Every change is marked "TweaksV:".
#include <windows.h>				// TweaksV: was #include "pch.h"
#include "MemScanner.h"				// TweaksV: the shared scanner
#include <cstdint>
// TweaksV: <string_view>, <cassert> and <fstream> removed - unused
#include "PackfileLimitAdjusterEnhanced_Game.h"
// TweaksV: #include "Hooking.h" removed - the call is redirected by TweaksV::RedirectCalls
#include "PackfileLimitAdjusterEnhanced_Logging.h"
#include "GameVersion.h"
#include <cstring>					// TweaksV: memcmp, memset
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVSettings.h"	// TweaksV: PACKFILEARRAY_SIZE, DRY_RUN, DIAGNOSTIC_MODE
#include "TweaksVPatch.h"	// TweaksV: DescribeAddress, WriteCode, RedirectCalls
#include "TweaksVSummary.h"	// TweaksV: start-up summary

namespace PackfileLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{

// TweaksV: the game edition flag is removed - TweaksV only runs in GTA5_Enhanced.exe
uint32_t VanillaPackFileArray_Size;
uint32_t VanillaPackFileArray_InConfigFile_Size;
uint32_t PackFileArray_Size;
void** newArray;

constexpr FileVersion VERSION_1_1013_17_E{ 1, 0, 1013, 17 }; // Mansions DLC
constexpr FileVersion VERSION_1_1158_13_E{ 1, 0, 1158, 13 }; // Kortz Center DLC

// TweaksV: set by FatalErrorExit(); initializeGame() stops at its next check
static bool fatalError = false;

// TweaksV: the new array always has the maximum size, whatever PACKFILEARRAY_SIZE says, because
// it is moved before the game's own size is read. Archive indices are 16-bit.
static const uint32_t MaxPackFileArray_Size = 65535;
static_assert(static_cast<int>(MaxPackFileArray_Size) == TweaksV::kPackfileArraySize.maxValue, "PACKFILEARRAY_SIZE's maximum must be the array's size"); // TweaksV

static const char* const kTag = "PackfileLimitAdjusterEnhanced"; // TweaksV: for the shared helpers' log lines

// TweaksV: declared here instead of in Game.h - a header must not declare static functions
static void EnhancedPatch();
static void SetStreamingInterface(char* si);

static void(*g_origSetStreamingInterface)(char*);

static void SetStreamingInterface(char* si)
{
	// TweaksV: the game calls this later, on its own thread - nothing may throw out of here, and
	// the original is always called
	if (si) { // TweaksV: + null check
		VanillaPackFileArray_InConfigFile_Size = *(uint32_t*)(si + 24);
		*(uint32_t*)(si + 24) = PackFileArray_Size; // archive count at offset 0x18 (TweaksV: comment reworded)
	}

	try {
		// TweaksV: three log lines - the original wrote one string with "\n" in it
		Logging("Vanilla Packfile List Size is: " + std::to_string(VanillaPackFileArray_Size));
		Logging("Vanilla Packfile List Size From ConfigFile is: " + std::to_string(VanillaPackFileArray_InConfigFile_Size));
		Logging("Packfile List Size Adjusted to: " + std::to_string(PackFileArray_Size));
	}
	catch (...) {
	}
	// Log everything here, as VanillaPackFileArray_InConfigFile_Size is only initialized when the hook is called

	g_origSetStreamingInterface(si);
}

void initializeGame()
{
	// TweaksV: ClearLog() removed - TweaksV.log is cleared once per start

	PackFileArray_Size = (uint32_t)TweaksV::GetNumber(TweaksV::kPackfileArraySize); // TweaksV: from TweaksV's settings table, which checks it (was .\PackfileLimitAdjusterEnhanced.ini); default 13000 (the original's was 12000)
	// TweaksV: the game edition check is removed - TweaksV only runs in GTA5_Enhanced.exe

	// TweaksV: two patterns depend on the game build; without it the right ones cannot be chosen
	if (GetCurrentProcessVersionComparable() == 0) {
		Logging("The game's file version could not be read, so the patterns for this game build cannot be chosen.");
		FatalErrorExit();
		return;
	}

	newArray = (void**)AllocateStubMemory(sizeof(void*) * MaxPackFileArray_Size, PAGE_READWRITE); // TweaksV: always the full array, read/write data - was the configured size, RWX
	if (!newArray) { // TweaksV: was not checked before the memset
		Logging("The new packfile array could not be allocated near the game.");
		FatalErrorExit();
		return;
	}

	memset(newArray, 0, sizeof(void*) * MaxPackFileArray_Size); // TweaksV: the full array

	// TweaksV: the array is moved FIRST and the limits are raised after, so a failure at any point
	// leaves the game safe - upstream raised the limits first. The game edition if/else became the
	// single EnhancedPatch() call below.
	AdjustPackFileArrayLimit();
	if (fatalError) { // TweaksV: stop at the first failed step - nothing points at the new array yet
		VirtualFree(newArray, 0, MEM_RELEASE);
		newArray = nullptr;
		return;
	}
	EnhancedPatch();
	if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN changed nothing - give the unused array back
		VirtualFree(newArray, 0, MEM_RELEASE);
		newArray = nullptr;
		return;
	}
	if (fatalError) // TweaksV: the array was moved, but the game still uses its own limits - harmless
		return;

	// TweaksV: the original lines follow when the game calls SetStreamingInterface (above)
	Logging("Packfile array moved and its limits raised from " + std::to_string(VanillaPackFileArray_Size) + " to " +
		std::to_string(PackFileArray_Size) + "; the archive count from the game's configuration is replaced when the game reads it.");
}

// TweaksV: the other game edition's patch function is removed

static void EnhancedPatch() {

	// TweaksV: pass 1 - find and check all three sites before anything is written

	// Direct size

	uint8_t* address;
	if (GetCurrentProcessVersionComparable() >= VERSION_1_1158_13_E.ToComparable()) {
		address = MemScanner::Find(TweaksV::Pattern("P-E-D3"));
		if (address) address += 7; // TweaksV: + null check
	}
	else if (GetCurrentProcessVersionComparable() >= VERSION_1_1013_17_E.ToComparable()) {
		address = MemScanner::Find(TweaksV::Pattern("P-E-D2"));
		if (address) address += 7; // TweaksV: + null check
	}
	else {
		address = MemScanner::Find(TweaksV::Pattern("P-E-D1"));
		if (address) address += 3; // TweaksV: + null check
	}
	if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner
		Logging("The packfile array's direct size limit was not found in the game code.");
		FatalErrorExit();
		return;
	}
	// TweaksV: the size must be the imm32 of "cmp rcx, imm32" (48 81 F9) and plausible
	if (memcmp(address - 3, "\x48\x81\xF9", 3) != 0) {
		Logging("The packfile array's direct size limit at " + TweaksV::DescribeAddress(address) + " is not a \"cmp rcx, imm32\".");
		FatalErrorExit();
		return;
	}
	VanillaPackFileArray_Size = *(uint32_t*)address; // TweaksV: read in pass 1, written in pass 2
	if (TweaksV::IsDiagnosticMode())
		Logging("DIAGNOSTIC: direct size at " + TweaksV::DescribeAddress(address) + " holds " + std::to_string(VanillaPackFileArray_Size));
	if (VanillaPackFileArray_Size == 0 || VanillaPackFileArray_Size > MaxPackFileArray_Size) {
		Logging("The game's own packfile list size, " + std::to_string(VanillaPackFileArray_Size) + ", is not plausible.");
		FatalErrorExit();
		return;
	}
	uint8_t* directAddr = address; // TweaksV: kept for pass 2 - "address" is reused below

	// Indirect size
	address = MemScanner::Find(TweaksV::Pattern("P-E-I1"));
	if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner
		Logging("The packfile array's indirect size limit was not found in the game code.");
		FatalErrorExit();
		return;
	}
	address += 2;
	uint8_t* indirectAddr = reinterpret_cast<uint8_t*>(address) + *(int32_t*)address + 4;
	if (TweaksV::IsDiagnosticMode())
		Logging("DIAGNOSTIC: indirect size variable at " + TweaksV::DescribeAddress(indirectAddr) + " holds " +
			(TweaksV::IsInsideGameImage(indirectAddr) ? std::to_string(*(uint32_t*)indirectAddr) : std::string("?")));
	if (!TweaksV::IsInsideGameImage(indirectAddr)) { // TweaksV: the variable must be the game's own
		Logging("The packfile array's indirect size variable is not inside the game: " + TweaksV::DescribeAddress(indirectAddr) + ".");
		FatalErrorExit();
		return;
	}

	address = MemScanner::Find(TweaksV::Pattern("P-E-H1"));
	if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner
		Logging("The call that reads the game's streaming configuration was not found in the game code.");
		FatalErrorExit();
		return;
	}
	address += 0x10;
	// TweaksV: it must be a call (E8) to a function inside the game - otherwise another mod has
	// hooked it already
	const uint8_t* callTarget = TweaksV::CallTarget(address);
	if (TweaksV::IsDiagnosticMode())
		Logging("DIAGNOSTIC: streaming configuration call at " + TweaksV::DescribeAddress(address) + " -> " +
			(callTarget ? TweaksV::DescribeAddress(callTarget) : std::string("not a call")));
	if (!callTarget || !TweaksV::IsInsideGameImage(callTarget)) {
		Logging("The call that reads the game's streaming configuration at " + TweaksV::DescribeAddress(address) +
			" is not a call into the game (another mod may have hooked it already).");
		FatalErrorExit();
		return;
	}

	// TweaksV: clamp to [the game's own size, 65535] - smaller would shrink the game's limits
	const uint32_t requestedSize = PackFileArray_Size;
	if (PackFileArray_Size < VanillaPackFileArray_Size)
		PackFileArray_Size = VanillaPackFileArray_Size;
	if (PackFileArray_Size > MaxPackFileArray_Size)
		PackFileArray_Size = MaxPackFileArray_Size;
	if (PackFileArray_Size != requestedSize)
		Logging("TweaksV.ini: PACKFILEARRAY_SIZE=" + std::to_string(requestedSize) + " is outside " +
			std::to_string(VanillaPackFileArray_Size) + ".." + std::to_string(MaxPackFileArray_Size) + " - using " +
			std::to_string(PackFileArray_Size) + ".");

	if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN stops after pass 1
		Logging("DRY_RUN: would change the packfile limits from " + std::to_string(VanillaPackFileArray_Size) + " to " +
			std::to_string(PackFileArray_Size) + " and hook the streaming configuration call at " + TweaksV::DescribeAddress(address) + ".");
		return;
	}

	// TweaksV: pass 2 - the original writes, for the sites found above: both sizes and the hooked call
	// together, through TweaksV's one code writer, so everything changes or nothing does (upstream set
	// the direct size's page to PAGE_EXECUTE_READWRITE a second time instead). The call goes
	// through a read+execute stub; the original is set before the call is redirected. Was HookManager::SetCall.
	const bool hooked = TweaksV::RedirectCalls(
		{ { address, reinterpret_cast<const void*>(&SetStreamingInterface), "streaming configuration call",
			reinterpret_cast<void**>(&g_origSetStreamingInterface) } },
		{ { directAddr, TweaksV::BytesOf(PackFileArray_Size), "packfile direct size limit" },
		  { indirectAddr, TweaksV::BytesOf(PackFileArray_Size), "packfile indirect size limit" } }, kTag);
	if (!hooked) { // TweaksV: nothing was written
		Logging("The call that reads the game's streaming configuration could not be redirected.");
		FatalErrorExit();
	}
}

// TweaksV: the absolute-address relocation function is removed - only the other game edition used it

void RelocateRelativePackFileArray(void* newPackFileArray, const std::vector<PatternPair>& list)
{
	// TweaksV: pass 1 - find and check every address before writing anything
	std::vector<uint8_t*> addresses;
	void* oldAddress = nullptr;

	for (auto& entry : list)
	{
		auto address = MemScanner::Find(*entry.pattern);
		if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner
			Logging(std::string("A reference to the packfile array was not found in the game code: ") + entry.pattern->label);
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
			Logging("The references to the packfile array do not all point to the game's own array: " +
				TweaksV::DescribeAddress(oldAddress) + " and " + TweaksV::DescribeAddress(curTarget) +
				" (another mod may have moved it already).");
			FatalErrorExit();
			return;
		}
		addresses.push_back(address);
	}

	if (TweaksV::IsDryRun()) // TweaksV: DRY_RUN stops after pass 1
	{
		Logging("DRY_RUN: would move the packfile array (" + std::to_string(addresses.size()) + " references).");
		return;
	}

	// TweaksV: pass 2 - the original write, for every address found above, through TweaksV's one code
	// writer: the array moves everywhere or nowhere
	std::vector<TweaksV::CodeWrite> writes;
	for (auto address : addresses)
	{
		writes.push_back({ address, TweaksV::BytesOf(static_cast<int32_t>((intptr_t)newPackFileArray - (intptr_t)address - 4)),
			"packfile array reference" });
	}
	if (!TweaksV::WriteCode(writes, kTag))
		FatalErrorExit();
}

void AdjustPackFileArrayLimit()
{
	std::vector<PatternPair> patternPairs;

	// TweaksV: only the GTA V Enhanced list is kept
	patternPairs = {
		// Data
		{ &TweaksV::Pattern("P-E-R01"), 3 },
		{ &TweaksV::Pattern("P-E-R02"), 3 },
		{ &TweaksV::Pattern("P-E-R03"), 3 },
		{ &TweaksV::Pattern("P-E-R04"), 5 },
		{ &TweaksV::Pattern("P-E-R05"), 5 },
		{ &TweaksV::Pattern("P-E-R06"), 3 },
		{ &TweaksV::Pattern("P-E-R07"), 3 },
		{ &TweaksV::Pattern("P-E-R08"), 3 },
		{ &TweaksV::Pattern("P-E-R09"), 39 },
		{ &TweaksV::Pattern("P-E-R10"), 7 },
		{ &TweaksV::Pattern("P-E-R11"), 3 },
		{ &TweaksV::Pattern("P-E-R12"), 3 },
		{ &TweaksV::Pattern("P-E-R13"), 3 },
		{ &TweaksV::Pattern("P-E-R14"), 3 },
		{ &TweaksV::Pattern("P-E-R15"), 3 },
		{ &TweaksV::Pattern("P-E-R16"), 3 },
		{ &TweaksV::Pattern("P-E-R17"), 41 },
		{ &TweaksV::Pattern("P-E-R18"), 3 },
		{ &TweaksV::Pattern("P-E-R19"), 3 },
		{ &TweaksV::Pattern("P-E-R20"), 3 },
		{ &TweaksV::Pattern("P-E-R21"), 3 },
		{ &TweaksV::Pattern("P-E-R22"), 3 },
		// Write
		{ &TweaksV::Pattern("P-E-R23"), 3},
		{ &TweaksV::Pattern("P-E-R24"), 3},
		{ &TweaksV::Pattern("P-E-R25"), 3},
		{ &TweaksV::Pattern("P-E-R26"), 3},
		{ &TweaksV::Pattern("P-E-R27"), 3},
		{ &TweaksV::Pattern("P-E-R28"), 3},
		{ &TweaksV::Pattern("P-E-R29"), 3},
		{ &TweaksV::Pattern("P-E-R30"), 3},
	};

	// TweaksV: the 1158.13+ pattern was "BF ? ? ? ? BE ? ? ? ? 4C 8D 05", which also matched obfuscated
	// code further on (RVA 0x582A227 on 1.0.1158.16); with its two constants it matches only this reference
	patternPairs.emplace_back(GetCurrentProcessVersionComparable() >= VERSION_1_1158_13_E.ToComparable() ?
		PatternPair{ &TweaksV::Pattern("P-E-R31a"), 13 } : PatternPair{ &TweaksV::Pattern("P-E-R31b"), 20 });

	RelocateRelativePackFileArray(newArray, patternPairs);
}

// TweaksV: FindPrevFreeRegion and AllocateStubMemory moved to StubMemory.cpp (one copy
// for every plugin); AllocateStubMemory gained the protect parameter used above

// TweaksV: kept under its original name, but it no longer shows a MessageBox or calls exit(1)
// inside DllMain: it logs, reports for the start-up summary and makes initializeGame()
// stop at its next check
void FatalErrorExit() {
	Logging("FATAL ERROR - PackfileLimitAdjusterEnhanced is skipped for this session");
	TweaksV::ReportFailure("PackfileLimitAdjusterEnhanced", "The packfile limit is not changed this session: a required "
		"step failed (details in TweaksV.log).");
	fatalError = true;
}

} // namespace PackfileLimitAdjusterEnhanced
