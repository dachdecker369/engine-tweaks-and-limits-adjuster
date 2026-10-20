// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the modkit limit: the game's array of vehicle modkit IDs, which MODKITARRAY_SIZE in
// [MODKIT_LIMIT] enlarges on the relocation core.
#pragma once

namespace TweaksV
{
	// Moves the modkit array and raises its limits, as [MODKIT_LIMIT] asks. Called once by DllMain.
	void InitModkitLimit();
}
