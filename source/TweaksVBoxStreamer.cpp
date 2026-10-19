// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the box streamer's traversal list (see TweaksVBoxStreamer.h).
//
// The game's box streamer answers "which map boxes lie in this area" in four query functions, each of
// which loops over the asset-flag bits and calls one of two tree searches. All four hand the searches
// one static list of 1000 entries of 8 bytes, and its capacity, 1000, as the searches' fifth argument
// ("mov dword ptr [rsp+20h], 1000"); a search stops at the capacity minus 16. With heavy map mods the
// list fills up, and the boxes it has no room for are never streamed: their LODs are missing.
//   TRAVERSAL_LIST_SIZE  the new list's capacity, from the game's own 1000 up to 100000
// Eight "lea r64, [rip+disp32]" load the list, two per query function. Two of the functions are
// near-identical copies, so the second lea of each cannot be told apart by the bytes around it. Instead,
// the three capacity sites that are followed by such a lea (B-E-L2..L4) give the list's address, and the
// game's code is scanned for every lea that loads it, which must find exactly eight. Then, all or
// nothing, the eight are pointed at the new list and the four capacities set to its size.
// Written for TweaksV; the idea comes from CitizenFX's fix for FiveM and 0x-FADED's port of it to
// GTA V Legacy - none of their code is used.

