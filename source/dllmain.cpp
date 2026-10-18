// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - merges HeapAdjusterEnhanced, ModkitLimitAdjusterEnhanced,
// PackfileLimitAdjusterEnhanced, RageOpenV and StraightToStoryMode (by
// Chiheb-Bacha) into one ASI plugin for GTA V Enhanced.
// License: GPL-3.0-only (see LICENSE).
//
// The one real DllMain. The five plugins' start functions are called at step 8 below,
// TweaksV's own DirectStorage part right after them.

#include <windows.h>

#include "ClearLog.h"		// Shared: ClearLog()
#include "IsEnhanced.h"		// Shared: IsEnhanced()
#include "TweaksVConflicts.h"
#include "TweaksVDirectStorage.h"
#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"
#include "HeapAdjusterEnhanced_Game.h"
#include "ModkitLimitAdjusterEnhanced_Game.h"
#include "PackfileLimitAdjusterEnhanced_Game.h"
#include "main.h"
#include "StraightToStoryMode_Game.h"

// Starts one part of TweaksV unless its ENABLED=0 or an old copy of it was found. No exception may
// leave DllMain: one from a part ends only that part.
template <typename F> static void RunPart(const TweaksV::SettingsSection& part, F&& start)
{
	if (!TweaksV::IsFeatureOn(part))
		return;
	try { start(); }
	catch (...) { TweaksV::ReportFailure(part.feature, "unexpected exception - this part is skipped for this session"); }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	(void)hModule;
	(void)lpReserved;

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		try	// no exception may leave DllMain
		{
			if (!IsEnhanced())										// 1. GTA V Enhanced only
				break;
			TweaksV::CreateOrUpdateIni();							// 2. TweaksV.ini exists from here on
			TweaksV::LoadSettings();								// 3. every setting, read once
			if (!TweaksV::IsOn(TweaksV::kEnabled))					// 4. master switch
				break;
			// TweaksV.asi now stays loaded until the game exits: RageOpenV's and Packfile's hooks
			// point into it, so an early FreeLibrary would crash the game at the next hooked call.
			HMODULE self = nullptr;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				reinterpret_cast<LPCWSTR>(&DllMain), &self);
			ClearLog();												// 5. once, for TweaksV.log
			TweaksV::WriteLogHeader();
			if (TweaksV::IsOn(TweaksV::kConsole))					// 6.
				TweaksV::OpenConsole();
			if (TweaksV::IsOn(TweaksV::kCheckForConflicts))			// 7.
				TweaksV::CheckForConflicts();

			// 8. The original plugins, in the order an ASI loader loaded their .asi files
			//    (alphabetical), then TweaksV's own DirectStorage part.
			bool phase1 = false;	// RageOpenV's GetSystemTimeAsFileTime hook is installed
			RunPart(TweaksV::kHeapSection, [] { HeapAdjusterEnhanced::initializeGame(); });
			RunPart(TweaksV::kModkitSection, [] { ModkitLimitAdjusterEnhanced::initializeGame(); });
			RunPart(TweaksV::kPackfileSection, [] { PackfileLimitAdjusterEnhanced::initializeGame(); });
			RunPart(TweaksV::kRageOpenVSection, [&] { phase1 = RageOpenV::DllMain(hModule, ul_reason_for_call, lpReserved) != FALSE; });
			RunPart(TweaksV::kStoryModeSection, [] { StraightToStoryMode::initializeGame(); });
			RunPart(TweaksV::kDirectStorageSection, [] { TweaksV::InitDirectStorage(); });

			if (!phase1)											// 9. otherwise RageOpenV's hook
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
	return TRUE;													// 10. always
}
