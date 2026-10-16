// TweaksV: carried over from PackfileLimitAdjusterEnhanced (Game.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0) - based on the
// original Packfile Limit Adjuster by alloc8or.
// Modified for TweaksV: GTA V Enhanced only, namespace, TweaksV.ini/TweaksV.log,
// the array is moved before the limits are raised, two-pass patching and safety fixes
// (B-PACK-01..08). Every change is marked "TweaksV:".
#include <windows.h>				// TweaksV: was #include "pch.h"
#include "MemScanner.h"				// TweaksV: the shared scanner
#include <cstdint>
#include <string_view>
#include <cassert>
#include "PackfileLimitAdjusterEnhanced_Game.h"
#include <fstream>
#include "Hooking.h"
#include "PackfileLimitAdjusterEnhanced_Logging.h"
#include "GameVersion.h"
#include <cstring>					// TweaksV: memcmp, memset
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVIni.h"		// TweaksV: TweaksV.ini, DRY_RUN, DIAGNOSTIC_MODE
#include "TweaksVPatch.h"	// TweaksV: DescribeAddress, MakeWritable, RestoreProtection
#include "TweaksVSummary.h"	// TweaksV: start-up summary

namespace PackfileLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{

// TweaksV: the game edition flag is removed - TweaksV only runs in GTA5_Enhanced.exe (F-PACK-1)
uint32_t VanillaPackFileArray_Size;
uint32_t VanillaPackFileArray_InConfigFile_Size;
uint32_t PackFileArray_Size;
void** newArray;

constexpr FileVersion VERSION_1_1013_17_E{ 1, 0, 1013, 17 }; // Mansions DLC
constexpr FileVersion VERSION_1_1158_13_E{ 1, 0, 1158, 13 }; // Kortz Center DLC

// TweaksV: set by FatalErrorExit(); initializeGame() stops at its next check (plan 5.8)
static bool fatalError = false;

// TweaksV: the new array always has the maximum size, whatever PACKFILEARRAY_SIZE says, because
// it is moved before the game's own size is read (plan 2.3, 5.9 R4). Archive indices are 16-bit.
static const uint32_t MaxPackFileArray_Size = 65535;

static const char* const kTag = "PackfileLimitAdjusterEnhanced"; // TweaksV: for the shared helpers' log lines

// TweaksV: declared here instead of in Game.h - a header must not declare static functions (B-PACK-08)
static void EnhancedPatch();
static void SetStreamingInterface(char* si);

static void(*g_origSetStreamingInterface)(char*);

static void SetStreamingInterface(char* si)
{
	// TweaksV: the game calls this later, on its own thread - nothing may throw out of here, and
	// the original is always called
	if (si) { // TweaksV: + null check
		VanillaPackFileArray_InConfigFile_Size = *(uint32_t*)(si + 24);
		*(uint32_t*)(si + 24) = PackFileArray_Size; // archive count at offset 0x18 (TweaksV: comment reworded, F-PACK-7)
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
	// TweaksV: ClearLog() removed - TweaksV.log is cleared once per start (plan 5.7)

	PackFileArray_Size = (uint32_t)GetPrivateProfileIntA("PACKFILE_SETTINGS", "PACKFILEARRAY_SIZE", 13000, TweaksV::GetIniPathA()); // TweaksV: TweaksV.ini (was .\PackfileLimitAdjusterEnhanced.ini); default 13000 (the original's was 12000)
	if (PackFileArray_Size == 0) { // TweaksV: 0 or not a number - use the default (plan 5.6)
		Logging("TweaksV.ini: PACKFILEARRAY_SIZE is not a valid size - using the default, 13000.");
		PackFileArray_Size = 13000;
	}
	// TweaksV: the game edition check is removed - TweaksV only runs in GTA5_Enhanced.exe (F-PACK-2)

	// TweaksV: two patterns depend on the game build; without it the right ones cannot be chosen
	if (GetCurrentProcessVersionComparable() == 0) {
		Logging("The game's file version could not be read, so the patterns for this game build cannot be chosen.");
		FatalErrorExit();
		return;
	}

	newArray = (void**)AllocateStubMemory(sizeof(void*) * MaxPackFileArray_Size, PAGE_READWRITE); // TweaksV: always the full array, read/write data - was the configured size, RWX (plan 5.9 R4)
	if (!newArray) { // TweaksV: was not checked before the memset (B-PACK-04)
		Logging("The new packfile array could not be allocated near the game.");
		FatalErrorExit();
		return;
	}

	memset(newArray, 0, sizeof(void*) * MaxPackFileArray_Size); // TweaksV: the full array

	// TweaksV: the array is moved FIRST and the limits are raised after, so a failure at any point
	// leaves the game safe - upstream raised the limits first (B-PACK-05, plan 2.3, 5.9 R8). The
	// game edition if/else became the single EnhancedPatch() call below (F-PACK-3).
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

// TweaksV: the other game edition's patch function is removed (F-PACK-4)

static void EnhancedPatch() {

	// TweaksV: pass 1 - find and check all three sites before anything is written (plan 5.9 R1/R2)

	// Direct size

	uint8_t* address;
	if (GetCurrentProcessVersionComparable() >= VERSION_1_1158_13_E.ToComparable()) {
		address = MemScanner::FindPatternBmh("48 83 C6 ? 48 81 F9");
		if (address) address += 7; // TweaksV: + null check
	}
	else if (GetCurrentProcessVersionComparable() >= VERSION_1_1013_17_E.ToComparable()) {
		address = MemScanner::FindPatternBmh("49 83 c3 ? 48 81 f9 ? ? ? ? 75 ? eb");
		if (address) address += 7; // TweaksV: + null check
	}
	else {
		address = MemScanner::FindPatternBmh("48 81 f9 ? ? ? ? 74 ? 48 83 3c ca 00");
		if (address) address += 3; // TweaksV: + null check
	}
	if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner (B-PACK-05)
		Logging("The packfile array's direct size limit was not found in the game code.");
		FatalErrorExit();
		return;
	}
	// TweaksV: the size must be the imm32 of "cmp rcx, imm32" (48 81 F9) and plausible (plan 5.9 R2)
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
	address = MemScanner::FindPatternBmh("3b 05 ? ? ? ? 0f 8d ? ? ? ? 89 c0");
	if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner (B-PACK-05)
		Logging("The packfile array's indirect size limit was not found in the game code.");
		FatalErrorExit();
		return;
	}
	address += 2;
	uint8_t* indirectAddr = reinterpret_cast<uint8_t*>(address) + *(int32_t*)address + 4;
	if (TweaksV::IsDiagnosticMode())
		Logging("DIAGNOSTIC: indirect size variable at " + TweaksV::DescribeAddress(indirectAddr) + " holds " +
			(TweaksV::IsInsideGameImage(indirectAddr) ? std::to_string(*(uint32_t*)indirectAddr) : std::string("?")));
	if (!TweaksV::IsInsideGameImage(indirectAddr)) { // TweaksV: the variable must be the game's own (plan 5.9 R2)
		Logging("The packfile array's indirect size variable is not inside the game: " + TweaksV::DescribeAddress(indirectAddr) + ".");
		FatalErrorExit();
		return;
	}

	address = MemScanner::FindPatternBmh("89 48 ? 48 8d 0d ? ? ? ? 48 89 08 48 89 c1 e8");
	if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner (B-PACK-05)
		Logging("The call that reads the game's streaming configuration was not found in the game code.");
		FatalErrorExit();
		return;
	}
	address += 0x10;
	// TweaksV: it must be a call (E8) to a function inside the game - otherwise another mod has
	// hooked it already (B-PACK-01, plan 5.9 R2)
	uint8_t* callTarget = (*address == 0xE8) ? address + 5 + *(int32_t*)(address + 1) : nullptr;
	if (TweaksV::IsDiagnosticMode())
		Logging("DIAGNOSTIC: streaming configuration call at " + TweaksV::DescribeAddress(address) + " -> " +
			(callTarget ? TweaksV::DescribeAddress(callTarget) : std::string("not a call")));
	if (!callTarget || !TweaksV::IsInsideGameImage(callTarget)) {
		Logging("The call that reads the game's streaming configuration at " + TweaksV::DescribeAddress(address) +
			" is not a call into the game (another mod may have hooked it already).");
		FatalErrorExit();
		return;
	}

