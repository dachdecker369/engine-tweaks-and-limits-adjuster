// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - TweaksV.ini: location, generation and readers (plan 5.6 and 4.7).
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

	// Absolute path of the TweaksV.ini in use (plan 4.7): the game folder's if it
	// exists, else the one in %LOCALAPPDATA%\TweaksV if that exists, else a new one
	// in the game folder - or in %LOCALAPPDATA%\TweaksV if the game folder is
	// read-only. W and A versions, because the original code calls both
	// GetPrivateProfileIntW and GetPrivateProfileIntA.
	const wchar_t* GetIniPathW();
	const char* GetIniPathA();

	// Picks the TweaksV.ini to use (see GetIniPathW) and writes the full default file
	// (Appendix E.1) if there is none. For an existing file, only adds keys that are
	// missing - never changes a value.
	void CreateOrUpdateIni();

	// False if TweaksV.ini could not be created anywhere: every reader then returns
	// its built-in default.
	bool IsIniAvailable();

	// The TweaksV.ini line of the log header: the path, and where it came from.
	std::string DescribeIniLocation();

	// Readers. Missing or invalid values return the default, out-of-range values are
	// clamped; invalid and clamped values are logged once per key. Booleans accept
	// 1/0, true/false, yes/no and on/off.
	bool GetIniBool(const char* section, const char* key, bool defaultValue);
	int GetIniInt(const char* section, const char* key, int defaultValue, int minValue, int maxValue);
	std::string GetIniString(const char* section, const char* key, const char* defaultValue);

	// The section's ENABLED switch, unless the feature was switched off for this
	// session (DisableFeatureForSession, e.g. by CheckForConflicts).
	bool IsFeatureOn(const char* section);
	void DisableFeatureForSession(const char* section);

	// The TESTING switches of [TWEAKSV], read once.
	bool IsDryRun();
	bool IsDiagnosticMode();
}
