// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the game's script programs: the hook where the game places one, and the layout of one. A script
// program is one of the game's compiled scripts (blip_controller, ...) as the game holds it in memory: its
// code and its strings in pages of 16 KB, its table of natives - the game's functions it calls - and its
// name. The game makes one in the program's resource constructor whenever it streams the script in, on its
// resource placement thread; the constructor calls the game's function that resolves the program's natives
// (Y-E-H1), and only afterwards enters the program in the game's registry of programs, where every script
// thread looks its program up each time it runs - so no script thread can run a program while the hook
// looks at it.
// This module owns the one redirect of that call, for every part of TweaksV that works on a script program:
// the call goes through a call stub to TweaksV, which hands the program to the module's users before and
// after the game resolves its natives. Its users: the test builds' TEST_DUMP_SCRIPT (TweaksVDump.h), which
// saves a program, and the Blips part (TweaksVBlips.h), which raises blip_controller's threshold. The facts
// are in PATTERNS.md, "Script programs".
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace TweaksV
{
	namespace Scripts
	{
		// A script program's fields, counted from its start, each with the pattern that pins it - checked when
		// TweaksV is compiled (TweaksVScripts.cpp) - or none; the ones no pattern pins are on PATTERNS.md's list
		// of facts to check after a game update.
		inline constexpr size_t kCodePagesOffset = 0x10;	// the code's page table: one 8-byte pointer per page (Y-E-I1, +3)
		inline constexpr size_t kCodeLengthOffset = 0x1C;	// the code's length in bytes, a dword (Y-E-N1, +4)
		inline constexpr size_t kNativeCountOffset = 0x2C;	// how many natives the program calls, a dword (no pattern)
		inline constexpr size_t kNativesOffset = 0x40;		// the native table, 8 bytes per native (Y-E-N1, +11)
		inline constexpr size_t kNameHashOffset = 0x58;		// the hash of the program's name, a dword (NameHash; no pattern)
		inline constexpr size_t kNameOffset = 0x60;			// the name: a pointer to a zero-terminated string (no pattern)
		inline constexpr size_t kStringPagesOffset = 0x68;	// the strings' page table, paged as the code is (no pattern)
		inline constexpr size_t kStringLengthOffset = 0x70;	// the strings' length in bytes, a dword (Y-E-H1, +18)
		inline constexpr size_t kProgramSize = 0x80;		// a program object is at least this long

		// The bounds a program's fields are read within: the code and the strings 16 MB each - a call's
		// target in the code is 24 bits - and 65536 natives - a native's index is 16 bits.
		inline constexpr uint32_t kMaxLength = 16u << 20;
		inline constexpr uint32_t kMaxNatives = 65536;

		// The code and the strings lie in pages of kPageSize bytes: a length gives (length + 0x3FFF) >> 14
		// pages, every one full but the last, which holds length & 0x3FFF bytes - 0x4000 when that is 0. The
		// rounding, kPageSize - 1, is pinned by Y-E-H1 (+10, "mov r14d, 0x3FFF").
		inline constexpr uint32_t kPageSize = 0x4000;
		constexpr uint32_t PageCount(uint32_t length)
		{
			return static_cast<uint32_t>((uint64_t{ length } + kPageSize - 1) >> 14);
		}
		constexpr uint32_t PageLength(uint32_t length, uint32_t page)	// 0 for a page past the last
		{
			const uint32_t pages = PageCount(length);
			if (page + 1 < pages)
				return kPageSize;
			if (page + 1 > pages)
				return 0;
			return (length & (kPageSize - 1)) ? (length & (kPageSize - 1)) : kPageSize;
		}

		// The game's hash of a script's name, the one a program holds at kNameHashOffset. Over the name's bytes
		// - a leading double quote skipped, the name then ending at the next one; A to Z lowered and '\' taken as
		// '/'; each byte sign-extended - h = (h + c) * 0x401 and h ^= h >> 6, all in 32 bits; then h *= 9,
		// h ^= h >> 11, h *= 0x8001; a result below 2 gets 2 added.
		constexpr uint32_t NameHash(std::string_view name)
		{
			size_t at = 0;
			size_t end = name.size();
			if (end > 0 && name[0] == '"')
			{
				at = 1;
				for (size_t quote = 1; quote < name.size(); quote++)
					if (name[quote] == '"')
					{
						end = quote;
						break;
					}
			}
			uint32_t hash = 0;
			for (; at < end; at++)
			{
				char c = name[at];
				if (c >= 'A' && c <= 'Z')
					c = static_cast<char>(c - 'A' + 'a');
				else if (c == '\\')
					c = '/';
				hash += static_cast<uint32_t>(static_cast<int32_t>(static_cast<signed char>(c)));
				hash *= 0x401;
				hash ^= hash >> 6;
			}
			hash *= 9;
			hash ^= hash >> 11;
			hash *= 0x8001;
			return hash < 2 ? hash + 2 : hash;
		}
		static_assert(NameHash("blip_controller") == 0x80CFE974, "TweaksV: the name hash must be the game's");

		// A field of a program - or of a table it points to - as it lies in memory, read only once IsReadable
		// (TweaksVPatch.h) has said the bytes can be read.
		template <typename T> T Field(const void* at, size_t offset)
		{
			T value{};
			memcpy(&value, static_cast<const uint8_t*>(at) + offset, sizeof(value));
			return value;
		}

		// A program's name - the string its field at kNameOffset points to - is read within this many bytes, the
		// terminating zero included.
		inline constexpr size_t kNameRead = 64;

		// The zero-terminated name at name into out, zero-padded, read within kNameRead bytes and only where it can
		// be read, a memory page at a time; false, and out all zeros, if it cannot be read or does not end within
		// them.
		bool ReadName(const char* name, char (&out)[kNameRead]);

		// A name ReadName read, for a log line: a byte that cannot be printed becomes '?'.
		std::string Printable(const char* name);

		// A user of the hook. before runs for every script program the game places, before the game resolves
		// its natives - the native table still holds their hashes - with how many programs the hook has seen
		// so far, this one included; what it returns comes back to after, which runs once the game has
		// resolved them, with what the game's function returned (1: every native found). Both run on the
		// game's thread that places the program, each inside a try, with the game's last error kept around
		// them; either may be null. tag is the user's log tag, which the module's lines carry too.
		struct User
		{
			const char* tag;
			void* (*before)(uint8_t* program, uint32_t placed);
			void (*after)(uint8_t* program, uint8_t resolved, void* context);
		};

		// Adds user to the hook - from DllMain only, into a fixed table: no allocation and no lock on the
		// game's thread for it. The first user makes the hook: Y-E-H1, Y-E-N1 and Y-E-I1 found, the call
		// checked - a call into the game, otherwise another mod may have hooked it already - and redirected
		// (RedirectCalls: DRY_RUN only logs that it would); DIAGNOSTIC_MODE logs what each site holds, under
		// user's tag. The outcome is kept: a later user joins a hook that is made, and gets the same reason as
		// the first when it could not be made. False, with reason - for the user's own PROBLEM line - when
		// the hook is not there.
		bool AddUser(const User& user, std::string& reason);
	}
}