	// TweaksV: clamp to [the game's own size, 65535] - smaller would shrink the game's limits (B-PACK-07)
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

	// TweaksV: pass 2 - the original writes, for the sites found above. Both sizes are made writable
	// first and the hook - the only step that can still fail - is set before they are written, so
	// everything changes or nothing does. The protections restored are the real old ones (upstream
	// set the direct size's page to PAGE_EXECUTE_READWRITE a second time instead, B-PACK-02).
	std::vector<uint8_t*> sizeSites = { directAddr, indirectAddr };
	std::vector<DWORD> oldProtects;
	if (!TweaksV::MakeWritable(sizeSites, sizeof(uint32_t), oldProtects, kTag)) {
		FatalErrorExit();
		return;
	}

	g_origSetStreamingInterface = reinterpret_cast<void(*)(char*)>(callTarget); // TweaksV: set before the call is redirected
	auto h = HookManager::SetCall((char*)address, SetStreamingInterface);
	if (!h) { // TweaksV: SetCall checks the call and the stub now (B-PACK-01) - nothing was written
		TweaksV::RestoreProtection(sizeSites, sizeof(uint32_t), oldProtects);
		Logging("The call that reads the game's streaming configuration could not be redirected.");
		FatalErrorExit();
		return;
	}
	g_origSetStreamingInterface = h->fn;

