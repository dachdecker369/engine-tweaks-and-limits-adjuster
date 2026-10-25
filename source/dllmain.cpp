// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - Engine Tweaks and Limits Adjuster, one ASI plugin for GTA V Enhanced: its parts raise the
// game's limits, serve a mods folder and unencrypted archives, skip the start-up screens and free the
// files DirectStorage keeps locked. Written anew from plugins by Chiheb-Bacha and others (CREDITS.md).
// License: GPL-3.0-only (see LICENSE).
//
// The one real DllMain. The parts - the feature table in TweaksVFeatures.cpp - are started at step 8
// below.

#include <windows.h>

#include "TweaksVConflicts.h"
#include "TweaksVDump.h"		// test builds: TEST_DUMP_CODE
#include "TweaksVFeatures.h"
#include "TweaksVGame.h"
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
			if (!TweaksV::IsGtaVEnhanced())										// 1. GTA V Enhanced only
				break;
			TweaksV::CreateOrUpdateIni();							// 2. TweaksV.ini exists from here on
			TweaksV::LoadSettings();								// 3. every setting, read once
			if (!TweaksV::IsOn(TweaksV::kEnabled))					// 4. master switch
				break;
			// TweaksV.asi now stays loaded until the game exits: RageOpenV's and Packfile's hooks
			// point into it, so an early FreeLibrary would crash the game at the next hooked call.
			HMODULE self = nullptr;
			if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				reinterpret_cast<LPCWSTR>(&DllMain), &self))
				TweaksV::ReportFailure("TweaksV", "TweaksV.asi could not be pinned in memory (" + TweaksV::DescribeError(GetLastError()) +
					"): an ASI loader that unloads it later would crash the game.");
			// The conflict check runs before the log is started, so that the header's Features: line says
			// off for every part it switches off; its lines are kept and written after the header.
			if (TweaksV::IsOn(TweaksV::kConflictCheck))			// 5.
				TweaksV::CheckForConflicts();
			TweaksV::ClearLogFile();												// 6. once, for TweaksV.log
			TweaksV::WriteLogHeader();
			if (TweaksV::IsOn(TweaksV::kConsole))					// 7.
				TweaksV::OpenConsole();
#ifdef TWEAKSV_TEST_BUILD
			TweaksV::StartCodeDump();								//    test builds: before anything is patched
#endif

			const bool summaryLater = TweaksV::StartFeatures();		// 8. every feature that is on
			if (!summaryLater)										// 9. unless a feature shows it
				TweaksV::ShowStartupSummaryLater();					//    itself, later (RageOpenV)
		}
		catch (...)	// never silent: the start-up summary names it, also when the log could not be started
		{
			try
			{
				TweaksV::ReportFailure("TweaksV", "Unexpected exception during the start-up - the rest of TweaksV is skipped this session.");
				TweaksV::ShowStartupSummaryLater();
			}
			catch (...)
			{
			}
		}
		break;

	case DLL_PROCESS_DETACH:
		TweaksV::CloseLog();
		break;
	}
	return TRUE;													// 10. always
}
