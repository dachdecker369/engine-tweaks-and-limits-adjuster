// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the video memory budget for streamed resources, measured first (see TweaksVTextureBudget.h).
//
// The game counts the video memory of the textures and buffers its streaming engine places in one object of its
// data, the counters: the buffers' and the textures' bytes and numbers, the most bytes of both so far (its own
// high-water mark), the most objects allowed and four budgets in bytes, the last two written once at the start-up
// from gameconfig.xml's ConfigGraphics - MaxStreamingResources, 98304 in the game's own file, and
// MaxStreamingMemory_0..3_Base, 1500, 1500, 3000 and 3000 MB. An index the texture quality gives picks the budget
// in use. A resource is placed only while the objects stay below the most and the bytes below the budget;
// otherwise the engine evicts what it cannot place and textures go missing, and an update check evicts ahead of
// time once the bytes pass a factor of the budget. The Resources part to come would raise the budget, so this
// module measures first whether a load reaches it, with DIAGNOSTIC_MODE=1 only (SPECIFICATIONS 11.10):
//   DIAGNOSTIC_MODE  Q-E-T1 to Q-E-T5 found once each - the start-up's stores of the most and the four budgets
//                    and the call after them, which the part's writing half would redirect (T1, T2), the budget's
//                    getter with the index (T3), the placement's check of the counters (T4) and the update check
//                    with its switch, flag and factors (T5) - every address checked inside the game and against
//                    the others, every site logged; then the counters read 10 seconds in and again every minute
//                    that brings a new high, a change of the update check's flag or of the budget in use; the
//                    update check's switch and factors read once
//   DRY_RUN          changes nothing here: the module writes nothing to the game
// Every read of the game's data - the counters, the index, the flag, the switch and its factors - is an aligned
// read after IsReadable; nothing is written or called. Windows' own budget for the game's video memory is not read:
// whether TweaksV asks DXGI for it or reads the game's cached copy is decided with the part.
// Written for TweaksV from the findings of its own research in the game's code (PATTERNS.md, "Resources"); the
// idea of a larger budget is that of zombieguy's Resource Adjuster for the Legacy game, which published no code -
// none was read.

#include "TweaksVTextureBudget.h"

