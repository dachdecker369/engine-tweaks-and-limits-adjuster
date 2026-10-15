/* Pattern class from https://github.com/CamxxCore/PauseMenuHelper */
// TweaksV: carried over from HeapAdjusterEnhanced (Heap Adjuster/pattern.h) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/HeapAdjusterEnhanced; the Pattern class is from CamxxCore's
// PauseMenuHelper (MIT). Modified for TweaksV: the search is done by the shared
// MemScanner; every change is marked "TweaksV:".

#pragma once

#include <Psapi.h>
#include <cstdio>        // TweaksV: snprintf
#include <string>        // TweaksV
#include "MemScanner.h"  // TweaksV: one scanner for everything

namespace HeapAdjusterEnhanced // TweaksV: one namespace per original plugin
{

template <typename T>
class Pattern
{
public:
	Pattern(const BYTE* bMask, const char* szMask) : bMask(bMask), szMask(szMask)
	{
		bSuccess = findPattern();
	}

	T get(int offset = 0)
	{
		return pResult + offset;
	}

	bool bSuccess;

private:
	bool findPattern()
	{
		// TweaksV: the search is done by the shared MemScanner. The byte string and its
		// "x?" mask become MemScanner's text form (e.g. "BA ? ? ? ? 45 31 C0"), so a miss
		// is logged and DIAGNOSTIC_MODE reports this pattern like every other one.
		std::string pattern;
		for (size_t i = 0; szMask[i]; i++)
		{
			char byteText[4];
			snprintf(byteText, sizeof(byteText), "%02X", bMask[i]);
			if (i > 0)
				pattern += ' ';
			pattern += (szMask[i] == 'x') ? byteText : "?";
		}

		auto *address = MemScanner::FindPatternBmh(pattern);
		if (address)
		{
			pResult = reinterpret_cast<T>(address);
			return true;
		}

		pResult = NULL;
		return false;
	}

	// TweaksV: bCompare removed - MemScanner does the comparison

	const BYTE *bMask; const char* szMask;
	T pResult;
};

class BytePattern : public Pattern<BYTE*>
{
public:
	BytePattern(const BYTE* bMask, const char* szMask) :
		Pattern<BYTE*>(bMask, szMask) {}
};

} // namespace HeapAdjusterEnhanced
