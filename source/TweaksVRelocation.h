// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the relocation core: moves one of the game's static arrays to a larger one and raises the
// limit the game's code carries for it. The four array parts - the modkit array, the packfile array,
// the box streamer's traversal list and the decal definitions array - describe their array in the
// tables below, and the core does the work once for all of them: it finds every site, checks
// everything before anything is written, clamps the setting, allocates the new array in full as
// read/write data near the game and writes every reference and every limit in one go, all or nothing.
// What is particular to a part - a hooked call, counted calls, a loop end - stays in the part's file,
// which reads its sites from the plan and adds its writes to the one write.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "TweaksVPatch.h"
#include "TweaksVPatterns.h"
#include "TweaksVSettings.h"

namespace TweaksV
{
	// Where a reference to the array lies in a pattern's match: the rel32 of a "lea r64, [rip+disp32]"
	// or of a rip-relative mov, so that the array is the address after the rel32 plus its value. opcode:
	// the bytes the instruction must start with, right before the rel32 (nullptr: not checked).
	struct ReferenceSite
	{
		size_t offset;			// of the rel32 in the match
		const char* opcode;
		size_t opcodeSize;
	};

	// Where a limit lies in a pattern's match. Immediate: an imm32 that holds N x mul + add, N being
	// the array's size. Variable: the rel32 of a variable the game compares against, which holds N + add.
	// opcode: the bytes the instruction at offset must start with (nullptr: not checked).
	struct LimitSite
	{
		enum class Kind { Immediate, Variable };
		Kind kind;
		size_t offset;			// of the instruction in the match
		const char* opcode;
		size_t opcodeSize;
		size_t valueOffset;		// of the imm32, or of the rel32 to the variable, in the match
		int mul;
		int add;
	};

	// A pattern of the part and what its match holds: a reference to the array, up to three limits, both
	// or neither - a site the part reads itself, through RelocationPlan::matches. matches: how often the
	// pattern must match, 1 or more for a site the game repeats; every match holds the same sites.
	struct Anchor
	{
		const GamePattern* pattern;
		size_t matches;
		bool hasReference;
		ReferenceSite reference;
		size_t limitCount;
		LimitSite limits[3];
	};

	// The builders of the tables.
	constexpr ReferenceSite Rel32(size_t offset, const char* opcode = nullptr, size_t opcodeSize = 0)
	{
		return { offset, opcode, opcodeSize };
	}

	constexpr LimitSite Imm32(size_t imm32, int mul, int add)
	{
		return { LimitSite::Kind::Immediate, imm32, nullptr, 0, imm32, mul, add };
	}

	constexpr LimitSite Imm32(size_t instruction, const char* opcode, size_t opcodeSize, size_t imm32, int mul, int add)
	{
		return { LimitSite::Kind::Immediate, instruction, opcode, opcodeSize, imm32, mul, add };
	}

	constexpr LimitSite Variable(size_t rel32, int add = 0)
	{
		return { LimitSite::Kind::Variable, rel32, nullptr, 0, rel32, 1, add };
	}

	constexpr LimitSite kNoLimit = { LimitSite::Kind::Immediate, 0, nullptr, 0, 0, 0, 0 };

	constexpr Anchor Site(const GamePattern& pattern)
	{
		return { &pattern, 1, false, {}, 0, { kNoLimit, kNoLimit, kNoLimit } };
	}

	constexpr Anchor Reference(const GamePattern& pattern, ReferenceSite reference)
	{
		return { &pattern, 1, true, reference, 0, { kNoLimit, kNoLimit, kNoLimit } };
	}

	constexpr Anchor Limit(const GamePattern& pattern, LimitSite limit)
	{
		return { &pattern, 1, false, {}, 1, { limit, kNoLimit, kNoLimit } };
	}

	constexpr Anchor Limits(const GamePattern& pattern, size_t matches, LimitSite a, LimitSite b)
	{
		return { &pattern, matches, false, {}, 2, { a, b, kNoLimit } };
	}

	constexpr Anchor Limits(const GamePattern& pattern, size_t matches, LimitSite a, LimitSite b, LimitSite c)
	{
		return { &pattern, matches, false, {}, 3, { a, b, c } };
	}

	constexpr Anchor Repeated(const GamePattern& pattern, size_t matches, LimitSite limit)
	{
		return { &pattern, matches, false, {}, 1, { limit, kNoLimit, kNoLimit } };
	}

	constexpr Anchor ReferenceAndLimit(const GamePattern& pattern, ReferenceSite reference, LimitSite limit)
	{
		return { &pattern, 1, true, reference, 1, { limit, kNoLimit, kNoLimit } };
	}

	// One array: its names, its sizes, its setting and its sites.
	struct RelocationSpec
	{
		const char* tag;					// the part's log tag
		const char* name;					// "modkit array", "box streamer's traversal list"
		const char* sizeName;				// "modkit list size": "The game's own <sizeName>, N, is not plausible."
		const char* job;					// "The modkit limit is not changed": the start of every failure line
		size_t entrySize;					// bytes per entry
		uint32_t maxEntries;				// the new array's size, and the setting's maximum
		const Setting* setting;				// the size asked for
		std::span<const Anchor> anchors;
		size_t scannedReferences;			// 0: the anchors' references are the ones moved; else exactly this many "lea r64,
											// [rip+disp32]" load the array, found by its address, and those are moved
		const char* leaPrefixes;			// the REX bytes a scanned lea may start with ("\x48\x4C"); nullptr: any with REX.W
		size_t leaPrefixCount;
	};

	// A limit as found: where its value lies (the imm32, or the variable itself) and how it encodes N.
	struct FoundLimit
	{
		uint8_t* value;
		int mul;
		int add;
	};

	// What FindRelocation found and checked.
	struct RelocationPlan
	{
		const uint8_t* array = nullptr;					// the game's own array
		uint32_t gameSize = 0;							// its number of entries
		uint32_t size = 0;								// the number to use: the setting, clamped to gameSize..maxEntries
		std::vector<std::vector<uint8_t*>> matches;		// per anchor, its matches - for the sites a part reads itself
		std::vector<uint8_t*> references;				// the rel32 fields that are moved
		std::vector<FoundLimit> limits;
	};

	// Reports a failure of the part for the start-up summary: "<job> this session: <why>".
	void RelocationFailed(const RelocationSpec& spec, const std::string& why);

	// Finds every site of spec and checks it: every anchor matches as often as it must, every reference
	// points at one array inside the game, every limit is the expected instruction and decodes to the
	// same plausible size, the scanned loads are exactly as many as expected. Clamps the setting to the
	// game's own size..maxEntries. False, reported, if the part must leave the game as it is.
	bool FindRelocation(const RelocationSpec& spec, RelocationPlan& plan);

	// DRY_RUN: logs what would change (with also, the part's own addition to the line) and returns true.
	// Otherwise allocates the new array - the full maxEntries, read/write, near the game, zeroed - and
	// writes every reference (its rel32 recomputed, with the distance check) and every limit (re-encoded
	// for plan.size), together with the part's own writes and redirected calls, all or nothing; then logs
	// "The <name> moved and its limit raised from A to B entries." False, reported, if nothing was written.
	bool ApplyRelocation(const RelocationSpec& spec, const RelocationPlan& plan, std::vector<CodeWrite> alsoWrite,
		const std::vector<CallRedirect>& redirects, const std::string& also);
}
