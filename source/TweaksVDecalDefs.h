// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the decal definitions limit: the kinds of decal (bullet holes, blood, scorch and tyre marks,
// liquids, graffiti, ...) the game reads from the decals.dat files. The game's own array holds 260, and
// once it is full every further definition is dropped silently - add-on and mod content comes last, so
// its decals never appear. DECALDEFS_SIZE in [DECALS] moves the array to a larger one and raises the
// limit; with DIAGNOSTIC_MODE=1 the part logs how many definitions are registered, and whether the
// game's own array would have dropped some.
#pragma once

namespace TweaksV
{
	// Moves the array and raises the limit, as [DECALS] asks. Called once by DllMain.
	void InitDecalDefs();
}
