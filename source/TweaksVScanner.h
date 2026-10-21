// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the scanner: finds TweaksV's patterns (TweaksVPatterns.h) in the running game's code, and
// every load of one address. The pattern parser is an altered version of ScriptHookVDotNet's memory
// scanner (crosire, kagikn and contributors; the C++ port by Chiheb-Bacha), zlib license - the notice
// is in TweaksVScanner.cpp and THIRD_PARTY_NOTICES.md. The search itself is TweaksV's own.
#pragma once

#include <cstdint>
#include <vector>

#include "TweaksVPatterns.h"

namespace TweaksV
{
	namespace Scanner
	{
		// The first match of a pattern in GTA5_Enhanced.exe's code, or nullptr; a miss is logged with the
		// pattern's label.
		uint8_t* Find(const GamePattern& pattern);

		// Every match of a pattern, in address order, at most limit of them (a site the game must have
		// exactly n times is asked for n + 1 - the decals part's "add decal" check). A miss is logged like
		// Find's; DIAGNOSTIC_MODE logs the matches as it does for Find.
		std::vector<uint8_t*> FindAll(const GamePattern& pattern, size_t limit);

		// Every "lea r64, [rip+disp32]" (a REX.W prefix, opcode 8D, ModRM mod 00 r/m 101: 7 bytes) in the
		// game's code that loads target, in address order, at most limit of them. For a static array the
		// game loads in places the bytes around them cannot tell apart (the relocation core): one unique
		// site gives the address, this finds every load of it. Nothing is logged.
		std::vector<uint8_t*> FindRipRelativeLeas(const void* target, size_t limit);
	}
}
