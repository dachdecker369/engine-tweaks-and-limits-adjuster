// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the game's own blips next to a mod's (see TweaksVBlips.h).
//
// The game's script blip_controller makes the vanilla blips behind a guard: a loop that calls the native that
// counts the active blips, compares the count with a threshold of its own, 150, and while the count is at or
// above it waits 2 seconds (WAIT 2000) and counts again. A map mod's blips count too, so with enough of them
// the script never gets past its guard. The part raises the threshold in the script's code each time the game
// places the script - at the start and again at every save load, a new program each time - through the
// script programs' hook (TweaksVScripts.h), on the game's resource placement thread, before any script thread
// can run the program:
//   ACTIVE_BLIP_LIMIT  the new threshold, from the game's own 150 up to 1997, the count's lowest answer with
//                      the pool full (PATTERNS.md, "Blips"); the default 1900 leaves about a hundred slots for
//                      the blips of mission scripts and the others
//   DIAGNOSTIC_MODE    at each placement, the two natives the guard calls - their indices, hashes and
//                      handlers; and the measurement: 10 seconds in and then every minute while the numbers
//                      change, how many of the pool's 2000 slots are in use and its high-water mark, read as
//                      they are - nothing is written or called for it
// A program whose hash is not blip_controller's costs one read. blip_controller's - its hash and its exact
// name - is read within bounds, every page through IsReadable, and its whole code decoded with the game's own
// opcode lengths: an instruction that cannot be decoded refuses the program, so the bytes inside operands,
// names and SWITCH tables are never taken for a match. The guard is matched at every instruction start, in a
// window of 256 bytes from there: the count's NATIVE (0 arguments, 1 result), any NOPs, the threshold's push
// (PUSH_CONST_U8 or PUSH_CONST_S16, 1 to 1997), IGE or ILT, maybe NOT, then JZ - or the fused IGE_JZ or
// ILT_JZ in their place - and the body: a push of 2000 and WAIT's NATIVE (1 argument, no result), then back to
// the count. From the branch, NOPs and jumps are followed to the exit and to the body, a circle failing the
// match; every target lies on an instruction start inside the window, and the two natives' indices below the
// native count and apart. Refused: no guard, a second one, a jump, call or SWITCH case from anywhere into the
// span the rewrite covers or its branch, a target computed while the script runs (CATCH, THROW, CALLINDIRECT),
// and too little room. The threshold found: the configured limit - left as it is; the game's own 150 -
// raised; any other - left as it is and logged, as the heap adjuster leaves a heap another adjuster set.
// After the game has resolved the natives, the window is read and matched again and must be as it was; then
// one WriteCode: the span between the count's NATIVE and the branch becomes NOPs, the new threshold's push -
// PUSH_CONST_U8 up to 255, PUSH_CONST_S16 above - right-aligned against the branch, and the comparison that
// keeps the guard's polarity: IGE where the branch leaves the guard while the count is below the threshold,
// as the game's does, ILT where it leaves it otherwise; a fused branch becomes a plain JZ with its target, and
// nothing after the span moves. A placement left as the game placed it is a problem, the first of its kind a
// session reported - after the start-up summary in a box of its own - the later ones counted in the log.
// Written for TweaksV from the facts in PATTERNS.md; the design - the match, its checks, the rewrite and the
// setting's range and default - is that of Blip Limit Fix by rootBrz, and the findings of its README on the
// blip pool; no line of its code was read or used (CREDITS.md).

