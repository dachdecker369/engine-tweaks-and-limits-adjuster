// TweaksV: carried over from StraightToStoryMode (Game.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/StraightToStoryMode (GPL-3.0). The splash and legal
// screen patches were adapted from ChaosModV (GPL-3.0); the landing page patch is by
// "Unknown Modder" (alloc8or).
// Modified for TweaksV: GTA V Enhanced only, namespace, one TweaksV.ini switch
// per screen, safety fixes (B-STORY-01..04). Every change is marked "TweaksV:".
#include <windows.h>				// TweaksV: was #include "pch.h"
#include "MemScanner.h"				// TweaksV: the shared scanner
#include <cstdint>
#include <string_view>
#include <cassert>
#include "StraightToStoryMode_Game.h"
#include <fstream>
#include "StraightToStoryMode_Logging.h"
#include <cstdio>					// TweaksV: snprintf
#include <cstring>					// TweaksV: memcpy
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVIni.h"		// TweaksV: the switches, DRY_RUN, DIAGNOSTIC_MODE
#include "TweaksVLog.h"		// TweaksV: DescribeError
#include "TweaksVSummary.h"	// TweaksV: start-up summary

namespace StraightToStoryMode // TweaksV: one namespace per original plugin
{

BYTE* splash;
BYTE* legal;
BYTE* landingPage;

// TweaksV: the TODO "add null checks for patterns and tolerate missing patterns by skipping
// the patch" is done: a missing pattern skips only that screen (B-STORY-01)

// TweaksV: "12 34 AB" - for log messages
static std::string HexBytes(const BYTE* bytes, size_t size)
{
	std::string text;
	for (size_t i = 0; i < size; i++)
	{
		char byteText[4];
		snprintf(byteText, sizeof(byteText), i ? " %02X" : "%02X", bytes[i]);
		text += byteText;
	}
	return text;
}

// TweaksV: one patch write - protection checked and the real old protection restored,
// instruction cache flushed (plan 5.9 R3); DRY_RUN writes nothing; DIAGNOSTIC_MODE logs
// the old and new bytes. Returns false if nothing could be written.
static bool WritePatch(BYTE* address, const BYTE* bytes, size_t size, const char* what)
{
	if (TweaksV::IsDryRun())
	{
		Logging(std::string("DRY_RUN: would patch the ") + what + ": " + HexBytes(address, size) + " -> " + HexBytes(bytes, size));
		return true;
	}

	DWORD oldProtect;
	if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		const DWORD error = GetLastError();
		TweaksV::ReportFailure("StraightToStoryMode", std::string("The ") + what + " could not be patched: the game code "
			"could not be made writable (" + TweaksV::DescribeError(error) + ").");
		return false;
	}

	const std::string before = HexBytes(address, size);
	memcpy(address, bytes, size);
	VirtualProtect(address, size, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), address, size);

	if (TweaksV::IsDiagnosticMode())
	{
		char rva[32];
		snprintf(rva, sizeof(rva), "RVA 0x%llX", (unsigned long long)(address - (BYTE*)GetModuleHandleW(nullptr)));
		Logging(std::string("DIAGNOSTIC: ") + what + " at " + rva + ": " + before + " -> " + HexBytes(address, size));
	}
	return true;
}

