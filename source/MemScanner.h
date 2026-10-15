// TweaksV: carried over from ModkitLimitAdjusterEnhanced, PackfileLimitAdjusterEnhanced
// and StraightToStoryMode by Chiheb-Bacha (three identical copies - this is the one
// shared copy), https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// Scanner code (C) crosire & kagikn & contributors (ScriptHookVDotNet, zlib license).
// Altered version - modified for TweaksV (see MemScanner.cpp).
#pragma once
#include <cstdint>
#include <string>
#include <vector> // TweaksV: for the byte+mask overload below

namespace MemScanner
{
    uint8_t* FindPatternBmh(const std::string& pattern);
    uint8_t* FindPatternBmh(const std::string& pattern, uint8_t* startAddress);
    uint8_t* FindPatternBmh(const std::string& pattern, uint8_t* startAddress, uint64_t size);
    // TweaksV: the internal byte+mask overload, declared so HeapAdjuster's BytePattern can use it
    uint8_t* FindPatternBmh(const std::vector<uint8_t>& pattern, const std::string& mask, uint8_t* startAddress, uint64_t size);
}
