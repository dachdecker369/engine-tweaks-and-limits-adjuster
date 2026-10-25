// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - DirectStorage: the files GTA V Enhanced keeps locked in its folder. UNLOCK_FILES gives a
// read-only open of a file in the game folder other than .rpf and .cache write and delete sharing, so the
// file stays free for other programs while it is open (nothing is refused, nothing outside the game folder
// is touched); with DIAGNOSTIC_MODE=1 the part logs the game's calls into DirectStorage and every file
// opened in the game folder.
#pragma once

namespace TweaksV
{
	// Installs the hooks that [DIRECTSTORAGE] and DIAGNOSTIC_MODE ask for. Called once by DllMain.
	void InitDirectStorage();
}
