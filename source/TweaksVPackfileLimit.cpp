// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the packfile limit (see TweaksVPackfileLimit.h).
//
// The game keeps its RPF archives in one static array - 7260 entries of 8 bytes on the current build -
// that 31 instructions reference (P-E-R01..R30, and R31a or R31b by game build) and two limits size: an
// immediate (P-E-D3, D2 or D1 by game build) and a variable the game compares against (P-E-I1).
// PACKFILEARRAY_SIZE in [PACKFILE_LIMIT] moves the array to one of 65535 entries, always allocated in
// full, and raises both limits to that size, clamped to the game's own size..65535, on the relocation
// core. The third limit lies in the game's configuration: the call that reads it (P-E-H1) is
// redirected through a call stub to a function that replaces the archive count read from
// gameconfig.xml with the same size and then calls the game's own function - in the same all-or-nothing
// write as the references and limits. A call another mod has already redirected is left alone.
// Written for TweaksV; the patterns come from PackfileLimitAdjusterEnhanced by Chiheb-Bacha, which
// builds on the Packfile Limit Adjuster by alloc8or (CREDITS.md).

#include "TweaksVPackfileLimit.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "GameVersion.h"
#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVRelocation.h"
#include "TweaksVSettings.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "PackfileLimit";

		constexpr uint32_t kMaxEntries = 65535;	// archive indices are 16-bit
		static_assert(kMaxEntries == static_cast<uint32_t>(kPackfileArraySize.maxValue), "TweaksV: PACKFILEARRAY_SIZE's maximum must be the array's size");

		constexpr FileVersion kVersion1013_17{ 1, 0, 1013, 17 };	// the Mansions DLC: the second direct size pattern
		constexpr FileVersion kVersion1158_13{ 1, 0, 1158, 13 };	// the Kortz Center DLC: the third, and the other reference

		constexpr char kCmpRcx[] = "\x48\x81\xF9";	// cmp rcx, imm32: the direct size
		constexpr size_t kCallOffset = 0x10;		// of the streaming configuration call in P-E-H1's match
		constexpr size_t kCountOffset = 24;			// of the archive count in the configuration the call reads

		// the 30 references every build has
		constexpr Anchor kReferences[] = {
			Reference(Pattern("P-E-R01"), Rel32(3)), Reference(Pattern("P-E-R02"), Rel32(3)), Reference(Pattern("P-E-R03"), Rel32(3)),
			Reference(Pattern("P-E-R04"), Rel32(5)), Reference(Pattern("P-E-R05"), Rel32(5)), Reference(Pattern("P-E-R06"), Rel32(3)),
			Reference(Pattern("P-E-R07"), Rel32(3)), Reference(Pattern("P-E-R08"), Rel32(3)), Reference(Pattern("P-E-R09"), Rel32(39)),
			Reference(Pattern("P-E-R10"), Rel32(7)), Reference(Pattern("P-E-R11"), Rel32(3)), Reference(Pattern("P-E-R12"), Rel32(3)),
			Reference(Pattern("P-E-R13"), Rel32(3)), Reference(Pattern("P-E-R14"), Rel32(3)), Reference(Pattern("P-E-R15"), Rel32(3)),
			Reference(Pattern("P-E-R16"), Rel32(3)), Reference(Pattern("P-E-R17"), Rel32(41)), Reference(Pattern("P-E-R18"), Rel32(3)),
			Reference(Pattern("P-E-R19"), Rel32(3)), Reference(Pattern("P-E-R20"), Rel32(3)), Reference(Pattern("P-E-R21"), Rel32(3)),
			Reference(Pattern("P-E-R22"), Rel32(3)), Reference(Pattern("P-E-R23"), Rel32(3)), Reference(Pattern("P-E-R24"), Rel32(3)),
			Reference(Pattern("P-E-R25"), Rel32(3)), Reference(Pattern("P-E-R26"), Rel32(3)), Reference(Pattern("P-E-R27"), Rel32(3)),
			Reference(Pattern("P-E-R28"), Rel32(3)), Reference(Pattern("P-E-R29"), Rel32(3)), Reference(Pattern("P-E-R30"), Rel32(3)),
		};
		// the reference that depends on the build
		constexpr Anchor kReference31a = Reference(Pattern("P-E-R31a"), Rel32(13));
		constexpr Anchor kReference31b = Reference(Pattern("P-E-R31b"), Rel32(20));
		// the direct size, one pattern per range of builds; the indirect size, a variable; the call that
		// reads the streaming configuration (the part reads its site itself)
		constexpr Anchor kDirect3 = Limit(Pattern("P-E-D3"), Imm32(4, kCmpRcx, 3, 7, 1, 0));
		constexpr Anchor kDirect2 = Limit(Pattern("P-E-D2"), Imm32(4, kCmpRcx, 3, 7, 1, 0));
		constexpr Anchor kDirect1 = Limit(Pattern("P-E-D1"), Imm32(0, kCmpRcx, 3, 3, 1, 0));
		constexpr Anchor kIndirect = Limit(Pattern("P-E-I1"), Variable(2));
		constexpr Anchor kCall = Site(Pattern("P-E-H1"));

		constexpr RelocationSpec kSpecBase = { kTag, "packfile array", "packfile list size", "The packfile limit is not changed",
			sizeof(void*), kMaxEntries, &kPackfileArraySize, {}, 0, nullptr, 0 };

		// ---- the streaming configuration call

		uint32_t g_size = 0;						// the size the hook writes into the configuration
		void (*g_original)(char*) = nullptr;		// the game's own function behind the call

		// The game calls this with its streaming configuration, read from gameconfig.xml, on a thread
		// of its own: nothing may throw out of here, and the original is always called.
		void SetStreamingInterface(char* configuration)
		{
			uint32_t held = 0;
			if (configuration)
			{
				memcpy(&held, configuration + kCountOffset, sizeof(held));
				memcpy(configuration + kCountOffset, &g_size, sizeof(g_size));
			}
			try
			{
				WriteLog(kTag, "The archive count of the game's configuration, " + std::to_string(held) + ", is replaced by " +
					std::to_string(g_size) + ".");
			}
			catch (...)
			{
			}
			g_original(configuration);
		}
	}

	void InitPackfileLimit()
	{
		// the patterns of this game build: without its file version they cannot be chosen
		const uint64_t version = GetCurrentProcessVersionComparable();
		if (version == 0)
		{
			RelocationFailed(kSpecBase, "the game's file version could not be read, so the patterns for this game build cannot be chosen.");
			return;
		}
		std::vector<Anchor> anchors(std::begin(kReferences), std::end(kReferences));
		anchors.push_back(version >= kVersion1158_13.ToComparable() ? kReference31a : kReference31b);
		anchors.push_back(version >= kVersion1158_13.ToComparable() ? kDirect3 : version >= kVersion1013_17.ToComparable() ? kDirect2 : kDirect1);
		anchors.push_back(kIndirect);
		anchors.push_back(kCall);
		RelocationSpec spec = kSpecBase;
		spec.anchors = anchors;

		RelocationPlan plan;
		if (!FindRelocation(spec, plan))
			return;

		// the streaming configuration call: a call into the game - one another mod has redirected is left alone
		uint8_t* const call = plan.matches.back()[0] + kCallOffset;
		const uint8_t* const target = IsReadable(call, 5) ? CallTarget(call) : nullptr;
		if (IsDiagnosticMode())
			WriteLog(kTag, "DIAGNOSTIC: streaming configuration call at " + DescribeAddress(call) + " -> " +
				(target ? DescribeAddress(target) : std::string("not a call")));
		if (!target || !IsInsideGameImage(target))
		{
			RelocationFailed(spec, "the call that reads the game's streaming configuration at " + DescribeAddress(call) +
				" is not a call into the game (another mod may have hooked it already).");
			return;
		}

		g_size = plan.size;
		const std::vector<CallRedirect> redirects = { { call, reinterpret_cast<const void*>(&SetStreamingInterface),
			"streaming configuration call", reinterpret_cast<void**>(&g_original) } };
		if (!ApplyRelocation(spec, plan, {}, redirects, " and redirect the streaming configuration call at " + DescribeAddress(call)))
			return;
		if (!IsDryRun())
			WriteLog(kTag, "The streaming configuration call at " + DescribeAddress(call) + " is redirected: the archive count the game's "
				"configuration holds is replaced by " + std::to_string(plan.size) + " when the game reads it.");
	}
}
