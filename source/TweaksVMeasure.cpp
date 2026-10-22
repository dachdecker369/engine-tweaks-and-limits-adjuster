// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the measurements of DIAGNOSTIC_MODE (see TweaksVMeasure.h).

#include "TweaksVMeasure.h"

namespace TweaksV
{
	DWORD RunMeasurement(const Measurement& measurement)
	{
		uint64_t last = UINT64_MAX;	// no summary yet: the first is always written
		for (DWORD wait = measurement.firstMs;; wait = kSummaryEveryMs)
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
