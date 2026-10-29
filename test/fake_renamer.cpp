// SPDX-License-Identifier: GPL-3.0-only
// Test harness for TweaksV: a program other than the game - an editor, a tool - that replaces a file by the
// classic rename, MoveFileExW with MOVEFILE_REPLACE_EXISTING, the usual safe way to save a file. The fake game
// (option "dsreplace") starts it while the stand-in dstorage.dll holds that file open in the game folder:
// TweaksV is not in this program, so Windows refuses the rename over the held file, as it refuses it to every
// program outside the game, whatever the sharing the game's handle has.
// Usage: renamer.exe FROM TO - the exit code is Windows' error, 0 when the rename went through.
#include <windows.h>

int wmain(int argc, wchar_t** argv)
{
	if (argc != 3)
		return ERROR_INVALID_PARAMETER;
	return MoveFileExW(argv[1], argv[2], MOVEFILE_REPLACE_EXISTING) ? 0 : static_cast<int>(GetLastError());
}
