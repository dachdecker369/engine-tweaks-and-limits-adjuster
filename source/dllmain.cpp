// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - merges HeapAdjusterEnhanced, ModkitLimitAdjusterEnhanced,
// PackfileLimitAdjusterEnhanced, RageOpenV and StraightToStoryMode (by
// Chiheb-Bacha) into one ASI plugin for GTA V Enhanced.
// License: GPL-3.0-only (see LICENSE).
//
// The one real DllMain. The features - the five plugins and TweaksV's own parts, in TweaksVFeatures.cpp -
// are started at step 8 below.

#include <windows.h>

#include "ClearLog.h"		// Shared: ClearLog()
#include "IsEnhanced.h"		// Shared: IsEnhanced()
#include "TweaksVConflicts.h"
#include "TweaksVDump.h"		// test builds: TEST_DUMP_CODE
#include "TweaksVFeatures.h"
#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

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
#ifdef TWEAKSV_TEST_BUILD
			TweaksV::StartCodeDump();								//    test builds: before anything is patched
#endif

			const bool summaryLater = TweaksV::StartFeatures();		// 8. every feature that is on
			if (!summaryLater)										// 9. unless a feature shows it
				TweaksV::ShowStartupSummaryLater();					//    itself, later (RageOpenV)
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
