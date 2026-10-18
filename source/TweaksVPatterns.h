// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - every byte pattern TweaksV looks for in GTA5_Enhanced.exe, defined once in kPatterns with
// the label PATTERNS.md gives it. After a game update this table is the one place in the code to
// change, and PATTERNS.md the one in the docs; the tests check that both list the same patterns. The
// code that uses a pattern names it with Pattern("label"), which is checked when TweaksV is compiled.
#pragma once

#include <string_view>

namespace TweaksV
{
	struct GamePattern
	{
		const char* label;	// as in PATTERNS.md: H- Heap, M- Modkit, P- Packfile, R- RageOpenV, S- StraightToStoryMode
		const char* text;	// the bytes in hex, one space apart; ? matches any byte
	};

	// In the order of PATTERNS.md; what each one finds is described there.
	inline constexpr GamePattern kPatterns[] = {
		// HeapAdjusterEnhanced: the heap size
		{ "H-E1", "BA ? ? ? ? 45 31 C0 41 B1 01 E8 ? ? ? ? 48 8D 0D" },

		// ModkitLimitAdjusterEnhanced: the references to the modkit array, then its hard-coded limits
		{ "M-E-R1", "31 c9 48 8d 05 ? ? ? ? eb" },
		{ "M-E-R2", "89 c2 48 8d 0d ? ? ? ? eb" },
		{ "M-E-R3", "41 0f b7 c2 48 8d 0d ? ? ? ? 0f b7 04 41" },
		{ "M-E-R4", "57 48 83 ec ? 48 8d 0d ? ? ? ? 41 b8" },
		{ "M-E-R5", "48 8d 3d ? ? ? ? 0f b7 3c 47" },
		{ "M-E-R6", "77 ? 48 8d 15 ? ? ? ? 0f b7 14 42" },
		{ "M-E-L1", "0f b7 c0 66 ba ? ? 3d ? ? ? ? 77" },
		{ "M-E-L2", "48 83 c5 ? 48 83 c0 ? 48 83 c7" },
		{ "M-E-L3", "74 ? 48 83 c5 ? 48 83 c3 ? 48 83 c0 ? 48 3d" },
		{ "M-E-L4", "48 83 c7 ? 48 83 c2 ? 48 83 c5 ? 48 81 fd" },
		{ "M-E-L5", "48 83 c7 ? 48 83 c2 ? 48 83 c6 ? 48 81 fe" },
		{ "M-E-L6", "41 b8 ? ? ? ? b2 ? e8 ? ? ? ? 66 0f 76 c0" },
		{ "M-E-L7", "0f b7 41 ? 66 bf ? ? 48 3d" },
		{ "M-E-L8", "41 0f b7 ca 66 b8 ? ? 81 f9" },

		// PackfileLimitAdjusterEnhanced: the direct size (one pattern per range of game builds), the
		// indirect size, the streaming configuration call, then the references to the packfile array -
		// the last one again per range of game builds
		{ "P-E-D3", "48 83 C6 ? 48 81 F9" },
		{ "P-E-D2", "49 83 c3 ? 48 81 f9 ? ? ? ? 75 ? eb" },
		{ "P-E-D1", "48 81 f9 ? ? ? ? 74 ? 48 83 3c ca 00" },
		{ "P-E-I1", "3b 05 ? ? ? ? 0f 8d ? ? ? ? 89 c0" },
		{ "P-E-H1", "89 48 ? 48 8d 0d ? ? ? ? 48 89 08 48 89 c1 e8" },
		{ "P-E-R01", "48 8d 15 ? ? ? ? 48 8b 3c ca 48 8b 2f" },
		{ "P-E-R02", "48 8d 3d ? ? ? ? 48 8b 0c cf 48 85 c9" },
		{ "P-E-R03", "48 8d 3d ? ? ? ? 48 8b 0c 39 48 85 c9" },
		{ "P-E-R04", "89 c0 48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 74" },
		{ "P-E-R05", "89 c0 48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 0f 84" },
		{ "P-E-R06", "48 8d 0d ? ? ? ? 4a 8b 0c c9 48 85 c9 0f 84 ? ? ? ? 48 8b 01" },
		{ "P-E-R07", "48 8d 0d ? ? ? ? 48 8b 1c 08 48 8b 03" },
		{ "P-E-R08", "48 8d 0d ? ? ? ? 4a 8b 0c c1 48 85 c9" },
		{ "P-E-R09", "48 83 ec ? 48 89 ce 48 8d 05 ? ? ? ? 48 89 01 c6 81" },
		{ "P-E-R10", "48 c1 e8 ? 48 8d 0d ? ? ? ? 48 8b 0c c1 48 8b 01" },
		{ "P-E-R11", "48 8d 0d ? ? ? ? 48 83 3c c1 ? 40 0f 95 c6 eb" },
		{ "P-E-R12", "48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 ff 90 ? ? ? ? 48 8b 48" },
		{ "P-E-R13", "48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 4c 8d 44 24" },
		{ "P-E-R14", "48 8d 0d ? ? ? ? 4c 8b 24 08 49 8b 04 24 4c 8d 44 24" },
		{ "P-E-R15", "48 8d 0d ? ? ? ? 48 8b 0c 08 48 85 c9 75" },
		{ "P-E-R16", "48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 0f 84 ? ? ? ? 48 89 f5" },
		{ "P-E-R17", "48 83 ec ? 89 d7 48 89 ce 48 8d 05 ? ? ? ? 48 89 01 c6 81 c1" },
		{ "P-E-R18", "48 8d 0d ? ? ? ? 48 8b 3c 08 48 8b 07 48 89 f9" },
		{ "P-E-R19", "48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 ff 90 ? ? ? ? 48 8b 40" },
		{ "P-E-R20", "48 8d 0d ? ? ? ? 48 8b 0c d1 48 85 c9 75" },
		{ "P-E-R21", "48 8d 0d ? ? ? ? 4c 8b 24 c1 4d 85 e4 0f 84" },
		{ "P-E-R22", "48 8d 0d ? ? ? ? 48 8b 0c c1 48 8b 01 ff 90 ? ? ? ? 48 8b 38" },
		{ "P-E-R23", "48 89 2d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 48 c7 44 08" },
		{ "P-E-R24", "48 89 35 ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 0f 1f 44 00 00" },
		{ "P-E-R25", "48 89 3d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 66 90" },
		{ "P-E-R26", "48 89 35 ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 66 66 66 2e 0f 1f 84 00 00 00 00 00" },
		{ "P-E-R27", "48 89 2d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 0f 1f 84 00 00 00 00 00" },
		{ "P-E-R28", "48 89 3d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 66 66 66 66 2e 0f 1f 84 00 00 00 00 00" },
		{ "P-E-R29", "48 89 3d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 0f 1f 40 00" },
		{ "P-E-R30", "48 89 35 ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 48 c7 44 08" },
		{ "P-E-R31a", "BF 08 00 00 00 BE 10 00 00 00 4C 8D 05" },
		{ "P-E-R31b", "41 ? ? ? ? ? ? ? ? ? ? 41 bb ? ? ? ? ? 8d" },

		// RageOpenV: the console, the RPF cache check, the 8 file functions, the 5 encryption calls, the
		// newmods mount call and archive function, and the game functions the newmods devices call
		{ "R-E-C1", "ff 15 ? ? ? ? 48 85 c0 74 ? 48 89 c1 31 d2" },
		{ "R-E-K1", "e8 ? ? ? ? 48 8d 0d ? ? ? ? 48 8d 15 ? ? ? ? 48 8d 35" },
		{ "R-E-F1", "41 56 56 57 53 48 81 ec ? ? ? ? 4c 89 c7 66 c7 ? ? ? ? ? 4c 8d 74 24" },
		{ "R-E-F2", "66 c7 01 ? ? 29 d9 d1 e9 66 89 4c 24 ? 31 ff 4c 8d 44 ? ? 48 89 d9 31 d2 ff 15 ? ? ? ? 85 c0" },
		{ "R-E-F3", "66 c7 01 ? ? 29 d9 d1 e9 66 89 4c 24 ? 31 ff 4c 8d 44 24 ? 48 89 d9 31 d2 ff 15 ? ? ? ? 8b 4c 24" },
		{ "R-E-F4", "41 56 56 57 53 48 81 ec ? ? ? ? 66 c7 ? ? ? ? ? 4c 8d 74 24" },
		{ "R-E-F5", "41 57 41 56 56 57 53 48 81 ec 60 02 00 00 48 89 d3 48 89 cf" },
		{ "R-E-F6", "66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 ? ? ? ? 85 c0 74 14 8b 44 24 64 8b 4c 24 68 48 c1 e0 20 48 09 c1 48 89 4c 24 38" },
		{ "R-E-F7", "66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 ? ? ? ? 85 c0 74 0a 48 8b 44 24 5c 48 89 44 24 38" },
		{ "R-E-F8", "56 57 53 48 81 ec 60 02 00 00 48 89 d7 48 89 ce c7 44 24 3c ff ff ff ff 48 89 d1 ba 3a 00 00 00" },
		{ "R-E-E1", "e8 ? ? ? ? 80 7c 24 2e ? 74 ? 48 8b 56" },
		{ "R-E-E2", "e8 ? ? ? ? 8b 46 ? 85 c0 74 ? b9" },
		{ "R-E-E3", "e8 ? ? ? ? 48 8b 46 ? 66 c7 00 00 ? eb" },
		{ "R-E-E4", "e8 ? ? ? ? 48 8b 46 ? 8b 4c 24 ? 48 8d 14 08" },
		{ "R-E-E5", "c6 86 ? ? ? ? ? 48 89 f1 4c 89 fa 45 89 f0 45 31 c9" },
		{ "R-E-M0", "41 57 41 56 41 55 41 54 56 57 55 53 48 81 ec ? ? ? ? 89 d6 49 89 ce" },
		{ "R-E-M1", "e8 ? ? ? ? 48 8d 05 ? ? ? ? 66 c7 44 58 02 00 00 66 89 3d" },
		{ "R-E-M2", "41 57 41 56 41 55 41 54 56 57 55 53 48 81 ec ? ? ? ? 45 89 cd 45 89 c6 49 89 d7 48 89 ce 8b 05" },
		{ "R-E-D1", "56 57 53 48 81 ec ? ? ? ? 48 83 79 ? ? 0f 84 ? ? ? ? 48 89 d7" },
		{ "R-E-D2", "56 57 53 48 81 ec ? ? ? ? 4c 89 cf 44 89 c3" },

		// StraightToStoryMode: the splash screen, the legal screen, the landing page
		{ "S-E1", "0F 85 A9 00 00 00 48 8D 54" },
		{ "S-E2", "00 E9 ? 05 00 00 E8 ? 07" },
		{ "S-E3", "E8 ? ? ? ? 8B 0D ? ? ? ? 83 C1 ? 84 C0" },
	};