#include "TweaksVBlips.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
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
#include "TweaksVScripts.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "Blips";

		// ---- blip_controller and its guard (PATTERNS.md, "Blips")

		constexpr std::string_view kScript = "blip_controller";
		constexpr uint32_t kScriptHash = Scripts::NameHash(kScript);
		static_assert(kScriptHash == 0x80CFE974, "TweaksV: blip_controller's hash must be the game's");
		constexpr int kGameThreshold = 150;		// the game's own threshold, and the lowest ACTIVE_BLIP_LIMIT
		constexpr int kMaxLimit = 1997;			// the count's lowest answer with the pool full, and the highest ACTIVE_BLIP_LIMIT
		static_assert(kGameThreshold == kActiveBlipLimit.minValue && kMaxLimit == kActiveBlipLimit.maxValue,
			"TweaksV: ACTIVE_BLIP_LIMIT runs from the game's own threshold to the count's lowest answer with the pool full");
		constexpr int kWaitTime = 2000;			// what the guard's body hands WAIT
		constexpr uint32_t kWindow = 256;		// the guard lies within this many bytes from its count's NATIVE
		constexpr uint32_t kNowhere = UINT32_MAX;

		// ---- the script interpreter's opcodes (PATTERNS.md, "Script programs")

		namespace Op
		{
			constexpr uint8_t kNop = 0x00;
			constexpr uint8_t kNot = 0x06;
			constexpr uint8_t kIge = 0x0B;			// the compares are signed 32-bit: count >= threshold
			constexpr uint8_t kIlt = 0x0C;			// count < threshold
			constexpr uint8_t kPushU8 = 0x25;		// PUSH_CONST_U8
			constexpr uint8_t kPushU32 = 0x28;		// PUSH_CONST_U32
			constexpr uint8_t kNative = 0x2C;		// +1: the argument count << 2 | the result count; +2: the native's index, big-endian
			constexpr uint8_t kEnter = 0x2D;		// 5 bytes and its name's, whose length is the byte at +4
			constexpr uint8_t kPushS16 = 0x43;		// PUSH_CONST_S16, signed
			constexpr uint8_t kJump = 0x55;			// J and JZ: a signed 16-bit offset from ip + 3
			constexpr uint8_t kJumpZero = 0x56;
			constexpr uint8_t kIgeJumpZero = 0x5A;	// the fused IEQ_JZ to ILE_JZ, 0x57 to 0x5C, jump when their comparison is false
			constexpr uint8_t kIltJumpZero = 0x5B;
			constexpr uint8_t kLastFused = 0x5C;
			constexpr uint8_t kCall = 0x5D;			// a 24-bit address in the code, unsigned
			constexpr uint8_t kPushU24 = 0x64;		// PUSH_CONST_U24
			constexpr uint8_t kSwitch = 0x65;		// +1: n; then n entries of a 32-bit value and an unsigned 16-bit offset from the entry's end
			constexpr uint8_t kCatch = 0x6D;		// CATCH, THROW and CALLINDIRECT: their targets are computed while the script runs
			constexpr uint8_t kThrow = 0x6E;
			constexpr uint8_t kCallIndirect = 0x6F;
			constexpr uint8_t kLast = 0x82;			// a byte above it is no instruction: the interpreter's fatal trap
		}
		constexpr uint8_t kCountCall = 0 << 2 | 1;	// the count's NATIVE: no argument, one result
		constexpr uint8_t kWaitCall = 1 << 2 | 0;	// WAIT's: one argument, no result

		// The lengths of the opcodes, the game's own table (PATTERNS.md, "The interpreter") as constants (D11): "When
		// Game Updates" item 5 checks it against the game's table after an update. 0 for the two of variable length,
		// ENTER and SWITCH.
		struct LengthRange
		{
			uint8_t first;
			uint8_t last;
			uint8_t length;
		};
		constexpr LengthRange kLengthRanges[] = { { 0x00, 0x24, 1 }, { 0x25, 0x25, 2 }, { 0x26, 0x26, 3 }, { 0x27, 0x27, 4 },
			{ 0x28, 0x29, 5 }, { 0x2A, 0x2B, 1 }, { 0x2C, 0x2C, 4 }, { 0x2D, 0x2D, 0 }, { 0x2E, 0x2E, 3 }, { 0x2F, 0x33, 1 },
			{ 0x34, 0x3E, 2 }, { 0x3F, 0x3F, 1 }, { 0x40, 0x42, 2 }, { 0x43, 0x5C, 3 }, { 0x5D, 0x64, 4 }, { 0x65, 0x65, 0 },
			{ 0x66, 0x67, 1 }, { 0x68, 0x6B, 2 }, { 0x6C, 0x82, 1 } };
		constexpr size_t kOpcodes = Op::kLast + 1;
		consteval std::array<uint8_t, kOpcodes> Lengths()
		{
			std::array<uint8_t, kOpcodes> lengths{};
			size_t next = 0;
			for (const LengthRange& range : kLengthRanges)
			{
				if (range.first != next || range.last < range.first)
					throw "TweaksV: the opcodes' lengths must cover 0x00 to 0x82, in order and once";
				for (size_t op = range.first; op <= range.last; op++)
					lengths[op] = range.length;
				next = size_t{ range.last } + 1;
			}
			if (next != kOpcodes)
				throw "TweaksV: the opcodes' lengths must cover 0x00 to 0x82, in order and once";
			return lengths;
		}
		constexpr std::array<uint8_t, kOpcodes> kLength = Lengths();
		static_assert(kLength[Op::kNop] == 1 && kLength[Op::kNot] == 1 && kLength[Op::kIge] == 1 && kLength[Op::kIlt] == 1 &&
			kLength[Op::kPushU8] == 2 && kLength[Op::kPushS16] == 3 && kLength[Op::kPushU24] == 4 && kLength[Op::kPushU32] == 5 &&
			kLength[Op::kNative] == 4 && kLength[Op::kJump] == 3 && kLength[Op::kJumpZero] == 3 && kLength[Op::kIgeJumpZero] == 3 &&
			kLength[Op::kIltJumpZero] == 3 && kLength[Op::kCall] == 4 && kLength[Op::kEnter] == 0 && kLength[Op::kSwitch] == 0,
			"TweaksV: the lengths the guard's match relies on");

		// ---- reading a program's code

		// A script program's code where the game keeps it: pages of kPageSize bytes, each checked readable before
		// a byte of it is read. The operands are little-endian, NATIVE's index aside.
		struct Code
		{
			uint8_t* const* pages = nullptr;	// the page table, at kCodePagesOffset
			uint32_t length = 0;

			uint8_t* Address(uint32_t at) const { return pages[at / Scripts::kPageSize] + at % Scripts::kPageSize; }
			uint8_t At(uint32_t at) const { return *Address(at); }
			int32_t S16(uint32_t at) const { return static_cast<int16_t>(At(at) | At(at + 1) << 8); }
			uint32_t U24(uint32_t at) const { return At(at) | At(at + 1) << 8 | static_cast<uint32_t>(At(at + 2)) << 16; }
			uint32_t U32(uint32_t at) const { return U24(at) | static_cast<uint32_t>(At(at + 3)) << 24; }
			uint16_t Index(uint32_t at) const { return static_cast<uint16_t>(At(at) << 8 | At(at + 1)); }
		};

		std::string Hex(uint64_t value)
		{
			char text[24];
			snprintf(text, sizeof(text), "0x%llX", static_cast<unsigned long long>(value));
			return text;
		}

		// The length of the instruction at at, which lies in the code; 0, with why, when it is none: a byte above
		// the last opcode, or an instruction that does not end within the code or within its page - SWITCH: its
		// count within its page, its entries within the page of at + 2, as the interpreter reads them.
		uint32_t InstructionLength(const Code& code, uint32_t at, std::string& why)
		{
			const uint8_t op = code.At(at);
			if (op > Op::kLast)
			{
				why = "the byte " + Hex(op) + " at " + Hex(at) + " is no instruction";
				return 0;
			}
			const uint64_t pageEnd = (uint64_t{ at } / Scripts::kPageSize + 1) * Scripts::kPageSize;
			uint64_t length = kLength[op];
			uint64_t inPage = length;	// the bytes that must lie in at's own page
			if (op == Op::kEnter || op == Op::kSwitch)
			{
				const uint64_t sizeAt = at + uint64_t{ op == Op::kEnter ? 4u : 1u };	// the byte that gives the length
				if (sizeAt >= code.length || sizeAt >= pageEnd)
				{
					why = "the instruction at " + Hex(at) + " runs past " + (sizeAt >= code.length ? "the end of the code" : "the end of its page");
					return 0;
				}
				length = op == Op::kEnter ? 5 + uint64_t{ code.At(static_cast<uint32_t>(sizeAt)) } : 2 + 6 * uint64_t{ code.At(static_cast<uint32_t>(sizeAt)) };
				inPage = op == Op::kEnter ? length : 2;
			}
			const bool tableSplit = op == Op::kSwitch && length > 2 &&
				(uint64_t{ at } + 2) / Scripts::kPageSize != (uint64_t{ at } + length - 1) / Scripts::kPageSize;
			if (at + length > code.length || at + inPage > pageEnd || tableSplit)
			{
				why = "the instruction at " + Hex(at) + " runs past " + (at + length > code.length ? "the end of the code" : "the end of its page");
				return 0;
			}
			return static_cast<uint32_t>(length);
		}

		// The instruction starts of a stretch of the code, a bit per byte from base up to end.
		struct Starts
		{
			const uint8_t* bits = nullptr;
			uint32_t base = 0;
			uint32_t end = 0;

			bool Has(int64_t at) const { return at >= base && at < end && (bits[(at - base) / 8] >> ((at - base) % 8) & 1); }
		};

		// Decodes the code from first up to end - an instruction may run past end, not past the code - marking each
		// instruction's start in bits, a bit per byte from first; false, with why, at an instruction it cannot
		// decode. computed gets the first CATCH, THROW or CALLINDIRECT, if it is still kNowhere.
		bool Decode(const Code& code, uint32_t first, uint32_t end, uint8_t* bits, uint32_t& computed, std::string& why)
		{
			for (uint32_t at = first; at < end;)
			{
				const uint32_t length = InstructionLength(code, at, why);
				if (length == 0)
					return false;
				bits[(at - first) / 8] |= static_cast<uint8_t>(1u << ((at - first) % 8));
				const uint8_t op = code.At(at);
				if ((op == Op::kCatch || op == Op::kThrow || op == Op::kCallIndirect) && computed == kNowhere)
					computed = at;
				at += length;
			}
			return true;
		}

		// ---- the guard

		// A guard as found: where its parts lie in the code, and what it holds.
		struct Guard
		{
			uint32_t count = 0;			// the count's NATIVE, the guard's first instruction
			uint32_t push = 0;			// the threshold's push
			uint32_t branch = 0;		// the JZ, or the fused compare-and-jump
			uint32_t windowEnd = 0;
			int threshold = 0;
			bool fused = false;
			bool leavesBelow = false;	// the branch leaves the guard while the count is below the threshold, as the game's does
			uint16_t countNative = 0;
			uint16_t waitNative = 0;

			uint32_t SpanStart() const { return count + kLength[Op::kNative]; }	// the span the rewrite covers runs from here to the branch
			bool operator==(const Guard&) const = default;
		};

		// The window of a guard's match: the instructions of the guard start in it, and its branches lead into it.
		struct Window
		{
			const Code& code;
			const Starts& starts;
			uint32_t first;
			uint32_t end;

			bool Holds(int64_t at) const { return at >= first && at < end && starts.Has(at); }
			bool Fits(int64_t at, uint32_t length) const { return Holds(at) && at + length <= end; }

			// From at, NOPs and jumps are followed to the first other instruction, which comes back; kNowhere, with
			// why, when a step leads off an instruction's start in the window, or the steps go round in a circle.
			uint32_t Follow(int64_t at, std::string& why) const
			{
				uint8_t seen[kWindow / 8] = {};
				for (;;)
				{
					if (!Holds(at))
					{
						why = "leads to " + (at < 0 ? std::string("before the code") : Hex(static_cast<uint64_t>(at))) + ", not to an instruction in its window";
						return kNowhere;
					}
					const uint32_t here = static_cast<uint32_t>(at);
					const uint8_t op = code.At(here);
					if (op != Op::kNop && op != Op::kJump)
						return here;
					const uint32_t index = here - first;
					if (seen[index / 8] >> (index % 8) & 1)
					{
						why = "goes round in a circle at " + Hex(here);
						return kNowhere;
					}
					seen[index / 8] |= static_cast<uint8_t>(1u << (index % 8));
					if (op == Op::kNop)
						at = int64_t{ here } + 1;
					else if (Fits(here, kLength[Op::kJump]))
						at = int64_t{ here } + kLength[Op::kJump] + code.S16(here + 1);
					else
					{
						why = "runs past its window at " + Hex(here);
						return kNowhere;
					}
				}
			}
		};

		// True, with guard, if the guard begins at at - an instruction start whose NATIVE is the count's - within
		// the window from there; a sequence that has the guard's head (the count, a threshold, the comparison and
		// the branch) but not the rest gets miss, why it is not the guard. nativeCount bounds the natives' indices.
		bool MatchAt(const Code& code, const Starts& starts, uint32_t at, uint32_t nativeCount, Guard& guard, std::string& miss)
		{
			if (code.At(at) != Op::kNative || code.At(at + 1) != kCountCall)
				return false;
			Guard found;
			found.count = at;
			found.windowEnd = static_cast<uint32_t>(std::min<uint64_t>(uint64_t{ at } + kWindow, code.length));
			const Window window{ code, starts, at, found.windowEnd };

			// the count, any NOPs, the threshold's push
			uint32_t ip = found.SpanStart();
			while (window.Fits(ip, 1) && code.At(ip) == Op::kNop)
				ip++;
			if (window.Fits(ip, kLength[Op::kPushU8]) && code.At(ip) == Op::kPushU8)
				found.threshold = code.At(ip + 1);
			else if (window.Fits(ip, kLength[Op::kPushS16]) && code.At(ip) == Op::kPushS16)
				found.threshold = code.S16(ip + 1);
			else
				return false;
			if (found.threshold < 1 || found.threshold > kMaxLimit)
				return false;
			found.push = ip;
			ip += kLength[code.At(ip)];

			// IGE or ILT, maybe NOT, then JZ - or the fused IGE_JZ or ILT_JZ in their place
			if (!window.Holds(ip))
				return false;
			const uint8_t compare = code.At(ip);
			bool greaterOrEqual = false;
			bool negated = false;
			if (compare == Op::kIge || compare == Op::kIlt)
			{
				greaterOrEqual = compare == Op::kIge;
				ip++;
				if (window.Fits(ip, 1) && code.At(ip) == Op::kNot)
				{
					negated = true;
					ip++;
				}
				if (!window.Fits(ip, kLength[Op::kJumpZero]) || code.At(ip) != Op::kJumpZero)
					return false;
			}
			else if ((compare == Op::kIgeJumpZero || compare == Op::kIltJumpZero) && window.Fits(ip, kLength[compare]))
			{
				greaterOrEqual = compare == Op::kIgeJumpZero;
				found.fused = true;
			}
			else
				return false;
			found.branch = ip;
			// JZ jumps when what it takes is 0, a fused jump when its comparison is false: either way the branch is
			// taken when the comparison, as written and negated or not, is false
			found.leavesBelow = greaterOrEqual != negated;
			const uint32_t next = found.branch + kLength[Op::kJumpZero];
			const int64_t target = int64_t{ next } + code.S16(found.branch + 1);

			// from here on the sequence has the guard's head: what it lacks is said
			std::string why;
			if (!window.Holds(target))
			{
				miss = "its branch leads to " + (target < 0 ? std::string("before the code") : Hex(static_cast<uint64_t>(target))) +
					", not to an instruction in its window";
				return false;
			}
			const int64_t exitSide = found.leavesBelow ? target : int64_t{ next };
			const int64_t bodySide = found.leavesBelow ? int64_t{ next } : target;

			// the body: a push of 2000, WAIT's NATIVE, then back to the count
			const uint32_t body = window.Follow(bodySide, why);
			if (body == kNowhere)
			{
				miss = "its body " + why;
				return false;
			}
			const uint8_t pushOp = code.At(body);
			const bool waits2000 = (pushOp == Op::kPushS16 && window.Fits(body, kLength[pushOp]) && code.S16(body + 1) == kWaitTime) ||
				(pushOp == Op::kPushU24 && window.Fits(body, kLength[pushOp]) && code.U24(body + 1) == static_cast<uint32_t>(kWaitTime)) ||
				(pushOp == Op::kPushU32 && window.Fits(body, kLength[pushOp]) && code.U32(body + 1) == static_cast<uint32_t>(kWaitTime));
			if (!waits2000)
			{
				miss = "its body at " + Hex(body) + " does not push " + std::to_string(kWaitTime) + " for WAIT";
				return false;
			}
			const uint32_t wait = body + kLength[pushOp];
			if (!window.Fits(wait, kLength[Op::kNative]) || code.At(wait) != Op::kNative || code.At(wait + 1) != kWaitCall)
			{
				miss = "its body at " + Hex(body) + " does not call WAIT, a native with one argument and no result, after its push of " +
					std::to_string(kWaitTime);
				return false;
			}
			found.waitNative = code.Index(wait + 2);
			const uint32_t back = window.Follow(int64_t{ wait } + kLength[Op::kNative], why);
			if (back != at)
			{
				miss = back == kNowhere ? "its body " + why : "its body leads to " + Hex(back) + ", not back to its count";
				return false;
			}

			// the exit, past NOPs and jumps: not back into the guard
			const uint32_t exit = window.Follow(exitSide, why);
			if (exit == kNowhere)
			{
				miss = "its way out " + why;
				return false;
			}
			if ((exit >= at && exit < next) || exit == body || exit == wait)
			{
				miss = "its way out leads back into it, at " + Hex(exit);
				return false;
			}

			// the natives: in the program's table, and two different ones
			found.countNative = code.Index(at + 2);
			if (uint32_t{ found.countNative } >= nativeCount || uint32_t{ found.waitNative } >= nativeCount)
			{
				const bool countBeyond = uint32_t{ found.countNative } >= nativeCount;
				miss = std::string(countBeyond ? "its count" : "its WAIT") + " calls native " + std::to_string(countBeyond ? found.countNative : found.waitNative) +
					", which is not below its native count, " + std::to_string(nativeCount);
				return false;
			}
			if (found.countNative == found.waitNative)
			{
				miss = "its count and its WAIT call the same native, " + std::to_string(found.countNative);
				return false;
			}
			guard = found;
			return true;
		}

		// The first instruction whose jump, call or SWITCH case leads into [first, end): its offset, with what it
		// is; kNowhere when there is none. A jump's 16-bit offset is signed; a CALL's 24-bit address and a SWITCH
		// entry's 16-bit offset are unsigned, as the interpreter reads them (PATTERNS.md).
		uint32_t LeadsInto(const Code& code, const Starts& starts, uint32_t first, uint32_t end, const char*& what)
		{
			const auto into = [&](int64_t target) { return target >= first && target < end; };
			for (uint32_t at = 0; at < code.length; at++)
			{
				if (!starts.Has(at))
					continue;
				const uint8_t op = code.At(at);
				if (op >= Op::kJump && op <= Op::kLastFused)	// J, JZ and the fused compare-and-jumps
				{
					if (into(int64_t{ at } + kLength[op] + code.S16(at + 1)))
					{
						what = "jump";
						return at;
					}
				}
				else if (op == Op::kCall)
				{
					if (into(code.U24(at + 1)))
					{
						what = "call";
						return at;
					}
				}
				else if (op == Op::kSwitch)
				{
					const uint32_t entries = code.At(at + 1);
					for (uint32_t entry = 0; entry < entries; entry++)
					{
						const uint32_t entryEnd = at + 2 + 6 * (entry + 1);
						const uint16_t offset = static_cast<uint16_t>(code.At(entryEnd - 2) | code.At(entryEnd - 1) << 8);
						if (into(int64_t{ entryEnd } + offset))
						{
							what = "SWITCH case";
							return at;
						}
					}
				}
			}
			return kNowhere;
		}

		// ---- the placements

		// Read once by InitBlips, before the hook can run
		int g_limit = kGameThreshold;			// ACTIVE_BLIP_LIMIT
		bool g_diagnostic = false;
		enum class Hook { On, DryRun, Failed };
		Hook g_hook = Hook::On;

		// On the game's resource placement thread
		std::atomic<uint32_t> g_placements{ 0 };			// blip_controller's this session
		std::atomic<bool> g_unreadableLogged{ false };
		std::atomic<bool> g_otherNameLogged{ false };
		// What blip_controller's guard holds after its last placement, for the measurement
		constexpr int kNotPlaced = -1;
		constexpr int kNotRaised = 0;
		std::atomic<int> g_guardHolds{ kNotPlaced };

		// Why a placement is left as the game placed it, each kind reported the first time a session
		enum class Failure { Bounds, Natives, Pages, Decode, NotFound, Twice, Computed, LeadsIn, Room, Changed, Write, Memory, Count };
		std::atomic<uint32_t> g_failures[static_cast<size_t>(Failure::Count)] = {};

		// The first failure of a kind this session is a problem - after the start-up summary in a box of its own -
		// and every later one a line in the log, counted: a save load places the program again, as it was.
		void Refuse(Failure kind, uint32_t placement, const std::string& why)
		{
			g_guardHolds = kNotRaised;
			const uint32_t times = g_failures[static_cast<size_t>(kind)].fetch_add(1) + 1;
			const std::string text = "blip_controller's threshold is not raised (placement " + std::to_string(placement) + "): " + why;
			if (times == 1)
				ReportFailure(kTag, text + ".");
			else
				WriteLog(kTag, text + " - " + std::to_string(times) + " times this session, reported once.");
		}

		std::string GuardAt(const Guard& guard)
		{
			return "its guard at " + Hex(guard.count) + " of its code";
		}

		// What before found, for after: the module calls a user's before and after in turn on one thread, so one
		// record per thread does - no allocation for it.
		struct Placement
		{
			uint32_t number = 0;				// blip_controller's placement this session
			const uint8_t* program = nullptr;
			Code code;
			uint32_t nativeCount = 0;
			uint8_t* pages[2] = {};				// the pages the window lies in, as first read
			uint32_t firstPage = 0;
			uint8_t window[kWindow] = {};		// the window's bytes, as first read
			Guard guard;
			uint64_t countHash = 0;				// the natives' hashes, before the game resolves them (DIAGNOSTIC_MODE)
			uint64_t waitHash = 0;
			bool write = false;					// the threshold is to be raised: bytes, from the span's start
			uint32_t size = 0;
			uint8_t bytes[kWindow] = {};
		};
		thread_local Placement t_placement;

		// blip_controller, as the game placed it: read, decoded, its guard matched and checked, the rewrite
		// prepared; the record for after, or nullptr when there is nothing more to do. May throw std::bad_alloc.
		Placement* Examine(const uint8_t* program, uint32_t number)
		{
			using namespace Scripts;
			const uint32_t length = Field<uint32_t>(program, kCodeLengthOffset);
			const uint32_t nativeCount = Field<uint32_t>(program, kNativeCountOffset);
			if (length == 0 || length > kMaxLength)
			{
				Refuse(Failure::Bounds, number, "its code length, " + std::to_string(length) + " bytes, is not within 1 byte to 16 MB");
				return nullptr;
			}
			if (nativeCount == 0 || nativeCount > kMaxNatives)
			{
				Refuse(Failure::Bounds, number, "its native count, " + std::to_string(nativeCount) + ", is not within 1 to " + std::to_string(kMaxNatives));
				return nullptr;
			}
			const auto natives = Field<const uint64_t*>(program, kNativesOffset);
			if (!IsReadable(natives, size_t{ nativeCount } * sizeof(uint64_t)))
			{
				Refuse(Failure::Natives, number, "its native table at " + DescribeAddress(natives) + " cannot be read");
				return nullptr;
			}
			const Code code{ Field<uint8_t* const*>(program, kCodePagesOffset), length };
			const uint32_t pages = PageCount(length);
			if (!IsReadable(code.pages, size_t{ pages } * sizeof(void*)))
			{
				Refuse(Failure::Pages, number, "its code's page table at " + DescribeAddress(code.pages) + " cannot be read");
				return nullptr;
			}
			for (uint32_t page = 0; page < pages; page++)
				if (!IsReadable(code.pages[page], PageLength(length, page)))
				{
					Refuse(Failure::Pages, number, "page " + std::to_string(page) + " of its code, at " + DescribeAddress(code.pages[page]) + ", cannot be read");
					return nullptr;
				}

			// the whole code decoded, every instruction's start known
			std::vector<uint8_t> bits((size_t{ length } + 7) / 8);
			uint32_t computed = kNowhere;
			std::string why;
			if (!Decode(code, 0, length, bits.data(), computed, why))
			{
				Refuse(Failure::Decode, number, "its code cannot be decoded: " + why);
				return nullptr;
			}
			const Starts starts{ bits.data(), 0, length };

			// the guard, tried at every instruction start: there must be one, and only one
			Guard guard;
			uint32_t matches = 0;
			uint32_t second = kNowhere;
			uint32_t missAt = kNowhere;
			std::string miss;
			for (uint32_t at = 0; at + kLength[Op::kNative] <= length; at++)
			{
				if (!starts.Has(at) || code.At(at) != Op::kNative)
					continue;
				Guard candidate;
				std::string notIt;
				if (MatchAt(code, starts, at, nativeCount, candidate, notIt))
				{
					if (++matches == 1)
						guard = candidate;
					else if (matches == 2)
						second = at;
				}
				else if (!notIt.empty() && missAt == kNowhere)
				{
					missAt = at;
					miss = notIt;
				}
			}
			if (matches == 0)
			{
				Refuse(Failure::NotFound, number, missAt == kNowhere ? std::string("its guard was not found in its code") :
					"its guard was not found in its code - the count and a comparison with a threshold are at " + Hex(missAt) + ", but " + miss);
				return nullptr;
			}
			if (matches > 1)
			{
				Refuse(Failure::Twice, number, "its guard was found twice in its code, at " + Hex(guard.count) + " and " + Hex(second) +
					" - which one is meant cannot be told");
				return nullptr;
			}
			if (computed != kNowhere)
			{
				Refuse(Failure::Computed, number, "its code holds a CATCH, THROW or CALLINDIRECT at " + Hex(computed) + ", whose target is computed "
					"while the script runs and could lead into " + GuardAt(guard));
				return nullptr;
			}
			const char* what = "";
			const uint32_t from = LeadsInto(code, starts, guard.SpanStart(), guard.branch + kLength[Op::kJumpZero], what);
			if (from != kNowhere)
			{
				Refuse(Failure::LeadsIn, number, std::string("a ") + what + " at " + Hex(from) + " leads into " + GuardAt(guard) + ", between " +
					Hex(guard.SpanStart()) + " and " + Hex(guard.branch + kLength[Op::kJumpZero] - 1));
				return nullptr;
			}

			// what before found, for after
			Placement& record = t_placement;
			record = Placement{};
			record.number = number;
			record.program = program;
			record.code = code;
			record.nativeCount = nativeCount;
			record.firstPage = guard.count / kPageSize;
			for (uint32_t page = record.firstPage; page <= (guard.windowEnd - 1) / kPageSize; page++)
				record.pages[page - record.firstPage] = code.pages[page];
			for (uint32_t at = guard.count; at < guard.windowEnd; at++)
				record.window[at - guard.count] = code.At(at);
			record.guard = guard;
			record.countHash = natives[guard.countNative];
			record.waitHash = natives[guard.waitNative];

			// the threshold: the configured limit already, the game's own, or another mod's
			if (guard.threshold == g_limit)
			{
				WriteLog(kTag, "blip_controller's guard at " + Hex(guard.count) + " of its code holds the threshold " + std::to_string(guard.threshold) +
					" already (ACTIVE_BLIP_LIMIT=" + std::to_string(g_limit) + ") - nothing to patch (placement " + std::to_string(number) + ").");
				g_guardHolds = guard.threshold;
				return g_diagnostic ? &record : nullptr;
			}
			if (guard.threshold != kGameThreshold)
			{
				WriteLog(kTag, "blip_controller's threshold is not raised (placement " + std::to_string(number) + "): " + GuardAt(guard) + " holds " +
					std::to_string(guard.threshold) + ", not the game's own " + std::to_string(kGameThreshold) + " - left as it is.");
				g_guardHolds = guard.threshold;
				return g_diagnostic ? &record : nullptr;
			}

			// the rewrite: NOPs, the new threshold's push right-aligned against the branch, the comparison that
			// keeps the guard's polarity; a fused branch becomes a plain JZ, its offset kept
			const uint32_t room = guard.branch - guard.SpanStart();
			const uint32_t pushLength = g_limit <= 0xFF ? kLength[Op::kPushU8] : kLength[Op::kPushS16];
			if (room < pushLength + 1)
			{
				Refuse(Failure::Room, number, GuardAt(guard) + " has " + std::to_string(room) + " bytes before its branch, and the threshold " +
					std::to_string(g_limit) + " needs " + std::to_string(pushLength + 1));
				return nullptr;
			}
			const uint32_t pushAt = guard.branch - 1 - pushLength;
			if (pushAt / kPageSize != (pushAt + pushLength - 1) / kPageSize)
			{
				Refuse(Failure::Room, number, "the push of the threshold " + std::to_string(g_limit) + " would run past the end of a page in " + GuardAt(guard));
				return nullptr;
			}
			uint8_t* out = record.bytes;
			out = std::fill_n(out, room - pushLength - 1, Op::kNop);
			if (pushLength == kLength[Op::kPushU8])
			{
				*out++ = Op::kPushU8;
				*out++ = static_cast<uint8_t>(g_limit);
			}
			else
			{
				*out++ = Op::kPushS16;
				*out++ = static_cast<uint8_t>(g_limit & 0xFF);
				*out++ = static_cast<uint8_t>(g_limit >> 8);
			}
			*out++ = guard.leavesBelow ? Op::kIge : Op::kIlt;
			if (guard.fused)
				*out++ = Op::kJumpZero;
			record.size = static_cast<uint32_t>(out - record.bytes);
			record.write = true;
			return &record;
		}

		// True if the program, its pages and the window's bytes are as before read them, and the window decoded
		// again gives the same guard.
		bool Unchanged(const uint8_t* program, const Placement& record)
		{
			using namespace Scripts;
			const Code& code = record.code;
			const Guard& guard = record.guard;
			if (!IsReadable(program, kProgramSize) || program != record.program || Field<uint32_t>(program, kCodeLengthOffset) != code.length ||
				Field<uint32_t>(program, kNativeCountOffset) != record.nativeCount || Field<uint8_t* const*>(program, kCodePagesOffset) != code.pages)
				return false;
			const uint32_t lastPage = (guard.windowEnd - 1) / kPageSize;
			if (!IsReadable(code.pages + record.firstPage, size_t{ lastPage - record.firstPage + 1 } * sizeof(void*)))
				return false;
			for (uint32_t page = record.firstPage; page <= lastPage; page++)
				if (code.pages[page] != record.pages[page - record.firstPage] || !IsReadable(code.pages[page], PageLength(code.length, page)))
					return false;
			for (uint32_t at = guard.count; at < guard.windowEnd; at++)
				if (code.At(at) != record.window[at - guard.count])
					return false;
			uint8_t bits[kWindow / 8] = {};
			uint32_t computed = kNowhere;
			std::string why;
			if (!Decode(code, guard.count, guard.windowEnd, bits, computed, why))
				return false;
			Guard again;
			std::string miss;
			return MatchAt(code, Starts{ bits, guard.count, guard.windowEnd }, guard.count, record.nativeCount, again, miss) && again == guard;
		}

		// After the game has resolved the natives: the natives' line (DIAGNOSTIC_MODE), and the rewrite, once the
		// window is found as it was.
		void Finish(const uint8_t* program, const Placement& record)
		{
			const Guard& guard = record.guard;
			if (record.write && !Unchanged(program, record))
			{
				Refuse(Failure::Changed, record.number, GuardAt(guard) + " changed between the game's placement and the write (another mod may be "
					"at work) - left as it is");
				return;
			}
			if (g_diagnostic)
			{
				const auto natives = IsReadable(program, Scripts::kProgramSize) ? Scripts::Field<const uint64_t*>(program, Scripts::kNativesOffset) : nullptr;
				const bool readable = IsReadable(natives, size_t{ record.nativeCount } * sizeof(uint64_t));
				const auto handler = [&](uint16_t index) { return readable ? DescribeAddress(reinterpret_cast<const void*>(natives[index])) : std::string("unreadable"); };
				char hashes[2][24];
				snprintf(hashes[0], sizeof(hashes[0]), "0x%016llX", static_cast<unsigned long long>(record.countHash));
				snprintf(hashes[1], sizeof(hashes[1]), "0x%016llX", static_cast<unsigned long long>(record.waitHash));
				WriteLog(kTag, "DIAGNOSTIC: blip_controller's count native index " + std::to_string(guard.countNative) + ", hash " + hashes[0] +
					", handler " + handler(guard.countNative) + "; WAIT index " + std::to_string(guard.waitNative) + ", hash " + hashes[1] + ", handler " +
					handler(guard.waitNative) + ".");
			}
			if (!record.write)
				return;

			// one write, all or nothing, a piece per page the span lies in
			const std::string what = "threshold in blip_controller's guard at " + Hex(guard.count) + " of its code (placement " +
				std::to_string(record.number) + ")";
			std::vector<CodeWrite> writes;
			const uint32_t first = guard.SpanStart();
			for (uint32_t at = first; at < first + record.size;)
			{
				const uint32_t pageEnd = (at / Scripts::kPageSize + 1) * Scripts::kPageSize;
				const uint32_t take = std::min(first + record.size, pageEnd) - at;
				writes.push_back({ record.code.Address(at), std::vector<uint8_t>(record.bytes + (at - first), record.bytes + (at - first) + take), what });
				at += take;
			}
			if (!WriteCode(writes, kTag))
			{
				Refuse(Failure::Write, record.number, "its code could not be changed (details in TweaksV.log)");
				return;
			}
			if (IsDryRun())
				return;
			WriteLog(kTag, "blip_controller's guard at " + Hex(guard.count) + " of its code: the threshold " + std::to_string(guard.threshold) + " -> " +
				std::to_string(g_limit) + " (placement " + std::to_string(record.number) + ").");
			g_guardHolds = g_limit;
		}

		// Before the game resolves a program's natives: another program costs the read of its hash; blip_controller
		// is examined, and what is to be done after comes back.
		void* BeforeResolution(uint8_t* program, uint32_t)
		{
			if (!IsReadable(program, Scripts::kProgramSize))
			{
				if (!g_unreadableLogged.exchange(true))
					WriteLog(kTag, "A script program at " + DescribeAddress(program) + " cannot be read, so it is not looked at (logged once).");
				return nullptr;
			}
			if (Scripts::Field<uint32_t>(program, Scripts::kNameHashOffset) != kScriptHash)
				return nullptr;
			char name[Scripts::kNameRead] = {};
			const bool named = Scripts::ReadName(Scripts::Field<const char*>(program, Scripts::kNameOffset), name);
			static_assert(kScript.size() < sizeof(name), "TweaksV: blip_controller's name is read whole, its zero included");
			if (!named || memcmp(name, kScript.data(), kScript.size()) != 0 || name[kScript.size()] != '\0')
			{
				if (!g_otherNameLogged.exchange(true))
					WriteLog(kTag, "A script program has the hash of blip_controller, " + Hex(kScriptHash) + ", but is named " +
						(named ? Scripts::Printable(name) : std::string("(a name that cannot be read)")) + " - left as it is (logged once).");
				return nullptr;
			}
			const uint32_t number = g_placements.fetch_add(1) + 1;
			try
			{
				return Examine(program, number);
			}
			catch (...)
			{
				Refuse(Failure::Memory, number, "there was not enough memory to read its code");
				return nullptr;
			}
		}

		// After the game has resolved them: what before found is done.
		void AfterResolution(uint8_t* program, uint8_t, void* context)
		{
			const Placement* const record = static_cast<const Placement*>(context);
			if (!record)
				return;
			try
			{
				Finish(program, *record);
			}
			catch (...)
			{
				Refuse(Failure::Memory, record->number, "there was not enough memory to change its code");
			}
		}

		constexpr Scripts::User kUser = { kTag, &BeforeResolution, &AfterResolution };

		// ---- the measurement (DIAGNOSTIC_MODE): the game's blip pool, read as it is

		constexpr uint32_t kPoolSlots = 2000;		// of 8 bytes each: a blip's pointer, or 0
		constexpr const GamePattern& kPoolPattern = Pattern("L-E-P1");	// the pool's allocator
		constexpr const GamePattern& kMarkPattern = Pattern("L-E-P2");	// its update of the high-water mark
		constexpr size_t kPoolLeaAt = 7;			// in L-E-P1's match: "lea rbx, [rip+disp32]", the pool
		constexpr size_t kMarkCmpAt = 3;			// in L-E-P2's match: "cmp [rip+disp32], esi", the mark
		static_assert(PatternByte(kPoolPattern, kPoolLeaAt) == 0x48 && PatternByte(kPoolPattern, kPoolLeaAt + 1) == 0x8D &&
			PatternByte(kPoolPattern, kPoolLeaAt + 2) == 0x1D, "TweaksV: L-E-P1 holds the lea of the blip pool at +7");
		static_assert(PatternByte(kMarkPattern, kMarkCmpAt) == 0x39 && PatternByte(kMarkPattern, kMarkCmpAt + 1) == 0x35,
			"TweaksV: L-E-P2 holds the cmp of the high-water mark at +3");
		constexpr uint64_t kPoolUnreadable = 0xFFFE;	// in the place of the slots in use

		const volatile uint64_t* g_pool = nullptr;
		const volatile int32_t* g_mark = nullptr;

		// The slots in use - a slot below the mark that is not null, the pool read in one pass, 8 bytes at a time,
		// without a lock, after IsReadable - the mark, and what blip_controller's guard holds.
		uint64_t PoolNumbers()
		{
			const int32_t mark = *g_mark;
			const uint32_t below = mark < 0 ? 0 : std::min<uint32_t>(static_cast<uint32_t>(mark), kPoolSlots);
			uint64_t inUse = kPoolUnreadable;
			if (IsReadable(const_cast<const uint64_t*>(g_pool), kPoolSlots * sizeof(uint64_t)))
			{
				inUse = 0;
				for (uint32_t slot = 0; slot < below; slot++)
					if (g_pool[slot] != 0)
						inUse++;
			}
			return inUse | uint64_t{ static_cast<uint32_t>(mark) } << 16 | uint64_t{ static_cast<uint16_t>(g_guardHolds.load()) } << 48;
		}

		void LogPool(uint64_t numbers)
		{
			const uint64_t inUse = numbers & 0xFFFF;
			const int32_t mark = static_cast<int32_t>(static_cast<uint32_t>(numbers >> 16));
			const int holds = static_cast<int16_t>(numbers >> 48);
			std::string line = inUse == kPoolUnreadable ? "DIAGNOSTIC: blips: the pool at " + DescribeAddress(const_cast<const uint64_t*>(g_pool)) +
				" cannot be read" : "DIAGNOSTIC: blips: " + std::to_string(inUse) + " of the pool's " + std::to_string(kPoolSlots) + " slots in use, "
				"the high-water mark at " + std::to_string(mark);
			if (holds == g_limit)
				line += "; blip_controller makes blips up to " + std::to_string(g_limit) + " (ACTIVE_BLIP_LIMIT).";
			else if (holds > kNotRaised)
				line += "; blip_controller makes blips up to " + std::to_string(holds) + ", its guard's own threshold (left as it is).";
			else if (holds == kNotRaised)
				line += "; blip_controller's threshold was not raised at its last placement (see above).";
			else if (g_hook == Hook::DryRun)
				line += "; DRY_RUN: blip_controller's threshold is not raised.";
			else if (g_hook == Hook::Failed)
				line += "; blip_controller's threshold is not raised this session (see above).";
			else
				line += "; blip_controller has not been placed yet.";
			WriteLog(kTag, line);
		}

		// The first summary 10 seconds in, then one a minute while the numbers change.
		constexpr Measurement kMeasurement = { "blip pool summary", kFirstSummaryMs, &PoolNumbers, &LogPool };

		// L-E-P1 and L-E-P2 found, the pool and its mark inside the game, and the measurement started; what keeps
		// it from starting is logged, not reported - an aid (NOTES.md, T-DIAG).
		void StartPoolMeasurement()
		{
			const uint8_t* const lea = Scanner::Find(kPoolPattern);	// both looked for, so that the log names each one missing
			const uint8_t* const cmp = Scanner::Find(kMarkPattern);
			if (!lea || !cmp)
			{
				WriteLog(kTag, std::string("DIAGNOSTIC: the blip pool is not measured: ") + (lea ? kMarkPattern.label : kPoolPattern.label) + " was not found.");
				return;
			}
			const uint8_t* const pool = Rel32Target(lea + kPoolLeaAt + 3);
			const uint8_t* const mark = Rel32Target(cmp + kMarkCmpAt + 2);
			if (!IsInsideGameImage(pool) || !IsInsideGameImage(pool + kPoolSlots * sizeof(uint64_t) - 1))
			{
				WriteLog(kTag, "DIAGNOSTIC: the blip pool is not measured: the pool at " + DescribeAddress(pool) + " (" + kPoolPattern.label + ") is not inside the game.");
				return;
			}
			if (!IsInsideGameImage(mark) || !IsInsideGameImage(mark + sizeof(int32_t) - 1))
			{
				WriteLog(kTag, "DIAGNOSTIC: the blip pool is not measured: its high-water mark at " + DescribeAddress(mark) + " (" + kMarkPattern.label +
					") is not inside the game.");
				return;
			}
			g_pool = reinterpret_cast<const volatile uint64_t*>(pool);
			g_mark = reinterpret_cast<const volatile int32_t*>(mark);
			WriteLog(kTag, "DIAGNOSTIC: the blip pool at " + DescribeAddress(pool) + " (" + kPoolPattern.label + "), " + std::to_string(kPoolSlots) +
				" slots, and its high-water mark at " + DescribeAddress(mark) + " (" + kMarkPattern.label + "): the slots in use are logged after 10 "
				"seconds, then every minute while the numbers change.");
			StartMeasurement<kMeasurement>();	// one that cannot start is logged
		}
	}

	void InitBlips()
	{
		g_limit = GetNumber(kActiveBlipLimit);	// before the hook can run
		g_diagnostic = IsDiagnosticMode();
		std::string reason;
		if (!Scripts::AddUser(kUser, reason))
		{
			g_hook = Hook::Failed;
			ReportFailure(kTag, "blip_controller's threshold is not raised this session: " + reason);
		}
		else if (IsDryRun())
		{
			g_hook = Hook::DryRun;
			WriteLog(kTag, "DRY_RUN: the natives' resolution call is not redirected, so blip_controller is not looked at - its guard is found and "
				"checked on a boot without DRY_RUN.");
		}
		else
			WriteLog(kTag, "Each time the game places blip_controller, its guard's threshold is set to " + std::to_string(g_limit) + " (ACTIVE_BLIP_LIMIT; "
				"the game's own: " + std::to_string(kGameThreshold) + ").");
		if (g_diagnostic)
			StartPoolMeasurement();
	}
}
