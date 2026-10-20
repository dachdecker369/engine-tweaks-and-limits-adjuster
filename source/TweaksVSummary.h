// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - failure collection and the one start-up summary message box.
#pragma once

#include <string>

namespace TweaksV
{
	// Records a problem for the start-up summary and logs it. Thread-safe.
	void ReportFailure(const char* plugin, const std::string& what);

	// One message box listing everything reported so far - only if ERROR_POPUP=1
	// and there is something to report. Shown at most once. Never call it from DllMain.
	void ShowStartupSummary();

	// The same, from a worker thread - safe to start from DllMain.
	void ShowStartupSummaryLater();
}
