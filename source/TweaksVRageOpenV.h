// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - RageOpenV: the mods folder, unencrypted (OPEN) RPF archives and the RPF cache check, three
// jobs with a switch each in [RAGEOPENV], plus the console patch with CONSOLE=1. The game's file system
// is not up in DllMain, so the part does its work when the game first asks for the time, and shows the
// start-up summary then.
#pragma once

namespace TweaksV
{
	// Hooks GetSystemTimeAsFileTime so that the part's start-up runs at the game's first time request.
	// True if the start-up shows the start-up summary - or a thread does, 30 seconds in, if the game never
	// asks - so DllMain must not; false if the hook could not be made or that thread could not be started:
	// DllMain shows it then. Called once by DllMain.
	bool InitRageOpenV();
}
