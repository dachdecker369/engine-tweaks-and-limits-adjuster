// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - its features, defined once in kFeatures (TweaksVFeatures.cpp): the section of TweaksV.ini
// that switches each one on, the old stand-alone plugins that do its job and the function that starts
// it. DllMain starts them in that order and the conflict check looks for their old plugins, so a new
// feature is one entry there, plus its section and settings in TweaksVSettings.h.
#pragma once

#include <span>

#include "TweaksVSettings.h"

namespace TweaksV
{
	struct Feature
	{
		const SettingsSection* section;	// its ENABLED switch and its name
		const wchar_t* oldPlugins[3];	// the stand-alone .asi files that do its job (unused places nullptr)
		bool (*start)();				// true if the feature shows the start-up summary itself, later
	};

	std::span<const Feature> Features();

	// Starts every feature that is on, in table order. No exception gets out of it: one from a feature
	// ends only that feature. True if one of them shows the start-up summary later, so DllMain must not.
	bool StartFeatures();
}
