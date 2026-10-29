// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - its features, defined once in kFeatures (TweaksVFeatures.cpp): the section of TweaksV.ini
// that switches each one on, the stand-alone plugins that do its job - the old ones TweaksV replaces and
// the ones that stay installed - and the function that starts it. DllMain starts them in that order and
// the conflict check looks for their plugins, so a new feature is one entry there, plus its section and
// settings in TweaksVSettings.h. Beside them kMeasuredFirst lists the limits of the parts to come that are
// measured first, with DIAGNOSTIC_MODE=1 only: the part's log tag and its module's Measure<Limit>().
#pragma once

#include <span>

#include "TweaksVSettings.h"

namespace TweaksV
{
	// A stand-alone plugin that does a feature's job too and stays installed, because another mod needs
	// it: the conflict check switches the feature off while it is there and says why (TweaksVConflicts.h).
	struct PeerPlugin
	{
		const wchar_t* file;		// "onigiri.asi"
		const char* description;	// what it is, for the start-up summary: "NaturalVision Enhanced's loader"
	};

	struct Feature
	{
		const SettingsSection* section;		// its ENABLED switch and its name
		const wchar_t* oldPlugins[3];		// the stand-alone .asi files TweaksV replaces (unused places nullptr)
		const PeerPlugin* peerPlugins[1];	// the ones that do its job too and stay installed (unused places nullptr)
		bool (*start)();					// true if the feature shows the start-up summary itself, later
	};

	std::span<const Feature> Features();

	// Starts every feature that is on, in table order, then - with DIAGNOSTIC_MODE=1 - every part measured
	// first. No exception gets out of it: one from a feature ends only that feature, one from a measurement
	// only that measurement. True if one of the features shows the start-up summary later, so DllMain must not.
	bool StartFeatures();
}
