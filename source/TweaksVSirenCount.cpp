// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the sirens per vehicle, measured first (see TweaksVSirenCount.h).
//
// The game draws a vehicle's siren lights in one function, the siren lights' renderer, which the vehicle
// lights' function calls once a frame for each vehicle whose sirens are on. It lights 20 sirens at most: its
// loop runs over 20 lights, and a model's bone structure names the bones of siren1 to siren20 and of their
// glass, siren_glass1 to siren_glass20, as signed bytes - so a model made for more sirens keeps the bones past
// siren20 dark, without an error, and a bone past index 127 cannot be named at all (PATTERNS.md,
// "SirenSettings"). Before the SirenSettings part raises anything, this module measures whether a mod load
// reaches the limit:
//   DIAGNOSTIC_MODE  the measurement, and nothing is looked for without it: the vehicle lights' call of the
//                    renderer goes through TweaksV's call stub to a wrapper that calls the game's renderer
//                    first, with the game's arguments, and then counts the vehicle's model once, in a fixed
//                    table of 1,024 models: on its first pass the siren and glass bones its bone structure
//                    names and 64 lookups by name - siren1 to siren32 and siren_glass1 to siren_glass32 -
//                    through the game's own find-bone-by-name, on every later pass one lookup in the table;
//                    10 seconds after the first model is counted, and then every minute while the numbers
//                    change, the log says how many models were seen, which carry siren bones past siren20
//                    that the game never lights and which lose siren bones past bone 127
//   DRY_RUN          the call is not redirected and nothing is counted: the line that would redirect it
// Every site is found and checked first: the call must lead into the game, to the function the renderer's bone
// read lies in by the game's unwind data, and find-bone-by-name's match must be a function start. A pattern
// missing, a site not as expected, a summary thread that cannot start or a call that cannot be redirected is
// logged, not reported - the measurement is an aid (NOTES.md, T-DIAG) - and nothing is redirected.
// Written for TweaksV from the research's own findings in the game's code (PATTERNS.md); no code of anyone's
// was read.

#include "TweaksVSirenCount.h"

