// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - test builds only (-p:TweaksVTestBuild=true): TEST_DUMP_CODE=<seconds> under [TWEAKSV] saves
// the game's code as it is in memory - decrypted, unlike GTA5_Enhanced.exe on disk - so that patterns
// can be looked for after a game update. GameCode-start.bin is taken before any part of TweaksV changes
// the game, GameCode-later.bin that many seconds later. Saved are the PE headers and every section the
// game does not write to - its code, constants and function table - never one it writes to, which can
// hold the player's data. Both files go to %LOCALAPPDATA%\TweaksV, never the game folder, and only if
// they are what their path says (TweaksVFiles.h).
//
// The file (format version 1, little-endian): a DumpHeader, one DumpSection per section, then the bytes
// of each section at its fileOffset. Bytes that could not be read are saved as zeros and counted.
#pragma once

#include <cstdint>

namespace TweaksV
{
	struct DumpHeader
	{
		char magic[8];				// "TWKVDUMP"
		uint32_t formatVersion;		// 1
		uint32_t sectionCount;
		uint64_t imageBase;			// where the game was in memory
		uint32_t imageSize;			// its SizeOfImage
		uint32_t timeDateStamp;		// its PE timestamp - the game build
		uint32_t secondsAfterStart;	// 0 for GameCode-start.bin
		uint32_t reserved;			// 0
	};
	static_assert(sizeof(DumpHeader) == 40);

	struct DumpSection
	{
		char name[8];				// as in the game's section table; "HEADERS" for the PE headers
		uint32_t rva;
		uint32_t size;
		uint32_t characteristics;	// the section's flags; 0 for the headers
		uint32_t unreadable;		// bytes that could not be read, saved as zeros
		uint64_t fileOffset;		// where its bytes start in this file
	};
	static_assert(sizeof(DumpSection) == 32);

	// Takes the first copy and starts the thread that saves it and takes the second. Does nothing if
	// TEST_DUMP_CODE is not set; a value that is not a number of seconds from 1 to 3600 is a problem.
	// DllMain calls it once, before StartFeatures(). Test builds only.
	void StartCodeDump();
}
