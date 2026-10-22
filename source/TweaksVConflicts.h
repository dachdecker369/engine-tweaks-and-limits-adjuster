// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - old stand-alone plugins still installed next to TweaksV.
#pragma once

namespace TweaksV
{
	// The folders ASI loaders load .asi files from: the game folder and these subfolders of it.
	inline constexpr const wchar_t* kLoaderFolders[] = { L"", L"\\scripts", L"\\plugins" };

	// Looks for the old plugins of every feature (TweaksVFeatures.h) in the game folder and its
	// "scripts" and "plugins" subfolders (kLoaderFolders). A match switches that feature off for this
	// session, is logged and goes into the start-up summary, which tells the user to delete the old
	// file.
	void CheckForConflicts();

	// True if p lies inside GTA5_Enhanced.exe's image. The relocation checks use it: an
	// array reference outside the image means another mod already relocated the array.
	bool IsInsideGameImage(const void* p);
}