#include <windows.h>

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
#include "TweaksVSettings.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "SirenSettings";

		// ---- the sites (PATTERNS.md, "SirenSettings", the siren count)

		constexpr const GamePattern& kCallPattern = Pattern("I-E-B1");		// the vehicle lights' call of the siren lights' renderer
		constexpr const GamePattern& kBoneReadPattern = Pattern("I-E-B2");	// the renderer's read of a siren's bone
		constexpr const GamePattern& kGlassReadPattern = Pattern("I-E-B3");	// a read of a siren's glass bone
		constexpr const GamePattern& kFindBonePattern = Pattern("I-E-B4");	// the start of find-bone-by-name
		constexpr size_t kRendererCallAt = 13;		// in I-E-B1's match: "call rel32" of the renderer, the call redirected
		constexpr size_t kGetterCallAt = 24;		// in I-E-B1's match: "call rel32" of the siren settings getter, its rel32 past the match
		constexpr size_t kCallSize = 5;
		static_assert(PatternByte(kCallPattern, 0) == 0x48 && PatternByte(kCallPattern, 1) == 0x89 && PatternByte(kCallPattern, 2) == 0x6C &&
			PatternByte(kCallPattern, 3) == 0x24 && PatternByte(kCallPattern, 4) == 0x28 && PatternByte(kCallPattern, 21) == 0x48 &&
			PatternByte(kCallPattern, 22) == 0x89 && PatternByte(kCallPattern, 23) == 0xE9,
			"TweaksV: I-E-B1 hands the renderer rbp as its 6th argument (mov [rsp+28h], rbp) and the getter the same (mov rcx, rbp): the model info");
		static_assert(PatternByte(kCallPattern, kRendererCallAt) == 0xE8 && PatternByte(kCallPattern, kGetterCallAt) == 0xE8,
			"TweaksV: I-E-B1 holds the renderer's call at +13 and the getter's at +24");

		// The model info leads through two pointers from its +0xB0 to the bone structure, whose signed byte at 0x10 +
		// a hierarchy ID is the index of that ID's bone, or below 0 for none: siren n's at 0x98 + n, its glass's at
		// 0xAC + n. find-bone-by-name takes the skeleton data - the skeleton's first field - and a name.
		constexpr size_t kBonesAt = 0xB0;			// in the model info: the pointer to the bone structure's holder (I-E-B2, +3)
		constexpr size_t kSirenBonesAt = 0x99;		// in the bone structure: siren1's byte, then siren2's ... siren20's (I-E-B2, +24)
		constexpr size_t kGlassBonesAt = 0xAD;		// in the bone structure: siren_glass1's byte ... siren_glass20's (I-E-B3, +4)
		constexpr size_t kBoneCountAt = 0x5E;		// in the skeleton data: the number of bones, a uint16 (I-E-B4, +18)
		constexpr size_t kNameHashAt = 0x18;		// in the model info: the hash of the model's name, which names it in the log
		constexpr size_t kBoneIndexAt = 0x42;		// in the bone find-bone-by-name returns: its index, a uint16
		constexpr size_t kBonesRead = 3;			// in I-E-B2's match: the disp32 of "mov rax, [rdi+disp32]", kBonesAt
		constexpr size_t kSirenBonesRead = 24;		// in I-E-B2's match: the disp32 of "movsx eax, byte [rax+rbp+disp32]", kSirenBonesAt
		constexpr size_t kGlassBonesRead = 4;		// in I-E-B3's match: the disp32 of "movsx edx, byte [rax+rbx+disp32]", kGlassBonesAt
		constexpr size_t kBoneCountRead = 18;		// in I-E-B4's match: the disp8 of "cmp word [rcx+disp8], 0", kBoneCountAt
		static_assert(PatternByte(kBoneReadPattern, kBonesRead) == kBonesAt && PatternByte(kBoneReadPattern, kSirenBonesRead) == kSirenBonesAt,
			"TweaksV: I-E-B2 reads the model info's +0xB0 and siren1's byte at +0x99");
		static_assert(PatternByte(kGlassReadPattern, kGlassBonesRead) == kGlassBonesAt, "TweaksV: I-E-B3 reads siren_glass1's byte at +0xAD");
		static_assert(PatternByte(kFindBonePattern, kBoneCountRead) == kBoneCountAt, "TweaksV: I-E-B4 tests the bone count at +0x5E first");

		constexpr int kGameSirens = 20;				// siren1 to siren20: the sirens the game lights, and the glass bones it names
		constexpr int kLookedUp = 32;				// siren1 to siren32: what packs made for more name, the top of the game's 32-bit sets
		constexpr int kNameableBones = 128;			// a bone's index as a signed byte: 0 to 127
		static_assert(kGlassBonesAt == kSirenBonesAt + kGameSirens, "TweaksV: the glass bones' bytes follow the sirens'");

		// "siren1" to "siren32" and "siren_glass1" to "siren_glass32", made when TweaksV is compiled.
		struct BoneName
		{
			char text[16];
		};
		consteval BoneName MakeBoneName(std::string_view prefix, int n)
		{
			BoneName name{};
			size_t i = 0;
			for (size_t c = 0; c < prefix.size() && c + 3 < sizeof(name.text); c++)	// room for two digits and the end
				name.text[i++] = prefix[c];
			if (n >= 10)
				name.text[i++] = static_cast<char>('0' + n / 10 % 10);
			name.text[i] = static_cast<char>('0' + n % 10);
			return name;
		}
		consteval std::array<BoneName, kLookedUp> MakeBoneNames(std::string_view prefix)
		{
			std::array<BoneName, kLookedUp> names{};
			for (int n = 1; n <= kLookedUp; n++)
				names[n - 1] = MakeBoneName(prefix, n);
			return names;
		}
		constexpr std::array<BoneName, kLookedUp> kBoneNames[2] = { MakeBoneNames("siren"), MakeBoneNames("siren_glass") };
		static_assert(std::string_view(kBoneNames[0][0].text) == "siren1" && std::string_view(kBoneNames[0][kLookedUp - 1].text) == "siren32" &&
			std::string_view(kBoneNames[1][kLookedUp - 1].text) == "siren_glass32", "TweaksV: the siren bones' names");
		constexpr const char* kKinds[2] = { "siren", "siren_glass" };

		// The renderer: the vehicle, three floats, a flag byte, the model info, the skeleton, a dword and the coronas'
		// list. The stack's arguments are passed on as 64-bit values, so that each slot reaches the renderer as the
		// game's call left it.
		using SirenLights = uint64_t (*)(void* vehicle, float a, float b, float c, uint64_t flag, const uint8_t* model, const uint8_t* skeleton,
			uint64_t value, void* coronas);
		using FindBone = const uint8_t* (*)(const uint8_t* skeletonData, const char* name);

		SirenLights g_renderer = nullptr;	// the game's renderer, set before the call is redirected
		FindBone g_findBone = nullptr;

		// ---- the vehicle models seen: a fixed table, filled by compare-exchange - no lock, no allocation

		constexpr int kSlotBits = 10;
		constexpr size_t kModelSlots = size_t{ 1 } << kSlotBits;	// 1,024
		constexpr size_t kProbes = 32;								// the slots a model is looked for in, from its hash on
		static_assert(kModelSlots <= kMaxLinesPerKind, "TweaksV: a line per model stays below the cap of the lines per event");

		enum : uint8_t
		{
			kReading = 0,	// claimed, its first pass under way
			kCounted = 1,	// its first pass done
		};

		// One vehicle model: what its first pass found, written before state, read by the summary after it. Index 0
		// of each pair is the sirens, 1 the glass bones.
		struct ModelSlot
		{
			std::atomic<const uint8_t*> model{ nullptr };	// the model info, never freed while the game runs
			std::atomic<uint8_t> state{ kReading };
			const char* unreadable;		// what could not be read, or nullptr
			uint32_t hash;				// the hash of the model's name
			uint16_t bones;				// its skeleton's bones
			uint16_t lostBone;			// the highest index of a siren bone found by name that its bone structure does not name
			uint8_t inTable[2];			// of siren1 to siren20: the bones its bone structure names
			uint8_t byName[2];			// of siren1 to siren32: the bones found by name
			uint8_t highest[2];			// the highest n of siren<n> found by name
			uint8_t past[2];			// found by name past siren20, never lit
			uint8_t lost[2];			// of siren1 to siren20: found by name, not named by its bone structure - past bone 127
			uint8_t missed[2];			// of siren1 to siren20: named by its bone structure, not found by name
			uint8_t moved[2];			// of siren1 to siren20: found by name at another index than its bone structure names
			bool written;				// its line written (the summary's thread only)
		};
		ModelSlot g_models[kModelSlots];

		std::atomic<uint64_t> g_passes{ 0 };		// the renderer's calls counted
		std::atomic<uint64_t> g_notHeld{ 0 };		// passes of models the table could not hold
		std::atomic<uint32_t> g_counted{ 0 };		// models whose first pass is done
		std::atomic<bool> g_firstCounted{ false };
		HANDLE g_firstModel = nullptr;				// set at the first model counted: the summary's clock

		size_t SlotOf(const uint8_t* model)
		{
			return static_cast<size_t>((reinterpret_cast<uintptr_t>(model) * 0x9E3779B97F4A7C15ull) >> (64 - kSlotBits));
		}

		// value from address, when IsReadable says the bytes there can be read: the game's data is never trusted.
		template <typename T> bool Read(const uint8_t* address, T& value)
		{
			if (!IsReadable(address, sizeof(T)))
				return false;
			memcpy(&value, address, sizeof(T));
			return true;
		}

		// A model's first pass: its bone structure's siren and glass bytes, its skeleton's bones, and the 64 lookups by
		// name. Every pointer of the game's goes through IsReadable before it is read.
		void ReadModel(ModelSlot& slot, const uint8_t* model, const uint8_t* skeleton)
		{
			Read(model + kNameHashAt, slot.hash);
			const uint8_t* holder = nullptr;
			const uint8_t* structure = nullptr;
			int8_t table[2][kGameSirens] = {};
			if (!Read(model + kBonesAt, holder) || !Read(holder, structure) || !IsReadable(structure + kSirenBonesAt, sizeof(table)))
			{
				slot.unreadable = "its bone structure (the model info's +0xB0)";
				return;
			}
			memcpy(table, structure + kSirenBonesAt, sizeof(table));	// siren1 to siren20, then siren_glass1 to siren_glass20
			const uint8_t* data = nullptr;
			if (!Read(skeleton, data) || !Read(data + kBoneCountAt, slot.bones))
			{
				slot.unreadable = "its skeleton data (the renderer's 7th argument, its first field)";
				return;
			}
			for (int kind = 0; kind < 2; kind++)
				for (int n = 1; n <= kLookedUp; n++)
				{
					const bool named = n <= kGameSirens && table[kind][n - 1] >= 0;
					if (named)
						slot.inTable[kind]++;
					const uint8_t* const bone = slot.bones ? g_findBone(data, kBoneNames[kind][n - 1].text) : nullptr;
					if (!bone)
					{
						if (named)
							slot.missed[kind]++;
						continue;
					}
					slot.byName[kind]++;
					slot.highest[kind] = static_cast<uint8_t>(n);
					if (n > kGameSirens)
					{
						slot.past[kind]++;
						continue;
					}
					uint16_t index = 0;
					const bool indexRead = Read(bone + kBoneIndexAt, index);
					if (!named)
					{
						slot.lost[kind]++;
						if (indexRead && index > slot.lostBone)
							slot.lostBone = index;
					}
					else if (indexRead && index != static_cast<uint16_t>(table[kind][n - 1]))
						slot.moved[kind]++;
				}
		}

		// Claimed by this pass: the first pass's work, the caller's last error kept around it, and the summary's clock
		// started at the first model counted.
		void FirstPass(ModelSlot& slot, const uint8_t* model, const uint8_t* skeleton)
		{
			const DWORD lastError = GetLastError();
			ReadModel(slot, model, skeleton);
			slot.state.store(kCounted, std::memory_order_release);
			g_counted.fetch_add(1, std::memory_order_release);
			if (!g_firstCounted.load(std::memory_order_relaxed) && !g_firstCounted.exchange(true) && g_firstModel)
				SetEvent(g_firstModel);
			SetLastError(lastError);
		}

		// Each pass: one lookup of the model in the table - its first pass claims a free slot and reads it, any later
		// one finds it. A model that finds neither among its slots is counted as not held.
		void CountPass(const uint8_t* model, const uint8_t* skeleton)
		{
			g_passes.fetch_add(1, std::memory_order_relaxed);
			if (!model)
				return;
			size_t at = SlotOf(model);
			for (size_t probe = 0; probe < kProbes; probe++, at = (at + 1) & (kModelSlots - 1))
			{
				ModelSlot& slot = g_models[at];
				const uint8_t* held = slot.model.load(std::memory_order_acquire);
				if (held == model)
					return;
				if (held)
					continue;
				if (slot.model.compare_exchange_strong(held, model, std::memory_order_acq_rel))
				{
					FirstPass(slot, model, skeleton);
					return;
				}
				if (held == model)	// another thread claimed it for this model just now
					return;
			}
			g_notHeld.fetch_add(1, std::memory_order_relaxed);
		}

		// The game's call of the renderer, redirected: the game's renderer first, with the game's arguments, then the
		// count - on the game's thread, once a frame for each vehicle whose sirens are on.
		uint64_t CountedSirenLights(void* vehicle, float a, float b, float c, uint64_t flag, const uint8_t* model, const uint8_t* skeleton,
			uint64_t value, void* coronas)
		{
			const uint64_t result = g_renderer(vehicle, a, b, c, flag, model, skeleton, value, coronas);
			CountPass(model, skeleton);
			return result;
		}

		// ---- the summary (DIAGNOSTIC_MODE), on its own thread

		uint64_t Numbers()
		{
			return g_counted.load() | (g_notHeld.load() ? uint64_t{ 1 } << 32 : 0);
		}

		// "12 and 12", "1 and 0" - a pair of a model's numbers, the sirens' first.
		std::string Pair(const uint8_t (&numbers)[2])
		{
			return std::to_string(numbers[0]) + " and " + std::to_string(numbers[1]);
		}

		// A model's line: what its bone structure names and what is found by name, and where the two part.
		std::string ModelLine(const ModelSlot& slot)
		{
			char head[64];
			snprintf(head, sizeof(head), "DIAGNOSTIC: vehicle model 0x%08X", slot.hash);
			if (slot.unreadable)
				return std::string(head) + " could not be read: " + slot.unreadable + ".";
			std::string line = std::string(head) + ", " + std::to_string(slot.bones) + " bones - siren and glass bones by name: " + Pair(slot.byName) +
				", up to ";
			for (int kind = 0; kind < 2; kind++)
				line += (kind ? " and " : "") + (slot.highest[kind] ? std::string(kKinds[kind]) + std::to_string(slot.highest[kind]) :
					"no " + std::string(kKinds[kind]));
			line += "; named by its bone structure: " + Pair(slot.inTable);
			if (slot.past[0] || slot.past[1])
				line += "; past siren20, never lit: " + Pair(slot.past);
			if (slot.lost[0] || slot.lost[1])
				line += "; lost past bone " + std::to_string(kNameableBones - 1) + ", found by name but not named by its bone structure: " +
					Pair(slot.lost) + " (the highest bone " + std::to_string(slot.lostBone) + ")";
			if (slot.missed[0] || slot.missed[1])
				line += "; named by its bone structure but not found by name: " + Pair(slot.missed);
			if (slot.moved[0] || slot.moved[1])
				line += "; found by name at another bone than its bone structure names: " + Pair(slot.moved);
			return line + ".";
		}

		// A model gets a line of its own when the limit binds it, or when it could not be read or its bones by name and
		// its bone structure disagree - which says the measurement itself is not right for it.
		bool WorthALine(const ModelSlot& slot)
		{
			return slot.unreadable || slot.past[0] || slot.past[1] || slot.lost[0] || slot.lost[1] || slot.missed[0] || slot.missed[1] ||
				slot.moved[0] || slot.moved[1];
		}

		// The models counted so far, then a line for each model that is worth one and has none yet.
		void LogSummary(uint64_t)
		{
			size_t seen = 0, past = 0, lost = 0, unreadable = 0;
			const ModelSlot* most = nullptr;	// the model whose bone structure names the most sirens - of two, the lower hash
			std::vector<ModelSlot*> lines;
			for (ModelSlot& slot : g_models)
			{
				if (!slot.model.load(std::memory_order_acquire) || slot.state.load(std::memory_order_acquire) != kCounted)
					continue;
				seen++;
				if (slot.unreadable)
					unreadable++;
				else
				{
					if (slot.past[0] || slot.past[1])
						past++;
					if (slot.lost[0] || slot.lost[1])
						lost++;
					if (!most || slot.inTable[0] > most->inTable[0] || (slot.inTable[0] == most->inTable[0] && slot.hash < most->hash))
						most = &slot;
				}
				if (!slot.written && WorthALine(slot))
					lines.push_back(&slot);
			}
			std::string line = "DIAGNOSTIC: " + std::to_string(seen) + " vehicle models with sirens seen in " + std::to_string(g_passes.load()) +
				" passes of the siren lights' renderer - the most sirens a model's bone structure names: ";
			if (most)
			{
				char model[32];
				snprintf(model, sizeof(model), " (model 0x%08X)", most->hash);
				line += std::to_string(most->inTable[0]) + " of " + std::to_string(kGameSirens) + model;
			}
			else
				line += "none read";
			line += "; " + std::to_string(past) + " with siren bones past siren20, which the game never lights; " + std::to_string(lost) +
				" losing siren bones past bone " + std::to_string(kNameableBones - 1) + ", which a bone structure cannot name; " +
				std::to_string(unreadable) + " that could not be read";
			if (const uint64_t notHeld = g_notHeld.load())
				line += "; the table full, " + std::to_string(notHeld) + " passes of models it could not hold not counted";
			WriteLog(kTag, line + ".");
			for (ModelSlot* slot : lines)
			{
				WriteLog(kTag, ModelLine(*slot));
				slot->written = true;
			}
		}

		// The first summary 10 seconds after the first model counted, then one a minute while the models change.
		constexpr Measurement kMeasurement = { "siren count summary", kFirstSummaryMs, &Numbers, &LogSummary, &g_firstModel };

		// ---- the sites, found and checked

		// The start of the function the game's unwind data puts address in, or nullptr where it names none.
		const uint8_t* FunctionStartOf(const uint8_t* address)
		{
			DWORD64 imageBase = 0;
			const PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(address), &imageBase, nullptr);
			return entry ? reinterpret_cast<const uint8_t*>(imageBase + entry->BeginAddress) : nullptr;
		}

		void NotMeasured(const std::string& why)
		{
			WriteLog(kTag, "DIAGNOSTIC: the siren count is not measured: " + why);
		}

		// "holds 0xB0" - an offset a site's instruction carries, for the DIAGNOSTIC lines
		std::string Holds(const uint8_t* at)
		{
			char text[16];
			snprintf(text, sizeof(text), "holds 0x%X", static_cast<unsigned>(*at));
			return text;
		}
	}

	void MeasureSirenCount()
	{
		// 1. the four sites, each looked for, so that the log names every one missing
		uint8_t* const call = Scanner::Find(kCallPattern);
		const uint8_t* const boneRead = Scanner::Find(kBoneReadPattern);
		const uint8_t* const glassRead = Scanner::Find(kGlassReadPattern);
		const uint8_t* const findBone = Scanner::Find(kFindBonePattern);
		const GamePattern* const missing = !call ? &kCallPattern : !boneRead ? &kBoneReadPattern : !glassRead ? &kGlassReadPattern :
			!findBone ? &kFindBonePattern : nullptr;
		if (missing)
		{
			NotMeasured(std::string(missing->label) + " was not found.");
			return;
		}

		// 2. the vehicle lights' two calls, each into the game: the renderer's, and the getter's after it, which takes the
		// same model info
		uint8_t* const rendererCall = call + kRendererCallAt;
		const uint8_t* const getterCall = call + kGetterCallAt;
		const uint8_t* const renderer = CallTarget(rendererCall);
		const uint8_t* const getter = IsReadable(getterCall, kCallSize) ? CallTarget(getterCall) : nullptr;
		WriteLog(kTag, "DIAGNOSTIC: siren lights' renderer call at " + DescribeAddress(rendererCall) + " -> " + DescribeAddress(renderer) +
			", siren settings getter call at " + DescribeAddress(getterCall) + " -> " +
			(getter ? DescribeAddress(getter) : std::string("cannot be read")) + " (" + kCallPattern.label + ")");
		if (!IsInsideGameImage(renderer))
		{
			NotMeasured("the siren lights' renderer call at " + DescribeAddress(rendererCall) + " is not a call into the game (another mod may "
				"have hooked it already) (" + kCallPattern.label + ").");
			return;
		}
		if (!getter || !IsInsideGameImage(getter))
		{
			NotMeasured("the siren settings getter call at " + DescribeAddress(getterCall) + " is not a call into the game (another mod may "
				"have hooked it already) (" + kCallPattern.label + ").");
			return;
		}

		// 3. the renderer is the function its bone read lies in, by the game's unwind data
		const uint8_t* const function = FunctionStartOf(boneRead);
		WriteLog(kTag, "DIAGNOSTIC: bone structure offset at " + DescribeAddress(boneRead + kBonesRead) + " " + Holds(boneRead + kBonesRead) +
			", siren1's bone offset at " + DescribeAddress(boneRead + kSirenBonesRead) + " " + Holds(boneRead + kSirenBonesRead) + ", in " +
			(function ? "the function at " + DescribeAddress(function) + " (unwind data)" : std::string("no function the game's unwind data names")) +
			" (" + kBoneReadPattern.label + ")");
		if (function != renderer)
		{
			NotMeasured(std::string(kBoneReadPattern.label) + " at " + DescribeAddress(boneRead) + " does not lie in the siren lights' renderer at " +
				DescribeAddress(renderer) + ": " + (function ? "the game's unwind data puts it in the function at " + DescribeAddress(function) :
				std::string("the game's unwind data names no function there")) + ".");
			return;
		}

		// 4. the glass bones' bytes, and find-bone-by-name: a function start
		WriteLog(kTag, "DIAGNOSTIC: siren_glass1's bone offset at " + DescribeAddress(glassRead + kGlassBonesRead) + " " +
			Holds(glassRead + kGlassBonesRead) + " (" + kGlassReadPattern.label + ")");
		std::string start;
		const bool isStart = IsFunctionStart(findBone, start);
		WriteLog(kTag, "DIAGNOSTIC: find-bone-by-name at " + DescribeAddress(findBone) + ", " + start + ", bone count offset at " +
			DescribeAddress(findBone + kBoneCountRead) + " " + Holds(findBone + kBoneCountRead) + " (" + kFindBonePattern.label + ")");
		if (!isStart)
		{
			NotMeasured("find-bone-by-name at " + DescribeAddress(findBone) + " is not a function start (" + start + ") (" + kFindBonePattern.label + ").");
			return;
		}
		g_findBone = reinterpret_cast<FindBone>(const_cast<uint8_t*>(findBone));

		// 5. a dry run: the line that would redirect the call, nothing counted
		const std::vector<CallRedirect> redirect = { { rendererCall, reinterpret_cast<const void*>(&CountedSirenLights), "siren lights' renderer call",
			reinterpret_cast<void**>(&g_renderer) } };
		if (IsDryRun())
		{
			RedirectCalls(redirect, {}, kTag);
			NotMeasured("DRY_RUN - the siren lights' renderer call is not redirected, so nothing is counted.");
			return;
		}

		// 6. the summary's thread, waiting for the first model counted - then the call: a measurement that cannot sum up
		// redirects nothing, and a call that cannot be redirected counts nothing
		g_firstModel = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if (!g_firstModel)
		{
			NotMeasured("its summary's event could not be made (" + DescribeError(GetLastError()) + ").");
			return;
		}
		if (!StartMeasurement<kMeasurement>())	// its line is written
		{
			NotMeasured("its summary thread could not be started.");
			return;
		}
		if (!RedirectCalls(redirect, {}, kTag))
		{
			NotMeasured("the siren lights' renderer call at " + DescribeAddress(rendererCall) + " could not be redirected (details above).");
			return;
		}
		WriteLog(kTag, "DIAGNOSTIC: the siren lights' renderer call at " + DescribeAddress(rendererCall) + " is redirected (" + kCallPattern.label +
			"): the vehicle models with sirens on, each counted once with its siren bones looked up by name up to siren" +
			std::to_string(kLookedUp) + ", are logged 10 seconds after the first, then every minute while they change.");
	}
}
