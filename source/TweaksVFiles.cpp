// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - TweaksV's own files are opened only when they are what their path says (see TweaksVFiles.h).

#include "TweaksVFiles.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <vector>

#include "TweaksVLog.h"

namespace TweaksV
{
	namespace
	{
		// The path of an open file or folder as Windows resolved it - every link on the way followed -
		// without the "\\?\" (or "\\?\UNC\") that GetFinalPathNameByHandleW puts in front. Empty if unknown.
		std::wstring FinalPath(HANDLE handle)
		{
			std::wstring path(MAX_PATH, L'\0');
			for (int attempt = 0; attempt < 2; ++attempt)
			{
				const DWORD length = GetFinalPathNameByHandleW(handle, path.data(), static_cast<DWORD>(path.size()),
					FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
				if (length == 0 || length > 32767)
					return {};
				if (length < path.size())
				{
					path.resize(length);
					if (path.compare(0, 8, L"\\\\?\\UNC\\") == 0)
						return L"\\\\" + path.substr(8);
					if (path.compare(0, 4, L"\\\\?\\") == 0)
						return path.substr(4);
					return path;
				}
				path.resize(length);	// too short: length is the size needed, the terminating null included
			}
			return {};
		}

		// Why the open file is not a plain file directly in folder - empty if it is one.
		std::string Check(HANDLE file, const std::wstring& folder, const wchar_t* name, FileFolder where)
		{
			BY_HANDLE_FILE_INFORMATION info{};
			if (!GetFileInformationByHandle(file, &info))
				return "its attributes cannot be read";
			if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
				return "it is a link";
			if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				return "it is a folder";
			if (info.nNumberOfLinks > 1)
				return "it is a hard link: the same file has another name as well";

			// The folder: TweaksV's own one must not be a link (a junction) itself - the game folder may be
			const DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (where == FileFolder::Own ? FILE_FLAG_OPEN_REPARSE_POINT : 0u);
			const HANDLE dir = CreateFileW(folder.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr, OPEN_EXISTING, flags, nullptr);
			if (dir == INVALID_HANDLE_VALUE)
				return "its folder cannot be checked";
			BY_HANDLE_FILE_INFORMATION dirInfo{};
			const bool dirKnown = GetFileInformationByHandle(dir, &dirInfo) != FALSE;
			const std::wstring dirPath = FinalPath(dir);
			CloseHandle(dir);
			if (!dirKnown || dirPath.empty())
				return "its folder cannot be checked";
			if (dirInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
				return "its folder is a link";

			// ...and the file must really be in that folder, not where a link on the way led
			std::wstring expected = dirPath;
			if (expected.back() != L'\\')
				expected += L'\\';
			expected += name;
			const std::wstring filePath = FinalPath(file);
			if (filePath.empty())
				return "its path cannot be checked";
			if (_wcsicmp(filePath.c_str(), expected.c_str()) != 0)
				return "a link on its path leads to another place";
			return {};
		}
	}

	std::string OwnFile::Reason() const
	{
		return unsafe.empty() ? DescribeError(error) : unsafe;
	}

	OwnFile OpenOwnFile(const std::wstring& folder, const wchar_t* name, FileFolder where, DWORD disposition, bool truncate)
	{
		OwnFile result;
		const std::wstring path = folder + L"\\" + name;
		// FILE_FLAG_OPEN_REPARSE_POINT: a link in the file's own place is opened itself, never followed;
		// OPEN_ALWAYS does not empty an existing file, so nothing changes before the check
		result.handle = CreateFileW(path.c_str(), GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, disposition, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (result.handle == INVALID_HANDLE_VALUE)
		{
			result.error = GetLastError();
			return result;
		}
		const bool created = disposition == CREATE_NEW || GetLastError() != ERROR_ALREADY_EXISTS;

		result.unsafe = Check(result.handle, folder, name, where);
		if (!result.unsafe.empty())
		{
			if (created)	// only a file this call made is removed again
			{
				FILE_DISPOSITION_INFO remove{ TRUE };
				SetFileInformationByHandle(result.handle, FileDispositionInfo, &remove, sizeof(remove));
			}
			CloseHandle(result.handle);
			result.handle = INVALID_HANDLE_VALUE;
			return result;
		}
		if (truncate && !SetEndOfFile(result.handle))	// the file position is still at the start
		{
			result.error = GetLastError();
			CloseHandle(result.handle);
			result.handle = INVALID_HANDLE_VALUE;
		}
		return result;
	}

	OwnFile HoldOwnFile(const std::wstring& folder, const wchar_t* name, FileFolder where)
	{
		OwnFile result;
		const std::wstring path = folder + L"\\" + name;
		// read access without FILE_SHARE_DELETE: while this handle is open, Windows lets nobody rename,
		// delete or replace the file, nor rename a folder above it; reading and writing it still work
		result.handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (result.handle == INVALID_HANDLE_VALUE)
		{
			result.error = GetLastError();
			return result;
		}
		result.unsafe = Check(result.handle, folder, name, where);
		if (!result.unsafe.empty())
		{
			CloseHandle(result.handle);
			result.handle = INVALID_HANDLE_VALUE;
		}
		return result;
	}

	OwnFile RenameOwnFile(const std::wstring& folder, const wchar_t* name, const wchar_t* newName, FileFolder where)
	{
		OwnFile result;
		const std::wstring target = folder + L"\\" + newName;

		// A file already at the new name is replaced - but only one that is what its path says: a link
		// there is left alone, and nothing is renamed.
		const HANDLE existing = CreateFileW(target.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (existing == INVALID_HANDLE_VALUE)
		{
			const DWORD error = GetLastError();
			if (error != ERROR_FILE_NOT_FOUND)
			{
				result.error = error;
				return result;
			}
		}
		else
		{
			const std::string unsafe = Check(existing, folder, newName, where);
			CloseHandle(existing);
			if (!unsafe.empty())
			{
				result.unsafe = "the " + ToUtf8(newName) + " already there is not replaced: " + unsafe;
				return result;
			}
		}

		// The file itself: opened for its name to change, checked, then renamed through the handle
		const std::wstring path = folder + L"\\" + name;
		result.handle = CreateFileW(path.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (result.handle == INVALID_HANDLE_VALUE)
		{
			result.error = GetLastError();
			return result;
		}
		result.unsafe = Check(result.handle, folder, name, where);
		if (result.unsafe.empty())
		{
			// FILE_RENAME_INFO ends in the new name, so it is sized for it here
			const size_t nameBytes = target.size() * sizeof(wchar_t);
			std::vector<uint8_t> buffer(offsetof(FILE_RENAME_INFO, FileName) + nameBytes + sizeof(wchar_t), 0);
			const auto info = reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
			info->ReplaceIfExists = TRUE;
			info->RootDirectory = nullptr;
			info->FileNameLength = static_cast<DWORD>(nameBytes);
			memcpy(info->FileName, target.c_str(), nameBytes + sizeof(wchar_t));
			if (!SetFileInformationByHandle(result.handle, FileRenameInfo, info, static_cast<DWORD>(buffer.size())))
				result.error = GetLastError();
		}
		CloseHandle(result.handle);
		result.handle = INVALID_HANDLE_VALUE;
		return result;
	}

	std::wstring FinalPathOf(const std::wstring& path)
	{
		const HANDLE handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);	// FILE_FLAG_BACKUP_SEMANTICS: a folder too
		if (handle == INVALID_HANDLE_VALUE)
			return {};
		const std::wstring finalPath = FinalPath(handle);
		CloseHandle(handle);
		return finalPath;
	}

	bool FileExists(const std::wstring& path)
	{
		const DWORD attributes = GetFileAttributesW(path.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
	}
}
