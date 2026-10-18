// TweaksV: carried over from RageOpenV (src/utils/rpf.h) by Chiheb-Bacha, based on ClosedIV by
// martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV: namespace, layout checks, unused helpers removed; every change is marked "TweaksV:".
#pragma once

namespace RageOpenV // TweaksV: one namespace per original plugin
{

struct DirectoryEntry
{
	uint32_t nameOffset;
	uint32_t entryType;
	uint32_t entryIndex;
	uint32_t entriesCnt;
};

struct BinaryEntry
{
	uint16_t nameOffset;
	uint8_t fileSize[3];
	uint8_t fileOffset[3];
	uint32_t realSize;
	uint32_t isEncrypted;

	// TweaksV: GetFileSize(), GetFileOffset() and IsCompressed() removed - never used
};

struct ResourceEntry
{
	uint16_t nameOffset;
	uint8_t fileSize[3];
	uint8_t fileOffset[3];
	uint32_t systemFlags;
	uint32_t graphicsFlags;

	// TweaksV: GetFileSize() and GetFileOffset() removed - never used
};

struct Entry
{
	union
	{
		DirectoryEntry dir;
		BinaryEntry bin;
		ResourceEntry res;
	};

	bool IsDirectory()
	{
		return dir.entryType == 0x7FFFFF00;
	}

	bool IsBinary()
	{
		return !IsDirectory() && (dir.entryType & 0x80000000) == 0L;
	}

	// TweaksV: IsResource() removed - never used
};

// TweaksV: every RPF7 entry is 16 bytes - the hooks walk the table in 16-byte steps
static_assert(sizeof(DirectoryEntry) == 16 && sizeof(BinaryEntry) == 16 && sizeof(ResourceEntry) == 16 &&
	sizeof(Entry) == 16, "RPF7 entries must be 16 bytes");

} // namespace RageOpenV
