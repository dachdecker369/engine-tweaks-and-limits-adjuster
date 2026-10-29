// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the weapon infos, a limit of the WeaponLimits part to come, measured first (see TweaksVWeaponInfos.h).
//
// The game parses each weapons.meta file - each data file of the type 0x4E - into one weapon info blob of 248 bytes
// and holds the blobs in one block on its heap: the weapon info manager's array, in the game's data, holds the
// block's pointer at +0, the number of blobs loaded at +8 and the room at +0xA, both 16-bit. A C++ static
// initializer of the game sets the array up once, with two immediates - "mov edx, 160" hands the number to the
// call that allocates and constructs that many blobs, "mov word [capacity], 160" gives the array its room - and the
// game's loader adds a blob for each file without any check, so a 161st file is parsed into the memory after the
// block. The part to come raises the two immediates together; it goes in only once a mod load reaches the limit,
// so the limit is measured first (SPECIFICATIONS 11.10). The table of the parts measured first starts this module
// with DIAGNOSTIC_MODE=1 only, after the parts; it reads no switch of its own, and DRY_RUN changes nothing in it:
//   - W-E-I1, the initializer's allocation, and W-E-I2, the loader's head, are found once each, and every site is
//     checked as the part's writing half will check it: the two immediates equal and not 0; the call into the
//     game, its target beginning with the allocation's bytes, which take the number from edx alone, and its
//     element size; the block's store and the capacity's store, the second 0xA after the first - the array, inside
//     the game; the loader's count read and written at the array + 8, its block read at the array, its blob size
//     the allocation's element size; the loader's start a function start, where the part's guard will hook it; and
//     exactly 2 references to the array's address in the game's code - the block's store and the initializer's
//     lea - so that a second store of a block would show. A pattern missing, a site not as expected or an address
//     outside the game is logged, not reported - an aid (NOTES.md, T-DIAG) - and nothing more is done;
//   - the array, read once at the start: still empty - no block, capacity 0 - it proves that its initializer runs
//     after TweaksV's start, as the part to come needs to raise the two immediates in time; set up already, it is
//     logged as such and measured all the same;
//   - the measurement, with nothing hooked: the count and the capacity read back - after IsReadable, 16 bits each,
//     without a lock - 10 seconds in and then every minute while they change: the weapons.meta files loaded,
//     against the room the game's code gives and the room the array has, the game's own 160 in this build, since
//     nothing raises it; and whether the array is full, or a file was already written past its end.
// Nothing is written to the game.
// Written for TweaksV from the facts its own research found in the running game (PATTERNS.md); no code of anyone's
// was read for it. The part to come takes its idea - the limit, a capacity handed as an immediate to the call that
// allocates the blobs - from Weapon Limits Adjuster by alexguirre, its MIT half (CREDITS.md, once the part is built).

