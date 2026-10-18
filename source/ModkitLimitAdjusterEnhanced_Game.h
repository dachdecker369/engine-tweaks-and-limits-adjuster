// TweaksV: carried over from ModkitLimitAdjusterEnhanced (Game.h) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once

#include <windows.h>		// TweaksV
#include <vector>
#include "TweaksVPatterns.h"	// TweaksV: the patterns; <string> removed - unused now
#include "StubMemory.h"		// TweaksV: FindPrevFreeRegion and AllocateStubMemory are shared now

namespace ModkitLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{

struct PatternPair
{
	const TweaksV::GamePattern* pattern;	// TweaksV: an entry of TweaksVPatterns.h - was std::string
	int offset;
};

struct PatternPairExtended
{
	const TweaksV::GamePattern* pattern;	// TweaksV: an entry of TweaksVPatterns.h - was std::string
	int offset;
	int mulModifier;
	int addModifier;
};

void initializeGame();
// TweaksV: the absolute-address relocation function is removed - only the other game edition used it
void RelocateRelativeModkitArray(void* newModkitArray, const std::vector<PatternPair>& list);
void AdjustModkitArrayLimit();
// TweaksV: FindPrevFreeRegion and AllocateStubMemory removed - shared now, see StubMemory.h
void FatalErrorExit();
void patchHardcodedArrayLimits();

} // namespace ModkitLimitAdjusterEnhanced
