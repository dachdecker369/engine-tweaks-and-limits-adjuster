// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - failure collection and the one start-up summary message box (plan 5.8).
#pragma once

#include <string>

namespace TweaksV
{
	// Records a problem for the start-up summary and logs it. Thread-safe.
	void ReportFailure(const char* plugin, const std::string& what);

	// One message box listing everything reported so far - only if SHOW_ERROR_POPUPS=1
	// and there is something to report. Shown at most once. Never call it from DllMain.
	void ShowStartupSummary();

	// The same, from a worker thread - safe to start from DllMain (plan 5.5 step 8).
	void ShowStartupSummaryLater();
}
