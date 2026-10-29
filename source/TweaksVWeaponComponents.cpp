// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the weapon components limit, measured first (see TweaksVWeaponComponents.h).
//
// The game keeps every weapon component definition of all weaponcomponents.meta files - each scope, clip,
// suppressor, flashlight, grip and tint a weapon can take - together in one sorted list: a static array of 470
// pointers with its count, an int32, right after it, so the list's size is the distance from the array to its
// count and no instruction carries it. After each weaponcomponents.meta it loads, the game rebuilds the list -
// the count set to 0, every file's definitions copied in without a bound, then sorted by their hash - and 15
// binary searches, bounded by the count, read it. The 471st definition is stored over the count itself, and the
// copy then writes through the garbage: memory corruption while the game loads. Every definition also takes a
// slot of a pool, 470 unless the game's configuration names another size, and a full pool ends the game in a
// trap. The WeaponLimits part to come moves the list on the relocation core and gives the pool the list's new
// size through the call that reads its size; first the limit is measured, here, with DIAGNOSTIC_MODE=1 only
// (the table of the parts measured first starts the module):
//   DIAGNOSTIC_MODE  every site the part's writing half will use is found and checked as it will check it, and
//                    logged: the rebuild's and the sort's loads of the list, which must name one list inside the
//                    game; the count's four references, which must name one count, right after the list and a
//                    whole number of 8-byte entries from it - the sort's entry size, which its pattern pins; the
//                    pool's size call, which must lead into the game, and the pool's default size; and every
//                    "lea r64, [rip+disp32]" that loads the list - exactly 17, each with a 48 or 4C prefix. Then
//                    the measurement, 10 seconds in and then every minute while they change: the definitions in
//                    the list, from its count, and the weaponcomponents.meta files loaded, from the game's own
//                    count of them, both read as they are - nothing is hooked or written
// DRY_RUN changes nothing here. A pattern missing, a site not as expected, an address outside the game or a
// thread that cannot start is logged, not reported - the module is a diagnostic aid - and nothing more is
// measured. The pool's size, which the game's configuration may set, is not read: the part's redirected call
// will log it.
// Written for TweaksV from its own findings in the game's code (PATTERNS.md, "WeaponLimits"); that the limit
// exists is what alexguirre's Weapon Limits Adjuster for GTA V Legacy says on its page - none of its code, nor
// FiveM's, from which its components half is adapted, was read or used.

