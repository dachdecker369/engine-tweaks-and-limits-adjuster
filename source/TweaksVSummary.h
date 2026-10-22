// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - failure collection and the one start-up summary message box.
#pragma once

#include <windows.h>

#include <string>

namespace TweaksV
{
	// Starts thread with an extra reference to TweaksV.asi, handed to the thread, which drops it when it
	// ends (FreeLibraryAndExitThread), so TweaksV.asi cannot be unloaded while the thread still runs its
	// code. False if it cannot start - logged, what naming the thread. Safe to call from DllMain.
	bool StartModuleThread(LPTHREAD_START_ROUTINE thread, const char* what);

	// Records a problem for the start-up summary and logs it. Thread-safe.
	void ReportFailure(const char* plugin, const std::string& what);

	// One message box listing everything reported so far - only if ERROR_POPUP=1
	// and there is something to report. Shown at most once. Never call it from DllMain.
	void ShowStartupSummary();

	// The same, from a worker thread - safe to start from DllMain.
	void ShowStartupSummaryLater();
}