void initializeGame()
{
	// TweaksV: ClearLog() removed - TweaksV.log is cleared once per start (plan 5.7)

	// TweaksV: the game edition check and the other edition's patches are removed -
	// TweaksV only runs in GTA5_Enhanced.exe (F-STORY-1..4)

	// TweaksV: one TweaksV.ini switch per screen (B-STORY-04)
	const bool skipSplashScreen = TweaksV::GetIniBool("STRAIGHTTOSTORYMODE_SETTINGS", "SKIP_SPLASH_SCREEN", true);
	const bool skipLegalScreen = TweaksV::GetIniBool("STRAIGHTTOSTORYMODE_SETTINGS", "SKIP_LEGAL_SCREEN", true);
	const bool skipLandingPage = TweaksV::GetIniBool("STRAIGHTTOSTORYMODE_SETTINGS", "SKIP_LANDING_PAGE", true);
	bool allApplied = true; // TweaksV: "Patch successful!" only if every requested patch was applied

	// Splash and Legal screen patches were adapted from ChaosModV
	// https://github.com/gta-chaos-mod/ChaosModV/blob/9301f701dcd29e558a7955f260a3fcb327e66a21/ChaosMod/Memory/Memory.cpp
	if (skipSplashScreen) // TweaksV
	{
		splash = MemScanner::FindPatternBmh("0F 85 A9 00 00 00 48 8D 54");
		if (!splash) // TweaksV: null check - skip only this screen (B-STORY-01)
		{
			TweaksV::ReportFailure("StraightToStoryMode", "The splash screen cannot be skipped: its game code was not "
				"found (a game update may have moved it).");
			allApplied = false;
		}
		else
		{
			splash -= 0x0D;

			if (splash[0] == 0xE9 && splash[5] == 0x90) // TweaksV: already patched (an old StraightToStoryMode.asi?) - skip quietly (B-STORY-03)
			{
				Logging("Splash screen: already patched - skipped.");
			}
			else if (splash[0] != 0x0F || splash[1] != 0x84) // TweaksV: must be "jz rel32" (B-STORY-02)
			{
				TweaksV::ReportFailure("StraightToStoryMode", "The splash screen cannot be skipped: the game code found is "
					"not the expected instruction (" + HexBytes(splash, 6) + ").");
				allApplied = false;
			}
			else
			{
				// TweaksV: "jz rel32" becomes "jmp rel32+1" + nop - the same target, now taken
				// always. Upstream hard-coded E9 86 01 00 00 90 (B-STORY-02).
				const int32_t jumpDisplacement = *(int32_t*)(splash + 2) + 1;
				BYTE patch[6] = { 0xE9, 0x00, 0x00, 0x00, 0x00, 0x90 };
				memcpy(patch + 1, &jumpDisplacement, 4);
				if (!WritePatch(splash, patch, 6, "splash screen"))
					allApplied = false;
			}
		}
	}
	else
	{
		Logging("Splash screen: not skipped (SKIP_SPLASH_SCREEN=0)."); // TweaksV
	}

	if (skipLegalScreen) // TweaksV
	{
		legal = MemScanner::FindPatternBmh("00 E9 ? 05 00 00 E8 ? 07");
		if (!legal) // TweaksV: null check - skip only this screen (B-STORY-01)
		{
			TweaksV::ReportFailure("StraightToStoryMode", "The legal screen cannot be skipped: its game code was not "
				"found (a game update may have moved it).");
			allApplied = false;
		}
		else
		{
			legal -= 0x0F;

			static const BYTE nops[6] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
			if (memcmp(legal, nops, 6) == 0) // TweaksV: already patched - skip quietly (B-STORY-03)
			{
				Logging("Legal screen: already patched - skipped.");
			}
			else if (legal[0] != 0x0F || (legal[1] & 0xF0) != 0x80) // TweaksV: expect a 6-byte conditional jump "jcc rel32" (plan A, S-E2)
			{
				TweaksV::ReportFailure("StraightToStoryMode", "The legal screen cannot be skipped: the game code found is "
					"not the expected instruction (" + HexBytes(legal, 6) + ").");
				allApplied = false;
			}
			else if (!WritePatch(legal, nops, 6, "legal screen")) // TweaksV: was VirtualProtect + memcpy + VirtualProtect
			{
				allApplied = false;
			}
		}
	}
	else
	{
		Logging("Legal screen: not skipped (SKIP_LEGAL_SCREEN=0)."); // TweaksV
	}


	// Thanks to "Unknown Modder" (alloc8or) for the landing page patch on Enhanced
	if (skipLandingPage) // TweaksV
	{
		landingPage = MemScanner::FindPatternBmh("E8 ? ? ? ? 8B 0D ? ? ? ? 83 C1 ? 84 C0");
		if (!landingPage) // TweaksV: null check - skip only this screen (B-STORY-01)
		{
			TweaksV::ReportFailure("StraightToStoryMode", "The landing page cannot be skipped: its game code was not "
				"found (a game update may have moved it).");
			allApplied = false;
		}
		else
		{
			landingPage += 1;
			BYTE* patchedFuncAddr = (BYTE*)(*(int32_t*)landingPage + landingPage + 4);

			static const BYTE returnZero[3] = { 0x31, 0xC0, 0xC3 }; // xor eax, eax; ret
			if (!TweaksV::IsInsideGameImage(patchedFuncAddr)) // TweaksV: the call must lead into the game (plan 5.9 R2)
			{
				TweaksV::ReportFailure("StraightToStoryMode", "The landing page cannot be skipped: the call found does not "
					"lead into the game code.");
				allApplied = false;
			}
			else if (memcmp(patchedFuncAddr, returnZero, 3) == 0) // TweaksV: already patched - skip quietly (B-STORY-03)
			{
				Logging("Landing page: already patched - skipped.");
			}
			else if (patchedFuncAddr[0] == 0xE9 || (patchedFuncAddr[0] == 0xFF && patchedFuncAddr[1] == 0x25)) // TweaksV: plan 4.4
			{
				TweaksV::ReportFailure("StraightToStoryMode", "The landing page is not skipped: another mod has already "
					"hooked the function it patches.");
				allApplied = false;
			}
			else if (!WritePatch(patchedFuncAddr, returnZero, 3, "landing page")) // TweaksV: was VirtualProtect + memcpy + VirtualProtect
			{
				allApplied = false;
			}
		}
	}
	else
	{
		Logging("Landing page: not skipped (SKIP_LANDING_PAGE=0)."); // TweaksV
	}

	if (allApplied && !TweaksV::IsDryRun()) // TweaksV
	{
		std::string msg = "Patch successful!";
		Logging(msg);
	}
}

// TweaksV: kept under its original name, but it no longer shows a MessageBox or calls
// exit(1) inside DllMain (plan 5.8). StraightToStoryMode now skips single screens instead.
void FatalErrorExit() {
	Logging("FATAL ERROR - StraightToStoryMode is skipped for this session");
	TweaksV::ReportFailure("StraightToStoryMode", "A fatal error occurred - StraightToStoryMode is skipped for this session.");
}

} // namespace StraightToStoryMode
