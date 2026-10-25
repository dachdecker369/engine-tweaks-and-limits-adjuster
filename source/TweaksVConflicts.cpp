// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - stand-alone plugins installed next to TweaksV that do the job of one of its parts.

#include "TweaksVConflicts.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <utility>

#include "TweaksVFeatures.h"
#include "TweaksVFiles.h"
#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	void CheckForConflicts()
	{
		int found = 0;
		for (const wchar_t* folder : kLoaderFolders)
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

				for (const PeerPlugin* peer : feature.peerPlugins)
				{
					if (!peer)
						continue;
					const std::wstring path = GetGameFolder() + folder + L"\\" + peer->file;
					if (!FileExists(path))
						continue;

					++found;
					// a part TweaksV.ini switches off has nothing to say about it. The file's own switch, not
					// IsFeatureOn: an old plugin found before may have switched the part off already, and
					// this one is still said, so the result does not depend on the order of the finds
					if (!IsEnabledInIni(*feature.section))
						continue;
					DisableFeatureForSession(*feature.section);
					ReportFailure("TweaksV", ToUtf8(path) + " is installed and does this job too (" + peer->description +
						"): TweaksV's " + feature.section->feature + " part is switched off while it is there, so nothing "
						"is patched twice. To stop this message, set ENABLED=0 under [" + feature.section->name +
						"] in TweaksV.ini.");
				}
			}
		}

		if (found == 0)
			WriteLog("TweaksV", "Conflict check: none of the plugins that do the job of one of TweaksV's parts were found.");
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
