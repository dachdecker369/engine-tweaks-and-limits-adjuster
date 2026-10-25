// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - stand-alone plugins installed next to TweaksV that do the job of one of its parts.
#pragma once

namespace TweaksV
{
	// The folders ASI loaders load .asi files from: the game folder and these subfolders of it.
	inline constexpr const wchar_t* kLoaderFolders[] = { L"", L"\\scripts", L"\\plugins" };

	// Looks for the plugins of every feature (TweaksVFeatures.h) in the game folder and its "scripts"
	// and "plugins" subfolders (kLoaderFolders). An old plugin switches its feature off for this
	// session, is logged and goes into the start-up summary, which tells the user to delete the old
	// file. A plugin that does the job too and stays installed does the same while its feature is on in
	// TweaksV.ini, and the summary names the switch that stops the message; a feature switched off
	// there has nothing said about it.
	void CheckForConflicts();

	// True if p lies inside GTA5_Enhanced.exe's image. The relocation checks use it: an
	// array reference outside the image means another mod already relocated the array.
	bool IsInsideGameImage(const void* p);
}
