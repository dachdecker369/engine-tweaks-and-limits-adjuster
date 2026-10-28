// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - its features (see TweaksVFeatures.h).

#include "TweaksVFeatures.h"

#include <windows.h>

#include <iterator>

#include "TweaksVBlips.h"
#include "TweaksVBoxStreamer.h"
#include "TweaksVDecals.h"
#include "TweaksVDirectStorage.h"
#include "TweaksVHeapAdjuster.h"
#include "TweaksVModkitLimit.h"
#include "TweaksVPackfileLimit.h"
#include "TweaksVRageOpenV.h"
#include "TweaksVStraightToStoryMode.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		// NaturalVision Enhanced's mods loader does RageOpenV's job and skips the splash and legal screens
		// by its own INI, as StraightToStoryMode does. It loads before TweaksV but patches those sites after
		// it, at the game's first request for the time, uses what it finds there unchecked and faults when
		// TweaksV has changed them. NVE needs it, so it stays, and those two parts are off while it is there.
		constexpr PeerPlugin kNveLoader{ L"onigiri.asi", "NaturalVision Enhanced's loader" };

		// The five original plugins in the order an ASI loader loaded their .asi files (alphabetical),
		// TweaksV's own box streamer and decal definitions parts among the limit adjusters, then its
		// DirectStorage part and, last, its Blips part. ZHeapAdjuster.asi is the rename the HeapAdjuster
		// author recommends; OpenRPF.asi and OpenIV.asi do RageOpenV's job, alloc8or's DirectStorageFix.asi
		// that of the DirectStorage part, rootBrz's BlipLimitFix.asi that of the Blips part, and the
		// closed-source DecalPatch.asi that of both the box streamer and the decal definitions parts, so it
		// switches both off.
		constexpr Feature kFeatures[] = {
			{ &kHeapSection, { L"HeapAdjuster.asi", L"ZHeapAdjuster.asi" }, {},
				[] { InitHeapAdjuster(); return false; } },
			{ &kModkitSection, { L"ModkitLimitAdjusterEnhanced.asi" }, {},
				[] { InitModkitLimit(); return false; } },
			{ &kPackfileSection, { L"PackfileLimitAdjusterEnhanced.asi" }, {},
				[] { InitPackfileLimit(); return false; } },
			{ &kBoxStreamerSection, { L"DecalPatch.asi" }, {},
				[] { InitBoxStreamer(); return false; } },
			{ &kDecalsSection, { L"DecalPatch.asi" }, {},
				[] { InitDecals(); return false; } },
			// RageOpenV's start-up runs later, in its GetSystemTimeAsFileTime hook, which then shows the summary
			{ &kRageOpenVSection, { L"RageOpenV.asi", L"OpenRPF.asi", L"OpenIV.asi" }, { &kNveLoader },
				[] { return InitRageOpenV(); } },
			{ &kStoryModeSection, { L"StraightToStoryMode.asi" }, { &kNveLoader },
				[] { InitStraightToStoryMode(); return false; } },
			{ &kDirectStorageSection, { L"DirectStorageFix.asi" }, {},
				[] { InitDirectStorage(); return false; } },
			{ &kBlipsSection, { L"BlipLimitFix.asi" }, {},
				[] { InitBlips(); return false; } },
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
