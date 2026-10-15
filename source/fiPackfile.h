// TweaksV: carried over from RageOpenV (src/gtav/rage/fiPackfile.h) by Chiheb-Bacha, based on
// ClosedIV by martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV: namespace and layout checks only; marked "TweaksV:".
#pragma once
#include <cstddef>	// TweaksV: offsetof
#include "fiDevice.h"

namespace RageOpenV // TweaksV: one namespace per original plugin
{
namespace rage
{
	// fields are offset by an extra 0x8 in enhanced, but we will adjust the pointer at runtime
	class fiPackfile : public fiDevice
	{
	public:
		virtual ~fiPackfile() = default;

		char pad_0x0008[0x8];
		char* headers;
		char* entriesDepth;
		char* entryTable;
		__int32 filesCount;
		char pad_0x002C[4];
		HANDLE openedFileHandle;
		char fileTime[8];
		__int64 currentBulkOffset;
		__int32 hashSalt;
		char pad_0x004C[4];
		void* relativeDevice;
		char pad_0x0058[0x28];
		char* packfileName;
		char pad_0x0088[0x8];
		char isBinaryEntry;
		char uint8_0x91;
		char pad_0x0092[2];
		__int32 physSortKey;
		char pad_0x0098[2];
		__int64 memoryPtr;
		char pad_0x00000[4];
		char packFileOpened;
		char pad_0x00A9[2];
		char uint8_0xAB;
		char nameTableStart[4];
		__int32 currentFileOffset;
		__int32 encryptionMagic;
		char uint8_0xB8;
		char pad_0x00B9[71];
	};

	// TweaksV: the code works with the COMPILED offsets of the three fields it uses (some field
	// names above do not match them, because memoryPtr is 8-byte aligned) - pinned so no change
	// can move them (plan A.6, verified with MSVC 19.51)
	static_assert(offsetof(fiPackfile, entryTable) == 0x20, "fiPackfile::entryTable moved");
	static_assert(offsetof(fiPackfile, filesCount) == 0x28, "fiPackfile::filesCount moved");
	static_assert(offsetof(fiPackfile, currentFileOffset) == 0xB4, "fiPackfile::currentFileOffset moved");
}
} // namespace RageOpenV
