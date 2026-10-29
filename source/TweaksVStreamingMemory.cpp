// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the streaming memory, measured first (see TweaksVStreamingMemory.h).
//
// The game streams the map, the props and every other streamed resource into a heap of its own, the resource
// heap. It starts with one region and adds one whenever none of its regions can serve an allocation, up to 32
// regions of the size its constructor asks for, 1024 MB, capped at 256 MB - 8 GB in all on 1.0.1158.16 - each
// committed in full when it is made and never given back, so the number of regions is its own high-water mark.
// With all 32 made, a streamed resource that does not fit is refused once the game has evicted what it can, and
// the game stamps the moment, its timeGetTime(), in a dword. The streaming allocator's two budgets - the sizes the
// Legacy game's adjusters raised - are set to the heap's size at every streaming update and nothing counts
// against them, so on this build they do not bind: the heap does (PATTERNS.md, "Resources"). Before the Resources
// part may raise the size of the heap's regions, this module measures how full a mod load gets the heap. The one
// switch it reads is DIAGNOSTIC_MODE, through the table of the parts measured first, which starts it with
// DIAGNOSTIC_MODE=1 alone:
//   Q-E-H1  the heap, its page size and the region size asked, and the call of its constructor
//   Q-E-A1  the streaming allocator - its budgets and what is counted against them - and the call of the
//           budgets' reset
//   Q-E-S1  the refusal stamp, and the call of the game's timeGetTime
//   Q-E-R1  the cap on a region's size, 256 MB in two immediates, which the part's writing half would raise
// Each is found once and checked - the three calls lead into the game, the three objects lie inside it and can be
// read - and logged with what it holds; then, 10 seconds in and every minute while the numbers change, the
// regions made of 32, their sizes and bytes in use, the streamed resources' bytes, the budgets with what is
// counted against them, and the refusals since the last summary - the stamp against GetTickCount's value, which
// counts the milliseconds since Windows started as timeGetTime does. Nothing is written, called or hooked, so
// DRY_RUN changes nothing here: the game's own data is read as it is, every field within its object and after
// IsReadable, without a lock. A pattern missing, a site not as expected, an address outside the game or a thread
// that cannot start is logged, not reported - an aid (NOTES.md, T-DIAG) - and nothing more is measured.
// Written for TweaksV from the findings of its own research in the running game's code (1.0.1158.16,
// 2026-10-29); no code of anyone's was read or used.

