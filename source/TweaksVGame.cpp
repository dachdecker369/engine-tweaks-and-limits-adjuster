// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the game it runs in (see TweaksVGame.h).

#include "TweaksVGame.h"

#include <windows.h>

#include <cstring>
#include <cwchar>
#include <vector>

#pragma comment(lib, "Version.lib")

namespace TweaksV
{
	namespace
	{
		// The running program's path, or empty.
		bool ProgramPath(wchar_t (&path)[MAX_PATH])
		{
			const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
			return length != 0 && length < MAX_PATH;
		}
	}

	bool IsGtaVEnhanced()
	{
		static const bool enhanced = []
		{
			wchar_t path[MAX_PATH] = {};
			if (!ProgramPath(path))
				return false;
			const wchar_t* name = wcsrchr(path, L'\\');
			name = name ? name + 1 : path;
			return _wcsicmp(name, L"GTA5_Enhanced.exe") == 0;
		}();
		return enhanced;
	}

	bool ReadGameVersion(GameVersion& version)
	{
		version = GameVersion{};
		wchar_t path[MAX_PATH] = {};
		if (!ProgramPath(path))
			return false;
		DWORD handle = 0;
		const DWORD size = GetFileVersionInfoSizeW(path, &handle);
		if (size == 0)
			return false;
		std::vector<uint8_t> info(size);
		if (!GetFileVersionInfoW(path, 0, size, info.data()))
			return false;
		VS_FIXEDFILEINFO* fixed = nullptr;
		UINT fixedSize = 0;
		if (!VerQueryValueW(info.data(), L"\\", reinterpret_cast<LPVOID*>(&fixed), &fixedSize) || !fixed || fixedSize < sizeof(*fixed))
			return false;
		version.major = HIWORD(fixed->dwFileVersionMS);
		version.minor = LOWORD(fixed->dwFileVersionMS);
		version.build = HIWORD(fixed->dwFileVersionLS);
		version.revision = LOWORD(fixed->dwFileVersionLS);
		return true;
	}

	uint64_t GameVersionNumber()
	{
		static const uint64_t number = []
		{
			GameVersion version;
			return ReadGameVersion(version) ? version.Number() : 0;
		}();
		return number;
	}
}