#include <windows.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
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

		// True if the pattern holds these bytes from at on - for the checks, when TweaksV is compiled, that an
		// instruction the code reads is the one its pattern pins.
		consteval bool PatternHolds(const GamePattern& pattern, size_t at, std::initializer_list<int> bytes)
		{
			for (const int byte : bytes)
				if (PatternByte(pattern, at++) != byte)
					return false;
			return true;
		}
		consteval bool PatternHolds(const GamePattern& pattern, size_t at, const uint8_t* bytes, size_t size)
		{
			for (size_t i = 0; i < size; i++)
				if (PatternByte(pattern, at + i) != bytes[i])
					return false;
			return true;
		}

		// ---- the counters, one object of the game's data (PATTERNS.md, "Resources"): offsets from its start

		constexpr size_t kBuffersBytes = 0x00;		// qword: the streamed buffers' video memory in bytes
		constexpr size_t kTexturesBytes = 0x08;		// qword: the streamed textures'
		constexpr size_t kBuffersNumber = 0x10;		// dword: how many buffers
		constexpr size_t kTexturesNumber = 0x14;	// dword: how many textures
		constexpr size_t kHighest = 0x18;			// qword: the most bytes of both so far, the game's own high-water mark
		constexpr size_t kMost = 0x20;				// dword: the most objects allowed (MaxStreamingResources)
		constexpr size_t kBudgets = 0x28;			// qwords: the budgets in bytes, one per index
		constexpr size_t kBudgetCount = 4;			// the indices 0 to 3 (MaxStreamingMemory_0..3_Base)
		constexpr size_t kCountersSize = kBudgets + kBudgetCount * sizeof(uint64_t);

		// The update check's switch, a dword, and its three factors, floats, one block of the game's data
		constexpr size_t kRaiseFactor = 4;			// it evicts once the bytes pass this much of the budget
		constexpr size_t kLowerFactor = 8;			// and stops once they are below this much
		constexpr size_t kWindowsFactor = 12;		// it evicts too once the game's video memory passes this much of Windows' budget
		constexpr size_t kSwitchSize = 16;

		// ---- the sites (PATTERNS.md, "Resources")

		constexpr const GamePattern& kStoreMost = Pattern("Q-E-T1");		// the start-up's store of the most, then of budgets 0 and 1
		constexpr const GamePattern& kStoreBudgets = Pattern("Q-E-T2");	// its store of budgets 2 and 3, then the call after them
		constexpr const GamePattern& kGetter = Pattern("Q-E-T3");			// the budget's getter
		constexpr const GamePattern& kPlacement = Pattern("Q-E-T4");		// a resource's placement: its check of the counters
		constexpr const GamePattern& kUpdate = Pattern("Q-E-T5");			// the update check

		// Q-E-T1: "mov ecx, [rax+disp32]", the configuration's most (+2 its offset); "mov [rip+disp32], ecx" (+6, its
		// rel32 at +8); "pmovsxdq xmm0, [rax+disp32]", budgets 0 and 1 in MB (+12, their offset at +17). Past the
		// match "psllq xmm0, 20" and "movdqu [rip+disp32], xmm0" (+21, the store's rel32 at +30) - the same two
		// instructions Q-E-T2 holds at +9 for budgets 2 and 3 - and Q-E-T2 itself (+34).
		constexpr size_t kMostOffsetAt = 2, kMostStoreAt = 6, kBudgets01OffsetAt = 17, kShiftAt = 21, kBudgets01StoreAt = 26;
		constexpr size_t kStoreBudgetsAt = 34;
		static_assert(PatternHolds(kStoreMost, 0, { 0x8B, 0x88 }) && PatternHolds(kStoreMost, kMostStoreAt, { 0x89, 0x0D }) &&
			PatternHolds(kStoreMost, 12, { 0x66, 0x0F, 0x38, 0x25, 0x80 }), "TweaksV: Q-E-T1 holds the loads and the store of the most");
		constexpr uint8_t kShiftAndStore[] = { 0x66, 0x0F, 0x73, 0xF0, 0x14, 0xF3, 0x0F, 0x7F, 0x05 };
		static_assert(PatternHolds(kStoreBudgets, 9, kShiftAndStore, sizeof(kShiftAndStore)) &&
			kShiftAt + sizeof(kShiftAndStore) == kBudgets01StoreAt + 4 && kBudgets01StoreAt + 8 == kStoreBudgetsAt,
			"TweaksV: Q-E-T2 holds at +9 the shift and the store that follow Q-E-T1");

		// Q-E-T2: "pmovsxdq xmm0, [rax+disp32]", budgets 2 and 3 (+5 their offset); the shift and "movdqu
		// [rip+disp32], xmm0" (+14, its rel32 at +18); then the call after the stores (+40), its rel32 past the match.
		constexpr size_t kBudgets23OffsetAt = 5, kBudgets23StoreAt = 14, kCallAt = 40, kCallSize = 5;
		static_assert(PatternHolds(kStoreBudgets, kBudgets23StoreAt, { 0xF3, 0x0F, 0x7F, 0x05 }) && PatternByte(kStoreBudgets, kCallAt) == 0xE8,
			"TweaksV: Q-E-T2 holds the store of budgets 2 and 3 at +14 and the call at +40");

		// Q-E-T3: "movsxd rax, [rip+disp32]", the index (its rel32 at +3), and "mov rax, [rcx+rax*8+0x28]": the budgets
		// at +0x28 of the counters, which the caller hands it in rcx.
		constexpr size_t kIndexAt = 3;
		static_assert(PatternHolds(kGetter, 0, { 0x48, 0x63, 0x05 }) && PatternByte(kGetter, 11) == static_cast<int>(kBudgets),
			"TweaksV: Q-E-T3 loads the index and reads the budgets at +0x28 of the counters");

		// Q-E-T4: the rel32 of its loads - the buffers' number (+2), the textures' number (+8), the most (+16, of a
		// "cmp"), the buffers' bytes (+25) and the textures' bytes (+32, past the match) - and the field each must be.
		struct PlacementLoad { size_t at; size_t field; const char* what; };
		constexpr PlacementLoad kPlacementLoads[] = { { 2, kBuffersNumber, "buffers' number" }, { 8, kTexturesNumber, "textures' number" },
			{ 16, kMost, "most objects" }, { 25, kBuffersBytes, "buffers' bytes" }, { 32, kTexturesBytes, "textures' bytes" } };
		constexpr size_t kPlacementCountersAt = 25, kPlacementPastAt = 32;
		static_assert(PatternHolds(kPlacement, 0, { 0x8B, 0x05 }) && PatternHolds(kPlacement, 6, { 0x8B, 0x0D }) && PatternHolds(kPlacement, 14, { 0x3B, 0x0D }) &&
			PatternHolds(kPlacement, 22, { 0x48, 0x8B, 0x05 }) && PatternHolds(kPlacement, 29, { 0x48, 0x8B, 0x35 }),
			"TweaksV: Q-E-T4 holds the loads of the counters");

		// Q-E-T5: "cmp dword [rip+disp32], 0", the switch (its disp32 at +2, an imm8 after it); "lea rcx, [rip+disp32]",
		// the counters handed to the getter (+16); the buffers' and the textures' bytes again (+23, +30); the getter's
		// call (+34). Past the match, at fixed distances: "cmp byte [rip+disp32], 0" (+0x4B), the flag, and the
		// factors' "mulss xmm, [rip+disp32]", the lower at +0x54 and the raise at +0xA0.
		constexpr size_t kSwitchAt = 2, kUpdateCountersAt = 16, kUpdateBuffersAt = 23, kUpdateTexturesAt = 30, kGetterCallAt = 34;
		constexpr size_t kFlagAt = 0x4B, kFlagSize = 7, kLowerAt = 0x54, kRaiseAt = 0xA0, kMulssMaxSize = 9;
		static_assert(PatternHolds(kUpdate, 0, { 0x83, 0x3D }) && PatternByte(kUpdate, 6) == 0x00 && PatternHolds(kUpdate, 13, { 0x48, 0x8D, 0x0D }) &&
			PatternHolds(kUpdate, 20, { 0x48, 0x8B, 0x3D }) && PatternHolds(kUpdate, 27, { 0x48, 0x8B, 0x35 }) && PatternByte(kUpdate, kGetterCallAt) == 0xE8,
			"TweaksV: Q-E-T5 holds the switch's compare, the loads of the counters and the getter's call");

		// What the checks found, set once everything is checked, before the measurement's thread starts
		const uint8_t* g_counters = nullptr;
		const uint8_t* g_index = nullptr;
		const uint8_t* g_flag = nullptr;
		const uint8_t* g_switch = nullptr;

		std::string Hex(uint64_t value)
		{
			char text[24];
			snprintf(text, sizeof(text), "0x%llX", static_cast<unsigned long long>(value));
			return text;
		}

		std::string At(const GamePattern& pattern)
		{
			return std::string(" (") + pattern.label + ")";
		}

		// The 4 bytes of an operand in the game's code, which need not be aligned.
		uint32_t Imm32(const uint8_t* at)
		{
			uint32_t value = 0;
			memcpy(&value, at, sizeof(value));
			return value;
		}

		// True if the size bytes from p all lie inside the game.
		bool Inside(const uint8_t* p, size_t size)
		{
			return IsInsideGameImage(p) && IsInsideGameImage(p + size - 1);
		}

		// Where "mulss xmm, [rip+disp32]" at p leads - F3, maybe a REX, 0F 59, a ModRM of mod 00 and r/m 101, the
		// disp32 - or nullptr when p holds no such instruction; p readable for kMulssMaxSize bytes.
		const uint8_t* MulssTarget(const uint8_t* p)
		{
			const size_t rex = (p[1] & 0xF0) == 0x40 ? 1 : 0;
			if (p[0] != 0xF3 || p[1 + rex] != 0x0F || p[2 + rex] != 0x59 || (p[3 + rex] & 0xC7) != 0x05)
				return nullptr;
			return Rel32Target(p + 4 + rex);
		}

		// A check that failed: the measurement is not started - logged, not reported, an aid (NOTES.md, T-DIAG).
		bool NotMeasured(const std::string& why)
		{
			WriteLog(kTag, "DIAGNOSTIC: the texture budget is not measured: " + why + ".");
			return false;
		}

		// Q-E-T1 to Q-E-T5 found and every site checked as the part's writing half would check it - each address
		// inside the game, the sites agreeing on the counters - and logged; false at the first that is not as the
		// research found it, logged.
		bool FindAndCheck()
		{
			const uint8_t* const storeMost = Scanner::Find(kStoreMost);	// all five looked for, so that the log names each one missing
			const uint8_t* const storeBudgets = Scanner::Find(kStoreBudgets);
			const uint8_t* const getter = Scanner::Find(kGetter);
			const uint8_t* const placement = Scanner::Find(kPlacement);
			const uint8_t* const update = Scanner::Find(kUpdate);
			if (!storeMost || !storeBudgets || !getter || !placement || !update)
				return NotMeasured(std::string((!storeMost ? kStoreMost : !storeBudgets ? kStoreBudgets : !getter ? kGetter : !placement ? kPlacement :
					kUpdate).label) + " was not found");

			// The start-up's stores: the most, then budgets 0 and 1 past Q-E-T1, then budgets 2 and 3 right after them
			const uint8_t* const most = Rel32Target(storeMost + kMostStoreAt + 2);
			if (!Inside(most, sizeof(uint32_t)))
				return NotMeasured("the most objects at " + DescribeAddress(most) + At(kStoreMost) + " are not inside the game");
			WriteLog(kTag, "DIAGNOSTIC: store of the most objects at " + DescribeAddress(storeMost + kMostStoreAt) + " -> " + DescribeAddress(most) +
				", from the configuration's +" + Hex(Imm32(storeMost + kMostOffsetAt)) + At(kStoreMost));
			const uint8_t* const shift = storeMost + kShiftAt;
			if (!IsReadable(shift, kStoreBudgetsAt - kShiftAt))
				return NotMeasured("the code after " + std::string(kStoreMost.label) + " at " + DescribeAddress(shift) + " cannot be read");
			if (memcmp(shift, kShiftAndStore, sizeof(kShiftAndStore)) != 0)
				return NotMeasured("the code after " + std::string(kStoreMost.label) + " at " + DescribeAddress(shift) + " is not the store of budgets 0 and 1: " +
					HexBytes(shift, kStoreBudgetsAt - kShiftAt));
			const uint8_t* const budgets = Rel32Target(storeMost + kBudgets01StoreAt + 4);
			if (!Inside(budgets, kBudgetCount * sizeof(uint64_t)))
				return NotMeasured("the budgets at " + DescribeAddress(budgets) + At(kStoreMost) + " are not inside the game");
			WriteLog(kTag, "DIAGNOSTIC: store of budgets 0 and 1 at " + DescribeAddress(storeMost + kBudgets01StoreAt) + " -> " + DescribeAddress(budgets) +
				", from the configuration's +" + Hex(Imm32(storeMost + kBudgets01OffsetAt)) + " in MB" + At(kStoreMost));
			if (storeBudgets != storeMost + kStoreBudgetsAt)
				return NotMeasured("the store of budgets 2 and 3 at " + DescribeAddress(storeBudgets) + At(kStoreBudgets) +
					" does not follow the store of budgets 0 and 1, which ends at " + DescribeAddress(storeMost + kStoreBudgetsAt));
			const uint8_t* const budgets23 = Rel32Target(storeBudgets + kBudgets23StoreAt + 4);
			if (budgets23 != budgets + 2 * sizeof(uint64_t))
				return NotMeasured("budgets 2 and 3 at " + DescribeAddress(budgets23) + At(kStoreBudgets) + " do not lie 16 bytes after budgets 0 and 1 at " +
					DescribeAddress(budgets));
			WriteLog(kTag, "DIAGNOSTIC: store of budgets 2 and 3 at " + DescribeAddress(storeBudgets + kBudgets23StoreAt) + " -> " + DescribeAddress(budgets23) +
				", from the configuration's +" + Hex(Imm32(storeBudgets + kBudgets23OffsetAt)) + " in MB" + At(kStoreBudgets));

			// The call after the stores, the one the part's writing half would redirect: a call into the game
			const uint8_t* const call = storeBudgets + kCallAt;
			const uint8_t* const callee = IsReadable(call, kCallSize) ? CallTarget(call) : nullptr;
			WriteLog(kTag, "DIAGNOSTIC: call after the budgets' stores at " + DescribeAddress(call) + " -> " +
				(callee ? DescribeAddress(callee) : std::string("not a call")) + At(kStoreBudgets));
			if (!callee || !IsInsideGameImage(callee))
				return NotMeasured("the call after the budgets' stores at " + DescribeAddress(call) + " is not a call into the game (another mod may have hooked it "
					"already)" + At(kStoreBudgets));

			// The getter: the index
			const uint8_t* const index = Rel32Target(getter + kIndexAt);
			if (!Inside(index, sizeof(int32_t)))
				return NotMeasured("the index at " + DescribeAddress(index) + At(kGetter) + " is not inside the game");
			WriteLog(kTag, "DIAGNOSTIC: budget getter at " + DescribeAddress(getter) + ": the index at " + DescribeAddress(index) + ", the budgets at +" +
				Hex(kBudgets) + " of the counters" + At(kGetter));

			// The placement's check: every load a field of the counters, the most and the budgets those the start-up writes
			const uint8_t* const counters = Rel32Target(placement + kPlacementCountersAt);
			if (!Inside(counters, kCountersSize))
				return NotMeasured("the counters at " + DescribeAddress(counters) + At(kPlacement) + " are not inside the game");
			if (!IsReadable(placement + kPlacementPastAt, sizeof(int32_t)))
				return NotMeasured("the load of the textures' bytes after " + std::string(kPlacement.label) + " at " + DescribeAddress(placement + kPlacementPastAt) +
					" cannot be read");
			for (const PlacementLoad& load : kPlacementLoads)
				if (Rel32Target(placement + load.at) != counters + load.field)
					return NotMeasured(std::string("the placement's check loads the ") + load.what + " from " + DescribeAddress(Rel32Target(placement + load.at)) +
						", not from +" + Hex(load.field) + " of the counters at " + DescribeAddress(counters) + At(kPlacement));
			if (counters + kMost != most)
				return NotMeasured("the most objects the placement's check reads at " + DescribeAddress(counters + kMost) + " are not those " +
					kStoreMost.label + " stores at " + DescribeAddress(most));
			if (counters + kBudgets != budgets)
				return NotMeasured("the budgets at " + DescribeAddress(budgets) + At(kStoreMost) + " are not at +" + Hex(kBudgets) + " of the counters at " +
					DescribeAddress(counters) + At(kPlacement));
			WriteLog(kTag, "DIAGNOSTIC: placement check at " + DescribeAddress(placement) + ": the numbers at " + DescribeAddress(counters + kBuffersNumber) + " and " +
				DescribeAddress(counters + kTexturesNumber) + ", the most at " + DescribeAddress(counters + kMost) + ", the bytes at " +
				DescribeAddress(counters + kBuffersBytes) + " and " + DescribeAddress(counters + kTexturesBytes) + At(kPlacement));

			// The update check: its switch, the counters, the getter's call; past the match its flag and its factors
			const uint8_t* const switchAt = Rel32Target(update + kSwitchAt) + 1;	// the disp32 ends a byte before the instruction
			if (!Inside(switchAt, kSwitchSize))
				return NotMeasured("the update check's switch and factors at " + DescribeAddress(switchAt) + At(kUpdate) + " are not inside the game");
			if (Rel32Target(update + kUpdateCountersAt) != counters || Rel32Target(update + kUpdateBuffersAt) != counters + kBuffersBytes ||
				Rel32Target(update + kUpdateTexturesAt) != counters + kTexturesBytes)
				return NotMeasured("the update check at " + DescribeAddress(update) + " does not read the counters at " + DescribeAddress(counters) + At(kUpdate));
			const uint8_t* const getterCall = update + kGetterCallAt;
			const uint8_t* const called = IsReadable(getterCall, kCallSize) ? CallTarget(getterCall) : nullptr;
			if (called != getter)
				return NotMeasured("the update check's call at " + DescribeAddress(getterCall) + " does not lead to the budget getter at " + DescribeAddress(getter) +
					At(kUpdate));
			WriteLog(kTag, "DIAGNOSTIC: update check at " + DescribeAddress(update) + ": its switch at " + DescribeAddress(switchAt) + ", the counters at " +
				DescribeAddress(counters) + ", the getter's call at " + DescribeAddress(getterCall) + " -> " + DescribeAddress(called) + At(kUpdate));
			const uint8_t* const compare = update + kFlagAt;
			if (!IsReadable(compare, kRaiseAt + kMulssMaxSize - kFlagAt))
				return NotMeasured("the update check's code after " + std::string(kUpdate.label) + " at " + DescribeAddress(compare) + " cannot be read");
			if (compare[0] != 0x80 || compare[1] != 0x3D || compare[6] != 0x00)
				return NotMeasured("the update check's code at " + DescribeAddress(compare) + " is not the compare of its flag: " + HexBytes(compare, kFlagSize) +
					At(kUpdate));
			const uint8_t* const flag = Rel32Target(compare + 2) + 1;	// an imm8 follows the disp32 here too
			if (!Inside(flag, 1))
				return NotMeasured("the update check's flag at " + DescribeAddress(flag) + At(kUpdate) + " is not inside the game");
			const uint8_t* const lower = update + kLowerAt;
			const uint8_t* const raise = update + kRaiseAt;
			if (MulssTarget(lower) != switchAt + kLowerFactor)
				return NotMeasured("the update check's code at " + DescribeAddress(lower) + " is not the multiplication by its factor at " +
					DescribeAddress(switchAt + kLowerFactor) + ": " + HexBytes(lower, kMulssMaxSize) + At(kUpdate));
			if (MulssTarget(raise) != switchAt + kRaiseFactor)
				return NotMeasured("the update check's code at " + DescribeAddress(raise) + " is not the multiplication by its factor at " +
					DescribeAddress(switchAt + kRaiseFactor) + ": " + HexBytes(raise, kMulssMaxSize) + At(kUpdate));
			WriteLog(kTag, "DIAGNOSTIC: update check's flag at " + DescribeAddress(compare) + " -> " + DescribeAddress(flag) + ", its factors at " +
				DescribeAddress(lower) + " -> " + DescribeAddress(switchAt + kLowerFactor) + " and " + DescribeAddress(raise) + " -> " +
				DescribeAddress(switchAt + kRaiseFactor) + At(kUpdate));

			g_counters = counters;
			g_index = index;
			g_flag = flag;
			g_switch = switchAt;
			return true;
		}

		// ---- the measurement (DIAGNOSTIC_MODE): the counters, read as they are

		template <typename T> T Load(const uint8_t* at)
		{
			return *reinterpret_cast<const volatile T*>(at);
		}

		struct Reading
		{
			bool readable = false;
			uint64_t buffers = 0, textures = 0, highest = 0;			// bytes
			uint32_t buffersNumber = 0, texturesNumber = 0, most = 0;
			uint64_t budgets[kBudgetCount] = {};
			int32_t index = 0;
			uint8_t flag = 0;
		};

		Reading ReadCounters()
		{
			Reading reading;
			if (!IsReadable(g_counters, kCountersSize) || !IsReadable(g_index, sizeof(int32_t)) || !IsReadable(g_flag, 1))
				return reading;
			reading.readable = true;
			reading.buffers = Load<uint64_t>(g_counters + kBuffersBytes);
			reading.textures = Load<uint64_t>(g_counters + kTexturesBytes);
			reading.buffersNumber = Load<uint32_t>(g_counters + kBuffersNumber);
			reading.texturesNumber = Load<uint32_t>(g_counters + kTexturesNumber);
			reading.highest = Load<uint64_t>(g_counters + kHighest);
			reading.most = Load<uint32_t>(g_counters + kMost);
			for (size_t i = 0; i < kBudgetCount; i++)
				reading.budgets[i] = Load<uint64_t>(g_counters + kBudgets + i * sizeof(uint64_t));
			reading.index = Load<int32_t>(g_index);
			reading.flag = Load<uint8_t>(g_flag);
			return reading;
		}

		uint64_t Bytes(const Reading& reading)	// of both, the textures' and the buffers'
		{
			return reading.textures > UINT64_MAX - reading.buffers ? UINT64_MAX : reading.textures + reading.buffers;
		}

		uint64_t Objects(const Reading& reading)
		{
			return uint64_t{ reading.buffersNumber } + reading.texturesNumber;
		}

		std::string Mb(uint64_t bytes)
		{
			return std::to_string(bytes >> 20);
		}

		// The measurement's state, kept by its thread alone: the reading of the last summary, TweaksV's own highs of
		// every reading, and how many readings had something new to say.
		Reading g_logged;
		uint64_t g_mostBytes = 0, g_mostObjects = 0;
		uint64_t g_loggedMostMb = 0, g_loggedMostObjects = 0;
		uint64_t g_news = 0;
		bool g_updateCheckLogged = false;

		// Changes with every reading that has something new to say: the first; a new high - of the bytes in MB, of
		// the objects, of the game's own mark in MB; the update check's flag raised or lowered; another index, budget
		// or most; the counters readable or not.
		uint64_t BudgetNumbers()
		{
			const Reading now = ReadCounters();
			if (now.readable)
			{
				g_mostBytes = std::max(g_mostBytes, Bytes(now));
				g_mostObjects = std::max(g_mostObjects, Objects(now));
			}
			const bool news = g_news == 0 || now.readable != g_logged.readable || (now.readable && ((g_mostBytes >> 20) > g_loggedMostMb ||
				g_mostObjects > g_loggedMostObjects || (now.highest >> 20) > (g_logged.highest >> 20) || now.flag != g_logged.flag ||
				now.index != g_logged.index || now.most != g_logged.most || memcmp(now.budgets, g_logged.budgets, sizeof(now.budgets)) != 0));
			if (news)
			{
				g_logged = now;
				g_loggedMostMb = g_mostBytes >> 20;
				g_loggedMostObjects = g_mostObjects;
				g_news++;
			}
			return g_news;
		}

		std::string Factor(const uint8_t* at)
		{
			char text[32];
			snprintf(text, sizeof(text), "%.3g", static_cast<double>(std::bit_cast<float>(Load<uint32_t>(at))));
			return text;
		}

		// Once, at the first summary: the update check's switch and factors, which lie in the game's data and which no
		// code is known to write.
		void LogUpdateCheck()
		{
			if (!IsReadable(g_switch, kSwitchSize))
			{
				WriteLog(kTag, "DIAGNOSTIC: texture budget: the update check's switch and factors at " + DescribeAddress(g_switch) + " cannot be read.");
				return;
			}
			const int32_t on = Load<int32_t>(g_switch);
			WriteLog(kTag, "DIAGNOSTIC: texture budget: the update check's switch holds " + std::to_string(on) + (on ? " (on)" :
				" (off: nothing is evicted ahead of time)") + ", its factors " + Factor(g_switch + kRaiseFactor) + " of the budget to start evicting, " +
				Factor(g_switch + kLowerFactor) + " to stop, and " + Factor(g_switch + kWindowsFactor) + " of the video memory Windows gives the game to "
				"start evicting too (read once).");
		}

		void LogBudget(uint64_t)
		{
			if (!g_updateCheckLogged)
			{
				g_updateCheckLogged = true;
				LogUpdateCheck();
			}
			const Reading& now = g_logged;
			if (!now.readable)
			{
				WriteLog(kTag, "DIAGNOSTIC: texture budget: the counters at " + DescribeAddress(g_counters) + " cannot be read.");
				return;
			}
			const bool known = now.index >= 0 && now.index < static_cast<int32_t>(kBudgetCount);
			const uint64_t budget = known ? now.budgets[static_cast<size_t>(now.index)] : 0;
			std::string line = "DIAGNOSTIC: texture budget: " + Mb(Bytes(now)) + " MB";
			line += known ? " of the " + Mb(budget) + " MB budget for the texture quality's index " + std::to_string(now.index) :
				" - the texture quality's index " + std::to_string(now.index) + " has no budget";
			line += " (MaxStreamingMemory_0..3_Base: " + Mb(now.budgets[0]) + ", " + Mb(now.budgets[1]) + ", " + Mb(now.budgets[2]) + " and " +
				Mb(now.budgets[3]) + " MB) - textures " + Mb(now.textures) + " MB, buffers " + Mb(now.buffers) + " MB; " + std::to_string(Objects(now)) +
				" of the " + std::to_string(now.most) + " objects allowed (MaxStreamingResources) - " + std::to_string(now.texturesNumber) + " textures, " +
				std::to_string(now.buffersNumber) + " buffers; the most so far " + Mb(now.highest) + " MB by the game's own mark, " + Mb(g_mostBytes) +
				" MB and " + std::to_string(g_mostObjects) + " objects at TweaksV's readings; the update check's flag is " +
				(now.flag ? "up: it evicts ahead of time" : "down");
			if (known && Bytes(now) >= budget)
				line += " - at the budget: the streaming engine evicts what it cannot place (texture loss)";
			if (Objects(now) >= now.most)
				line += " - at the most objects: the streaming engine evicts what it cannot place (texture loss)";
			WriteLog(kTag, line + ".");
		}

		// The first summary 10 seconds in, then one every minute that has something new to say.
		constexpr Measurement kMeasurement = { "texture budget summary", kFirstSummaryMs, &BudgetNumbers, &LogBudget };
	}

	void MeasureTextureBudget()
	{
		if (!FindAndCheck())
			return;
		WriteLog(kTag, "DIAGNOSTIC: the texture budget's counters at " + DescribeAddress(g_counters) + At(kPlacement) + ": the streamed textures and buffers "
			"are logged after 10 seconds, then every minute while their highs, the update check's flag or the budget in use change.");
		StartMeasurement<kMeasurement>();	// one that cannot start is logged
	}
}