#include "TweaksVStreamingMemory.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

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
		constexpr const char* kTag = "Resources";

		constexpr uint64_t kKilobyte = 1024;
		constexpr uint64_t kMegabyte = 1024 * kKilobyte;
		constexpr size_t kCallSize = 5;				// E8 rel32

		// The imm32 a pattern pins at index, little-endian, or -1 when a byte of it is a wildcard - for the checks,
		// when TweaksV is compiled, that a value the code reads is the one its pattern holds.
		consteval int64_t PatternImm32(const GamePattern& pattern, size_t index)
		{
			int64_t value = 0;
			for (size_t i = 0; i < 4; i++)
			{
				const int byte = PatternByte(pattern, index + i);
				if (byte < 0)
					return -1;
				value |= static_cast<int64_t>(byte) << (8 * i);
			}
			return value;
		}

		// ---- the sites (PATTERNS.md, "Resources")

		// Q-E-H1, in the game's start-up: "lea rcx, [rip+disp32]" of the resource heap, "mov edx, imm32" its page
		// size, "mov r8d, imm32" the region size asked, then the call of its constructor, whose rel32 lies beyond the
		// match
		constexpr const GamePattern& kHeapPattern = Pattern("Q-E-H1");
		constexpr size_t kHeapLeaAt = 0;
		constexpr size_t kPageSizeAt = 8;
		constexpr size_t kRegionAskedAt = 14;
		constexpr size_t kConstructorCallAt = 18;
		static_assert(PatternByte(kHeapPattern, kHeapLeaAt) == 0x48 && PatternByte(kHeapPattern, kHeapLeaAt + 1) == 0x8D &&
			PatternByte(kHeapPattern, kHeapLeaAt + 2) == 0x0D && PatternByte(kHeapPattern, kPageSizeAt - 1) == 0xBA &&
			PatternByte(kHeapPattern, kRegionAskedAt - 2) == 0x41 && PatternByte(kHeapPattern, kRegionAskedAt - 1) == 0xB8 &&
			PatternByte(kHeapPattern, kConstructorCallAt) == 0xE8,
			"TweaksV: Q-E-H1 holds the heap's lea at +0, its page size at +8, the region size asked at +14 and the call at +18");

		// Q-E-A1, at the end of the streaming update: "lea rcx, [rip+disp32]" of the streaming allocator, then the
		// call of the budgets' reset
		constexpr const GamePattern& kAllocatorPattern = Pattern("Q-E-A1");
		constexpr size_t kAllocatorLeaAt = 0;
		constexpr size_t kResetCallAt = 7;
		static_assert(PatternByte(kAllocatorPattern, kAllocatorLeaAt) == 0x48 && PatternByte(kAllocatorPattern, kAllocatorLeaAt + 1) == 0x8D &&
			PatternByte(kAllocatorPattern, kAllocatorLeaAt + 2) == 0x0D && PatternByte(kAllocatorPattern, kResetCallAt) == 0xE8,
			"TweaksV: Q-E-A1 holds the allocator's lea at +0 and the reset's call at +7");

		// Q-E-S1, in the loader's step after a refused allocation: the call of the game's timeGetTime, then
		// "mov [rip+disp32], eax" of the refusal stamp
		constexpr const GamePattern& kStampPattern = Pattern("Q-E-S1");
		constexpr size_t kClockCallAt = 7;
		constexpr size_t kStampMovAt = 12;
		static_assert(PatternByte(kStampPattern, kClockCallAt) == 0xE8 && PatternByte(kStampPattern, kStampMovAt) == 0x89 &&
			PatternByte(kStampPattern, kStampMovAt + 1) == 0x05, "TweaksV: Q-E-S1 holds the clock's call at +7 and the stamp's mov at +12");

		// Q-E-R1, in the heap's function that makes a region: "mov rbx, [r15+0x1D8]", the region size asked;
		// "cmp rbx, imm32" and "mov eax, imm32", the cap, 256 MB in both; then the floor the size is halved down to
		// while Windows refuses a region. The pattern pins the field and both caps, so a cap another mod has set
		// makes it not found: the part's writing half, which leaves such a cap as it is, needs those bytes as
		// wildcards.
		constexpr const GamePattern& kCapPattern = Pattern("Q-E-R1");
		constexpr size_t kRegionFieldAt = 3;
		constexpr size_t kCapCmpAt = 10;
		constexpr size_t kCapMovAt = 15;
		constexpr size_t kFloorAt = 26;
		constexpr int64_t kRegionField = 0x1D8;		// the heap's region size asked
		constexpr int64_t kGameCap = 0x10000000;		// 256 MB
		static_assert(PatternByte(kCapPattern, kRegionFieldAt - 1) == 0x9F && PatternImm32(kCapPattern, kRegionFieldAt) == kRegionField &&
			PatternByte(kCapPattern, kCapCmpAt - 1) == 0xFB && PatternImm32(kCapPattern, kCapCmpAt) == kGameCap &&
			PatternByte(kCapPattern, kCapMovAt - 1) == 0xB8 && PatternImm32(kCapPattern, kCapMovAt) == kGameCap &&
			PatternByte(kCapPattern, kFloorAt - 1) == 0xFB && PatternImm32(kCapPattern, kFloorAt) >= 0,
			"TweaksV: Q-E-R1 holds the field 0x1D8 at +3, the game's cap in both immediates at +10 and +15 and the floor at +26");

		// ---- the game's data (PATTERNS.md, "When Game Updates" item 5)

		// The resource heap's object, 0x3FF0 bytes up to the next heap: the count of its regions, a dword at +0x1E4;
		// per region i < 32 its own allocator, 0x1E0 bytes at +0x1E8 + i x 0x1E0, and its memory, 0 while the region
		// is not made, at +0x3EE8 + i x 8. A region's allocator: its pages in use at +0x28, its page size at +0x1D0,
		// its size at +0x1D8, qwords.
		constexpr size_t kHeapObjectSize = 0x3FF0;
		constexpr size_t kRegionsMadeAt = 0x1E4;
		constexpr size_t kRegionAllocatorsAt = 0x1E8;
		constexpr size_t kRegionAllocatorSize = 0x1E0;
		constexpr size_t kRegionMemoryAt = 0x3EE8;
		constexpr uint32_t kMaxRegions = 32;
		constexpr size_t kPagesInUseAt = 0x28;
		constexpr size_t kRegionPageSizeAt = 0x1D0;
		constexpr size_t kRegionSizeAt = 0x1D8;
		static_assert(kRegionAllocatorsAt + kMaxRegions * kRegionAllocatorSize <= kRegionMemoryAt && kRegionMemoryAt + kMaxRegions * 8 <= kHeapObjectSize &&
			kRegionSizeAt + 8 <= kRegionAllocatorSize, "TweaksV: the resource heap's fields lie inside its object");

		// The streaming allocator, 0x38 bytes up to the next object: the virtual budget at +0x08 and what is counted
		// against it at +0x10, the physical budget at +0x18 and what is counted against it at +0x20, the streaming
		// engine's bytes in the heap at +0x30, qwords.
		constexpr size_t kAllocatorSize = 0x38;
		constexpr size_t kBudgetAt[2] = { 0x08, 0x18 };		// virtual, physical
		constexpr size_t kUsedAt[2] = { 0x10, 0x20 };
		constexpr size_t kStreamedAt = 0x30;
		static_assert(kStreamedAt + 8 <= kAllocatorSize, "TweaksV: the streaming allocator's fields lie inside it");

		// The refusal stamp: a dword, the game's timeGetTime() at the last streamed resource refused for memory, 0
		// before the first.
		constexpr size_t kStampSize = sizeof(uint32_t);

		uint32_t Imm32At(const uint8_t* address)
		{
			uint32_t value = 0;
			memcpy(&value, address, sizeof(value));
			return value;
		}

		// "8 KB", "256 MB" - for the site lines
		std::string SizeText(uint64_t bytes)
		{
			if (bytes != 0 && bytes % kMegabyte == 0)
				return std::to_string(bytes / kMegabyte) + " MB";
			if (bytes != 0 && bytes % kKilobyte == 0)
				return std::to_string(bytes / kKilobyte) + " KB";
			return std::to_string(bytes) + " bytes";
		}

		// "0x1D8" - an offset an instruction carries, for the site lines
		std::string HexText(uint32_t value)
		{
			char text[16];
			snprintf(text, sizeof(text), "0x%X", value);
			return text;
		}

		// "RVA 0x13F5C0", or "not a call" for a call that cannot be read
		std::string CallText(const uint8_t* target)
		{
			return target ? DescribeAddress(target) : std::string("not a call");
		}

		// ---- the measurement (DIAGNOSTIC_MODE): the game's data, read as it is

		const uint8_t* g_heap = nullptr;
		const uint8_t* g_allocator = nullptr;
		const uint8_t* g_stamp = nullptr;
		uint64_t g_heapMost = 0;	// the most the heap holds: 32 regions of the size asked, capped, less a page each

		// The game changes these while the measurement reads them, under its own lock: each is read once, as it is.
		uint64_t Qword(const uint8_t* object, size_t at)
		{
			return *reinterpret_cast<const volatile uint64_t*>(object + at);
		}

		uint32_t Dword(const uint8_t* object, size_t at)
		{
			return *reinterpret_cast<const volatile uint32_t*>(object + at);
		}

		// Sums and products of the values read, which a game update could make anything: they stop at UINT64_MAX.
		uint64_t Add(uint64_t a, uint64_t b)
		{
			return a > UINT64_MAX - b ? UINT64_MAX : a + b;
		}

		uint64_t Multiply(uint64_t a, uint64_t b)
		{
			return b != 0 && a > UINT64_MAX / b ? UINT64_MAX : a * b;
		}

		// Bytes in whole MB, rounded to the nearest.
		uint64_t Megabytes(uint64_t bytes)
		{
			return bytes / kMegabyte + (bytes % kMegabyte >= kMegabyte / 2 ? 1 : 0);
		}

		std::string Mb(uint64_t bytes)
		{
			return std::to_string(Megabytes(bytes));
		}

		// One summary's reading of the game's data, made by ReadStreamingMemory and written by LogSummary, both on
		// the measurement's thread, one right after the other (TweaksVMeasure.h); no other thread touches it.
		enum class Unreadable : uint8_t { None, Heap, Allocator, Stamp };
		struct Reading
		{
			Unreadable unreadable = Unreadable::None;
			uint32_t regions = 0;			// the regions made: the slots whose memory is not 0
			uint32_t counted = 0;			// the heap's own count of them
			uint64_t size = 0;				// their sizes, summed
			uint64_t inUse = 0;				// their bytes in use
			uint64_t streamed = 0;			// the streaming engine's bytes in the heap
			uint64_t budget[2] = {};		// the virtual and the physical budget
			uint64_t used[2] = {};			// what is counted against each
			uint32_t stamp = 0;				// timeGetTime() at the last refusal, 0 for none
			uint32_t now = 0;				// GetTickCount's value at the reading
		};
		Reading g_reading;
		uint32_t g_lastStamp = 0;			// the stamp at the last summary
		bool g_summarised = false;

		// Reads the heap, the allocator and the stamp, each after IsReadable; what decides whether a summary is
		// written comes back: the regions made, the bytes in use in MB and the stamp.
		uint64_t ReadStreamingMemory()
		{
			Reading reading;
			if (!IsReadable(g_heap, kHeapObjectSize))
				reading.unreadable = Unreadable::Heap;
			else if (!IsReadable(g_allocator, kAllocatorSize))
				reading.unreadable = Unreadable::Allocator;
			else if (!IsReadable(g_stamp, kStampSize))
				reading.unreadable = Unreadable::Stamp;
			else
			{
				reading.counted = Dword(g_heap, kRegionsMadeAt);
				for (uint32_t region = 0; region < kMaxRegions; region++)
				{
					if (Qword(g_heap, kRegionMemoryAt + region * sizeof(uint64_t)) == 0)
						continue;	// not made
					const uint8_t* const allocator = g_heap + kRegionAllocatorsAt + region * kRegionAllocatorSize;
					reading.regions++;
					reading.size = Add(reading.size, Qword(allocator, kRegionSizeAt));
					reading.inUse = Add(reading.inUse, Multiply(Qword(allocator, kPagesInUseAt), Qword(allocator, kRegionPageSizeAt)));
				}
				for (size_t i = 0; i < 2; i++)
				{
					reading.budget[i] = Qword(g_allocator, kBudgetAt[i]);
					reading.used[i] = Qword(g_allocator, kUsedAt[i]);
				}
				reading.streamed = Qword(g_allocator, kStreamedAt);
				reading.stamp = Dword(g_stamp, 0);
				reading.now = static_cast<uint32_t>(GetTickCount64());	// GetTickCount's value, without its 49-day trap
			}
			g_reading = reading;
			const uint64_t regions = reading.unreadable == Unreadable::None ? reading.regions : kMaxRegions + static_cast<uint64_t>(reading.unreadable);
			return regions | std::min<uint64_t>(Megabytes(reading.inUse), 0x3FFFFFF) << 6 | uint64_t{ reading.stamp } << 32;
		}

		void LogSummary(uint64_t)
		{
			const Reading& reading = g_reading;
			if (reading.unreadable != Unreadable::None)
			{
				const bool heap = reading.unreadable == Unreadable::Heap, allocator = reading.unreadable == Unreadable::Allocator;
				WriteLog(kTag, std::string("DIAGNOSTIC: streaming memory: the ") + (heap ? "resource heap" : allocator ? "streaming allocator" : "refusal stamp") +
					" at " + DescribeAddress(heap ? g_heap : allocator ? g_allocator : g_stamp) + " cannot be read.");
				return;
			}
			std::string line = "DIAGNOSTIC: streaming memory: the resource heap has " + std::to_string(reading.regions) + " of its " +
				std::to_string(kMaxRegions) + " regions";
			if (reading.counted != reading.regions)	// a game update that moved a field, or a region made between two reads
				line += " (its own count: " + std::to_string(static_cast<int32_t>(reading.counted)) + ")";
			line += ", " + Mb(reading.size) + " of at most " + Mb(g_heapMost) + " MB, " + Mb(reading.inUse) + " MB of them in use, " + Mb(reading.streamed) +
				" MB by streamed resources; the streaming budgets " + Mb(reading.budget[0]) + " MB virtual and " + Mb(reading.budget[1]) + " MB physical, ";
			if (reading.used[0] == 0 && reading.used[1] == 0)
				line += "nothing counted against them; ";
			else	// what makes the budgets bind: on 1.0.1158.16 nothing counts against them
				line += Mb(reading.used[0]) + " MB and " + Mb(reading.used[1]) + " MB counted against them - so on this build they may bind "
					"(PATTERNS.md, \"Resources\"); ";
			if (reading.stamp == 0)
				line += "no streamed resource refused for memory so far.";
			else
			{
				uint32_t ago = reading.now - reading.stamp;	// both count milliseconds since Windows started, modulo 2^32
				if (ago >= 0x80000000u)
					ago = 0;	// a stamp a little ahead of GetTickCount's coarser clock
				const std::string when = "the last " + std::to_string(ago / 1000) + " s ago";
				if (g_summarised && reading.stamp == g_lastStamp)
					line += "no streamed resource refused for memory since the last summary, " + when + ".";
				else
					line += std::string("streamed resources were refused for memory ") + (g_summarised ? "since the last summary" : "before this summary") +
						", " + when + (reading.regions >= kMaxRegions ? " - the heap has all " + std::to_string(kMaxRegions) + " of its regions." :
						" - the heap still had room, so another budget refused them.");
			}
			g_lastStamp = reading.stamp;
			g_summarised = true;
			WriteLog(kTag, line);
		}

		// The first summary 10 seconds in, then one a minute while the regions, the bytes in use or the stamp change.
		constexpr Measurement kMeasurement = { "streaming memory summary", kFirstSummaryMs, &ReadStreamingMemory, &LogSummary };

		// Why nothing is measured: an aid's failure, logged, not reported (NOTES.md, T-DIAG).
		void NotMeasured(const std::string& why)
		{
			WriteLog(kTag, "DIAGNOSTIC: the streaming memory is not measured: " + why);
		}

		// A call of the game's own that must lead into the game; false, logged, otherwise.
		bool CallsIntoGame(const uint8_t* call, const uint8_t* target, const char* what, const GamePattern& pattern)
		{
			if (target && IsInsideGameImage(target))
				return true;
			NotMeasured(std::string("the ") + what + " call at " + DescribeAddress(call) + " is not a call into the game (another mod may have hooked it "
				"already) (" + pattern.label + ").");
			return false;
		}

		// An object of the game's that the measurement reads: inside the game, every byte of it, and readable;
		// false, logged, otherwise.
		bool CanBeRead(const uint8_t* object, size_t size, const char* what, const GamePattern& pattern)
		{
			if (!IsInsideGameImage(object) || !IsInsideGameImage(object + size - 1))
			{
				NotMeasured(std::string("the ") + what + " at " + DescribeAddress(object) + " (" + pattern.label + ") is not inside the game.");
				return false;
			}
			if (!IsReadable(object, size))
			{
				NotMeasured(std::string("the ") + what + " at " + DescribeAddress(object) + " (" + pattern.label + ") cannot be read.");
				return false;
			}
			return true;
		}
	}

	void MeasureStreamingMemory()
	{
		// 1. the four sites, each looked for, so that the log names each one missing
		const uint8_t* const heapSite = Scanner::Find(kHeapPattern);
		const uint8_t* const allocatorSite = Scanner::Find(kAllocatorPattern);
		const uint8_t* const stampSite = Scanner::Find(kStampPattern);
		const uint8_t* const capSite = Scanner::Find(kCapPattern);
		const GamePattern* const missing = !heapSite ? &kHeapPattern : !allocatorSite ? &kAllocatorPattern : !stampSite ? &kStampPattern :
			!capSite ? &kCapPattern : nullptr;
		if (missing)
		{
			NotMeasured(std::string(missing->label) + " was not found.");
			return;
		}

		// 2. what each holds, logged before anything is checked, so that the log shows every site
		const uint8_t* const heap = Rel32Target(heapSite + kHeapLeaAt + 3);
		const uint32_t pageSize = Imm32At(heapSite + kPageSizeAt);
		const uint32_t regionAsked = Imm32At(heapSite + kRegionAskedAt);
		const uint8_t* const constructorCall = heapSite + kConstructorCallAt;
		const uint8_t* const constructor = IsReadable(constructorCall, kCallSize) ? CallTarget(constructorCall) : nullptr;
		WriteLog(kTag, "DIAGNOSTIC: resource heap reference at " + DescribeAddress(heapSite + kHeapLeaAt + 3) + " -> " + DescribeAddress(heap) +
			", page size at " + DescribeAddress(heapSite + kPageSizeAt) + " holds " + SizeText(pageSize) + ", region size asked at " +
			DescribeAddress(heapSite + kRegionAskedAt) + " holds " + SizeText(regionAsked) + ", constructor call at " + DescribeAddress(constructorCall) +
			" -> " + CallText(constructor) + " (" + kHeapPattern.label + ")");

		const uint8_t* const allocator = Rel32Target(allocatorSite + kAllocatorLeaAt + 3);
		const uint8_t* const resetCall = allocatorSite + kResetCallAt;
		const uint8_t* const reset = CallTarget(resetCall);
		WriteLog(kTag, "DIAGNOSTIC: streaming allocator reference at " + DescribeAddress(allocatorSite + kAllocatorLeaAt + 3) + " -> " +
			DescribeAddress(allocator) + ", budgets' reset call at " + DescribeAddress(resetCall) + " -> " + CallText(reset) + " (" +
			kAllocatorPattern.label + ")");

		const uint8_t* const stamp = Rel32Target(stampSite + kStampMovAt + 2);
		const uint8_t* const clockCall = stampSite + kClockCallAt;
		const uint8_t* const clock = CallTarget(clockCall);
		WriteLog(kTag, "DIAGNOSTIC: refusal stamp reference at " + DescribeAddress(stampSite + kStampMovAt + 2) + " -> " + DescribeAddress(stamp) +
			", timeGetTime call at " + DescribeAddress(clockCall) + " -> " + CallText(clock) + " (" + kStampPattern.label + ")");

		const uint32_t cap = Imm32At(capSite + kCapCmpAt);
		WriteLog(kTag, "DIAGNOSTIC: region size cap at " + DescribeAddress(capSite + kCapCmpAt) + " holds " + SizeText(cap) + " and at " +
			DescribeAddress(capSite + kCapMovAt) + " holds " + SizeText(Imm32At(capSite + kCapMovAt)) + ", region size field at " +
			DescribeAddress(capSite + kRegionFieldAt) + " holds " + HexText(Imm32At(capSite + kRegionFieldAt)) + ", region size floor at " +
			DescribeAddress(capSite + kFloorAt) + " holds " + SizeText(Imm32At(capSite + kFloorAt)) + " (" + kCapPattern.label + ")");

		// 3. the checks: the three calls lead into the game, the three objects lie inside it and can be read
		if (!CallsIntoGame(constructorCall, constructor, "resource heap's constructor", kHeapPattern) ||
			!CallsIntoGame(resetCall, reset, "budgets' reset", kAllocatorPattern) || !CallsIntoGame(clockCall, clock, "timeGetTime", kStampPattern) ||
			!CanBeRead(heap, kHeapObjectSize, "resource heap", kHeapPattern) || !CanBeRead(allocator, kAllocatorSize, "streaming allocator", kAllocatorPattern) ||
			!CanBeRead(stamp, kStampSize, "refusal stamp", kStampPattern))
			return;

		// 4. the most the heap holds: 32 regions of the size asked, capped, each a page less than that, as the game
		// sizes a region
		const uint64_t regionSize = std::min<uint64_t>(regionAsked, cap);
		const uint64_t regionPages = pageSize != 0 ? regionSize / pageSize : 0;
		g_heapMost = regionPages > 0 ? kMaxRegions * (regionPages - 1) * pageSize : 0;
		g_heap = heap;
		g_allocator = allocator;
		g_stamp = stamp;
		WriteLog(kTag, "DIAGNOSTIC: the resource heap at " + DescribeAddress(heap) + " (" + kHeapPattern.label + "), pages of " + SizeText(pageSize) +
			", regions of " + SizeText(regionSize) + ", " + std::to_string(kMaxRegions) + " at most; the streaming allocator at " + DescribeAddress(allocator) +
			" (" + kAllocatorPattern.label + "); the refusal stamp at " + DescribeAddress(stamp) + " (" + kStampPattern.label + "): the regions, the bytes "
			"in use and the refusals are logged after 10 seconds, then every minute while they change.");
		StartMeasurement<kMeasurement>();	// one that cannot start is logged
	}
}
