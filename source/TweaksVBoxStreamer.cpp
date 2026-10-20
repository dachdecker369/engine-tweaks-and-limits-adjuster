// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the box streamer's traversal list (see TweaksVBoxStreamer.h).
//
// The game's box streamer answers "which map boxes lie in this area" in four query functions, each of
// which loops over the asset-flag bits and calls one of two tree searches. All four hand the searches
// one static list of 1000 entries of 8 bytes, and its capacity, 1000, as the searches' fifth argument
// ("mov dword ptr [rsp+20h], 1000"); a search stops at the capacity minus 16. With heavy map mods the
// list fills up, and the boxes it has no room for are never streamed: their LODs are missing.
//   TRAVERSAL_LIST_SIZE  the new list's capacity, from the game's own 1000 up to 100000
//   DIAGNOSTIC_MODE      the measurement: the four calls to the tree searches go through TweaksV's call
//                        stubs to a wrapper that only counts; 10 seconds in and then every minute, the
//                        log says how many searches ran, the most entries one returned and how many
//                        returned more than the game's own list could hold - the number that says
//                        whether a map load needs the larger list at all
// Eight "lea r64, [rip+disp32]" load the list, two per query function. Two of the functions are
// near-identical copies, so the second lea of each cannot be told apart by the bytes around it. Instead
// the relocation core takes the list's address from the lea after three of the capacity sites
// (B-E-L2..L4) and scans the game's code for every lea that loads it, which must find exactly eight;
// the four capacities are the limit sites. Then, all or nothing, the eight are pointed at the new list
// and the four capacities set to its size - and, with DIAGNOSTIC_MODE, the four calls redirected.
// Written for TweaksV; the idea comes from CitizenFX's fix for FiveM and 0x-FADED's port of it to
// GTA V Legacy - none of their code is used.

