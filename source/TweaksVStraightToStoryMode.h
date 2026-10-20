// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - straight to story mode: the splash screen, the legal screen and the landing page between
// the start of the game and story mode, each skipped by one patch when its switch in
// [STRAIGHTTOSTORYMODE] is on.
#pragma once

namespace TweaksV
{
	// Patches the screens that [STRAIGHTTOSTORYMODE] asks to skip. Called once by DllMain.
	void InitStraightToStoryMode();
}