#include "TweaksVBoxStreamer.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "MemScanner.h"
#include "StubMemory.h"
#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "BoxStreamer";

		constexpr size_t kEntrySize = 8;			// one entry of the list
		constexpr uint32_t kMaxListSize = 100000;	// the new list always has this many entries, whatever TRAVERSAL_LIST_SIZE says
		static_assert(static_cast<int>(kMaxListSize) == kTraversalListSize.maxValue, "TweaksV: TRAVERSAL_LIST_SIZE's maximum must be the list's size");
		constexpr size_t kListReferences = 8;		// the lea instructions that load the list - every one of them, on 1.0.1158.16
		constexpr size_t kLeaSize = 7;				// REX.W, 8D, ModRM, disp32

		// A capacity site: where the imm32 of "mov dword ptr [rsp+20h], imm32" lies in the pattern's match
		// and, for the three sites followed by "lea r13, [rip+disp32]", where that lea's rel32 lies (0: not
		// followed by one).
		struct CapacitySite
		{
			const GamePattern* pattern;
			int capacityOffset;
			int referenceOffset;
		};

		constexpr CapacitySite kCapacitySites[] = {
			{ &Pattern("B-E-L1"), 13, 0 },
			{ &Pattern("B-E-L2"), 15, 22 },
			{ &Pattern("B-E-L3"), 13, 20 },
			{ &Pattern("B-E-L4"), 11, 18 },
		};

		// Logs why and reports for the start-up summary; the caller then leaves the game as it is.
		void Fail(const std::string& why)
		{
			WriteLog(kTag, why);
			ReportFailure(kTag, "The box streamer's traversal list is not enlarged this session: a required step failed "
				"(details in TweaksV.log).");
		}

		// Where the rel32 at address leads: the address right after it, plus the rel32.
		const uint8_t* Rel32Target(const uint8_t* address)
		{
			int32_t rel = 0;
			memcpy(&rel, address, sizeof(rel));
			return address + sizeof(rel) + rel;
		}
	}

	void InitBoxStreamer()
	{
		// 1. the four capacity sites: every one must be the expected instruction and hold the same size -
		// the game's own - and the lea after three of them must load the same list
		uint8_t* capacities[std::size(kCapacitySites)] = {};
		uint32_t gameSize = 0;
		const uint8_t* list = nullptr;
		for (size_t i = 0; i < std::size(kCapacitySites); i++)
		{
			const CapacitySite& site = kCapacitySites[i];
			uint8_t* const match = MemScanner::Find(*site.pattern);
			if (!match)
			{
				Fail(std::string("A capacity of the box streamer's traversal list was not found in the game code: ") + site.pattern->label);
				return;
			}
			uint8_t* const capacity = match + site.capacityOffset;
			if (memcmp(capacity - 4, "\xC7\x44\x24\x20", 4) != 0)	// mov dword ptr [rsp+20h], imm32
			{
				Fail("The capacity of the box streamer's traversal list at " + DescribeAddress(capacity) +
					" is not a \"mov dword ptr [rsp+20h], imm32\" (" + site.pattern->label + ").");
				return;
			}
			uint32_t value = 0;
			memcpy(&value, capacity, sizeof(value));
			if (IsDiagnosticMode())
				WriteLog(kTag, "DIAGNOSTIC: capacity at " + DescribeAddress(capacity) + " holds " + std::to_string(value));
			if (gameSize == 0)
				gameSize = value;
			else if (value != gameSize)
			{
				Fail("The capacities of the box streamer's traversal list disagree about the game's own size: " +
					std::to_string(gameSize) + " and " + std::to_string(value) + " (" + site.pattern->label + ").");
				return;
			}
			capacities[i] = capacity;

			if (site.referenceOffset == 0)
				continue;
			const uint8_t* const reference = match + site.referenceOffset;
			if (memcmp(reference - 3, "\x4C\x8D\x2D", 3) != 0)	// lea r13, [rip+disp32]
			{
				Fail("The reference to the box streamer's traversal list at " + DescribeAddress(reference) +
					" is not a \"lea r13, [rip+disp32]\" (" + site.pattern->label + ").");
				return;
			}
			const uint8_t* const target = Rel32Target(reference);
			if (IsDiagnosticMode())
				WriteLog(kTag, "DIAGNOSTIC: reference at " + DescribeAddress(reference) + " -> " + DescribeAddress(target));
			if (!list)
				list = target;
			else if (target != list)
			{
				Fail("The references to the box streamer's traversal list do not all point to the same list: " +
					DescribeAddress(list) + " and " + DescribeAddress(target) + " (" + site.pattern->label + ").");
				return;
			}
		}
		if (gameSize == 0 || gameSize > kMaxListSize)
		{
			Fail("The game's own box streamer traversal list size, " + std::to_string(gameSize) + ", is not plausible.");
			return;
		}
		if (!IsInsideGameImage(list))	// the game's own list lies inside the game
		{
			Fail("The box streamer's traversal list is not inside the game: " + DescribeAddress(list) +
				" (another mod may have moved it already).");
			return;
		}
		if (IsDiagnosticMode())
			WriteLog(kTag, "DIAGNOSTIC: the traversal list is at " + DescribeAddress(list) + " - " + std::to_string(gameSize) +
				" entries of " + std::to_string(kEntrySize) + " bytes");

		// 2. every lea that loads the list: the game has exactly kListReferences of them - fewer means
		// another mod has changed some, more that the game has changed
		const std::vector<uint8_t*> references = MemScanner::FindRipRelativeLeas(list, kListReferences + 1);
		if (IsDiagnosticMode())
			for (const uint8_t* reference : references)
				WriteLog(kTag, "DIAGNOSTIC: the list is loaded at " + DescribeAddress(reference));
		if (references.size() != kListReferences)
		{
			Fail("The game code loads the box streamer's traversal list in " + std::to_string(references.size()) +
				(references.size() > kListReferences ? " or more" : "") + " places, not the expected " +
				std::to_string(kListReferences) + " (another mod may have moved some of them, or the game has changed).");
			return;
		}

		// 3. the size to use: [the game's own size, kMaxListSize] - smaller would shrink the game's list
		const uint32_t requested = static_cast<uint32_t>(GetNumber(kTraversalListSize));
		uint32_t size = requested;
		if (size < gameSize)
			size = gameSize;
		if (size > kMaxListSize)
			size = kMaxListSize;
		if (size != requested)
			WriteLog(kTag, "TweaksV.ini: TRAVERSAL_LIST_SIZE=" + std::to_string(requested) + " is outside " + std::to_string(gameSize) +
				".." + std::to_string(kMaxListSize) + " - using " + std::to_string(size) + ".");

		if (IsDryRun())	// everything found and checked, nothing written
		{
			WriteLog(kTag, "DRY_RUN: would move the box streamer's traversal list (" + std::to_string(references.size()) +
				" references) and raise its capacity from " + std::to_string(gameSize) + " to " + std::to_string(size) + " (" +
				std::to_string(std::size(capacities)) + " sites).");
			return;
		}

		// 4. the new list: always the full kMaxListSize entries, read/write data near the game (a lea
		// reaches +-2 GB), zeroed by Windows
		uint8_t* const newList = static_cast<uint8_t*>(AllocateStubMemory(kEntrySize * kMaxListSize, PAGE_READWRITE));
		if (!newList)
		{
			Fail("The new box streamer traversal list could not be allocated near the game.");
			return;
		}

		// 5. one write, all or nothing: the eight references to the new list, the four capacities to its size
		std::vector<CodeWrite> writes;
		for (uint8_t* reference : references)
		{
			uint8_t* const rel32 = reference + kLeaSize - sizeof(int32_t);
			const intptr_t distance = newList - (rel32 + sizeof(int32_t));
			if (distance < INT32_MIN || distance > INT32_MAX)
			{
				VirtualFree(newList, 0, MEM_RELEASE);
				Fail("The new box streamer traversal list is out of reach of the game code at " + DescribeAddress(reference) + ".");
				return;
			}
			writes.push_back({ rel32, BytesOf(static_cast<int32_t>(distance)), "box streamer list reference" });
		}
		for (uint8_t* capacity : capacities)
			writes.push_back({ capacity, BytesOf(size), "box streamer list capacity" });
		if (!WriteCode(writes, kTag))
		{
			VirtualFree(newList, 0, MEM_RELEASE);	// nothing points at it: WriteCode wrote nothing
			Fail("The game code could not be changed.");
			return;
		}

		WriteLog(kTag, "Box streamer traversal list moved and its capacity raised from " + std::to_string(gameSize) + " to " +
			std::to_string(size) + " entries.");
	}
}