	// A text the scanner can use: bytes as two hex digits or ?, one space apart, at least one of them
	// not ? (a pattern of only ? would match anywhere).
	consteval bool IsPatternText(std::string_view text)
	{
		const auto hex = [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); };
		bool aByte = false;
		size_t i = 0;
		while (i < text.size())
		{
			if (text[i] == '?')
				i += 1;
			else if (i + 1 < text.size() && hex(text[i]) && hex(text[i + 1]))
			{
				aByte = true;
				i += 2;
			}
			else
				return false;
			if (i < text.size() && (text[i] != ' ' || i + 1 == text.size()))
				return false;
			i += 1;
		}
		return aByte;
	}

	consteval bool PatternsAreValid()
	{
		for (size_t i = 0; i < std::size(kPatterns); i++)
		{
			if (!IsPatternText(kPatterns[i].text))
				return false;
			for (size_t j = 0; j < i; j++)
				if (std::string_view(kPatterns[i].label) == kPatterns[j].label)
					return false;
		}
		return true;
	}
	static_assert(PatternsAreValid(), "TweaksV: a pattern in kPatterns is malformed, or a label is used twice");

	// The entry with this label - checked when TweaksV is compiled: a label that is not in kPatterns
	// does not build.
	consteval const GamePattern& Pattern(std::string_view label)
	{
		for (const GamePattern& pattern : kPatterns)
			if (label == pattern.label)
				return pattern;
		throw "TweaksV: this pattern is not in kPatterns";
	}
}
