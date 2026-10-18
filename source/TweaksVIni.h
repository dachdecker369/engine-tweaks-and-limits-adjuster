// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - TweaksV.ini, the file: where it is, creating it and adding keys that are missing. What is
// in it - every setting, its default and its reader - is in TweaksVSettings.h.
#pragma once

#include <string>

namespace TweaksV
{
	// Folder of GTA5_Enhanced.exe, without a trailing backslash. TweaksV.ini,
	// TweaksV.log, mods\ and newmods\ live there, wherever TweaksV.asi itself sits.
	const std::wstring& GetGameFolder();

	// %LOCALAPPDATA%\TweaksV - where TweaksV.ini and TweaksV.log go when the game
	// folder is read-only (e.g. under C:\Program Files). Empty if unavailable. The
	// folder is not created here.
	std::wstring GetFallbackFolder();

	// Absolute path of the TweaksV.ini in use: the game folder's if it exists, else the one in
	// %LOCALAPPDATA%\TweaksV if that exists, else a new one in the game folder - or in
	// %LOCALAPPDATA%\TweaksV if the game folder is read-only.
	const wchar_t* GetIniPathW();

	// Picks the TweaksV.ini to use (see GetIniPathW) and writes the full default file
	// (DefaultIniText) if there is none. For an existing file, only adds keys that are
	// missing - never changes a value. The file is used only if it is what its path says
	// (TweaksVFiles.h), and is then held until ReleaseIniFile(); a file that fails the check
	// is reported and not read at all, so every setting keeps its default.
	void CreateOrUpdateIni();

	// Lets go of the TweaksV.ini that CreateOrUpdateIni() holds - once LoadSettings() has read it.
	void ReleaseIniFile();

	// The TweaksV.ini line of the log header: the path, and where it came from.
	std::string DescribeIniLocation();

	// A key's value as written in TweaksV.ini, without the spaces around it; empty if the key is
	// missing. At most 255 characters are read. Only LoadSettings() reads TweaksV.ini this way -
	// everything else asks TweaksVSettings.h.
	std::wstring ReadIniValue(const char* section, const char* key);
}
