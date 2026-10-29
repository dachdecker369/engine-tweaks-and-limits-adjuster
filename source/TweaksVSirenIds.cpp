// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the siren setting IDs, measured first (see TweaksVSirenIds.h).
//
// A siren setting of carcols.meta - the lights and the pattern of an emergency vehicle's sirens - carries an ID, and a
// vehicle's variation in carvariations.meta names its siren setting by that ID. On the whole load path the ID is one
// byte: the parser keeps a larger number's low byte, and a table of 256 bytes maps an ID to the setting's position in
// the game's array, a byte too, in which index 0 means no sirens. So the game has 256 IDs and 255 settings a vehicle
// can use, and it binds silently: a pack's setting whose ID is in use is dropped, a clash within one file moves the
// later setting to the first free ID, and a setting placed past position 255 is reached as another one. The
// SirenSettings part to come widens the IDs to 16 bits; first the limit is measured:
//   DIAGNOSTIC_MODE  the one switch this module reads, through the table of the parts measured first: with it on, the
//                    module finds the limit's 14 patterns - its measurement's and those the part's writing half will
//                    use - checks every site as that writing half will and logs it, and starts the measurement: 10
//                    seconds in and then every minute while the numbers change, the siren settings loaded against the
//                    255 a vehicle can use, the IDs in use against the table's 256, the settings past position 255 and
//                    the IDs given to more than one setting. Without it nothing is looked for; DRY_RUN changes nothing
//                    here, since nothing is written.
// The sites: the parser registrations of the two members that carry an ID - the siren setting's member 0 at +0x00, the
// variation's member 6 at +0x4B - with their offsets, and their member data's offset field and type, read in the game's
// data and never written, logged at the start and again at the first summary: the game may register its structures after
// TweaksV has started, and a type other than a byte's (0x01 or 0x02) is a finding for the writing half that stops
// nothing; the simple member's XML reader, which stores a byte or a word by that type; the four
// lea of the ID table, which a scan of the game's code must find exactly; in the loaders, the nine byte loads and the
// two byte stores of an ID, the four checks for "none" (0xFF), the two IDs given when none is free, the two IDs stored
// into a variation and the four free-ID searches' limits of 256; a pack's dedupe and its compare loop; the three
// constructors that give an ID its default; the vehicle metadata's init, its last store into the table and its call of
// the base loader; and the getter of a model's siren setting, for the measurement. Every site is found and checked
// before the measurement starts; the first that is not as the findings have it is logged, and nothing is measured - an
// aid, not a problem (NOTES.md, T-DIAG). The measurement reads the game's data without a lock and writes nothing: the
// carcols data's siren settings through CopyMemorySafely, since their array grows while packs load, and the ID table
// after IsReadable.
// Written for TweaksV from the findings of its own research in the game's code; no code of anyone's was read. The
// part's idea and its credit come with its writing half (CREDITS.md).

#include "TweaksVSirenIds.h"

