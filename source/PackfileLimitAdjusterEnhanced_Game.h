// TweaksV: carried over from PackfileLimitAdjusterEnhanced (Game.h) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once

#include <windows.h>		// TweaksV
#include <string>
#include <vector>
#include "StubMemory.h"		// TweaksV: FindPrevFreeRegion and AllocateStubMemory are shared now

namespace PackfileLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{

struct PatternPair
{
	std::string pattern;
	int offset;
};

void initializeGame();
// TweaksV: the other game edition's patch function removed (F-PACK-4)
// TweaksV: "static void EnhancedPatch();" and "static void SetStreamingInterface(char* si);" moved
// into Game.cpp - a header must not declare static functions (B-PACK-08)
// TweaksV: the absolute-address relocation function is removed - only the other game edition used it (F-PACK-5)
void RelocateRelativePackFileArray(void* newPackFileArray, const std::vector<PatternPair>& list);
void AdjustPackFileArrayLimit();
// TweaksV: FindPrevFreeRegion and AllocateStubMemory removed - shared now, see StubMemory.h (plan 4.1)
void FatalErrorExit();

} // namespace PackfileLimitAdjusterEnhanced
