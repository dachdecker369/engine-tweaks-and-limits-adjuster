// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - old stand-alone plugins still installed next to TweaksV (plan 4.4).

#include "TweaksVConflicts.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <utility>

#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		struct OldPlugin
		{
			const wchar_t* file;
			const char* section;	// the TweaksV feature it duplicates
			const char* feature;
		};

		// The plugins TweaksV replaces. ZHeapAdjuster.asi is the rename the HeapAdjuster
		// author recommends; OpenRPF.asi and OpenIV.asi do RageOpenV's job, and alloc8or's
		// DirectStorageFix.asi does the job of TweaksV's own DirectStorage part.
		constexpr OldPlugin kOldPlugins[] = {
			{ L"HeapAdjuster.asi", "HEAP_SETTINGS", "HeapAdjusterEnhanced" },
			{ L"ZHeapAdjuster.asi", "HEAP_SETTINGS", "HeapAdjusterEnhanced" },
			{ L"ModkitLimitAdjusterEnhanced.asi", "MODKIT_SETTINGS", "ModkitLimitAdjusterEnhanced" },
			{ L"PackfileLimitAdjusterEnhanced.asi", "PACKFILE_SETTINGS", "PackfileLimitAdjusterEnhanced" },
			{ L"RageOpenV.asi", "RAGEOPENV_SETTINGS", "RageOpenV" },
			{ L"OpenRPF.asi", "RAGEOPENV_SETTINGS", "RageOpenV" },
			{ L"OpenIV.asi", "RAGEOPENV_SETTINGS", "RageOpenV" },
			{ L"StraightToStoryMode.asi", "STRAIGHTTOSTORYMODE_SETTINGS", "StraightToStoryMode" },
			{ L"DirectStorageFix.asi", "DIRECTSTORAGE_SETTINGS", "DirectStorage" },
		};

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
			for (const OldPlugin& plugin : kOldPlugins)
			{
				const std::wstring path = GetGameFolder() + folder + L"\\" + plugin.file;
				if (!FileExists(path))
					continue;

				++found;
				DisableFeatureForSession(plugin.section);
				ReportFailure("TweaksV", "Old plugin found: " + ToUtf8(path) + ". TweaksV's " + plugin.feature +
					" part is switched off for this session so nothing is patched twice. "
					"Delete the old file - TweaksV replaces it.");
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