	*(uint32_t*)directAddr = PackFileArray_Size;
	*reinterpret_cast<uint32_t*>(indirectAddr) = PackFileArray_Size;
	TweaksV::RestoreProtection(sizeSites, sizeof(uint32_t), oldProtects);
}

// TweaksV: the absolute-address relocation function is removed - only the other game edition used it (F-PACK-5)

void RelocateRelativePackFileArray(void* newPackFileArray, const std::vector<PatternPair>& list)
{
	// TweaksV: pass 1 - find and check every address before writing anything (plan 5.9 R1)
	std::vector<uint8_t*> addresses;
	void* oldAddress = nullptr;

	for (auto& entry : list)
	{
		auto address = MemScanner::FindPatternBmh(entry.pattern);
		if (!address) { // TweaksV: a miss used to exit(1) inside MemScanner (B-PACK-05)
			Logging("A reference to the packfile array was not found in the game code: " + entry.pattern);
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
		// moved it (B-PACK-03/06, plan 5.9 R6)
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

	// TweaksV: pass 2 - the original write, for every address found above; every site is made
	// writable first, so the array moves everywhere or nowhere (plan 5.9 R1/R3)
	std::vector<DWORD> oldProtects;
	if (!TweaksV::MakeWritable(addresses, sizeof(int32_t), oldProtects, kTag))
	{
		FatalErrorExit();
		return;
	}
	for (auto address : addresses)
	{
		*(int32_t*)address = static_cast<int32_t>((intptr_t)newPackFileArray - (intptr_t)address - 4);
	}
	TweaksV::RestoreProtection(addresses, sizeof(int32_t), oldProtects);
}

void AdjustPackFileArrayLimit()
{
	std::vector<PatternPair> patternPairs;

	// TweaksV: only the GTA V Enhanced list is kept (F-PACK-6)
	patternPairs = {
		// Data
		{ "48 8d 15 ? ? ? ? 48 8b 3c ca 48 8b 2f", 3 },
		{ "48 8d 3d ? ? ? ? 48 8b 0c cf 48 85 c9", 3 },
		{ "48 8d 3d ? ? ? ? 48 8b 0c 39 48 85 c9", 3 },
		{ "89 c0 48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 74", 5 },
		{ "89 c0 48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 0f 84", 5 },
		{ "48 8d 0d ? ? ? ? 4a 8b 0c c9 48 85 c9 0f 84 ? ? ? ? 48 8b 01", 3 },
		{ "48 8d 0d ? ? ? ? 48 8b 1c 08 48 8b 03", 3 },
		{ "48 8d 0d ? ? ? ? 4a 8b 0c c1 48 85 c9", 3 },
		{ "48 83 ec ? 48 89 ce 48 8d 05 ? ? ? ? 48 89 01 c6 81", 39 },
		{ "48 c1 e8 ? 48 8d 0d ? ? ? ? 48 8b 0c c1 48 8b 01", 7 },
		{ "48 8d 0d ? ? ? ? 48 83 3c c1 ? 40 0f 95 c6 eb", 3 },
		{ "48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 ff 90 ? ? ? ? 48 8b 48", 3 },
		{ "48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 4c 8d 44 24", 3 },
		{ "48 8d 0d ? ? ? ? 4c 8b 24 08 49 8b 04 24 4c 8d 44 24", 3 },
		{ "48 8d 0d ? ? ? ? 48 8b 0c 08 48 85 c9 75", 3 },
		{ "48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 0f 84 ? ? ? ? 48 89 f5", 3 },
		{ "48 83 ec ? 89 d7 48 89 ce 48 8d 05 ? ? ? ? 48 89 01 c6 81 c1", 41 },
		{ "48 8d 0d ? ? ? ? 48 8b 3c 08 48 8b 07 48 89 f9", 3 },
		{ "48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 ff 90 ? ? ? ? 48 8b 40", 3 },
		{ "48 8d 0d ? ? ? ? 48 8b 0c d1 48 85 c9 75", 3 },
		{ "48 8d 0d ? ? ? ? 4c 8b 24 c1 4d 85 e4 0f 84", 3 },
		{ "48 8d 0d ? ? ? ? 48 8b 0c c1 48 8b 01 ff 90 ? ? ? ? 48 8b 38", 3 },
		// Write
		{ "48 89 2d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 48 c7 44 08", 3},
		{ "48 89 35 ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 0f 1f 44 00 00", 3},
		{ "48 89 3d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 66 90", 3},
		{ "48 89 35 ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 66 66 66 2e 0f 1f 84 00 00 00 00 00", 3},
		{ "48 89 2d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 0f 1f 84 00 00 00 00 00", 3},
		{ "48 89 3d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 66 66 66 66 2e 0f 1f 84 00 00 00 00 00", 3},
		{ "48 89 3d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 0f 1f 40 00", 3},
		{ "48 89 35 ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 48 c7 44 08", 3},
	};

	// TweaksV: the 1158.13+ pattern was "BF ? ? ? ? BE ? ? ? ? 4C 8D 05", which also matched obfuscated
	// code further on (RVA 0x582A227 on 1.0.1158.16); with its two constants it matches only this reference (T2)
	patternPairs.emplace_back(GetCurrentProcessVersionComparable() >= VERSION_1_1158_13_E.ToComparable() ?
		PatternPair{ "BF 08 00 00 00 BE 10 00 00 00 4C 8D 05", 13 } : PatternPair{ "41 ? ? ? ? ? ? ? ? ? ? 41 bb ? ? ? ? ? 8d", 20 });

	RelocateRelativePackFileArray(newArray, patternPairs);
}

// TweaksV: FindPrevFreeRegion and AllocateStubMemory moved to StubMemory.cpp (one copy
// for every plugin); AllocateStubMemory gained the protect parameter used above

// TweaksV: kept under its original name, but it no longer shows a MessageBox or calls exit(1)
// inside DllMain (plan 5.8): it logs, reports for the start-up summary and makes initializeGame()
// stop at its next check
void FatalErrorExit() {
	Logging("FATAL ERROR - PackfileLimitAdjusterEnhanced is skipped for this session");
	TweaksV::ReportFailure("PackfileLimitAdjusterEnhanced", "The packfile limit is not changed this session: a required "
		"check failed (details in TweaksV.log).");
	fatalError = true;
}

} // namespace PackfileLimitAdjusterEnhanced