#include <windows.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVMeasure.h"
#include "TweaksVPatch.h"
#include "TweaksVPatterns.h"
#include "TweaksVScanner.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "SirenSettings";

		// ---- the game's data (PATTERNS.md, "SirenSettings")

		constexpr size_t kIds = 256;				// the ID table: per ID the position of the setting that has it, a byte; kNone when free
		constexpr uint8_t kNone = 0xFF;				// no ID given, and a free entry of the table
		constexpr size_t kPositions = 256;			// the positions an index reaches - it is a byte
		constexpr size_t kUsable = kPositions - 1;	// and the ones a vehicle can use: index 0 means no sirens
		constexpr size_t kSettingSize = 0x5E8;		// a siren setting, its ID the byte at +0x00
		constexpr size_t kSettingsAt = 0x48;		// in the carcols data: the siren settings' array, then their count and capacity
		constexpr size_t kTableLastFill = 0xF0;		// the vehicle metadata's init fills the table 16 bytes at a time, the last here
		constexpr size_t kMemberOffsetAt = 0x08;	// in a member's data: its offset, written when the parser registers its structure
		constexpr size_t kMemberTypeAt = 0x10;		// and its type: 0x01 or 0x02 store a byte, 0x03 or 0x04 a word

		// ---- the patterns, in the order of PATTERNS.md

		enum Match : size_t { kP1, kP2, kP3, kT1, kT2, kT3, kT4, kD1, kD2, kC1, kC2, kC3, kR1, kM1, kMatchCount };
		constexpr const GamePattern* kLimitPatterns[kMatchCount] = { &Pattern("I-E-P1"), &Pattern("I-E-P2"), &Pattern("I-E-P3"),
			&Pattern("I-E-T1"), &Pattern("I-E-T2"), &Pattern("I-E-T3"), &Pattern("I-E-T4"), &Pattern("I-E-D1"), &Pattern("I-E-D2"),
			&Pattern("I-E-C1"), &Pattern("I-E-C2"), &Pattern("I-E-C3"), &Pattern("I-E-R1"), &Pattern("I-E-M1") };

		// "0F B6 2C 0F", bytes as PATTERNS.md writes them: how many, and the one at an index
		constexpr size_t HexSize(std::string_view hex)
		{
			return (hex.size() + 1) / 3;
		}
		constexpr uint8_t HexByte(std::string_view hex, size_t index)
		{
			const auto digit = [](char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
			return static_cast<uint8_t>(digit(hex[3 * index]) * 16 + digit(hex[3 * index + 1]));
		}
		consteval bool IsHexText(std::string_view hex)
		{
			const auto isHex = [](char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); };
			if (hex.size() % 3 != 2)
				return false;
			for (size_t i = 0; i < hex.size(); i++)
				if (i % 3 == 2 ? hex[i] != ' ' : !isHex(hex[i]))
					return false;
			return true;
		}

		// The little-endian dword of four bytes of a text
		constexpr uint32_t HexDword(std::string_view hex, size_t index)
		{
			return HexByte(hex, index) | static_cast<uint32_t>(HexByte(hex, index + 1)) << 8 | static_cast<uint32_t>(HexByte(hex, index + 2)) << 16 |
				static_cast<uint32_t>(HexByte(hex, index + 3)) << 24;
		}

		// True if a match's pattern pins these bytes at this offset - for the checks, when TweaksV is compiled, that the
		// module reads a match where its pattern says what lies there
		consteval bool Pins(Match match, size_t at, std::string_view hex)
		{
			for (size_t i = 0; i < HexSize(hex); i++)
				if (PatternByte(*kLimitPatterns[match], at + i) != HexByte(hex, i))
					return false;
			return true;
		}

		// True if a match's pattern pins this one byte, a constant of the module, at this offset
		consteval bool PinsByte(Match match, size_t at, size_t value)
		{
			return PatternByte(*kLimitPatterns[match], at) == static_cast<int>(value);
		}

		constexpr size_t kNameHashAt = 6;		// P1, P2: the imm32 of "mov dword [rip+disp32], imm32" (C7 05), the structure's name hash
		constexpr size_t kMemberTableAt = 23;	// P1, P2: the rel32 of "lea rax, [rip+disp32]" (48 8D 05 at +20), the member data's pointers
		constexpr size_t kOffsetsAt = 37;		// P1, P2: the rel32 of the next "lea rax" (48 8D 05 at +34), past the match: the offsets
		static_assert(Pins(kP1, 0, "C7 05") && Pins(kP1, 20, "48 8D 05") && Pins(kP1, 34, "48 8D 05") && PatternByte(*kLimitPatterns[kP1], kOffsetsAt) == -1 &&
			Pins(kP2, 0, "C7 05") && Pins(kP2, 20, "48 8D 05") && Pins(kP2, 34, "48 8D 05") && PatternByte(*kLimitPatterns[kP2], kOffsetsAt) == -1,
			"TweaksV: I-E-P1 and I-E-P2 hold the name hash's store and the two lea of their tables, the second's rel32 past the match");
		constexpr size_t kReaderMemberAt = 3;	// P3: the member data in the member, the disp8 of "mov rax, [rcx+disp8]" (48 8B 41)
		constexpr size_t kReaderTypeAt = 7;		// P3: the type in the member data, the disp8 of "movzx ecx, byte [rax+disp8]" (0F B6 48)
		constexpr size_t kReaderTypesAt = 11;	// P3: the highest type, the imm8 of "cmp rcx, imm8" (48 83 F9), then "ja" (0F 87)
		static_assert(Pins(kP3, 0, "48 8B 41 08 0F B6 48 10 48 83 F9 21 0F 87") && PinsByte(kP3, kReaderTypeAt, kMemberTypeAt),
			"TweaksV: I-E-P3 holds the XML reader's reads of the member data and its type, at +0x10");
		constexpr size_t kTableLoads = 4;		// the lea of the ID table, T1 to T4's: every one the game has
		constexpr size_t kTableLeaAt[kTableLoads] = { 10, 23, 10, 16 };	// in T1 to T4: the rel32 of each one's lea of the table
		static_assert(Pins(kT1, 7, "4C 8D 05") && Pins(kT2, 20, "48 8D 1D") && PatternByte(*kLimitPatterns[kT2], 23) == -1 && Pins(kT3, 7, "4C 8D 05") &&
			Pins(kT4, 13, "48 8D 2D") && PatternByte(*kLimitPatterns[kT4], 16) == -1,
			"TweaksV: I-E-T1 to I-E-T4 hold lea r8, lea rbx, lea r8 and lea rbp of the ID table, the second's and the fourth's rel32 past the match");
		static_assert(PinsByte(kT2, 4, kSettingsAt + 8) && PinsByte(kT2, 17, kSettingsAt),
			"TweaksV: I-E-T2 reads the siren settings' count at +0x50 and their array at +0x48 of the carcols data");
		constexpr size_t kCarcolsAtT4 = 3;		// T4: the disp32 of "mov r8, [rip+disp32]" (4C 8B 05), the carcols pointer
		constexpr size_t kCarcolsAtM1 = 15;		// M1: the disp32 of "mov rcx, [rip+disp32]" (48 8B 0D at +12), the same pointer
		static_assert(Pins(kT4, 0, "4C 8B 05") && Pins(kM1, 12, "48 8B 0D"), "TweaksV: I-E-T4 and I-E-M1 read the carcols pointer");
		constexpr size_t kLoopAt = 12;			// D2: the compare loop, "cmp [rbp+rsi], dl" (38 54 35 00)
		constexpr size_t kLoopJzAt = 18;		// D2: the rel32 of the loop's "jz rel32" (0F 84 at +16)
		static_assert(Pins(kD2, kLoopAt, "38 54 35 00 0F 84"), "TweaksV: I-E-D2 holds the dedupe's compare loop and its jz");
		constexpr size_t kFillAt = 4;			// R1: the disp32 of "movdqa [rip+disp32], xmm0" (66 0F 7F 05), the table's last fill
		constexpr size_t kLoaderCallAt = 13;	// R1: "call rel32" (E8), the base loader
		static_assert(Pins(kR1, 0, "66 0F 7F 05") && Pins(kR1, kLoaderCallAt, "E8"), "TweaksV: I-E-R1 holds the table's last fill and the base loader's call");
		constexpr size_t kModelIndexAt = 3;		// M1: a model's siren setting index, the disp32 of "movzx eax, byte [rcx+disp32]" (0F B6 81)
		constexpr size_t kGetterSizeAt = 22;	// M1: a setting's size, the imm32 of "imul rax, rax, imm32" (48 69 C0)
		constexpr size_t kGetterArrayAt = 29;	// M1: the settings' array, the disp8 of "add rax, [rcx+disp8]" (48 03 41)
		static_assert(Pins(kM1, 0, "0F B6 81") && Pins(kM1, 19, "48 69 C0") && Pins(kM1, kGetterSizeAt, "E8 05 00 00") &&
			HexDword("E8 05 00 00", 0) == kSettingSize && Pins(kM1, 26, "48 03 41") && PinsByte(kM1, kGetterArrayAt, kSettingsAt),
			"TweaksV: I-E-M1 holds a setting's size, 0x5E8, and the settings' array at +0x48");

		// ---- the sites the part's writing half will change, or rely on and leave as they are: the game's bytes there

		enum class Kind { Bytes, Limit };	// Limit: a free-ID search's "cmp rsi, imm32", the imm32 the table's entries

		struct ByteSite
		{
			Match match;
			int offset;					// from the match's start; negative before it
			std::string_view bytes;		// the game's bytes there
			const char* what;			// for the log
			Kind kind = Kind::Bytes;
		};

		constexpr ByteSite kSites[] = {
			// the base loader's loop over the variations (T1): the variation's ID and the setting's ID
			{ kT1, 0x93, "0F B6 2C 0F", "siren setting ID load" },
			{ kT1, 0x97, "48 81 FD FF 00 00 00", "siren setting ID check for none" },
			{ kT1, 0xC4, "0F B6 1C 2B", "siren setting ID load" },
			{ kT1, 0xC8, "88 1C 0F", "variation's siren setting ID store" },
			// the base builder's siren part (T2): a setting's ID, a new one stored, the two free-ID searches
			{ kT2, 0x29, "40 88 31", "siren setting ID store" },
			{ kT2, 0x30, "0F B6 2C 39", "siren setting ID load" },
			{ kT2, 0x4B, "0F B6 2C 39", "siren setting ID load" },
			{ kT2, 0x4F, "48 81 FD FF 00 00 00", "siren setting ID check for none" },
			{ kT2, 0x85, "48 81 FE 00 01 00 00", "free-ID search limit", Kind::Limit },
			{ kT2, 0xCD, "48 81 FE 00 01 00 00", "free-ID search limit", Kind::Limit },
			{ kT2, 0xD6, "40 B6 FF", "siren setting ID given when none is free" },
			// a pack's loop over its variations (T3), as the base one
			{ kT3, 0x8D, "0F B6 3C 03", "siren setting ID load" },
			{ kT3, 0x91, "48 81 FF FF 00 00 00", "siren setting ID check for none" },
			{ kT3, 0xBE, "0F B6 14 17", "siren setting ID load" },
			{ kT3, 0xC2, "88 14 03", "variation's siren setting ID store" },
			// a pack's builder (T4), as the base one
			{ kT4, 0x21, "40 88 33", "siren setting ID store" },
			{ kT4, 0x28, "0F B6 0C 3B", "siren setting ID load" },
			{ kT4, 0x42, "0F B6 34 3B", "siren setting ID load" },
			{ kT4, 0x46, "48 81 FE FF 00 00 00", "siren setting ID check for none" },
			{ kT4, 0x81, "48 81 FE 00 01 00 00", "free-ID search limit", Kind::Limit },
			{ kT4, 0xC9, "48 81 FE 00 01 00 00", "free-ID search limit", Kind::Limit },
			{ kT4, 0xD2, "40 B6 FF", "siren setting ID given when none is free" },
			// a pack's dedupe (D1): the pack's ID, and its check for none, on the low byte
			{ kD1, 0x0B, "43 0F B6 54 3D 00", "pack dedupe's siren setting ID load" },
			{ kD1, 0x15, "80 FA FF", "pack dedupe's check for none" },
			// the siren setting's constructor (C1): the store the writing half replaces - +0x1C is written again before
			// the constructor returns - and the ID's default
			{ kC1, 0x08, "C7 41 1C 00 00 00 00", "siren setting constructor's first store at +0x1C" },
			{ kC1, 0xA0, "C6 06 FF", "siren setting constructor's ID default" },
			{ kC1, 0xC9, "89 46 1C", "siren setting constructor's second store at +0x1C" },
			// the variation's constructors, in place (C2) and allocating (C3): the stores from +0x30 to the IDs' defaults
			{ kC2, -0x0F, "C7 41 30 00 00 00 00 48 C7 41 38 00 00 00 00 C7 41 40 00 00 00 00 C7 41 48 00 00 FF FF C3",
				"variation's in-place constructor's stores" },
			{ kC3, -0x0F, "C7 40 30 00 00 00 00 48 C7 40 38 00 00 00 00 C7 40 40 00 00 00 00 C7 40 48 00 00 FF FF 48 83 C4 28 C3",
				"variation's allocating constructor's stores" },
		};

		// Every site's bytes well-formed and, where it shares bytes with its match, the pattern's; a limit is "cmp rsi, imm32"
		// with the table's entries
		consteval bool SitesAreValid()
		{
			for (const ByteSite& site : kSites)
			{
				if (!IsHexText(site.bytes))
					return false;
				for (size_t i = 0; i < HexSize(site.bytes); i++)
				{
					const int at = site.offset + static_cast<int>(i);
					const int pinned = at < 0 ? -1 : PatternByte(*kLimitPatterns[site.match], static_cast<size_t>(at));
					if (pinned != -1 && pinned != HexByte(site.bytes, i))
						return false;
				}
				if (site.kind == Kind::Limit && (HexSize(site.bytes) != 7 || site.bytes.substr(0, 8) != "48 81 FE" || HexDword(site.bytes, 3) != kIds))
					return false;
			}
			return true;
		}
		static_assert(SitesAreValid(), "TweaksV: a site of the siren setting IDs is malformed or disagrees with its pattern");

		// ---- the checks

		uint8_t* g_matches[kMatchCount] = {};
		const uint8_t* g_table = nullptr;			// the ID table, the game's own
		const uint8_t* g_carcolsPointer = nullptr;	// the game's pointer to its carcols data
		std::string g_why;							// why the limit is not measured, for its line

		bool Fail(std::string why)
		{
			g_why = std::move(why);
			return false;
		}

		// "0x4B" - for a number the game holds
		std::string Hex(uint64_t value)
		{
			char text[24];
			snprintf(text, sizeof(text), "0x%llX", static_cast<unsigned long long>(value));
			return text;
		}

		// size bytes at address inside the game and readable: what a site outside its match must be, and a table or a
		// member's data the game's code leads to - the scan read the matches alone
		bool Readable(const void* address, size_t size, std::string_view what, const char* label)
		{
			const auto first = static_cast<const uint8_t*>(address);
			if (!IsInsideGameImage(first) || !IsInsideGameImage(first + size - 1))
				return Fail("the " + std::string(what) + " is not inside the game: " + DescribeAddress(address) + " (" + label + ").");
			if (!IsReadable(address, size))
				return Fail("the " + std::string(what) + " at " + DescribeAddress(address) + " cannot be read (" + label + ").");
			return true;
		}

		template <typename T> T ReadAt(const uint8_t* at)
		{
			T value{};
			memcpy(&value, at, sizeof(value));
			return value;
		}

		// A match's sites from the table, each logged; the first one not as the game has it ends the checks
		bool CheckSitesOf(Match match)
		{
			const char* const label = kLimitPatterns[match]->label;
			for (const ByteSite& site : kSites)
			{
				if (site.match != match)
					continue;
				const uint8_t* const at = g_matches[match] + site.offset;
				const size_t size = HexSize(site.bytes);
				if (!Readable(at, size, site.what, label))
					return false;
				for (size_t i = 0; i < size; i++)
					if (at[i] != HexByte(site.bytes, i))
						return Fail("the " + std::string(site.what) + " at " + DescribeAddress(at) + " holds " + HexBytes(at, size) + ", not " +
							std::string(site.bytes) + " (" + label + ").");
				if (site.kind == Kind::Limit)	// as the relocation core logs a limit
					WriteLog(kTag, "DIAGNOSTIC: limit at " + DescribeAddress(at + 3) + " holds " + std::to_string(kIds) + " (N, " + label + ") -> game size " +
						std::to_string(kIds));
				else
					WriteLog(kTag, "DIAGNOSTIC: " + std::string(site.what) + " at " + DescribeAddress(at) + ": " + HexBytes(at, size) + " (" + label + ")");
			}
			return true;
		}

		// A parser registration (P1, P2) and the member of its structure that carries a siren setting ID
		struct Registration
		{
			Match match;
			const char* structure;		// "vehicle variations'"
			const char* member;			// "variation's siren setting ID member"
			size_t id;					// the member's index
			uint32_t idOffset;			// and its offset in the structure
			size_t neighbour;			// the member that bounds the room the ID would widen into
			uint32_t neighbourOffset;
			size_t members;				// how many members the structure has, its table ending in a null pointer; 0: not checked
		};
		constexpr Registration kSirenSetting = { kP1, "siren settings'", "siren setting's ID member", 0, 0x00, 2, 0x04, 0 };
		constexpr Registration kVariation = { kP2, "vehicle variations'", "variation's siren setting ID member", 6, 0x4B, 5, 0x4A, 7 };
		static_assert(kP1 == 0 && kP2 == 1, "TweaksV: the two registrations' members are kept by their match");
		const uint8_t* g_memberData[2] = {};		// the two ID members' data, by their match: logged again at the first summary

		// The ID member's offset field and its type as the game holds them now, logged; a type other than a byte's (0x01
		// or 0x02) is a finding for the part's writing half, logged too, and stops nothing - the measurement does not
		// depend on it, and the game may register its structures after TweaksV has started
		void LogMemberData(const Registration& r, const uint8_t* data, const std::string& head)
		{
			const char* const label = kLimitPatterns[r.match]->label;
			const uint8_t type = data[kMemberTypeAt];
			WriteLog(kTag, "DIAGNOSTIC: " + std::string(r.member) + head + "its data at " + DescribeAddress(data) + ", offset field " +
				Hex(ReadAt<uint64_t>(data + kMemberOffsetAt)) + ", type " + Hex(type) + " (" + label + ")");
			if (type != 0x01 && type != 0x02)
				WriteLog(kTag, "DIAGNOSTIC: the " + std::string(r.member) + "'s type at " + DescribeAddress(data + kMemberTypeAt) + " is " + Hex(type) +
					", not 0x1 or 0x2, which store a byte - a finding for the part's writing half (" + label + ").");
		}

		// The tables the registration leads to, inside the game; the member and its neighbour at their offsets, and as many
		// members as the findings have; the member's data inside the game, read, never written
		bool CheckRegistration(const Registration& r)
		{
			const uint8_t* const match = g_matches[r.match];
			const char* const label = kLimitPatterns[r.match]->label;
			if (!Readable(match + kOffsetsAt, sizeof(int32_t), "lea of the " + std::string(r.structure) + " offsets table", label))
				return false;
			const uint8_t* const memberTable = Rel32Target(match + kMemberTableAt);
			const uint8_t* const offsetTable = Rel32Target(match + kOffsetsAt);
			WriteLog(kTag, "DIAGNOSTIC: " + std::string(r.structure) + " parser registration at " + DescribeAddress(match) + ": name hash " +
				Hex(ReadAt<uint32_t>(match + kNameHashAt)) + ", member data table at " + DescribeAddress(memberTable) + ", offsets table at " +
				DescribeAddress(offsetTable) + " (" + label + ")");
			const size_t pointers = std::max(r.id, r.members) + 1;	// the member's pointer, and the null after the last member
			if (!Readable(memberTable, pointers * sizeof(uint64_t), std::string(r.structure) + " member data table", label) ||
				!Readable(offsetTable, (std::max(r.id, r.neighbour) + 1) * sizeof(uint32_t), std::string(r.structure) + " offsets table", label))
				return false;
			const uint32_t idOffset = ReadAt<uint32_t>(offsetTable + r.id * sizeof(uint32_t));
			const uint32_t neighbourOffset = ReadAt<uint32_t>(offsetTable + r.neighbour * sizeof(uint32_t));
			if (idOffset != r.idOffset || neighbourOffset != r.neighbourOffset)
				return Fail("the " + std::string(r.structure) + " members " + std::to_string(r.id) + " and " + std::to_string(r.neighbour) + " are at +" +
					Hex(idOffset) + " and +" + Hex(neighbourOffset) + ", not +" + Hex(r.idOffset) + " and +" + Hex(r.neighbourOffset) + " (" + label + ").");
			std::string count;
			if (r.members)
			{
				size_t found = 0;
				while (found < pointers && ReadAt<const uint8_t*>(memberTable + found * sizeof(uint64_t)))
					found++;
				if (found != r.members)
					return Fail("the " + std::string(r.structure) + " parser registration has " + std::to_string(found) +
						(found == pointers ? " or more" : "") + " members, not " + std::to_string(r.members) + " (" + label + ").");
				count = " of " + std::to_string(r.members);
			}
			const uint8_t* const data = ReadAt<const uint8_t*>(memberTable + r.id * sizeof(uint64_t));
			if (!Readable(data, kMemberTypeAt + 1, std::string(r.member) + "'s data", label))
				return false;
			g_memberData[r.match] = data;
			LogMemberData(r, data, ": member " + std::to_string(r.id) + count + " at +" + Hex(idOffset) + ", member " + std::to_string(r.neighbour) +
				" at +" + Hex(neighbourOffset) + "; ");
			return true;
		}

		// The two ID members once more, at the first summary: what the game holds there once it has started
		void LogMembersAgain()
		{
			for (const Registration* r : { &kSirenSetting, &kVariation })
			{
				const uint8_t* const data = g_memberData[r->match];
				if (IsReadable(data, kMemberTypeAt + 1))
					LogMemberData(*r, data, ", at the first summary: ");
				else
					WriteLog(kTag, "DIAGNOSTIC: " + std::string(r->member) + ", at the first summary: its data at " + DescribeAddress(data) +
						" cannot be read (" + kLimitPatterns[r->match]->label + ")");
			}
		}

		// The simple member's XML reader: where it finds a member's data and type - pinned by its pattern, logged
		bool CheckReader()
		{
			const uint8_t* const match = g_matches[kP3];
			WriteLog(kTag, "DIAGNOSTIC: simple member's XML reader at " + DescribeAddress(match) + ": the member data at +" + Hex(match[kReaderMemberAt]) +
				" of the member, its type at +" + Hex(match[kReaderTypeAt]) + ", types up to " + Hex(match[kReaderTypesAt]) + " (" +
				kLimitPatterns[kP3]->label + ")");
			return true;
		}

		// T4's carcols pointer, inside the game - the getter's must be the same
		bool CheckCarcolsPointer()
		{
			const uint8_t* const match = g_matches[kT4];
			const uint8_t* const pointer = Rel32Target(match + kCarcolsAtT4);
			WriteLog(kTag, "DIAGNOSTIC: carcols pointer load at " + DescribeAddress(match) + " -> " + DescribeAddress(pointer) + " (" +
				kLimitPatterns[kT4]->label + ")");
			if (!Readable(pointer, sizeof(void*), "carcols pointer", kLimitPatterns[kT4]->label))
				return false;
			g_carcolsPointer = pointer;
			return true;
		}

		// The ID table: T1 to T4's lea, each logged as the relocation core logs a reference, must load one table inside
		// the game, and a scan of the game's code must find those four and no more; each match's sites with it
		bool CheckTable()
		{
			for (size_t i = 0; i < kTableLoads; i++)
			{
				const Match match = static_cast<Match>(kT1 + i);
				const char* const label = kLimitPatterns[match]->label;
				if (match == kT4 && !CheckCarcolsPointer())
					return false;
				const uint8_t* const field = g_matches[match] + kTableLeaAt[i];
				if (!Readable(field, sizeof(int32_t), "lea of the siren setting ID table", label))
					return false;
				const uint8_t* const target = Rel32Target(field);
				WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(field) + " -> " + DescribeAddress(target) + " (" + label + ")");
				if (!g_table)
				{
					if (!IsInsideGameImage(target) || !IsInsideGameImage(target + kIds - 1))
						return Fail("the siren setting ID table is not inside the game: " + DescribeAddress(target) + " (another mod may have moved it already).");
					g_table = target;
				}
				else if (target != g_table)
					return Fail("the references to the siren setting ID table do not all point to the game's own table: " + DescribeAddress(g_table) +
						" and " + DescribeAddress(target) + " (" + label + ").");
				if (!CheckSitesOf(match))
					return false;
			}
			WriteLog(kTag, "DIAGNOSTIC: the siren setting ID table is at " + DescribeAddress(g_table) + " - " + std::to_string(kIds) + " entries of 1 byte");
			const std::vector<uint8_t*> loads = Scanner::FindRipRelativeLeas(g_table, kTableLoads + 1);
			for (const uint8_t* load : loads)
				WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(load) + " -> " + DescribeAddress(g_table) + " (lea scan)");
			if (loads.size() != kTableLoads)
				return Fail("the game code loads the siren setting ID table in " + std::to_string(loads.size()) + (loads.size() > kTableLoads ? " or more" : "") +
					" places, not the expected " + std::to_string(kTableLoads) + " (another mod may have moved some of them, or the game has changed).");
			return true;
		}

		// A pack's dedupe: its compare loop, and where its jz leads - inside the game
		bool CheckDedupeLoop()
		{
			const uint8_t* const match = g_matches[kD2];
			const char* const label = kLimitPatterns[kD2]->label;
			const uint8_t* const target = Rel32Target(match + kLoopJzAt);
			WriteLog(kTag, "DIAGNOSTIC: pack dedupe's compare loop at " + DescribeAddress(match + kLoopAt) + ", its jz -> " + DescribeAddress(target) + " (" +
				label + ")");
			if (!IsInsideGameImage(target))
				return Fail("the jz of the pack dedupe's compare loop at " + DescribeAddress(match + kLoopJzAt - 2) + " does not lead into the game (another "
					"mod may have changed it) (" + label + ").");
			return true;
		}

		// The vehicle metadata's init: its last store into the ID table, then the call of the base loader, into the game
		bool CheckLoaderCall()
		{
			const uint8_t* const match = g_matches[kR1];
			const char* const label = kLimitPatterns[kR1]->label;
			const uint8_t* const fill = Rel32Target(match + kFillAt);
			const uint8_t* const call = match + kLoaderCallAt;
			const uint8_t* const loader = CallTarget(call);
			WriteLog(kTag, "DIAGNOSTIC: base loader call at " + DescribeAddress(call) + " -> " + DescribeAddress(loader) + ", after the siren setting ID "
				"table's last fill at " + DescribeAddress(match) + " -> " + DescribeAddress(fill) + " (" + label + ")");
			if (fill != g_table + kTableLastFill)
				return Fail("the store before the base loader call at " + DescribeAddress(match) + " fills " + DescribeAddress(fill) + ", not the siren "
					"setting ID table's last 16 bytes at " + DescribeAddress(g_table + kTableLastFill) + " (" + label + ").");
			if (!IsInsideGameImage(loader))
				return Fail("the base loader call at " + DescribeAddress(call) + " is not a call into the game (another mod may have hooked it already) (" +
					label + ").");
			return true;
		}

		// The getter of a model's siren setting - what the measurement reads, pinned by its pattern - and its carcols pointer,
		// which must be T4's
		bool CheckGetter()
		{
			const uint8_t* const match = g_matches[kM1];
			const char* const label = kLimitPatterns[kM1]->label;
			const uint8_t* const pointer = Rel32Target(match + kCarcolsAtM1);
			WriteLog(kTag, "DIAGNOSTIC: siren settings getter at " + DescribeAddress(match) + ": a model's siren setting index at +" +
				Hex(ReadAt<uint32_t>(match + kModelIndexAt)) + ", the settings at +" + Hex(match[kGetterArrayAt]) + " of the carcols data, " +
				Hex(ReadAt<uint32_t>(match + kGetterSizeAt)) + " bytes each; the carcols pointer at " + DescribeAddress(pointer) + " (" + label + ")");
			if (pointer != g_carcolsPointer)
				return Fail("the carcols pointer the siren settings getter reads, " + DescribeAddress(pointer) + ", is not the one a pack's builder reads, " +
					DescribeAddress(g_carcolsPointer) + " (" + label + ").");
			return true;
		}

		// ---- the measurement: the siren settings loaded and the IDs in use, read as they are

		enum class State : uint64_t { Counted, NotLoaded, Unreadable };

		// The carcols data's siren settings, as the game keeps them at kSettingsAt
		struct Settings
		{
			const uint8_t* array;
			uint16_t count;
			uint16_t capacity;
		};
		static_assert(offsetof(Settings, count) == 8 && offsetof(Settings, capacity) == 10, "TweaksV: the siren settings' count and capacity follow their array");
		constexpr size_t kSettingsSize = offsetof(Settings, capacity) + sizeof(uint16_t);

		const uint8_t* g_unreadable = nullptr;	// what could not be read, for the summary - both run on the measurement's thread

		// The settings, read as they are this moment - plausible, or false
		bool ReadSettings(const uint8_t* carcols, Settings& settings)
		{
			settings = {};
			return CopyMemorySafely(&settings, carcols + kSettingsAt, kSettingsSize) && settings.count <= settings.capacity &&
				(settings.count == 0 || settings.array);
		}

		uint64_t Numbers(State state, uint64_t settings = 0, uint64_t inUse = 0, uint64_t givenTwice = 0)
		{
			return static_cast<uint64_t>(state) << 56 | givenTwice << 25 | inUse << 16 | settings;
		}

		// The settings loaded (16 bits), the IDs in use (9 bits: up to 256) and the IDs given to more than one setting (16
		// bits) - or why they cannot be counted. The game's pointer and table after IsReadable; the settings through
		// CopyMemorySafely, their ID one byte each, and read again afterwards: an array the game grew meanwhile is not
		// counted this time. Nothing is allocated: the IDs seen go into two bitmaps on the stack.
		uint64_t SettingsNumbers()
		{
			if (!IsReadable(g_carcolsPointer, sizeof(void*)))
			{
				g_unreadable = g_carcolsPointer;
				return Numbers(State::Unreadable);
			}
			const uint8_t* const carcols = *reinterpret_cast<const uint8_t* const volatile*>(g_carcolsPointer);
			if (!carcols)
				return Numbers(State::NotLoaded);
			Settings settings{}, after{};
			if (!ReadSettings(carcols, settings))
			{
				g_unreadable = carcols + kSettingsAt;
				return Numbers(State::Unreadable);
			}
			uint64_t seen[kIds / 64] = {}, twice[kIds / 64] = {};
			for (size_t i = 0; i < settings.count; i++)
			{
				uint8_t id = 0;
				if (!CopyMemorySafely(&id, reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(settings.array) + i * kSettingSize), sizeof(id)))
				{
					g_unreadable = settings.array;
					return Numbers(State::Unreadable);
				}
				const uint64_t bit = uint64_t{ 1 } << (id % 64);
				if (seen[id / 64] & bit)
					twice[id / 64] |= bit;
				seen[id / 64] |= bit;
			}
			if (!ReadSettings(carcols, after) || after.array != settings.array || after.count != settings.count)
			{
				g_unreadable = settings.array;
				return Numbers(State::Unreadable);
			}
			if (!IsReadable(g_table, kIds))
			{
				g_unreadable = g_table;
				return Numbers(State::Unreadable);
			}
			const auto table = reinterpret_cast<const volatile uint8_t*>(g_table);
			uint64_t inUse = 0;
			for (size_t id = 0; id < kIds; id++)
				if (table[id] != kNone)
					inUse++;
			uint64_t givenTwice = 0;
			for (const uint64_t word : twice)
				givenTwice += static_cast<uint64_t>(std::popcount(word));
			return Numbers(State::Counted, settings.count, inUse, givenTwice);
		}

		bool g_firstSummary = true;	// the measurement's thread alone reads and writes it

		void LogSettings(uint64_t numbers)
		{
			if (g_firstSummary)
			{
				g_firstSummary = false;
				LogMembersAgain();
			}
			const auto state = static_cast<State>(numbers >> 56);
			if (state == State::NotLoaded)
				WriteLog(kTag, "DIAGNOSTIC: no siren settings loaded yet - the game has not read carcols.ymt.");
			else if (state == State::Unreadable)
				WriteLog(kTag, "DIAGNOSTIC: the siren settings at " + DescribeAddress(g_unreadable) + " could not be read this time (the game may have been "
					"changing them).");
			else
			{
				const uint64_t settings = numbers & 0xFFFF;
				const uint64_t past = settings > kPositions ? settings - kPositions : 0;
				WriteLog(kTag, "DIAGNOSTIC: siren settings loaded: " + std::to_string(settings) + " (vehicles can use " + std::to_string(kUsable) + " at most); "
					"siren setting IDs in use: " + std::to_string(numbers >> 16 & 0x1FF) + " of " + std::to_string(kIds) + "; settings past position " +
					std::to_string(kPositions - 1) + ", which no vehicle can reach: " + std::to_string(past) + "; IDs given to more than one setting: " +
					std::to_string(numbers >> 25 & 0xFFFF) + ".");
			}
		}

		// The first summary 10 seconds in, then one a minute while the numbers change.
		constexpr Measurement kMeasurement = { "siren settings summary", kFirstSummaryMs, &SettingsNumbers, &LogSettings };
	}

	void MeasureSirenIds()
	{
		// every pattern is looked for, so that the log names each one missing; the line names the first
		const char* missing = nullptr;
		for (size_t i = 0; i < kMatchCount; i++)
		{
			g_matches[i] = Scanner::Find(*kLimitPatterns[i]);
			if (!g_matches[i] && !missing)
				missing = kLimitPatterns[i]->label;
		}
		if (missing)
		{
			WriteLog(kTag, std::string("DIAGNOSTIC: the siren settings are not measured: ") + missing + " was not found.");
			return;
		}
		// every site in the order of the patterns; the first that is not as the findings have it ends the checks
		if (!CheckRegistration(kSirenSetting) || !CheckRegistration(kVariation) || !CheckReader() || !CheckTable() || !CheckSitesOf(kD1) ||
			!CheckDedupeLoop() || !CheckSitesOf(kC1) || !CheckSitesOf(kC2) || !CheckSitesOf(kC3) || !CheckLoaderCall() || !CheckGetter())
		{
			WriteLog(kTag, "DIAGNOSTIC: the siren settings are not measured: " + g_why);
			return;
		}
		WriteLog(kTag, "DIAGNOSTIC: the siren settings, through the carcols pointer at " + DescribeAddress(g_carcolsPointer) + " (" + kLimitPatterns[kM1]->label +
			"), and the siren setting ID table at " + DescribeAddress(g_table) + ", " + std::to_string(kIds) + " IDs (" + kLimitPatterns[kT1]->label + " to " +
			kLimitPatterns[kT4]->label + "): the settings loaded and the IDs in use are logged after 10 seconds, then every minute while the numbers change.");
		StartMeasurement<kMeasurement>();	// one that cannot start is logged
	}
}
