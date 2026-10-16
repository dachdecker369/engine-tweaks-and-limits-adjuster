// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - merges HeapAdjusterEnhanced, ModkitLimitAdjusterEnhanced,
// PackfileLimitAdjusterEnhanced, RageOpenV and StraightToStoryMode (by
// Chiheb-Bacha) into one ASI plugin for GTA V Enhanced.
// License: GPL-3.0-only (see LICENSE).
//
// The one real DllMain (plan 5.5). The five plugins' start functions are called at
// step 7 below, TweaksV's own DirectStorage part right after them.

#include <windows.h>

#include "ClearLog.h"		// Shared: ClearLog()
#include "IsEnhanced.h"		// Shared: IsEnhanced()
#include "TweaksVConflicts.h"
#include "TweaksVDirectStorage.h"
#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVSummary.h"
#include "HeapAdjusterEnhanced_Game.h"
#include "ModkitLimitAdjusterEnhanced_Game.h"
#include "PackfileLimitAdjusterEnhanced_Game.h"
#include "main.h"
#include "StraightToStoryMode_Game.h"

// Runs one original entry function; no exception may leave DllMain (plan 5.8).
template <typename F> static void RunPlugin(const char* tag, F&& f)
{
	try { f(); }
	catch (...) { TweaksV::ReportFailure(tag, "unexpected exception - this part is skipped for this session"); }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	(void)hModule;
	(void)lpReserved;

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		try	// no exception may leave DllMain (plan 5.8)
		{
			if (!IsEnhanced())										// 1. GTA V Enhanced only
				break;
			TweaksV::CreateOrUpdateIni();							// 2. TweaksV.ini exists from here on
			if (!TweaksV::GetIniBool("TWEAKSV", "ENABLED", true))	// 3. master switch
				break;
			// TweaksV.asi now stays loaded until the game exits: RageOpenV's and Packfile's hooks
			// point into it, so an early FreeLibrary would crash the game at the next hooked call.
			HMODULE self = nullptr;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				reinterpret_cast<LPCWSTR>(&DllMain), &self);
			ClearLog();												// 4. once, for TweaksV.log
			TweaksV::WriteLogHeader();
			if (TweaksV::GetIniBool("TWEAKSV", "CONSOLE", false))	// 5.
				TweaksV::OpenConsole();
			if (TweaksV::GetIniBool("TWEAKSV", "CHECK_FOR_CONFLICTS", true))	// 6.
				TweaksV::CheckForConflicts();

			// 7. The original plugins, in the order an ASI loader loaded their .asi files
			//    (alphabetical). Each is skipped if its ENABLED=0 or an old copy of it
			//    was found.
			bool phase1 = false;	// RageOpenV's GetSystemTimeAsFileTime hook is installed
			if (TweaksV::IsFeatureOn("HEAP_SETTINGS"))
				RunPlugin("HeapAdjusterEnhanced", [] { HeapAdjusterEnhanced::initializeGame(); });
			if (TweaksV::IsFeatureOn("MODKIT_SETTINGS"))
				RunPlugin("ModkitLimitAdjusterEnhanced", [] { ModkitLimitAdjusterEnhanced::initializeGame(); });
			if (TweaksV::IsFeatureOn("PACKFILE_SETTINGS"))
				RunPlugin("PackfileLimitAdjusterEnhanced", [] { PackfileLimitAdjusterEnhanced::initializeGame(); });
			if (TweaksV::IsFeatureOn("RAGEOPENV_SETTINGS"))			// installs HookGetSystemTimeAsFileTime (Phase 1)
				RunPlugin("RageOpenV", [&] { phase1 = RageOpenV::DllMain(hModule, ul_reason_for_call, lpReserved) != FALSE; });
			if (TweaksV::IsFeatureOn("STRAIGHTTOSTORYMODE_SETTINGS"))
				RunPlugin("StraightToStoryMode", [] { StraightToStoryMode::initializeGame(); });
			if (TweaksV::IsFeatureOn("DIRECTSTORAGE_SETTINGS"))		// TweaksV's own part; for now it only logs
				RunPlugin("DirectStorage", [] { TweaksV::InitDirectStorage(); });

			if (!phase1)											// 8. otherwise RageOpenV's hook
				TweaksV::ShowStartupSummaryLater();					//    shows it, in Phase 1
		}
		catch (...)
		{
		}
		break;

	case DLL_PROCESS_DETACH:
		TweaksV::CloseLog();
		break;
	}
	return TRUE;													// 9. always
}
