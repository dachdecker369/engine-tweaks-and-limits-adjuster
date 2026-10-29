// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - its features (see TweaksVFeatures.h).

#include "TweaksVFeatures.h"

#include <windows.h>

#include <initializer_list>
#include <iterator>

#include "TweaksVBlips.h"
#include "TweaksVBoxStreamer.h"
#include "TweaksVDecals.h"
#include "TweaksVDirectStorage.h"
#include "TweaksVHeapAdjuster.h"
#include "TweaksVLog.h"
#include "TweaksVModkitLimit.h"
#include "TweaksVPackfileLimit.h"
// [ped props]
#include "TweaksVPedProps.h"
#include "TweaksVRageOpenV.h"
// [siren count]
#include "TweaksVSirenCount.h"
// [siren ids]
#include "TweaksVSirenIds.h"
#include "TweaksVStraightToStoryMode.h"
// [streaming memory]
#include "TweaksVSummary.h"
// [texture budget]
#include "TweaksVTextureBudget.h"
// [weapon components]
#include "TweaksVWeaponComponents.h"
// [weapon infos]
#include "TweaksVWeaponInfos.h"

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

		// A limit of a part to come, measured before its part writes anything (SPECIFICATIONS 11.10): with
		// DIAGNOSTIC_MODE=1 its module finds and checks the sites of the limit and measures it.
		struct MeasuredFirst
		{
			const char* feature;	// the part's log tag: "WeaponLimits"
			void (*start)();		// the module's Measure<Limit>()
		};

		// The parts measured first, one entry per limit in the order of SPECIFICATIONS 11.10. A limit leaves the
		// table when its part is built, or when it is not, so the table may be empty.
		constexpr std::initializer_list<MeasuredFirst> kMeasuredFirst = {
			// [weapon infos]
			{ "WeaponLimits", MeasureWeaponInfos },
			// [weapon components]
			{ "WeaponLimits", MeasureWeaponComponents },
			// [siren ids]
			{ "SirenSettings", MeasureSirenIds },
			// [siren count]
			{ "SirenSettings", MeasureSirenCount },
			// [ped props]
			{ "PedProps", MeasurePedProps },
			// [texture budget]
			{ "Resources", MeasureTextureBudget },
			// [streaming memory]
		};
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
		// The parts measured first, after the parts and with DIAGNOSTIC_MODE=1 only. A measurement is an aid
		// (SPECIFICATIONS 12): an exception ends only that one and is logged, not reported.
		if (IsDiagnosticMode())
			for (const MeasuredFirst& measured : kMeasuredFirst)
			{
				try
				{
					measured.start();
				}
				catch (...)
				{
					WriteLog(measured.feature, "DIAGNOSTIC: unexpected exception - a measurement of this part is skipped for this session.");
				}
			}
		return summaryLater;
	}
}