#include "TweaksVBoxStreamer.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVRelocation.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "BoxStreamer";

		constexpr size_t kEntrySize = 8;			// one entry of the list
		constexpr uint32_t kMaxListSize = 100000;	// the new list always has this many entries, whatever TRAVERSAL_LIST_SIZE says
		static_assert(kMaxListSize == static_cast<uint32_t>(kTraversalListSize.maxValue), "TweaksV: TRAVERSAL_LIST_SIZE's maximum must be the list's size");
		constexpr size_t kListReferences = 8;		// the lea instructions that load the list - every one of them, on 1.0.1158.16
		constexpr size_t kTreeSearches = 2;			// the tree searches the query functions call
		constexpr int kSearchHeadroom = 16;			// a search stops this many entries short of the capacity
		constexpr size_t kCallSize = 5;				// "call rel32"
		constexpr DWORD kFirstSummaryMs = 10000;	// DIAGNOSTIC_MODE: the first summary, then one every minute
		constexpr DWORD kSummaryEveryMs = 60000;

		constexpr char kMovCapacity[] = "\xC7\x44\x24\x20";	// mov dword ptr [rsp+20h], imm32
		constexpr char kLeaR13[] = "\x4C\x8D\x2D";			// lea r13, [rip+disp32]

		// The four capacity sites: the imm32 of the mov, and for the three followed by the lea of the list,
		// its rel32. The call to the tree search after each site, and which of the two searches it calls,
		// is the part's own: kCalls, in the same order.
		constexpr Anchor kAnchors[] = {
			Limit(Pattern("B-E-L1"), Imm32(9, kMovCapacity, 4, 13, 1, 0)),
			ReferenceAndLimit(Pattern("B-E-L2"), Rel32(22, kLeaR13, 3), Imm32(11, kMovCapacity, 4, 15, 1, 0)),
			ReferenceAndLimit(Pattern("B-E-L3"), Rel32(20, kLeaR13, 3), Imm32(9, kMovCapacity, 4, 13, 1, 0)),
			ReferenceAndLimit(Pattern("B-E-L4"), Rel32(18, kLeaR13, 3), Imm32(7, kMovCapacity, 4, 11, 1, 0)),
		};
		struct TreeSearchCall { size_t offset; size_t search; };
		constexpr TreeSearchCall kCalls[] = { { 30, 0 }, { 34, 1 }, { 42, 0 }, { 30, 1 } };
		static_assert(std::size(kCalls) == std::size(kAnchors), "TweaksV: one tree-search call per capacity site");

		constexpr RelocationSpec kSpec = { kTag, "box streamer's traversal list", "box streamer traversal list size",
			"The box streamer's traversal list is not enlarged", kEntrySize, kMaxListSize, &kTraversalListSize, kAnchors, kListReferences, nullptr, 0 };

		// ---- the measurement (DIAGNOSTIC_MODE)

		// The tree searches' arguments: the tree, the area's two corners, the list and its capacity; the
		// result is the number of entries written (0 or less: none).
		using TreeSearch = int (*)(void* tree, const void* min, const void* max, void* list, int capacity);
		TreeSearch g_originalSearch[kTreeSearches] = {};
		int g_capacity = 0;								// the capacity in use
		int g_vanillaSize = 0;							// the game's own list
		int g_vanillaLimit = 0;							// the most entries a search could return into it
		std::atomic<uint64_t> g_searches{ 0 };
		std::atomic<uint64_t> g_overVanilla{ 0 };		// searches that returned more than g_vanillaLimit
		std::atomic<int> g_largest{ 0 };

		// On the game's streaming thread, for every search: counters and nothing else.
		void Count(int found)
		{
			g_searches.fetch_add(1, std::memory_order_relaxed);
			if (found > g_vanillaLimit)
				g_overVanilla.fetch_add(1, std::memory_order_relaxed);
			int largest = g_largest.load(std::memory_order_relaxed);
			while (found > largest && !g_largest.compare_exchange_weak(largest, found, std::memory_order_relaxed))
			{
			}
		}

		int CountedSearch0(void* tree, const void* min, const void* max, void* list, int capacity)
		{
			const int found = g_originalSearch[0](tree, min, max, list, capacity);
			Count(found);
			return found;
		}

		int CountedSearch1(void* tree, const void* min, const void* max, void* list, int capacity)
		{
			const int found = g_originalSearch[1](tree, min, max, list, capacity);
			Count(found);
			return found;
		}

		constexpr TreeSearch kCountedSearch[kTreeSearches] = { &CountedSearch0, &CountedSearch1 };

		void LogSummary()
		{
			WriteLog(kTag, "DIAGNOSTIC: " + std::to_string(g_searches.load()) + " searches so far; the largest returned " +
				std::to_string(g_largest.load()) + " of " + std::to_string(g_capacity) + " entries; " +
				std::to_string(g_overVanilla.load()) + " returned more than the game's own list (" + std::to_string(g_vanillaSize) +
				" entries) could hold.");
		}

		// The first summary 10 seconds in, then one a minute while the numbers change.
		DWORD WINAPI SummaryThread(LPVOID)
		{
			uint64_t last = UINT64_MAX;
			for (DWORD wait = kFirstSummaryMs;; wait = kSummaryEveryMs)
			{
				Sleep(wait);
				const uint64_t searches = g_searches.load();
				if (searches == last)
					continue;
				last = searches;
				try
				{
					LogSummary();
				}
				catch (...)
				{
				}
			}
		}
	}

	void InitBoxStreamer()
	{
		// 1. the list and its sites, on the core: the four capacities, the list's address, the eight loads
		RelocationPlan plan;
		if (!FindRelocation(kSpec, plan))
			return;

		// 2. with DIAGNOSTIC_MODE, the four tree-search calls: each must lead into the game, each pair to
		// the same tree search; they are redirected to the counting wrappers in the same write
		const bool diagnostic = IsDiagnosticMode();
		std::vector<CallRedirect> redirects;
		if (diagnostic)
		{
			const uint8_t* searches[kTreeSearches] = {};
			for (size_t i = 0; i < std::size(kCalls); i++)
			{
				const std::string label = kAnchors[i].pattern->label;
				uint8_t* const call = plan.matches[i][0] + kCalls[i].offset;
				const uint8_t* const target = IsReadable(call, kCallSize) ? CallTarget(call) : nullptr;
				WriteLog(kTag, "DIAGNOSTIC: tree-search call at " + DescribeAddress(call) + " -> " +
					(target ? DescribeAddress(target) : std::string("not a call")));
				if (!target || !IsInsideGameImage(target))
				{
					RelocationFailed(kSpec, "the tree-search call at " + DescribeAddress(call) + " is not a call into the game (another mod may "
						"have hooked it already) (" + label + ").");
					return;
				}
				const size_t search = kCalls[i].search;
				if (!searches[search])
					searches[search] = target;
				else if (target != searches[search])
				{
					RelocationFailed(kSpec, "the tree-search calls of the box streamer do not lead to the same function: " +
						DescribeAddress(searches[search]) + " and " + DescribeAddress(target) + " (" + label + ").");
					return;
				}
				redirects.push_back({ call, reinterpret_cast<const void*>(kCountedSearch[search]), "tree-search call",
					reinterpret_cast<void**>(&g_originalSearch[search]) });
			}
			g_capacity = static_cast<int>(plan.size);
			g_vanillaSize = static_cast<int>(plan.gameSize);
			g_vanillaLimit = g_vanillaSize > kSearchHeadroom ? g_vanillaSize - kSearchHeadroom : 0;
		}

		// 3. one write, all or nothing: the eight references, the four capacities, the four calls
		if (!ApplyRelocation(kSpec, plan, {}, redirects, diagnostic ? " and redirect the 4 tree-search calls (DIAGNOSTIC_MODE)" : std::string()))
			return;
		if (!diagnostic || IsDryRun())
			return;

		WriteLog(kTag, "DIAGNOSTIC: the " + std::to_string(std::size(kCalls)) + " tree-search calls are counted; a summary follows "
			"after 10 seconds, then every minute while the numbers change.");
		const HANDLE thread = CreateThread(nullptr, 0, SummaryThread, nullptr, 0, nullptr);
		if (thread)
			CloseHandle(thread);
		else
			ReportFailure(kTag, "DIAGNOSTIC: the summary thread could not be started (" + DescribeError(GetLastError()) +
				") - the searches are counted but never logged.");
	}
}
