// TweaksV: carried over from StraightToStoryMode (Game.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/StraightToStoryMode (GPL-3.0). The splash and legal
// screen patches were adapted from ChaosModV (GPL-3.0); the landing page patch is by
// "Unknown Modder" (alloc8or).
// Modified for TweaksV: GTA V Enhanced only, namespace, one TweaksV.ini switch
// per screen, safety fixes. Every change is marked "TweaksV:".
#include <windows.h>				// TweaksV: was #include "pch.h"
#include "MemScanner.h"				// TweaksV: the shared scanner
#include <cstdint>
// TweaksV: <string_view>, <cassert> and <fstream> removed - unused
#include "StraightToStoryMode_Game.h"
#include "StraightToStoryMode_Logging.h"
#include <cstring>					// TweaksV: memcpy
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVSettings.h"	// TweaksV: the switches, DRY_RUN
#include "TweaksVPatch.h"		// TweaksV: WriteCode, HexBytes
#include "TweaksVSummary.h"	// TweaksV: start-up summary

namespace StraightToStoryMode // TweaksV: one namespace per original plugin
{

BYTE* splash;
BYTE* legal;
BYTE* landingPage;

// TweaksV: the TODO "add null checks for patterns and tolerate missing patterns by skipping
// the patch" is done: a missing pattern skips only that screen

// TweaksV: one patch write, through TweaksV's one code writer (TweaksVPatch.h), which checks the
// protection and puts it back, flushes the instruction cache and handles DRY_RUN and
// DIAGNOSTIC_MODE; a failure goes to the start-up summary. Returns false if nothing
// could be written.
static bool WritePatch(BYTE* address, const BYTE* bytes, size_t size, const char* what)
{
	if (TweaksV::WriteCode({ { address, std::vector<uint8_t>(bytes, bytes + size), what } }, "StraightToStoryMode"))
		return true;
	TweaksV::ReportFailure("StraightToStoryMode", std::string("The ") + what + " could not be patched: the game code "
		"could not be made writable (details in TweaksV.log).");
	return false;
}

void initializeGame()
{
	// TweaksV: ClearLog() removed - TweaksV.log is cleared once per start

	// TweaksV: the game edition check and the other edition's patches are removed -
	// TweaksV only runs in GTA5_Enhanced.exe

	// TweaksV: one TweaksV.ini switch per screen
	const bool skipSplashScreen = TweaksV::IsOn(TweaksV::kSkipSplashScreen);
	const bool skipLegalScreen = TweaksV::IsOn(TweaksV::kSkipLegalScreen);
	const bool skipLandingPage = TweaksV::IsOn(TweaksV::kSkipLandingPage);
	bool allApplied = true; // TweaksV: "Patch successful!" only if every requested patch was applied

	// Splash and Legal screen patches were adapted from ChaosModV
	// https://github.com/gta-chaos-mod/ChaosModV/blob/9301f701dcd29e558a7955f260a3fcb327e66a21/ChaosMod/Memory/Memory.cpp
	if (skipSplashScreen) // TweaksV
	{
		splash = MemScanner::Find(TweaksV::Pattern("S-E1"));
		if (!splash) // TweaksV: null check - skip only this screen
		{
			TweaksV::ReportFailure("StraightToStoryMode", "The splash screen cannot be skipped: its game code was not "
				"found (a game update may have moved it).");
			allApplied = false;
		}
		else
		{
			splash -= 0x0D;

			if (splash[0] == 0xE9 && splash[5] == 0x90) // TweaksV: already patched (an old StraightToStoryMode.asi?) - skip quietly
			{
				Logging("Splash screen: already patched - skipped.");
			}
			else if (splash[0] != 0x0F || splash[1] != 0x84) // TweaksV: must be "jz rel32"
			{
				TweaksV::ReportFailure("StraightToStoryMode", "The splash screen cannot be skipped: the game code found is "
					"not the expected instruction (" + TweaksV::HexBytes(splash, 6) + ").");
				allApplied = false;
			}
			else
			{
				// TweaksV: "jz rel32" becomes "jmp rel32+1" + nop - the same target, now taken
				// always. Upstream hard-coded E9 86 01 00 00 90.
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
		legal = MemScanner::Find(TweaksV::Pattern("S-E2"));
		if (!legal) // TweaksV: null check - skip only this screen
		{
			TweaksV::ReportFailure("StraightToStoryMode", "The legal screen cannot be skipped: its game code was not "
				"found (a game update may have moved it).");
			allApplied = false;
		}
		else
		{
			legal -= 0x0F;

			static const BYTE nops[6] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
			if (memcmp(legal, nops, 6) == 0) // TweaksV: already patched - skip quietly
			{
				Logging("Legal screen: already patched - skipped.");
			}
			else if (legal[0] != 0x0F || legal[1] != 0x84) // TweaksV: must be "jz rel32", as in the game (0F 84 07 05 00 00 on 1.0.1158.16); the jump distance is not checked, as game updates change it
			{
				TweaksV::ReportFailure("StraightToStoryMode", "The legal screen cannot be skipped: the game code found is "
					"not the expected instruction (" + TweaksV::HexBytes(legal, 6) + ").");
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
		landingPage = MemScanner::Find(TweaksV::Pattern("S-E3"));
		if (!landingPage) // TweaksV: null check - skip only this screen
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
			if (!TweaksV::IsInsideGameImage(patchedFuncAddr)) // TweaksV: the call must lead into the game
			{
				TweaksV::ReportFailure("StraightToStoryMode", "The landing page cannot be skipped: the call found does not "
					"lead into the game code.");
				allApplied = false;
			}
			else if (memcmp(patchedFuncAddr, returnZero, 3) == 0) // TweaksV: already patched - skip quietly
			{
				Logging("Landing page: already patched - skipped.");
			}
			else if (patchedFuncAddr[0] == 0xE9 || (patchedFuncAddr[0] == 0xFF && patchedFuncAddr[1] == 0x25)) // TweaksV: a jump at its start is another mod's hook
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

// TweaksV: FatalErrorExit() removed - it showed a MessageBox and called exit(1) inside DllMain;
// StraightToStoryMode now skips single screens instead, so nothing calls it

} // namespace StraightToStoryMode
