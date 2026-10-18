// TweaksV: carried over from ModkitLimitAdjusterEnhanced, PackfileLimitAdjusterEnhanced
// and StraightToStoryMode by Chiheb-Bacha (three identical copies - this is the one
// shared copy), https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// Scanner code (C) crosire & kagikn & contributors (ScriptHookVDotNet, zlib license).
// Altered version - modified for TweaksV (see MemScanner.cpp).
#pragma once
#include <cstdint>
#include "TweaksVPatterns.h"	// TweaksV: the patterns

namespace MemScanner
{
    // TweaksV: the first match of one of TweaksV's patterns (TweaksVPatterns.h) in GTA5_Enhanced.exe's
    // code, or nullptr; a miss is logged with the pattern's label. Was FindPatternBmh(text), named after
    // the original's Boyer-Moore-Horspool search, which TweaksV replaced (see MemScanner.cpp). The
    // overloads with a start address or a size, and the byte+mask one, are removed: nothing used them.
    uint8_t* Find(const TweaksV::GamePattern& pattern);
}
