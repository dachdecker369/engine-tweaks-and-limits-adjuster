// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - old stand-alone plugins still installed next to TweaksV (plan 4.4).
#pragma once

namespace TweaksV
{
	// Looks for the old plugins TweaksV replaces in the game folder and its "scripts"
	// and "plugins" subfolders (ASI loaders load from all three). A match switches the
	// matching TweaksV feature off for this session, is logged and goes into the
	// start-up summary, which tells the user to delete the old file.
	void CheckForConflicts();

	// True if p lies inside GTA5_Enhanced.exe's image. The relocation checks use it: an
	// array reference outside the image means another mod already relocated the array
	// (plan 5.9 R6).
	bool IsInsideGameImage(const void* p);
}
