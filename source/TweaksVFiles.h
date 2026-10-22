// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - TweaksV's own files (TweaksV.log, TweaksV.ini) are opened only when they are what their
// path says: a plain file in the folder it belongs in. A link placed there - a symbolic link, a hard
// link, a junction on the way - could otherwise make TweaksV write to another file, which matters
// when the game runs as administrator.
#pragma once

#include <windows.h>

#include <string>

namespace TweaksV
{
	// The folder one of TweaksV's own files is in. The game folder may itself be a link (an install
	// moved to another drive); TweaksV's own folder in %LOCALAPPDATA% may not.
	enum class FileFolder { Game, Own };

	struct OwnFile
	{
		HANDLE handle = INVALID_HANDLE_VALUE;
		DWORD error = ERROR_SUCCESS;	// the Windows error, if the file could not be opened
		std::string unsafe;				// why the file is not used, if it is not what its path says

		// It could not be opened because its folder (or the file itself) is read-only: normal under
		// C:\Program Files, so not a problem - unlike every other reason.
		bool ReadOnly() const { return unsafe.empty() && (error == ERROR_ACCESS_DENIED || error == ERROR_WRITE_PROTECT); }

		// Why it is not used, for a message: unsafe, else the Windows error.
		std::string Reason() const;
	};

	// Opens folder\name for writing - OPEN_ALWAYS, or CREATE_NEW for a new file only - and checks it
	// before anything is written. A file that fails the check is closed, deleted if this call created
	// it, and comes back with unsafe set. truncate empties a file that passes.
	OwnFile OpenOwnFile(const std::wstring& folder, const wchar_t* name, FileFolder where, DWORD disposition, bool truncate);

	// Checks an existing folder\name the same way and holds it: until the handle is closed, neither
	// the file nor a folder above it can be renamed, deleted or replaced, so the functions that open
	// it by its path (GetPrivateProfileStringW, WritePrivateProfileStringW) reach this very file.
	OwnFile HoldOwnFile(const std::wstring& folder, const wchar_t* name, FileFolder where);

	// Renames folder\name to folder\newName through a handle on the file, after the same check: a link
	// is never renamed. A file already at the new name is replaced, when it passes the check itself;
	// otherwise nothing is renamed and unsafe says why. Comes back without a handle: error is
	// ERROR_SUCCESS once the file has its new name.
	OwnFile RenameOwnFile(const std::wstring& folder, const wchar_t* name, const wchar_t* newName, FileFolder where);

	// The path of an existing file or folder as Windows resolves it - every link on the way followed,
	// without the "\\?\" in front - so that two spellings of one folder (a short name, a junction) can be
	// told to be the same. Empty if it cannot be opened or its path not read.
	std::wstring FinalPathOf(const std::wstring& path);

	// True if path names a file (not a folder) that exists.
	bool FileExists(const std::wstring& path);
}
