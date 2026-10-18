// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the one log file, TweaksV.log, shared by every part of TweaksV.
#pragma once

#include <windows.h>

#include <string>
#include <string_view>

namespace TweaksV
{
	// Appends "hh:mm:ss.mmm [Tag] text" to TweaksV.log and, if open, the console.
	// Thread-safe; lines are cut at 2 KB. Lines written before the log has been
	// started (ClearLog + WriteLogHeader) are kept and written after the header.
	void WriteLog(std::string_view tag, std::string_view text);

	// Truncates TweaksV.log in the game folder - or, if that folder is not writable,
	// %LOCALAPPDATA%\TweaksV\TweaksV.log. Called by the shared ClearLog().
	void ClearLogFile();

	// The first lines of TweaksV.log: versions, paths and switches. Then
	// writes any lines that were logged before the log was started.
	void WriteLogHeader();

	// Flushes and closes TweaksV.log (DLL_PROCESS_DETACH).
	void CloseLog();

	// Opens a console window that mirrors the log. RageOpenV's code, moved out of
	// RageOpenV's DllMain so the console also works when RageOpenV is off.
	void OpenConsole();

	// Absolute path of the log file in use (empty if logging to a file is off).
	std::wstring GetLogPathW();

	// Text helpers shared by TweaksV's files.
	std::string ToUtf8(std::wstring_view text);
	std::wstring FromUtf8(std::string_view text);
	std::string DescribeError(DWORD error);
}