#include "TweaksVWeaponInfos.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
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

		// W-E-I1, in the initializer: "mov edx, imm32" - the number of blobs allocated -, the call that allocates
		// them, "mov [rip+disp32], rax" - the block into the array - and "mov word [rip+disp32], imm16" - the capacity,
		// whose imm16 follows its disp32, so that the instruction ends 2 bytes after the disp32
		constexpr const GamePattern& kInitPattern = Pattern("W-E-I1");
		constexpr size_t kNumberAt = 10;			// BA imm32
		constexpr size_t kCallAt = 15;				// E8 rel32
		constexpr size_t kBlockStoreAt = 20;		// 48 89 05 disp32
		constexpr size_t kCapacityStoreAt = 27;		// 66 C7 05 disp32 imm16
		constexpr size_t kCapacityStoreSize = 9;
		static_assert(PatternByte(kInitPattern, kNumberAt) == 0xBA && PatternByte(kInitPattern, kCallAt) == 0xE8 &&
			PatternByte(kInitPattern, kBlockStoreAt) == 0x48 && PatternByte(kInitPattern, kBlockStoreAt + 1) == 0x89 &&
			PatternByte(kInitPattern, kBlockStoreAt + 2) == 0x05 && PatternByte(kInitPattern, kCapacityStoreAt) == 0x66 &&
			PatternByte(kInitPattern, kCapacityStoreAt + 1) == 0xC7 && PatternByte(kInitPattern, kCapacityStoreAt + 2) == 0x05 &&
			PatternByte(kInitPattern, kCapacityStoreAt + kCapacityStoreSize) == 0x48,
			"TweaksV: W-E-I1 holds the mov edx, the call, the block's store and the capacity's store at +10, +15, +20 and +27");

		// W-E-I2, the loader's head: "movzx eax, word [rip+disp32]" and "mov word [rip+disp32], cx" - the count, read
		// and written back one higher -, "imul r13, rax, imm32" - the blob's size - and "add r13, [rip+disp32]" - the
		// block: the new blob is the block + the old count x the size. The loader starts 0x18 bytes before the match.
		constexpr const GamePattern& kLoaderPattern = Pattern("W-E-I2");
		constexpr size_t kCountReadAt = 0;			// 0F B7 05 disp32
		constexpr size_t kCountWriteAt = 10;		// 66 89 0D disp32
		constexpr size_t kBlobSizeAt = 17;			// 4C 69 E8 imm32
		constexpr size_t kBlockReadAt = 24;			// 4C 03 2D disp32
		constexpr size_t kLoaderStart = 0x18;		// the loader starts this many bytes before the match
		static_assert(PatternByte(kLoaderPattern, kCountReadAt) == 0x0F && PatternByte(kLoaderPattern, kCountReadAt + 1) == 0xB7 &&
			PatternByte(kLoaderPattern, kCountReadAt + 2) == 0x05 && PatternByte(kLoaderPattern, kCountWriteAt) == 0x66 &&
			PatternByte(kLoaderPattern, kCountWriteAt + 1) == 0x89 && PatternByte(kLoaderPattern, kCountWriteAt + 2) == 0x0D &&
			PatternByte(kLoaderPattern, kBlobSizeAt) == 0x4C && PatternByte(kLoaderPattern, kBlobSizeAt + 1) == 0x69 &&
			PatternByte(kLoaderPattern, kBlobSizeAt + 2) == 0xE8 && PatternByte(kLoaderPattern, kBlockReadAt) == 0x4C &&
			PatternByte(kLoaderPattern, kBlockReadAt + 1) == 0x03 && PatternByte(kLoaderPattern, kBlockReadAt + 2) == 0x2D,
			"TweaksV: W-E-I2 holds the count's read and write, the blob size's imul and the block's add at +0, +10, +17 and +24");
		constexpr size_t kDispAt = 3;				// in each of these instructions: the disp32 after its prefix, opcode and ModRM

		// The allocation W-E-I1's call leads to begins "push rsi; push rdi; sub rsp, 28h; mov esi, edx; movsxd rdi, edx;
		// imul rcx, rdi, imm32" - the number from edx alone, times the element size, the imm32 after these bytes. Read
		// at the call's target, not looked for: a pattern of it matches twice in the game.
		constexpr uint8_t kAllocationHead[] = { 0x56, 0x57, 0x48, 0x83, 0xEC, 0x28, 0x89, 0xD6, 0x48, 0x63, 0xFA, 0x48, 0x69, 0xCF };
		constexpr size_t kAllocationRead = sizeof(kAllocationHead) + sizeof(uint32_t);	// the bytes and the element size

		// The weapon info manager's array of blobs, in the game's data
		constexpr size_t kCountOffset = 8;			// the blobs loaded, 16-bit
		constexpr size_t kCapacityOffset = 0xA;		// the room, 16-bit
		constexpr size_t kArraySize = 0xC;			// the block's pointer, the count and the capacity
		constexpr size_t kExpectedReferences = 2;	// every lea and mov store of its address: the block's store and the initializer's lea

		template <typename T> T ValueAt(const uint8_t* address)
		{
			T value{};
			memcpy(&value, address, sizeof(value));
			return value;
		}

		std::string Hex(uint32_t value)
		{
			char text[16];
			snprintf(text, sizeof(text), "0x%X", value);
			return text;
		}

		// " (W-E-I1)" - the pattern a line's site lies in
		std::string In(const GamePattern& pattern)
		{
			return std::string(" (") + pattern.label + ")";
		}

		// Why the weapon infos are not measured this session: logged, not reported - an aid (NOTES.md, T-DIAG).
		void NotMeasured(const std::string& why)
		{
			WriteLog(kTag, "DIAGNOSTIC: the weapon infos are not measured: " + why);
		}

		// ---- the measurement: the array's count and capacity, read as they are

		constexpr uint64_t kUnreadable = uint64_t{ 1 } << 32;	// in the place of the numbers

		const volatile uint8_t* g_array = nullptr;	// the weapon info manager's array of blobs
		uint32_t g_gameSize = 0;					// the room the game's code gives it: W-E-I1's two immediates

		// The count and the capacity, after IsReadable and without a lock: each 16 bits, aligned, read in one load.
		uint64_t ArrayNumbers()
		{
			if (!IsReadable(const_cast<const uint8_t*>(g_array), kArraySize))
				return kUnreadable;
			const uint16_t count = *reinterpret_cast<const volatile uint16_t*>(g_array + kCountOffset);
			const uint16_t capacity = *reinterpret_cast<const volatile uint16_t*>(g_array + kCapacityOffset);
			return count | uint64_t{ capacity } << 16;
		}

		void LogArray(uint64_t numbers)
		{
			if (numbers == kUnreadable)
			{
				WriteLog(kTag, "DIAGNOSTIC: the weapon info blob array at " + DescribeAddress(const_cast<const uint8_t*>(g_array)) + " cannot be read.");
				return;
			}
			const uint32_t count = numbers & 0xFFFF;
			const uint32_t capacity = (numbers >> 16) & 0xFFFF;
			std::string line = "DIAGNOSTIC: " + std::to_string(count) + " weapons.meta files loaded (the game's own array holds " +
				std::to_string(g_gameSize) + (capacity == 0 ? std::string(", and the array is not set up yet") : ", this one " + std::to_string(capacity)) + ")";
			if (count > g_gameSize && count <= capacity)
				line += " - the game's own array would have overflowed by " + std::to_string(count - g_gameSize) + " of them";
			if (count > capacity)
				line += " - past its end: the game's memory is already damaged";
			else if (count == capacity && capacity != 0)
				line += " - it is full: the next file would be written past its end";
			WriteLog(kTag, line + ".");
		}

		// The first summary 10 seconds in, then one a minute while the numbers change.
		constexpr Measurement kMeasurement = { "weapon infos summary", kFirstSummaryMs, &ArrayNumbers, &LogArray };
	}

	void MeasureWeaponInfos()
	{
		// 1. the two patterns, both looked for, so that the log names each one missing
		const uint8_t* const init = Scanner::Find(kInitPattern);
		const uint8_t* const loader = Scanner::Find(kLoaderPattern);
		if (!init || !loader)
		{
			NotMeasured(std::string(init ? kLoaderPattern.label : kInitPattern.label) + " was not found.");
			return;
		}

		// 2. W-E-I1's two immediates, the blobs allocated and the array's capacity: equal - the exit destructor destroys
		// as many blobs as the capacity says - and not 0; the imm16 holds no more than the 16-bit count can
		const uint8_t* const allocatedAt = init + kNumberAt + 1;
		const uint8_t* const capacityAt = init + kCapacityStoreAt + kCapacityStoreSize - sizeof(uint16_t);
		const uint32_t allocated = ValueAt<uint32_t>(allocatedAt);
		const uint32_t capacity = ValueAt<uint16_t>(capacityAt);
		WriteLog(kTag, "DIAGNOSTIC: limit at " + DescribeAddress(allocatedAt) + " holds " + std::to_string(allocated) + In(kInitPattern) +
			": the blobs the game allocates");
		WriteLog(kTag, "DIAGNOSTIC: limit at " + DescribeAddress(capacityAt) + " holds " + std::to_string(capacity) + In(kInitPattern) +
			": the array's capacity");
		if (allocated != capacity)
		{
			NotMeasured("the two limits of the weapon info blob array disagree: " + std::to_string(allocated) + " at " + DescribeAddress(allocatedAt) +
				" and " + std::to_string(capacity) + " at " + DescribeAddress(capacityAt) + " (" + kInitPattern.label + "; another mod may have changed one).");
			return;
		}
		if (allocated == 0)
		{
			NotMeasured("the limits of the weapon info blob array hold 0" + In(kInitPattern) + ".");
			return;
		}

		// 3. the call to the allocation: into the game, the allocation's first bytes there and its element size
		const uint8_t* const call = init + kCallAt;
		const uint8_t* const allocation = CallTarget(call);
		const bool readable = IsInsideGameImage(allocation) && IsReadable(allocation, kAllocationRead);
		WriteLog(kTag, "DIAGNOSTIC: allocation call at " + DescribeAddress(call) + " -> " + DescribeAddress(allocation) + In(kInitPattern) +
			(readable ? ": " + HexBytes(allocation, kAllocationRead) : std::string()));
		if (!IsInsideGameImage(allocation))
		{
			NotMeasured("the allocation call at " + DescribeAddress(call) + " is not a call into the game (another mod may have hooked it already)" +
				In(kInitPattern) + ".");
			return;
		}
		if (!readable || memcmp(allocation, kAllocationHead, sizeof(kAllocationHead)) != 0)
		{
			NotMeasured("the allocation at " + DescribeAddress(allocation) + (readable ? " does not begin with the bytes it is expected to" :
				" cannot be read") + In(kInitPattern) + ".");
			return;
		}
		const uint32_t elementSize = ValueAt<uint32_t>(allocation + sizeof(kAllocationHead));

		// 4. the block's store, which gives the array, and the capacity's store, 0xA after it: the array inside the game
		const uint8_t* const blockStore = init + kBlockStoreAt + kDispAt;
		const uint8_t* const array = Rel32Target(blockStore);
		const uint8_t* const capacityStore = init + kCapacityStoreAt + kDispAt;
		const uint8_t* const capacityField = Rel32Target(capacityStore) + sizeof(uint16_t);	// the instruction ends after its imm16
		WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(blockStore) + " -> " + DescribeAddress(array) + In(kInitPattern) +
			": the block's store - the weapon info blob array");
		WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(capacityStore) + " -> " + DescribeAddress(capacityField) + In(kInitPattern) +
			": the capacity's store");
		if (!IsInsideGameImage(array) || !IsInsideGameImage(array + kArraySize - 1))
		{
			NotMeasured("the weapon info blob array at " + DescribeAddress(array) + In(kInitPattern) + " is not inside the game.");
			return;
		}
		if (capacityField != array + kCapacityOffset)
		{
			NotMeasured("the capacity's store at " + DescribeAddress(capacityStore) + " leads to " + DescribeAddress(capacityField) +
				", not to the array's capacity at " + DescribeAddress(array + kCapacityOffset) + In(kInitPattern) + ".");
			return;
		}

		// 5. W-E-I2, the loader: its count read and written at the array + 8, its block read at the array, its blob
		// size the allocation's element size
		const struct
		{
			const uint8_t* field;
			const char* what;
			const uint8_t* expected;
		} loaderReferences[] = {
			{ loader + kCountReadAt + kDispAt, "the loader's read of the count", array + kCountOffset },
			{ loader + kCountWriteAt + kDispAt, "the loader's write of the count", array + kCountOffset },
			{ loader + kBlockReadAt + kDispAt, "the loader's read of the block", array },
		};
		for (const auto& reference : loaderReferences)
		{
			const uint8_t* const target = Rel32Target(reference.field);
			WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(reference.field) + " -> " + DescribeAddress(target) + In(kLoaderPattern) +
				": " + reference.what);
			if (target != reference.expected)
			{
				NotMeasured(std::string(reference.what) + " at " + DescribeAddress(reference.field) + " leads to " + DescribeAddress(target) +
					", not to " + DescribeAddress(reference.expected) + In(kLoaderPattern) + ".");
				return;
			}
		}
		const uint8_t* const blobSizeAt = loader + kBlobSizeAt + kDispAt;
		const uint32_t blobSize = ValueAt<uint32_t>(blobSizeAt);
		WriteLog(kTag, "DIAGNOSTIC: blob size at " + DescribeAddress(blobSizeAt) + " holds " + Hex(blobSize) + In(kLoaderPattern));
		if (blobSize != elementSize)
		{
			NotMeasured("the loader's blob size, " + Hex(blobSize) + " at " + DescribeAddress(blobSizeAt) + ", is not the allocation's element size, " +
				Hex(elementSize) + In(kLoaderPattern) + ".");
			return;
		}
		if (blobSize == 0)
		{
			NotMeasured("the blob size of the loader and the allocation is 0" + In(kLoaderPattern) + ".");
			return;
		}

		// 6. the loader's start, where the part's guard will hook it: a function start
		const uint8_t* const loaderStart = loader - kLoaderStart;
		std::string detail;
		const bool functionStart = IsFunctionStart(loaderStart, detail);
		const std::string where = DescribeAddress(loaderStart) + " (" + kLoaderPattern.label + " - " + Hex(kLoaderStart) + ")";
		WriteLog(kTag, "DIAGNOSTIC: the loader at " + where + ": " + detail);
		if (!functionStart)
		{
			NotMeasured("the loader at " + where + " is not a function start (" + detail + ").");
			return;
		}

		// 7. every lea and mov store of the array's address in the game's code: exactly the block's store, which gave the
		// array, and the initializer's lea - a second store of a block, or another reference, would show here
		const std::vector<uint8_t*> references = Scanner::FindRipRelativeReferences(array, kExpectedReferences + 1);
		for (const uint8_t* reference : references)
			WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(reference + kDispAt) + " -> " + DescribeAddress(array) + " (scan)");
		if (references.size() != kExpectedReferences)
		{
			NotMeasured("the game code holds " + std::to_string(references.size()) + (references.size() > kExpectedReferences ? " or more" : "") +
				(references.size() == 1 ? " reference" : " references") + " to the weapon info blob array, not the expected " +
				std::to_string(kExpectedReferences) + " (another mod may have changed one, or the game has changed).");
			return;
		}

		// 8. the array as TweaksV finds it: still empty - no block, capacity 0, the game's data as the game was loaded -
		// while the initializer has not run, the proof that it runs after TweaksV's start; set up already, it is
		// measured all the same
		if (!IsReadable(array, kArraySize))
		{
			NotMeasured("the weapon info blob array at " + DescribeAddress(array) + " cannot be read.");
			return;
		}
		const uint64_t block = *reinterpret_cast<const volatile uint64_t*>(array);
		const uint32_t loaded = *reinterpret_cast<const volatile uint16_t*>(array + kCountOffset);
		const uint32_t room = *reinterpret_cast<const volatile uint16_t*>(array + kCapacityOffset);
		const std::string state = " (" + std::string(block ? "a block" : "no block") + ", " + std::to_string(loaded) + " loaded, capacity " +
			std::to_string(room) + ")";
		if (block == 0 && room == 0)
			WriteLog(kTag, "DIAGNOSTIC: the weapon info blob array at " + DescribeAddress(array) + " is still empty at TweaksV's start" + state +
				": its initializer runs after TweaksV's DllMain, so the part to come can raise its limit in time.");
		else
			WriteLog(kTag, "DIAGNOSTIC: the weapon info blob array at " + DescribeAddress(array) + " is set up already at TweaksV's start" + state +
				": its initializer ran before TweaksV's DllMain, so the part to come could not raise its limit this session - measured all the same.");

		// 9. the measurement
		g_array = array;
		g_gameSize = allocated;
		WriteLog(kTag, "DIAGNOSTIC: the weapon info blob array at " + DescribeAddress(array) + In(kInitPattern) + ", " + std::to_string(allocated) +
			" blobs of " + std::to_string(elementSize) + " bytes: the weapons.meta files loaded and its room are logged after 10 seconds, then every "
			"minute while the numbers change.");
		StartMeasurement<kMeasurement>();	// one that cannot start is logged
	}
}
