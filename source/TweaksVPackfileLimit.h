// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the packfile limit: the game's array of RPF archives, which PACKFILEARRAY_SIZE in
// [PACKFILE_LIMIT] enlarges on the relocation core, and the archive count of its configuration.
#pragma once

namespace TweaksV
{
	// Moves the packfile array, raises its limits and hooks the configuration call, as [PACKFILE_LIMIT]
	// asks. Called once by DllMain.
	void InitPackfileLimit();
}
