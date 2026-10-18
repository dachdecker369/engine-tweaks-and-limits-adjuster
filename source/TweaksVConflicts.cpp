// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - old stand-alone plugins still installed next to TweaksV.

#include "TweaksVConflicts.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <utility>

#include "TweaksVFeatures.h"
#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		// ASI loaders load .asi files from the game folder and these subfolders.
		constexpr const wchar_t* kFolders[] = { L"", L"\\scripts", L"\\plugins" };

		bool FileExists(const std::wstring& path)
		{
			const DWORD attributes = GetFileAttributesW(path.c_str());
			return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
		}
	}

	void CheckForConflicts()
	{
		int found = 0;
		for (const wchar_t* folder : kFolders)
		{
			for (const Feature& feature : Features())
			{
				for (const wchar_t* file : feature.oldPlugins)
				{
					if (!file)
						continue;
					const std::wstring path = GetGameFolder() + folder + L"\\" + file;
					if (!FileExists(path))
						continue;

					++found;
					DisableFeatureForSession(*feature.section);
					ReportFailure("TweaksV", "Old plugin found: " + ToUtf8(path) + ". TweaksV's " + feature.section->feature +
						" part is switched off for this session so nothing is patched twice. "
						"Delete the old file - TweaksV replaces it.");
				}
			}
		}

		if (found == 0)
			WriteLog("TweaksV", "Conflict check: none of the old plugins TweaksV replaces were found.");
	}

	bool IsInsideGameImage(const void* p)
	{
		static const std::pair<uintptr_t, uintptr_t> image = []
		{
			const auto base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
			const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE)
				return std::pair<uintptr_t, uintptr_t>(0, 0);
			const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE)
				return std::pair<uintptr_t, uintptr_t>(0, 0);
			const auto start = reinterpret_cast<uintptr_t>(base);
			return std::pair<uintptr_t, uintptr_t>(start, start + nt->OptionalHeader.SizeOfImage);
		}();

		const auto address = reinterpret_cast<uintptr_t>(p);
		return address >= image.first && address < image.second;
	}
}