#include "TweaksVWeaponComponents.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
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
		constexpr const char* kTag = "WeaponLimits";

		// ---- the list, its count and the pool's size (PATTERNS.md, "WeaponLimits")

		constexpr uint32_t kEntrySize = 8;					// an entry of the list: a definition's pointer
		constexpr uint32_t kMaxEntries = 16384;				// the most entries a game's own list or pool is taken to have: more is no plausible size
		constexpr size_t kListLoads = 17;					// the "lea r64, [rip+disp32]" that load the list - every one of them, on 1.0.1158.16
		constexpr uint8_t kLeaPrefixes[] = { 0x48, 0x4C };	// the REX bytes the game's loads of the list start with
		constexpr size_t kLeaSize = 7;						// a "lea r64, [rip+disp32]"
		constexpr uint32_t kPoolNameHash = 0xBAE33CCF;		// the game's hash of the pool's name, "cweaponcomponentinfo"

		constexpr const GamePattern& kRebuildPattern = Pattern("W-E-C1");	// the rebuild: the count's reset, the files' count, its lea of the list
		constexpr const GamePattern& kSortPattern = Pattern("W-E-C2");		// the sort: the count, its lea of the list, the entry size
		constexpr const GamePattern& kUpdatePattern = Pattern("W-E-C3");	// the rebuild's update of the count, before its unbounded store
		constexpr const GamePattern& kPoolPattern = Pattern("W-E-C4");		// the pool's size: the configuration, the pool's name and default, the call

		// A rip-relative operand in a match: where its disp32 lies, and where its instruction ends - the address it
		// leads to is counted from there, past an immediate that may follow the disp32.
		struct RipOperand
		{
			size_t field;
			size_t end;
		};
		constexpr RipOperand kCountReset = { 2, 10 };		// W-E-C1: "mov dword [rip+disp32], 0" - the count, set to 0
		constexpr RipOperand kFilesRead = { 14, 18 };		// W-E-C1: "movzx r8d, word [rip+disp32]" - the files' count
		constexpr RipOperand kRebuildLoad = { 34, 38 };		// W-E-C1: "lea r10, [rip+disp32]" - the list
		constexpr RipOperand kSortCount = { 3, 7 };			// W-E-C2: "movsxd rdx, dword [rip+disp32]" - the count
		constexpr RipOperand kSortLoad = { 14, 18 };		// W-E-C2: "lea rcx, [rip+disp32]" - the list
		constexpr size_t kEntrySizeAt = 27;					// W-E-C2: the imm32 of "mov r8d, 8" - qsort's entry size
		constexpr RipOperand kUpdateRead = { 3, 7 };		// W-E-C3: "movsxd rdx, dword [rip+disp32]" - the count
		constexpr RipOperand kUpdateWrite = { 12, 16 };		// W-E-C3: "mov [rip+disp32], ecx" - the count
		constexpr RipOperand kConfiguration = { 3, 7 };		// W-E-C4: "mov rcx, [rip+disp32]" - the game's configuration
		constexpr size_t kHashAt = 8;						// W-E-C4: the imm32 of "mov edx, imm32" - the pool's name
		constexpr size_t kDefaultAt = 14;					// W-E-C4: the imm32 of "mov r8d, imm32" - the pool's default size
		constexpr size_t kSizeCallAt = 18;					// W-E-C4: "call rel32" - the size lookup, the call the part redirects

		// The bytes a pattern holds from index at, none of them a wildcard - checked when TweaksV is compiled, so that
		// each offset above reads the instruction its comment names.
		consteval bool PatternHolds(const GamePattern& pattern, size_t at, std::string_view bytes)
		{
			for (size_t i = 0; i < bytes.size(); i++)
				if (PatternByte(pattern, at + i) != static_cast<uint8_t>(bytes[i]))
					return false;
			return true;
		}
		// The imm32 a pattern holds at index at, or -1 when a byte of it is a wildcard.
		consteval int64_t PatternImm32(const GamePattern& pattern, size_t at)
		{
			int64_t value = 0;
			for (size_t i = 0; i < 4; i++)
			{
				const int byte = PatternByte(pattern, at + i);
				if (byte < 0)
					return -1;
				value |= static_cast<int64_t>(byte) << (8 * i);
			}
			return value;
		}
		static_assert(PatternHolds(kRebuildPattern, kCountReset.field - 2, "\xC7\x05") && PatternImm32(kRebuildPattern, kCountReset.field + 4) == 0 &&
			kCountReset.end == kCountReset.field + 8, "TweaksV: W-E-C1 holds the count's reset, mov dword [rip+disp32], 0, at +0");
		static_assert(PatternHolds(kRebuildPattern, kFilesRead.field - 4, "\x44\x0F\xB7\x05") && kFilesRead.end == kFilesRead.field + 4,
			"TweaksV: W-E-C1 holds the files' count's movzx r8d, word [rip+disp32] at +10");
		static_assert(PatternHolds(kRebuildPattern, kRebuildLoad.field - 3, "\x4C\x8D\x15") && kRebuildLoad.end == kRebuildLoad.field + 4,
			"TweaksV: W-E-C1 holds the list's lea r10, [rip+disp32] at +31");
		static_assert(PatternHolds(kSortPattern, kSortCount.field - 3, "\x48\x63\x15") && kSortCount.end == kSortCount.field + 4,
			"TweaksV: W-E-C2 holds the count's movsxd rdx, dword [rip+disp32] at +0");
		static_assert(PatternHolds(kSortPattern, kSortLoad.field - 3, "\x48\x8D\x0D") && kSortLoad.end == kSortLoad.field + 4,
			"TweaksV: W-E-C2 holds the list's lea rcx, [rip+disp32] at +11");
		static_assert(PatternHolds(kSortPattern, kEntrySizeAt - 2, "\x41\xB8") && PatternImm32(kSortPattern, kEntrySizeAt) == kEntrySize,
			"TweaksV: W-E-C2 holds qsort's entry size, mov r8d, 8, at +25");
		static_assert(PatternHolds(kUpdatePattern, kUpdateRead.field - 3, "\x48\x63\x15") && kUpdateRead.end == kUpdateRead.field + 4 &&
			PatternHolds(kUpdatePattern, kUpdateWrite.field - 2, "\x89\x0D") && kUpdateWrite.end == kUpdateWrite.field + 4,
			"TweaksV: W-E-C3 holds the count's movsxd rdx, dword [rip+disp32] at +0 and its mov [rip+disp32], ecx at +10");
		static_assert(PatternHolds(kPoolPattern, kConfiguration.field - 3, "\x48\x8B\x0D") && kConfiguration.end == kConfiguration.field + 4,
			"TweaksV: W-E-C4 holds the configuration's mov rcx, [rip+disp32] at +0");
		static_assert(PatternHolds(kPoolPattern, kHashAt - 1, "\xBA") && PatternImm32(kPoolPattern, kHashAt) == kPoolNameHash,
			"TweaksV: W-E-C4 holds the pool's name, mov edx, 0xBAE33CCF, at +7");
		static_assert(PatternHolds(kPoolPattern, kDefaultAt - 2, "\x41\xB8") && PatternImm32(kPoolPattern, kDefaultAt) == -1,
			"TweaksV: W-E-C4 holds the pool's default, mov r8d, imm32, at +12, the imm32 read");
		static_assert(PatternHolds(kPoolPattern, kSizeCallAt, "\xE8"), "TweaksV: W-E-C4 holds the size lookup's call at +18");

		// Where a rip-relative operand of a match leads.
		const uint8_t* Target(const uint8_t* match, const RipOperand& operand)
		{
			int32_t disp = 0;
			memcpy(&disp, match + operand.field, sizeof(disp));
			return match + operand.end + disp;
		}

		uint32_t Imm32At(const uint8_t* address)
		{
			uint32_t value = 0;
			memcpy(&value, address, sizeof(value));
			return value;
		}

		// True if the size bytes at p lie inside the game and can be read.
		bool InsideGame(const void* p, size_t size)
		{
			return IsInsideGameImage(p) && IsInsideGameImage(static_cast<const uint8_t*>(p) + size - 1) && IsReadable(p, size);
		}

		std::string Hex32(uint32_t value)
		{
			char text[16];
			snprintf(text, sizeof(text), "0x%08X", value);
			return text;
		}

		// "1 weaponcomponents.meta file", "23 weaponcomponents.meta files"
		std::string Counted(uint64_t n, const char* thing)
		{
			return std::to_string(n) + " " + thing + (n == 1 ? "" : "s");
		}

		// What keeps the module from measuring: logged, not reported - a diagnostic aid (NOTES.md, T-DIAG).
		void NotMeasured(const std::string& why)
		{
			WriteLog(kTag, "DIAGNOSTIC: the weapon components are not measured: " + why);
		}

		// ---- the measurement (DIAGNOSTIC_MODE): the list's count and the files' count, read as they are

		const volatile int32_t* g_count = nullptr;		// the list's count: the definitions of every file loaded
		const volatile uint16_t* g_files = nullptr;		// the game's count of the files' blobs: the weaponcomponents.meta files loaded
		uint32_t g_listSize = 0;						// the game's own list: 470 entries on 1.0.1158.16
		constexpr uint64_t kUnreadable = uint64_t{ 1 } << 48;	// in the place of the two counts

		// The count in the low 32 bits, the files above them - both read without a lock, after IsReadable.
		uint64_t Definitions()
		{
			if (!IsReadable(const_cast<const int32_t*>(g_count), sizeof(int32_t)) || !IsReadable(const_cast<const uint16_t*>(g_files), sizeof(uint16_t)))
				return kUnreadable;
			return uint64_t{ static_cast<uint32_t>(*g_count) } | uint64_t{ *g_files } << 32;
		}

		void LogDefinitions(uint64_t numbers)
		{
			if (numbers == kUnreadable)
			{
				WriteLog(kTag, "DIAGNOSTIC: weapon components: the list's count at " + DescribeAddress(const_cast<const int32_t*>(g_count)) +
					" or the files' count at " + DescribeAddress(const_cast<const uint16_t*>(g_files)) + " cannot be read.");
				return;
			}
			const int32_t count = static_cast<int32_t>(static_cast<uint32_t>(numbers));
			const std::string files = Counted(static_cast<uint16_t>(numbers >> 32), "weaponcomponents.meta file");
			const std::string size = std::to_string(g_listSize);
			// past the list, the definition after its last entry was stored over the count: what it reads is no count
			if (count < 0 || static_cast<uint32_t>(count) > g_listSize)
				WriteLog(kTag, "DIAGNOSTIC: the weapon component list's count reads " + std::to_string(count) + ", from " + files +
					" - outside the game's own list of " + size + ": the list has overflowed, a definition past its end written over the count "
					"and the memory after it, and the game is likely to crash.");
			else
				WriteLog(kTag, "DIAGNOSTIC: " + Counted(static_cast<uint32_t>(count), "weapon component definition") + " from " + files +
					" (the game's own list holds " + size + ")" + (static_cast<uint32_t>(count) == g_listSize ? " - it is full: one more "
					"definition crashes the game while it loads." : "."));
		}

		// The first summary 10 seconds in, then one a minute while the numbers change.
		constexpr Measurement kMeasurement = { "weapon components summary", kFirstSummaryMs, &Definitions, &LogDefinitions };
	}

	void MeasureWeaponComponents()
	{
		// 1. the four sites, each looked for, so that the log names each one missing
		const uint8_t* const rebuild = Scanner::Find(kRebuildPattern);
		const uint8_t* const sort = Scanner::Find(kSortPattern);
		const uint8_t* const update = Scanner::Find(kUpdatePattern);
		const uint8_t* const pool = Scanner::Find(kPoolPattern);
		const GamePattern* const missing = !rebuild ? &kRebuildPattern : !sort ? &kSortPattern : !update ? &kUpdatePattern : !pool ? &kPoolPattern : nullptr;
		if (missing)
		{
			NotMeasured(std::string(missing->label) + " was not found.");
			return;
		}

		// 2. the list: the rebuild's lea loads it inside the game, and the sort's the same list
		const uint8_t* const list = Target(rebuild, kRebuildLoad);
		WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(rebuild + kRebuildLoad.field) + " -> " + DescribeAddress(list) + " (" +
			kRebuildPattern.label + ")");
		if (!IsInsideGameImage(list))
		{
			NotMeasured("the weapon component list is not inside the game: " + DescribeAddress(list) + " (another mod may have moved it already).");
			return;
		}
		const uint8_t* const sorted = Target(sort, kSortLoad);
		WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(sort + kSortLoad.field) + " -> " + DescribeAddress(sorted) + " (" +
			kSortPattern.label + ")");
		if (sorted != list)
		{
			NotMeasured("the references to the weapon component list do not all point to the game's own list: " + DescribeAddress(list) + " and " +
				DescribeAddress(sorted) + " (" + kSortPattern.label + ").");
			return;
		}

		// 3. the count: the rebuild's reset, the sort's read and the update's read and write name one int32 inside the
		// game, right after the list - the list's size is the distance, a whole number of entries of the sort's size
		struct CountReference
		{
			const uint8_t* match;
			RipOperand operand;
			const GamePattern& pattern;
		};
		const CountReference references[] = { { rebuild, kCountReset, kRebuildPattern }, { sort, kSortCount, kSortPattern },
			{ update, kUpdateRead, kUpdatePattern }, { update, kUpdateWrite, kUpdatePattern } };
		const uint8_t* count = nullptr;
		for (const CountReference& reference : references)
		{
			const uint8_t* const target = Target(reference.match, reference.operand);
			WriteLog(kTag, "DIAGNOSTIC: count reference at " + DescribeAddress(reference.match + reference.operand.field) + " -> " +
				DescribeAddress(target) + " (" + reference.pattern.label + ")");
			if (!count)
			{
				if (!InsideGame(target, sizeof(int32_t)))
				{
					NotMeasured("the weapon component list's count is not inside the game: " + DescribeAddress(target) + " (" + reference.pattern.label + ").");
					return;
				}
				count = target;
			}
			else if (target != count)
			{
				NotMeasured("the references to the weapon component list's count do not all point to one count: " + DescribeAddress(count) + " and " +
					DescribeAddress(target) + " (" + reference.pattern.label + ").");
				return;
			}
		}
		WriteLog(kTag, "DIAGNOSTIC: entry size at " + DescribeAddress(sort + kEntrySizeAt) + " holds " + std::to_string(Imm32At(sort + kEntrySizeAt)) +
			" (" + kSortPattern.label + ")");
		const ptrdiff_t distance = count - list;
		if (distance <= 0 || distance % kEntrySize != 0)
		{
			NotMeasured("the weapon component list's count at " + DescribeAddress(count) + " does not lie a whole number of " +
				std::to_string(kEntrySize) + "-byte entries after the list at " + DescribeAddress(list) + ".");
			return;
		}
		const uint64_t size = static_cast<uint64_t>(distance) / kEntrySize;
		if (size > kMaxEntries)
		{
			NotMeasured("the game's own weapon component list size, " + std::to_string(size) + ", is not plausible.");
			return;
		}

		// 4. the files' count: the blobs the rebuild walks, one per weaponcomponents.meta loaded - a uint16 inside the game
		const uint8_t* const files = Target(rebuild, kFilesRead);
		WriteLog(kTag, "DIAGNOSTIC: files' count reference at " + DescribeAddress(rebuild + kFilesRead.field) + " -> " + DescribeAddress(files) +
			" (" + kRebuildPattern.label + ")");
		if (!InsideGame(files, sizeof(uint16_t)))
		{
			NotMeasured("the count of weaponcomponents.meta files is not inside the game: " + DescribeAddress(files) + " (" + kRebuildPattern.label + ").");
			return;
		}

		// 5. the pool's size: the call that reads it leads into the game - not to another mod's hook - and its default is
		// a plausible size; the pool's name is pinned by W-E-C4
		const uint8_t* const sizeCall = pool + kSizeCallAt;
		const uint8_t* const lookup = CallTarget(sizeCall);
		const uint32_t poolDefault = Imm32At(pool + kDefaultAt);
		WriteLog(kTag, "DIAGNOSTIC: pool size call at " + DescribeAddress(sizeCall) + " -> " + DescribeAddress(lookup) + " for the pool " +
			Hex32(Imm32At(pool + kHashAt)) + ", its default at " + DescribeAddress(pool + kDefaultAt) + " holds " + std::to_string(poolDefault) +
			", the game's configuration at " + DescribeAddress(Target(pool, kConfiguration)) + " (" + kPoolPattern.label + ")");
		if (!lookup || !IsInsideGameImage(lookup))
		{
			NotMeasured("the pool size call at " + DescribeAddress(sizeCall) + " is not a call into the game (another mod may have hooked it already) (" +
				kPoolPattern.label + ").");
			return;
		}
		if (poolDefault == 0 || poolDefault > kMaxEntries)
		{
			NotMeasured("the pool's default size at " + DescribeAddress(pool + kDefaultAt) + " holds " + std::to_string(poolDefault) +
				", which is no plausible size (" + kPoolPattern.label + ").");
			return;
		}

		// 6. every lea that loads the list - exactly 17, each with a 48 or 4C prefix: the complete set the writing half moves
		const std::vector<uint8_t*> loads = Scanner::FindRipRelativeLeas(list, kListLoads + 1);
		for (const uint8_t* load : loads)
			WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(load) + " -> " + DescribeAddress(list) + " (lea scan)");
		if (loads.size() != kListLoads)
		{
			NotMeasured("the game code loads the weapon component list in " + std::to_string(loads.size()) + (loads.size() > kListLoads ? " or more" : "") +
				" places, not the expected " + std::to_string(kListLoads) + " (another mod may have moved some of them, or the game has changed).");
			return;
		}
		for (const uint8_t* load : loads)
			if (load[0] != kLeaPrefixes[0] && load[0] != kLeaPrefixes[1])
			{
				NotMeasured("the load of the weapon component list at " + DescribeAddress(load) + " is not a \"lea r64, [rip+disp32]\" with a " +
					HexBytes(kLeaPrefixes, 1) + " or " + HexBytes(kLeaPrefixes + 1, 1) + " prefix: " + HexBytes(load, kLeaSize) + ".");
				return;
			}

		// 7. the measurement: the two counts, read as they are, 10 seconds in and then every minute while they change
		g_count = reinterpret_cast<const volatile int32_t*>(count);
		g_files = reinterpret_cast<const volatile uint16_t*>(files);
		g_listSize = static_cast<uint32_t>(size);
		WriteLog(kTag, "DIAGNOSTIC: the weapon component list at " + DescribeAddress(list) + ", " + std::to_string(size) + " entries of " +
			std::to_string(kEntrySize) + " bytes up to its count at " + DescribeAddress(count) + ", and the count of weaponcomponents.meta files at " +
			DescribeAddress(files) + ": the definitions and the files are logged after 10 seconds, then every minute while the numbers change.");
		StartMeasurement<kMeasurement>();	// one that cannot start is logged
	}
}
