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
//
// TEST_DUMP_SCRIPT=<name> saves one of the game's script programs as the game places it in memory - the
// archives store the scripts encrypted - so that a script's code can be read: each time the game places the
// program of that name (1 to 63 letters, digits and _, compared without case), on its resource placement
// thread, through the hook of TweaksVScripts.h, at most 16 times a session. The file goes to
// %LOCALAPPDATA%\TweaksV\Script-<name>-<n>.bin, the name in lower case and n counting this program's
// placements from 1, never to the game folder, and only if it is what its path says.
//
// The file (format version 1, little-endian), a layout a reader is written against: a ScriptDumpHeader,
// then the program object's first 0x80 bytes, its code page after page, its native table before the game
// resolves it (the natives' hashes) and after (their handlers' addresses), and its strings page after
// page, each block at its offset. Bytes that could not be read are saved as zeros and counted.
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

	struct ScriptDumpHeader
	{
		char magic[8];					// "TWKVSCRP"
		uint32_t formatVersion;			// 1
		uint32_t headerSize;			// 176
		char name[64];					// the program's name as the game holds it, zero-padded
		uint32_t nameHash;				// +0x58
		uint32_t codeLength;			// +0x1C
		uint32_t nativeCount;			// +0x2C
		uint32_t stringLength;			// +0x70
		uint32_t placement;				// 1 for this program's first placement this session
		uint32_t threadId;				// the thread that placed it
		uint32_t programsPlaced;		// every program the hook saw so far, this one included
		uint8_t resolved;				// what the game's resolution returned: 1 = every native found
		uint8_t reserved1[3];			// 0
		uint64_t imageBase;				// where the game was in memory
		uint32_t imageSize;				// its SizeOfImage
		uint32_t timeDateStamp;			// its PE timestamp - the game build
		uint32_t unreadable;			// bytes that could not be read, saved as zeros
		uint32_t reserved2;				// 0
		uint64_t programOffset;			// 0x80 bytes: the program object's first 0x80 bytes, as they are
		uint64_t codeOffset;			// codeLength bytes, page after page
		uint64_t nativesBeforeOffset;	// nativeCount x 8: the table before the game's resolution - the hashes
		uint64_t nativesAfterOffset;	// nativeCount x 8: the table after it - the handlers' addresses, as they are
		uint64_t stringsOffset;			// stringLength bytes, page after page
	};
	static_assert(sizeof(ScriptDumpHeader) == 176);

	// Takes the first copy and starts the thread that saves it and takes the second. Does nothing if
	// TEST_DUMP_CODE is not set; a value that is not a number of seconds from 1 to 3600 is a problem.
	// DllMain calls it once, before StartFeatures(). Test builds only.
	void StartCodeDump();

	// Joins the script programs' hook (TweaksVScripts.h) for TEST_DUMP_SCRIPT. Does nothing if it is not
	// set; a value that is not a script's name, and a hook that cannot be made, are problems, and nothing is
	// hooked. DllMain calls it once, after StartCodeDump(). Test builds only.
	void StartScriptDump();
}
