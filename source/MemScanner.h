// TweaksV: carried over from ModkitLimitAdjusterEnhanced, PackfileLimitAdjusterEnhanced
// and StraightToStoryMode by Chiheb-Bacha (three identical copies - this is the one
// shared copy), https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// Scanner code (C) crosire & kagikn & contributors (ScriptHookVDotNet, zlib license).
// Altered version - modified for TweaksV (see MemScanner.cpp).
#pragma once
#include <cstdint>
#include <string>

namespace MemScanner
{
    // TweaksV: the first match of a pattern such as "48 8D 0D ? ? ? ?" in GTA5_Enhanced.exe's code, or
    // nullptr. The name is the original's; the search is TweaksV's (see MemScanner.cpp). The overloads
    // with a start address or a size, and the byte+mask one, are removed: nothing used them.
    uint8_t* FindPatternBmh(const std::string& pattern);
}
