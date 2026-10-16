// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - DirectStorage: the files GTA V Enhanced keeps locked in its folder. UNLOCK_FILES lets
// DirectStorage open only the .rpf and .cache files in the game folder (files outside it, such as the
// saves, are not touched); LOG_CALLS and LOG_FILE_OPENS are for testing.
#pragma once

namespace TweaksV
{
	// Installs the hooks that [DIRECTSTORAGE_SETTINGS] asks for. Called once by DllMain.
	void InitDirectStorage();
}
