// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - DirectStorage: how GTA V Enhanced opens the files in its folder. For now this part only
// logs ([DIRECTSTORAGE_SETTINGS] LOG_CALLS and LOG_FILE_OPENS); it changes nothing in the game.
#pragma once

namespace TweaksV
{
	// Installs the logging hooks that [DIRECTSTORAGE_SETTINGS] asks for. Called once by DllMain.
	void InitDirectStorage();
}
