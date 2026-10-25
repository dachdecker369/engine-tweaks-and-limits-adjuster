// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the measurements: one loop for every part that sums up what it counts - DIAGNOSTIC_MODE's,
// and DirectStorage's UNLOCK_FILES summary.
#pragma once

#include <windows.h>

#include <cstdint>

#include "TweaksVSummary.h"

namespace TweaksV
{
	constexpr DWORD kFirstSummaryMs = 10000;		// a measurement's first summary, 10 seconds in
	constexpr DWORD kSummaryEveryMs = 60000;		// then one a minute while its numbers change
	constexpr uint64_t kMaxLinesPerKind = 20000;	// lines of a kind that comes per event; then the events are only counted

	// A part's measurement: numbers() changes whenever the summary has something new to say, and
	// summary(numbers) writes it, given what numbers() returned. Both run on the measurement's thread.
	// start, if given, is an event the part sets at the first thing it counts, from its hook, which so
	// starts no thread itself: the thread waits for it before firstMs begin. An event that could not be
	// made (null) starts the clock at once.
	struct Measurement
	{
		const char* thread;					// the thread's name, for the line when it cannot start
		DWORD firstMs;						// when the first summary comes: kFirstSummaryMs, or kSummaryEveryMs
		uint64_t (*numbers)();
		void (*summary)(uint64_t numbers);
		const HANDLE* start = nullptr;		// the part's event; nullptr: firstMs from the thread's start
	};

	// The loop of a measurement's thread: firstMs after its start (the thread's, or the part's event),
	// then every kSummaryEveryMs, the summary is written when the numbers have changed since the last one -
	// the first is always written. Inside a try, so nothing leaves the thread; it never returns.
	DWORD RunMeasurement(const Measurement& measurement);

	template <const Measurement& kMeasurement> DWORD WINAPI MeasurementThread(LPVOID)
	{
		return RunMeasurement(kMeasurement);
	}

	// Starts the thread of a measurement through StartModuleThread; the thread never ends, so it keeps
	// its reference to TweaksV.asi. One that cannot start is logged, not reported - a measurement is a
	// diagnostic aid (SPECIFICATIONS 12) - and false comes back.
	template <const Measurement& kMeasurement> bool StartMeasurement()
	{
		return StartModuleThread(MeasurementThread<kMeasurement>, kMeasurement.thread);
	}
}
