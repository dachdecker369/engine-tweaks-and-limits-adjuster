// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the modkit limit (see TweaksVModkitLimit.h).
//
// The game keeps the IDs of vehicle modkits in one static array - 1024 entries of 2 bytes on the
// current build - that six "lea" load (M-E-R1..R6) and eight instructions size: as N-1, as N+3 and as
// 2N (M-E-L1..L8). MODKITARRAY_SIZE in [MODKIT_LIMIT] moves the array to one of 65536 entries, always
// allocated in full, and raises the eight limits to that size, clamped to the game's own size..65536,
// on the relocation core: every site is checked before the one write, and the six leas must be every
// reference the game's code has to the array, so an array another mod has moved, a limit that
// disagrees, or a reference no pattern names leaves the game as it is.
// Written for TweaksV; the patterns come from ModkitLimitAdjusterEnhanced by Chiheb-Bacha, which builds
// on the ModkitLimit Extender by Tanuki / 0x-FADED (CREDITS.md).

#include "TweaksVModkitLimit.h"

#include <cstdint>

#include "TweaksVRelocation.h"
#include "TweaksVSettings.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "ModkitLimit";

		constexpr uint32_t kMaxEntries = 65536;	// modkit IDs are 16-bit
		static_assert(kMaxEntries == static_cast<uint32_t>(kModkitArraySize.maxValue), "TweaksV: MODKITARRAY_SIZE's maximum must be the array's size");
		constexpr size_t kGameReferences = 6;		// every rip-relative reference to the array the game's code has, on 1.0.1158.16

		constexpr Anchor kAnchors[] = {
			// the six references: the rel32 of a lea
			Reference(Pattern("M-E-R1"), Rel32(5)),
			Reference(Pattern("M-E-R2"), Rel32(5)),
			Reference(Pattern("M-E-R3"), Rel32(7)),
			Reference(Pattern("M-E-R4"), Rel32(8)),
			Reference(Pattern("M-E-R5"), Rel32(3)),
			Reference(Pattern("M-E-R6"), Rel32(5)),
			// the eight limits: imm32 = N x mul + add
			Limit(Pattern("M-E-L1"), Imm32(8, 1, -1)),
			Limit(Pattern("M-E-L2"), Imm32(12, "\x48\x81\xFF", 3, 15, 1, 3)),	// cmp rdi, imm32
			Limit(Pattern("M-E-L3"), Imm32(16, 1, 3)),
			Limit(Pattern("M-E-L4"), Imm32(15, 1, 3)),
			Limit(Pattern("M-E-L5"), Imm32(15, 1, 3)),
			Limit(Pattern("M-E-L6"), Imm32(2, 2, 0)),
			Limit(Pattern("M-E-L7"), Imm32(10, 1, -1)),
			Limit(Pattern("M-E-L8"), Imm32(10, 1, -1)),
		};

		constexpr RelocationSpec kSpec = { kTag, "modkit array", "modkit list size", "The modkit limit is not changed",
			sizeof(uint16_t), kMaxEntries, &kModkitArraySize, kAnchors, 0, nullptr, 0, kGameReferences };
	}

	void InitModkitLimit()
	{
		RelocationPlan plan;
		if (FindRelocation(kSpec, plan))
			ApplyRelocation(kSpec, plan, {}, {}, "");
	}
}
