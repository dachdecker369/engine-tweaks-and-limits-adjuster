// TweaksV: carried over from ModkitLimitAdjusterEnhanced, PackfileLimitAdjusterEnhanced
// and StraightToStoryMode by Chiheb-Bacha (three identical copies - this is the one
// shared copy), https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// Scanner code (C) crosire & kagikn & contributors (ScriptHookVDotNet, zlib license).
// Altered version - modified for TweaksV (see MemScanner.cpp).
#pragma once
#include <cstdint>
#include <vector>		// TweaksV: FindRipRelativeLeas
#include "TweaksVPatterns.h"	// TweaksV: the patterns

namespace MemScanner
{
    // TweaksV: the first match of one of TweaksV's patterns (TweaksVPatterns.h) in GTA5_Enhanced.exe's
    // code, or nullptr; a miss is logged with the pattern's label. Was FindPatternBmh(text), named after
    // the original's Boyer-Moore-Horspool search, which TweaksV replaced (see MemScanner.cpp). The
    // overloads with a start address or a size, and the byte+mask one, are removed: nothing used them.
    uint8_t* Find(const TweaksV::GamePattern& pattern);

    // TweaksV: every match of one of TweaksV's patterns, in address order, at most limit of them (a site
    // the game must have exactly n times is asked for n + 1 - TweaksVDecals.cpp's "add decal" check).
    // A miss is logged like Find's; DIAGNOSTIC_MODE logs the matches as it does for Find.
    std::vector<uint8_t*> FindAll(const TweaksV::GamePattern& pattern, size_t limit);

    // TweaksV: every "lea r64, [rip+disp32]" (a REX.W prefix, opcode 8D, ModRM mod 00 r/m 101: 7 bytes) in
    // the game's code that loads target, in address order, at most limit of them. For a static array the
    // game loads in places the bytes around them cannot tell apart (TweaksVRelocation.cpp): one unique
    // site gives the address, this finds every load of it. Nothing is logged.
    std::vector<uint8_t*> FindRipRelativeLeas(const void* target, size_t limit);
}
