// TweaksV: carried over from PackfileLimitAdjusterEnhanced (Game.h) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once

#include <windows.h>		// TweaksV
#include <vector>
#include "TweaksVPatterns.h"	// TweaksV: the patterns; <string> removed - unused now
#include "StubMemory.h"		// TweaksV: FindPrevFreeRegion and AllocateStubMemory are shared now

namespace PackfileLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{

struct PatternPair
{
	const TweaksV::GamePattern* pattern;	// TweaksV: an entry of TweaksVPatterns.h - was std::string
	int offset;
};

void initializeGame();
// TweaksV: the other game edition's patch function removed
// TweaksV: "static void EnhancedPatch();" and "static void SetStreamingInterface(char* si);" moved
// into Game.cpp - a header must not declare static functions
// TweaksV: the absolute-address relocation function is removed - only the other game edition used it
void RelocateRelativePackFileArray(void* newPackFileArray, const std::vector<PatternPair>& list);
void AdjustPackFileArrayLimit();
// TweaksV: FindPrevFreeRegion and AllocateStubMemory removed - shared now, see StubMemory.h
void FatalErrorExit();

} // namespace PackfileLimitAdjusterEnhanced
