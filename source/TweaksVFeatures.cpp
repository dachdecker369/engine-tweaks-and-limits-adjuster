// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - its features (see TweaksVFeatures.h).

#include "TweaksVFeatures.h"

#include <windows.h>

#include <iterator>

#include "HeapAdjusterEnhanced_Game.h"
#include "ModkitLimitAdjusterEnhanced_Game.h"
#include "PackfileLimitAdjusterEnhanced_Game.h"
#include "main.h"
#include "StraightToStoryMode_Game.h"
#include "TweaksVDirectStorage.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		// The five original plugins in the order an ASI loader loaded their .asi files (alphabetical),
		// then TweaksV's own DirectStorage part. ZHeapAdjuster.asi is the rename the HeapAdjuster author
		// recommends; OpenRPF.asi and OpenIV.asi do RageOpenV's job, and alloc8or's DirectStorageFix.asi
		// that of the DirectStorage part.
		constexpr Feature kFeatures[] = {
			{ &kHeapSection, { L"HeapAdjuster.asi", L"ZHeapAdjuster.asi" },
				[] { HeapAdjusterEnhanced::initializeGame(); return false; } },
			{ &kModkitSection, { L"ModkitLimitAdjusterEnhanced.asi" },
				[] { ModkitLimitAdjusterEnhanced::initializeGame(); return false; } },
			{ &kPackfileSection, { L"PackfileLimitAdjusterEnhanced.asi" },
				[] { PackfileLimitAdjusterEnhanced::initializeGame(); return false; } },
			// RageOpenV's start-up runs later, in its GetSystemTimeAsFileTime hook, which then shows the summary
			{ &kRageOpenVSection, { L"RageOpenV.asi", L"OpenRPF.asi", L"OpenIV.asi" },
				[] { return RageOpenV::DllMain(nullptr, DLL_PROCESS_ATTACH, nullptr) != FALSE; } },
			{ &kStoryModeSection, { L"StraightToStoryMode.asi" },
				[] { StraightToStoryMode::initializeGame(); return false; } },
			{ &kDirectStorageSection, { L"DirectStorageFix.asi" },
				[] { InitDirectStorage(); return false; } },
		};

		// Every section of TweaksV.ini but [TWEAKSV] belongs to a feature, in the same order.
		static_assert(std::size(kFeatures) + 1 == std::size(kSections), "TweaksV: kFeatures needs one entry per section but [TWEAKSV]");
		consteval bool InSectionOrder()
		{
			for (size_t i = 0; i < std::size(kFeatures); i++)
				if (kFeatures[i].section != kSections[i + 1])
					return false;
			return true;
		}
		static_assert(InSectionOrder(), "TweaksV: kFeatures must be in the order of kSections");
	}

	std::span<const Feature> Features()
	{
		return kFeatures;
	}

	bool StartFeatures()
	{
		bool summaryLater = false;
		for (const Feature& feature : kFeatures)
		{
			if (!IsFeatureOn(*feature.section))
				continue;
			try
			{
				if (feature.start())
					summaryLater = true;
			}
			catch (...)
			{
				ReportFailure(feature.section->feature, "unexpected exception - this part is skipped for this session");
			}
		}
		return summaryLater;
	}
}
