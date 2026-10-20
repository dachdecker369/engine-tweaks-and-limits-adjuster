// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - DirectStorage: the files GTA V Enhanced keeps locked in its folder. UNLOCK_FILES lets
// DirectStorage open only the .rpf and .cache files in the game folder (files outside it, such as the
// saves, are not touched); with DIAGNOSTIC_MODE=1 the part logs the game's calls into DirectStorage and
// every file opened in the game folder.
#pragma once

namespace TweaksV
{
	// Installs the hooks that [DIRECTSTORAGE] and DIAGNOSTIC_MODE ask for. Called once by DllMain.
	void InitDirectStorage();
}
