// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the measurements (see TweaksVMeasure.h).

#include "TweaksVMeasure.h"

#ifdef TWEAKSV_TEST_BUILD
#include <cwchar>
#include <string>

#include "TweaksVLog.h"
#endif

namespace TweaksV
{
	namespace
	{
		// How long a measurement's thread waits between its summaries after the first: a minute - in test builds
		// TWEAKSV_TEST_SUMMARY_MS=<ms> in the environment (100..60000) instead, so that a suite sees a summary come
		// again within the minute a fake game runs, said once per measurement.
		DWORD SummaryEveryMs(const Measurement& measurement)
		{
#ifdef TWEAKSV_TEST_BUILD
			wchar_t text[8]{};
			const DWORD length = GetEnvironmentVariableW(L"TWEAKSV_TEST_SUMMARY_MS", text, 8);
			if (length > 0 && length < 8)
			{
				wchar_t* end = nullptr;
				const long ms = wcstol(text, &end, 10);
				if (end && *end == L'\0' && ms >= 100 && ms <= 60000)
				{
					WriteLog("TweaksV", std::string("TEST BUILD: the ") + measurement.thread + " comes every " + std::to_string(ms) +
						" ms after its first (TWEAKSV_TEST_SUMMARY_MS=" + std::to_string(ms) + ").");
					return static_cast<DWORD>(ms);
				}
			}
#else
			(void)measurement;
#endif
			return kSummaryEveryMs;
		}
	}

	DWORD RunMeasurement(const Measurement& measurement)
	{
		if (measurement.start && *measurement.start)
			WaitForSingleObject(*measurement.start, INFINITE);	// the part's first event starts the clock
		DWORD every = kSummaryEveryMs;
		try	// nothing may leave a thread of this module
		{
			every = SummaryEveryMs(measurement);
		}
		catch (...)
		{
		}
		uint64_t last = UINT64_MAX;	// no summary yet: the first is always written
		for (DWORD wait = measurement.firstMs;; wait = every)
		{
			Sleep(wait);
			try	// nothing may leave a thread of this module
			{
				const uint64_t now = measurement.numbers();
				if (now == last)
					continue;
				last = now;
				measurement.summary(now);
			}
			catch (...)
			{
			}
		}
	}
}
