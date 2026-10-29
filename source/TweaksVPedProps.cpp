// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the props per anchor of a ped model, measured first (see TweaksVPedProps.h).
//
// A ped model's props - hats, glasses, watches - hang on 13 anchors (0 is the head's) and come in variation files,
// the model's own and one per clothing pack. The game counts an anchor's props over all of a model's files in 32
// bits, but narrows that total to 8 bits where it checks a prop - in the check the set-prop native asks (A-E-T1)
// and in the helmet code (A-E-T2..T5) - and narrows a worn prop's index to 8 bits before it looks up the prop's
// metadata (A-E-G1, twice, and A-E-G3..G6). So from 256 props on one anchor, all files together, the game refuses
// the props from (total mod 256) on and takes a worn prop past 255 for another one. The PedProps part to come will
// widen those eleven narrowings from 8 to 16 bits; it goes in only once a mod load reaches the limit, so the limit
// is measured first (SPECIFICATIONS.md 11.10):
//   DIAGNOSTIC_MODE  the one switch: the table of the parts measured first starts the module only with it on.
//                    Every pattern is found as often as it must match and every site checked as the part will
//                    check it - each narrowing a movzx of 8 bits, T1's call into the game (the texture count),
//                    T2..T5's to the per-anchor count whose start A-E-C1 matches, the calls after G1 and G3..G6
//                    all to one function, the metadata lookup - and the complete sets: the game must call the
//                    per-anchor count exactly 7 times and the metadata lookup exactly 17 times, so that a caller
//                    an update adds, which would narrow again, shows. Every site is logged. Then the measurement:
//                    10 seconds in and then every minute while the numbers change, every ped model of the model
//                    info store (A-E-S1), its variation files and their props per anchor (the layout A-E-C1 and
//                    A-E-C2 pin), the most props on one anchor, and a line for every anchor whose total is past
//                    255 and every file that holds more than 255 on one anchor.
// It writes nothing to the game and hooks nothing. The game adds variation files while it loads its packs, so the
// measurement reads its heap through CopyMemorySafely, within bounds of its own, without a lock. A pattern not
// found, a site not as expected, an address outside the game or a thread that cannot start is logged, not reported
// - an aid (NOTES.md, T-DIAG) - and nothing more is measured. DRY_RUN changes nothing here.
// Written for TweaksV from the research's own findings in the game's code (PATTERNS.md, "PedProps"); no code of
// anyone's was read. The part's idea, and its credit, come with its writing half (ROADMAP.md).

#include "TweaksVPedProps.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVMeasure.h"
#include "TweaksVPatch.h"
#include "TweaksVPatterns.h"
#include "TweaksVScanner.h"
#include "TweaksVScripts.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "PedProps";

		// ---- the eleven narrowings the part will widen, and the calls around them (PATTERNS.md, "PedProps")

		constexpr int kMovzx8 = 0xB6;		// 0F B6, movzx r32, r/m8 - the part will make it 0F B7, movzx r32, r/m16
		constexpr int kCallOpcode = 0xE8;	// call rel32
		constexpr size_t kCallSize = 5;

		enum class Callee { TextureCount, PerAnchorCount, MetadataLookup };	// where a narrowing's call leads

		const char* CalleeName(Callee callee)
		{
			switch (callee)
			{
			case Callee::TextureCount:
				return "texture count";
			case Callee::PerAnchorCount:
				return "per-anchor count";
			default:
				return "metadata lookup";
			}
		}

		// A narrowing - movzx r32, r8: 0F B6 and its ModRM, a REX prefix before them where it has one - and the call in
		// its pattern's match.
		struct Narrowing
		{
			const GamePattern* pattern;
			size_t matches;			// how often its pattern must match
			size_t at;				// the movzx in the match
			size_t length;			// its bytes: 3, or 4 with a REX prefix
			const char* narrows;	// what it narrows, for the log
			size_t call;			// the call in the match
			Callee callee;			// where that call must lead
		};

		constexpr const char* kTotal = "an anchor's total";
		constexpr const char* kIndex = "a worn prop's index";
		constexpr Narrowing kNarrowings[] = {
			{ &Pattern("A-E-T1"), 1, 0, 3, kTotal, 8, Callee::TextureCount },		// the check the set-prop native asks
			{ &Pattern("A-E-T2"), 1, 10, 3, kTotal, 5, Callee::PerAnchorCount },	// the helmet code: the head's total
			{ &Pattern("A-E-T3"), 1, 10, 3, kTotal, 5, Callee::PerAnchorCount },	// after each of its four calls of
			{ &Pattern("A-E-T4"), 1, 10, 3, kTotal, 5, Callee::PerAnchorCount },	// the per-anchor count
			{ &Pattern("A-E-T5"), 1, 10, 3, kTotal, 5, Callee::PerAnchorCount },
			{ &Pattern("A-E-G1"), 2, 15, 4, kIndex, 23, Callee::MetadataLookup },	// two copies of one lookup
			{ &Pattern("A-E-G3"), 1, 14, 3, kIndex, 25, Callee::MetadataLookup },	// for three lookups, the first at +25
			{ &Pattern("A-E-G4"), 1, 22, 4, kIndex, 29, Callee::MetadataLookup },
			{ &Pattern("A-E-G5"), 1, 13, 4, kIndex, 23, Callee::MetadataLookup },
			{ &Pattern("A-E-G6"), 1, 5, 4, kIndex, 22, Callee::MetadataLookup },	// for six lookups, the first at +22
		};

		// Each narrowing as the table says, against its pattern's bytes, when TweaksV is compiled: 0F B6 - which the
		// pattern pins, so that a match is the check the part will make - with a REX prefix where it is 4 bytes long,
		// its ModRM a register (mod 11) that is never AH..BH, whose 16-bit form would be another register, so that the
		// part's 16-bit read takes the low 16 bits of the register the game narrows; and an E8 where its call is.
		consteval bool NarrowingsArePinned()
		{
			for (const Narrowing& site : kNarrowings)
			{
				const size_t opcode = site.at + site.length - 3;
				const bool rex = site.length == 4;
				if (site.length != 3 && !rex)
					return false;
				if (rex && (PatternByte(*site.pattern, site.at) & 0xF0) != 0x40)
					return false;
				if (PatternByte(*site.pattern, opcode) != 0x0F || PatternByte(*site.pattern, opcode + 1) != kMovzx8)
					return false;
				const int modrm = PatternByte(*site.pattern, opcode + 2);
				if (modrm < 0 || (modrm & 0xC0) != 0xC0 || (!rex && (modrm & 7) >= 4))
					return false;
				if (PatternByte(*site.pattern, site.call) != kCallOpcode)
					return false;
			}
			return true;
		}
		static_assert(NarrowingsArePinned(), "TweaksV: each ped props narrowing and its call must be where its pattern pins them");

		// The complete sets: how often the game calls each of the two functions (PATTERNS.md, "PedProps").
		constexpr size_t kPerAnchorCountCalls = 7;		// the helmet code's: four narrowed (A-E-T2..T5), three unused
		constexpr size_t kMetadataLookupCalls = 17;		// 13 after the narrowings, 1 in the random props' builder, 2 in
														// their picker, 1 in a thunk

		// ---- the measurement's sites: the layout it reads, each value checked when TweaksV is compiled against the
		// bytes its pattern holds

		constexpr const GamePattern& kCountPattern = Pattern("A-E-C1");	// the per-anchor count's start
		constexpr const GamePattern& kLoopPattern = Pattern("A-E-C2");	// its loop over a variation file's anchors
		constexpr const GamePattern& kStorePattern = Pattern("A-E-S1");	// the model info store's lookup and a ped's type

		// The dword a pattern holds at index, or -1 where it holds a wildcard.
		consteval int64_t PatternDword(const GamePattern& pattern, size_t index)
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

		constexpr size_t kCollectionOffset = 0x240;	// a model info's variation collection, a pointer (A-E-C1, +9)
		constexpr size_t kFileCountOffset = 8;		// the collection's count of variation files, a word; the files' pointers
													// at +0 (A-E-C1, +17)
		constexpr size_t kAnchorsOffset = 0x58;		// a variation file's anchors, a pointer (A-E-C2, +39)
		constexpr size_t kAnchorCountOffset = 0x60;	// their count, a word (A-E-C2, +27)
		constexpr size_t kAnchorEntrySize = 0x18;	// an anchor's entry: shl rsi, 3 and lea rsi, [rsi+rsi*2] (A-E-C2, +40)
		constexpr size_t kPropCountOffset = 8;		// an entry's props, a word (A-E-C2, +5)
		constexpr size_t kAnchorIdOffset = 0x10;	// an entry's anchor, a dword (no pattern: compared after the loop's alignment)
		constexpr uint32_t kAnchorCount = 13;		// the anchors, 0..12 (no pattern)
		constexpr size_t kTypeOffset = 0x9D;		// a model info's type, a byte (A-E-S1, +81)
		constexpr uint8_t kTypeMask = 0x1F;			// its low 5 bits (A-E-S1, +86)
		constexpr uint8_t kPedType = 6;				// a ped's (A-E-S1, +88)
		constexpr size_t kHashOffset = 0x18;		// a model info's hash, a dword (no pattern)
		static_assert(PatternDword(kCountPattern, 9) == kCollectionOffset && PatternByte(kCountPattern, 17) == kFileCountOffset,
			"TweaksV: A-E-C1 holds the variation collection's offset at +9 and its count's at +17");
		static_assert(PatternByte(kLoopPattern, 5) == kPropCountOffset && PatternByte(kLoopPattern, 27) == kAnchorCountOffset &&
			PatternByte(kLoopPattern, 39) == kAnchorsOffset, "TweaksV: A-E-C2 holds the props', the anchors' count's and the anchors' offsets");
		static_assert(PatternByte(kLoopPattern, 40) == 0x48 && PatternByte(kLoopPattern, 41) == 0xC1 && PatternByte(kLoopPattern, 42) == 0xE6 &&
			PatternDword(kLoopPattern, 44) == 0x76348D48 && (size_t{ 1 } << PatternByte(kLoopPattern, 43)) * 3 == kAnchorEntrySize,
			"TweaksV: A-E-C2 holds an anchor entry's size as shl rsi, 3 and lea rsi, [rsi+rsi*2] at +40");
		static_assert(PatternDword(kStorePattern, 81) == kTypeOffset && PatternByte(kStorePattern, 86) == kTypeMask &&
			PatternByte(kStorePattern, 88) == kPedType, "TweaksV: A-E-S1 holds a model info's type offset, its mask and the ped type");

		// A-E-S1's four references to the model info store: the disp32 of each instruction, which ends the instruction
		struct StoreGlobal
		{
			const char* name;
			size_t at;			// the disp32 in the match
			size_t size;		// what it references, in bytes
		};
		constexpr StoreGlobal kStoreGlobals[] = {
			{ "count", 2, sizeof(int32_t) },				// cmp eax, [rip+disp32]: the model indices, a dword
			{ "bitmap's pointer", 15, sizeof(uint64_t) },	// mov rcx, [rip+disp32]: a bit per index, set for a model
			{ "array's pointer", 39, sizeof(uint64_t) },	// mov rcx, [rip+disp32]: an element per index, the model info's
															// pointer its first 8 bytes
			{ "stride", 59, sizeof(uint64_t) },				// imul rax, [rip+disp32]: an element's bytes, a qword
		};
		enum StoreIndex { kStoreCount, kStoreBitmap, kStoreArray, kStoreStride };
		static_assert(PatternByte(kStorePattern, 0) == 0x3B && PatternByte(kStorePattern, 1) == 0x05 && PatternByte(kStorePattern, 12) == 0x48 &&
			PatternByte(kStorePattern, 13) == 0x8B && PatternByte(kStorePattern, 14) == 0x0D && PatternByte(kStorePattern, 36) == 0x48 &&
			PatternByte(kStorePattern, 37) == 0x8B && PatternByte(kStorePattern, 38) == 0x0D && PatternDword(kStorePattern, 55) == 0x05AF0F48,
			"TweaksV: A-E-S1 holds the four instructions that reference the model info store, each ending in its disp32");

		// The bounds a pass reads within (SPECIFICATIONS.md 11.10.5): the store's count, its stride, and the files and
		// anchor entries one pass may read - far above any load, so that data the pass misreads cannot keep it busy.
		// A collection's files and a file's anchors are words, at most 65535 each.
		constexpr int32_t kMaxModels = 1000000;
		constexpr uint64_t kMinStride = 8;
		constexpr uint64_t kMaxStride = 4096;
		constexpr uint64_t kMaxReadsPerPass = 4000000;
		constexpr uint32_t kGameLimit = 255;		// the props on one anchor the game's 8-bit checks take whole; one variation
													// file holds at most this many on an anchor, its metadata keeping a prop's
													// index in a byte

		// The two models the summary names; every other one by its hash - the release game holds no model names. A
		// model's hash is the game's hash of its name, as a script's is.
		constexpr uint32_t kFreemodeMale = Scripts::NameHash("mp_m_freemode_01");
		constexpr uint32_t kFreemodeFemale = Scripts::NameHash("mp_f_freemode_01");
		static_assert(kFreemodeMale == 0x705E61F2 && kFreemodeFemale == 0x9C9EFFD8, "TweaksV: the freemode models' hashes must be the game's");

		const volatile int32_t* g_storeCount = nullptr;
		const volatile uint64_t* g_storeBitmap = nullptr;	// the global that holds the bitmap's pointer
		const volatile uint64_t* g_storeArray = nullptr;	// the global that holds the array's pointer
		const volatile uint64_t* g_storeStride = nullptr;

		// ---- the measurement: the model info store walked, every ped model's props per anchor

		// An anchor whose total is past 255, or a variation file that holds more than 255 on one anchor.
		struct Item
		{
			uint32_t model;		// its hash
			uint32_t props;		// the anchor's total, or the file's
			uint32_t files;		// a total: the files that bring props for the anchor; a file: the model's files
			uint16_t file;		// 0 for a total; a file's place among the model's files, from 1
			uint8_t anchor;
		};

		// One pass's result, from Numbers to LogSummary - both run on the measurement's thread.
		struct Pass
		{
			bool counted = false;	// false: the pass ended early, and why says why
			char why[256] = {};
			uint32_t models = 0;	// ped models with variation data
			uint64_t files = 0;		// their variation files
			uint64_t unknown = 0;	// anchor entries that name no anchor of 0..12
			uint32_t unknownModel = 0;
			uint32_t unknownAnchor = 0;
			uint32_t mostProps = 0;	// the most props on one anchor, and where
			uint32_t mostModel = 0;
			uint32_t mostAnchor = 0;
			uint32_t mostFiles = 0;
			uint32_t mostInOne = 0;
			uint64_t totalsPast = 0;	// anchors past 255, every one
			uint64_t filesPast = 0;		// files past 255 on one anchor, every one
			size_t itemCount = 0;		// the items kept: one line each, as long as the cap allows
			Item items[kMaxLinesPerKind];
		};
		Pass g_pass;
		uint64_t g_itemLines = 0;		// the lines of anchors and files past 255 written this session
		bool g_unknownLogged = false;

		std::string Hex(uint64_t value)
		{
			char text[24];
			snprintf(text, sizeof(text), "0x%llX", static_cast<unsigned long long>(value));
			return text;
		}

		std::string ModelName(uint32_t hash)
		{
			if (hash == kFreemodeMale)
				return "mp_m_freemode_01";
			if (hash == kFreemodeFemale)
				return "mp_f_freemode_01";
			char text[24];
			snprintf(text, sizeof(text), "model 0x%08X", hash);
			return text;
		}

		// "1 file", "2 files"
		std::string Count(uint64_t n, const char* one, const char* many)
		{
			return std::to_string(n) + " " + (n == 1 ? one : many);
		}

		// The pass ends here: why, for the summary's line.
		void Stop(Pass& pass, const std::string& why)
		{
			snprintf(pass.why, sizeof(pass.why), "%s", why.c_str());
		}

		// Reads a value of the game's heap; false when it cannot be read at that moment.
		template <typename T> bool Read(T& value, uint64_t address)
		{
			return CopyMemorySafely(&value, reinterpret_cast<const void*>(address), sizeof(T));
		}

		void Keep(Pass& pass, const Item& item)
		{
			if (pass.itemCount < std::size(pass.items))
				pass.items[pass.itemCount++] = item;
		}

		// One ped model: its variation files' props per anchor - the game counts an anchor's first entry in each file,
		// as its per-anchor count does - added up per anchor, every count taken whole. False when the pass ends.
		bool CountModel(Pass& pass, uint32_t hash, uint64_t collection, uint64_t& reads)
		{
			uint64_t files = 0;
			uint16_t fileCount = 0;
			if (!Read(files, collection) || !Read(fileCount, collection + kFileCountOffset))
			{
				Stop(pass, "the variation collection of " + ModelName(hash) + " at " + DescribeAddress(reinterpret_cast<const void*>(collection)) +
					" cannot be read.");
				return false;
			}
			struct Sum
			{
				uint32_t props, files, inOne;
			} sums[kAnchorCount] = {};
			uint32_t present = 0;
			for (uint32_t f = 0; f < fileCount; f++)
			{
				uint64_t file = 0;
				uint64_t anchors = 0;
				uint16_t anchorCount = 0;
				if (++reads > kMaxReadsPerPass)
				{
					Stop(pass, "more than " + std::to_string(kMaxReadsPerPass) + " variation files and anchor entries to read in one pass - the model "
						"info store does not hold what the module expects.");
					return false;
				}
				if (!Read(file, files + f * sizeof(uint64_t)))
				{
					Stop(pass, "the variation files of " + ModelName(hash) + " at " + DescribeAddress(reinterpret_cast<const void*>(files)) + " cannot be read.");
					return false;
				}
				if (!file)
					continue;
				present++;
				if (!Read(anchors, file + kAnchorsOffset) || !Read(anchorCount, file + kAnchorCountOffset))
				{
					Stop(pass, "a variation file of " + ModelName(hash) + " at " + DescribeAddress(reinterpret_cast<const void*>(file)) + " cannot be read.");
					return false;
				}
				uint32_t seen = 0;	// the anchors this file has counted
				for (uint32_t a = 0; a < anchorCount; a++)
				{
					uint8_t entry[kAnchorEntrySize];
					if (++reads > kMaxReadsPerPass)
					{
						Stop(pass, "more than " + std::to_string(kMaxReadsPerPass) + " variation files and anchor entries to read in one pass - the model "
							"info store does not hold what the module expects.");
						return false;
					}
					if (!CopyMemorySafely(entry, reinterpret_cast<const void*>(anchors + a * kAnchorEntrySize), sizeof(entry)))
					{
						Stop(pass, "the anchors of a variation file of " + ModelName(hash) + " at " + DescribeAddress(reinterpret_cast<const void*>(anchors)) +
							" cannot be read.");
						return false;
					}
					uint16_t props = 0;
					uint32_t anchor = 0;
					memcpy(&props, entry + kPropCountOffset, sizeof(props));
					memcpy(&anchor, entry + kAnchorIdOffset, sizeof(anchor));
					if (anchor >= kAnchorCount)
					{
						if (!pass.unknown++)
						{
							pass.unknownModel = hash;
							pass.unknownAnchor = anchor;
						}
						continue;
					}
					if (seen & (1u << anchor))
						continue;
					seen |= 1u << anchor;
					if (!props)
						continue;
					Sum& sum = sums[anchor];
					sum.props += props;
					sum.files++;
					if (props > sum.inOne)
						sum.inOne = props;
					if (props > kGameLimit)
					{
						pass.filesPast++;
						Keep(pass, { hash, props, fileCount, static_cast<uint16_t>(f + 1), static_cast<uint8_t>(anchor) });
					}
				}
			}
			pass.models++;
			pass.files += present;
			for (uint32_t anchor = 0; anchor < kAnchorCount; anchor++)
			{
				const Sum& sum = sums[anchor];
				if (sum.props > pass.mostProps)
				{
					pass.mostProps = sum.props;
					pass.mostModel = hash;
					pass.mostAnchor = anchor;
					pass.mostFiles = sum.files;
					pass.mostInOne = sum.inOne;
				}
				if (sum.props > kGameLimit)
				{
					pass.totalsPast++;
					Keep(pass, { hash, sum.props, sum.files, 0, static_cast<uint8_t>(anchor) });
				}
			}
			return true;
		}

		// The whole store, read once without a lock: every model index below the count whose bit is set, the model
		// info its element's first 8 bytes point to, and every ped model's variation files. Every read of the heap
		// through CopyMemorySafely, within the bounds above; the store's own globals lie inside the game, checked once.
		void Walk(Pass& pass)
		{
			pass.counted = false;
			pass.why[0] = '\0';
			pass.models = 0;
			pass.files = pass.unknown = pass.totalsPast = pass.filesPast = 0;
			pass.unknownModel = pass.unknownAnchor = 0;
			pass.mostProps = pass.mostModel = pass.mostAnchor = pass.mostFiles = pass.mostInOne = 0;
			pass.itemCount = 0;

			const int32_t count = *g_storeCount;
			const uint64_t bitmap = *g_storeBitmap;
			const uint64_t array = *g_storeArray;
			const uint64_t stride = *g_storeStride;
			if (count < 0 || count > kMaxModels)
			{
				Stop(pass, "the model info store's count, " + std::to_string(count) + ", is not plausible.");
				return;
			}
			if (count > 0 && bitmap && array && (stride < kMinStride || stride > kMaxStride || stride % 8 != 0))
			{
				Stop(pass, "the model info store's stride, " + std::to_string(stride) + ", is not plausible.");
				return;
			}
			uint64_t reads = 0;
			for (int32_t first = 0; bitmap && array && first < count; first += 32)
			{
				uint32_t bits = 0;
				if (!Read(bits, bitmap + static_cast<uint64_t>(first / 32) * sizeof(uint32_t)))
				{
					Stop(pass, "the model info store's bitmap at " + DescribeAddress(reinterpret_cast<const void*>(bitmap)) + " cannot be read.");
					return;
				}
				for (int32_t index = first; bits && index < count; index++, bits >>= 1)
				{
					if (!(bits & 1))
						continue;
					uint64_t info = 0;
					uint8_t type = 0;
					if (!Read(info, array + static_cast<uint64_t>(index) * stride))
					{
						Stop(pass, "the model info store's array at " + DescribeAddress(reinterpret_cast<const void*>(array)) + " cannot be read.");
						return;
					}
					if (!info)
						continue;
					if (!Read(type, info + kTypeOffset))
					{
						Stop(pass, "the model info at " + DescribeAddress(reinterpret_cast<const void*>(info)) + " (model index " + std::to_string(index) +
							") cannot be read.");
						return;
					}
					if ((type & kTypeMask) != kPedType)
						continue;
					uint32_t hash = 0;
					uint64_t collection = 0;
					if (!Read(hash, info + kHashOffset) || !Read(collection, info + kCollectionOffset))
					{
						Stop(pass, "the model info at " + DescribeAddress(reinterpret_cast<const void*>(info)) + " (model index " + std::to_string(index) +
							") cannot be read.");
						return;
					}
					if (collection && !CountModel(pass, hash, collection, reads))	// null: a ped model without variation data
						return;
				}
			}
			pass.counted = true;
		}

		// FNV-1a over the bytes of a value, for the number that changes whenever the summary has something new to say.
		template <typename T> void Mix(uint64_t& hash, const T& value)
		{
			const auto bytes = reinterpret_cast<const uint8_t*>(&value);
			for (size_t i = 0; i < sizeof(T); i++)
				hash = (hash ^ bytes[i]) * 0x100000001B3ull;
		}

		uint64_t Numbers()
		{
			Pass& pass = g_pass;
			Walk(pass);
			uint64_t hash = 0xCBF29CE484222325ull;
			Mix(hash, pass.counted);
			for (const char* c = pass.why; *c; c++)
				Mix(hash, *c);
			for (const uint64_t value : { uint64_t{ pass.models }, pass.files, pass.unknown, uint64_t{ pass.mostProps }, uint64_t{ pass.mostModel },
				uint64_t{ pass.mostAnchor }, uint64_t{ pass.mostFiles }, uint64_t{ pass.mostInOne }, pass.totalsPast, pass.filesPast })
				Mix(hash, value);
			for (size_t i = 0; i < pass.itemCount; i++)
			{
				const Item& item = pass.items[i];
				for (const uint32_t value : { item.model, item.props, item.files, uint32_t{ item.file }, uint32_t{ item.anchor } })
					Mix(hash, value);
			}
			return hash == UINT64_MAX ? 0 : hash;	// UINT64_MAX is the measurement loop's "no summary yet"
		}

		void LogItem(const Item& item)
		{
			std::string line = "DIAGNOSTIC: ped props: " + ModelName(item.model) + ", anchor " + std::to_string(item.anchor) + ": ";
			if (item.file == 0)
				line += Count(item.props, "prop", "props") + " from " + Count(item.files, "file", "files") + " - past the game's own limit of " +
					std::to_string(kGameLimit) + ": the game's own checks refuse its props from index " + std::to_string(item.props % (kGameLimit + 1)) + " on.";
			else
				line += "its file " + std::to_string(item.file) + " of " + std::to_string(item.files) + " holds " + std::to_string(item.props) +
					" props - a file holds " + std::to_string(kGameLimit) + " at most, its props past " + std::to_string(kGameLimit) +
					" stay unusable with or without TweaksV.";
			WriteLog(kTag, line);
		}

		void LogSummary(uint64_t)
		{
			const Pass& pass = g_pass;
			if (!pass.counted)
			{
				WriteLog(kTag, std::string("DIAGNOSTIC: the ped props are not counted: ") + pass.why);
				return;
			}
			if (!pass.models)
			{
				WriteLog(kTag, "DIAGNOSTIC: ped props: no ped model with variation data yet.");
				return;
			}
			std::string line = "DIAGNOSTIC: ped props: " + Count(pass.models, "ped model", "ped models") + " with variation data in " +
				Count(pass.files, "file", "files");
			if (pass.mostProps)
				line += "; the most props on one anchor: " + std::to_string(pass.mostProps) + " (" + ModelName(pass.mostModel) + ", anchor " +
					std::to_string(pass.mostAnchor) + ", from " + Count(pass.mostFiles, "file", "files") + ", at most " + std::to_string(pass.mostInOne) + " in one)";
			else
				line += "; no props on any anchor";
			if (pass.unknown)
				line += "; " + Count(pass.unknown, "anchor entry names", "anchor entries name") + " no anchor of 0.." + std::to_string(kAnchorCount - 1);
			if (pass.totalsPast || pass.filesPast)
				line += "; past " + std::to_string(kGameLimit) + ": " + Count(pass.totalsPast, "anchor", "anchors") + " and " +
					Count(pass.filesPast, "file", "files") + ", each in a line of its own.";
			else
				line += "; no anchor and no file past " + std::to_string(kGameLimit) + ".";
			WriteLog(kTag, line);
			if (pass.unknown && !g_unknownLogged)
			{
				g_unknownLogged = true;
				WriteLog(kTag, "DIAGNOSTIC: ped props: an anchor entry of " + ModelName(pass.unknownModel) + " names anchor " +
					std::to_string(pass.unknownAnchor) + ", not one of 0.." + std::to_string(kAnchorCount - 1) + " - such entries are counted apart (logged once).");
			}
			for (size_t i = 0; i < pass.itemCount; i++)
			{
				if (g_itemLines == kMaxLinesPerKind)
				{
					g_itemLines++;	// the cap's line once
					WriteLog(kTag, "DIAGNOSTIC: " + std::to_string(kMaxLinesPerKind) + " lines of ped props past " + std::to_string(kGameLimit) +
						" written - from now on they are only counted; the summary every minute has the numbers.");
				}
				if (g_itemLines >= kMaxLinesPerKind)
					break;
				g_itemLines++;
				LogItem(pass.items[i]);
			}
		}

		// The first summary 10 seconds in, then one a minute while the numbers change.
		constexpr Measurement kMeasurement = { "ped props summary", kFirstSummaryMs, &Numbers, &LogSummary };

		// "DIAGNOSTIC: the ped props are not measured: <why>" - logged, not reported: an aid (NOTES.md, T-DIAG)
		void NotMeasured(const std::string& why)
		{
			WriteLog(kTag, "DIAGNOSTIC: the ped props are not measured: " + why);
		}

		// "1 time, not the expected 2." - "3 or more times, not ..." for a search that stopped at its limit
		std::string Times(size_t found, size_t expected)
		{
			return std::to_string(found) + (found > expected ? " or more" : "") + (found == 1 ? " time" : " times") + ", not the expected " +
				std::to_string(expected) + ".";
		}
	}

	void MeasurePedProps()
	{
		// 1. the thirteen patterns, each as often as it must match: every one is looked for, so that the log names
		// each one missing, and the first problem in the table's order is the reason
		std::vector<uint8_t*> found[std::size(kNarrowings)];
		std::string problem;
		for (size_t i = 0; i < std::size(kNarrowings); i++)
		{
			const Narrowing& site = kNarrowings[i];
			const std::string label = site.pattern->label;
			if (site.matches == 1)
			{
				if (uint8_t* const match = Scanner::Find(*site.pattern))
					found[i].push_back(match);
			}
			else
				found[i] = Scanner::FindAll(*site.pattern, site.matches + 1);	// a match more shows
			if (found[i].size() != site.matches && problem.empty())
				problem = found[i].empty() ? label + " was not found." : "the game code holds " + label + " " + Times(found[i].size(), site.matches);
		}
		const uint8_t* const perAnchorCount = Scanner::Find(kCountPattern);
		const uint8_t* const loop = Scanner::Find(kLoopPattern);
		const uint8_t* const store = Scanner::Find(kStorePattern);
		const GamePattern* const layout[] = { &kCountPattern, &kLoopPattern, &kStorePattern };
		const uint8_t* const layoutMatch[] = { perAnchorCount, loop, store };
		for (size_t i = 0; i < std::size(layout) && problem.empty(); i++)
			if (!layoutMatch[i])
				problem = std::string(layout[i]->label) + " was not found.";
		if (!problem.empty())
		{
			NotMeasured(problem);
			return;
		}

		// 2. every narrowing - 0F B6, pinned by its pattern - and the call in its match: T1's into the game, the texture
		// count; T2..T5's to the per-anchor count, whose start A-E-C1 matches; the ones after G1 and G3..G6 all to one
		// function, the metadata lookup
		const uint8_t* lookup = nullptr;
		for (size_t i = 0; i < std::size(kNarrowings); i++)
		{
			const Narrowing& site = kNarrowings[i];
			const std::string label = site.pattern->label;
			for (const uint8_t* const match : found[i])
			{
				const uint8_t* const movzx = match + site.at;
				WriteLog(kTag, "DIAGNOSTIC: narrowing at " + DescribeAddress(movzx) + " holds " + HexBytes(movzx, site.length) + " (" + site.narrows +
					", " + label + ")");
				const uint8_t* const call = match + site.call;
				const uint8_t* const target = IsReadable(call, kCallSize) ? CallTarget(call) : nullptr;	// its rel32 can lie past the match
				const std::string name = CalleeName(site.callee);
				WriteLog(kTag, "DIAGNOSTIC: " + name + " call at " + DescribeAddress(call) + " -> " + (target ? DescribeAddress(target) :
					std::string("not a call")) + " (" + label + ")");
				if (!target || !IsInsideGameImage(target))
				{
					NotMeasured("the " + name + " call at " + DescribeAddress(call) + " is not a call into the game (another mod may have hooked it "
						"already) (" + label + ").");
					return;
				}
				if (site.callee == Callee::PerAnchorCount && target != perAnchorCount)
				{
					NotMeasured("the per-anchor count call at " + DescribeAddress(call) + " leads to " + DescribeAddress(target) + ", not to the "
						"per-anchor count at " + DescribeAddress(perAnchorCount) + " (" + kCountPattern.label + ") (" + label + ").");
					return;
				}
				if (site.callee == Callee::MetadataLookup)
				{
					if (!lookup)
						lookup = target;
					else if (target != lookup)
					{
						NotMeasured("the metadata lookup calls do not lead to the same function: " + DescribeAddress(lookup) + " and " +
							DescribeAddress(target) + " (" + label + ").");
						return;
					}
				}
			}
		}

		// 3. the complete sets: every call of the per-anchor count and of the metadata lookup in the game's code -
		// exactly as many as the research found, so that a caller a game update adds, which would narrow again, shows
		struct Callers
		{
			const char* name;
			const uint8_t* function;
			size_t expected;
		};
		const Callers callers[] = { { "per-anchor count", perAnchorCount, kPerAnchorCountCalls }, { "metadata lookup", lookup, kMetadataLookupCalls } };
		for (const Callers& function : callers)
		{
			const std::vector<uint8_t*> calls = Scanner::FindCallsTo(function.function, function.expected + 1);	// a call more shows
			for (const uint8_t* const call : calls)
				WriteLog(kTag, "DIAGNOSTIC: " + std::string(function.name) + " call at " + DescribeAddress(call) + " -> " +
					DescribeAddress(function.function) + " (call scan)");
			if (calls.size() != function.expected)
			{
				NotMeasured("the game code calls the " + std::string(function.name) + " at " + DescribeAddress(function.function) + " in " +
					std::to_string(calls.size()) + (calls.size() > function.expected ? " or more" : "") + " places, not the expected " +
					std::to_string(function.expected) + " (another mod may have redirected some of them, or the game has changed).");
				return;
			}
		}

		// 4. the measurement's sites: the layout A-E-C1 and A-E-C2 pin, checked when TweaksV is compiled, and the model
		// info store's four globals, each inside the game
		WriteLog(kTag, "DIAGNOSTIC: per-anchor count at " + DescribeAddress(perAnchorCount) + ": a model info's variation collection at +" +
			Hex(kCollectionOffset) + ", its count of variation files at +" + Hex(kFileCountOffset) + " (" + kCountPattern.label + ")");
		WriteLog(kTag, "DIAGNOSTIC: its loop at " + DescribeAddress(loop) + ": a variation file's anchors at +" + Hex(kAnchorsOffset) + ", their count at +" +
			Hex(kAnchorCountOffset) + ", entries of " + Hex(kAnchorEntrySize) + " bytes, an entry's props at +" + Hex(kPropCountOffset) + " (" +
			kLoopPattern.label + ")");
		const uint8_t* globals[std::size(kStoreGlobals)] = {};
		std::string line = "DIAGNOSTIC: model info store at " + DescribeAddress(store) + ":";
		for (size_t i = 0; i < std::size(kStoreGlobals); i++)
		{
			const StoreGlobal& global = kStoreGlobals[i];
			globals[i] = Rel32Target(store + global.at);
			if (!IsInsideGameImage(globals[i]) || !IsInsideGameImage(globals[i] + global.size - 1))
			{
				NotMeasured("the model info store's " + std::string(global.name) + " at " + DescribeAddress(globals[i]) + " (" + kStorePattern.label +
					") is not inside the game.");
				return;
			}
			if (!IsReadable(globals[i], global.size))
			{
				NotMeasured("the model info store's " + std::string(global.name) + " at " + DescribeAddress(globals[i]) + " (" + kStorePattern.label +
					") cannot be read.");
				return;
			}
			line += std::string(i ? ", its " : " its ") + global.name + " at " + DescribeAddress(globals[i]);
		}
		WriteLog(kTag, line + "; a model info's type in the low 5 bits of +" + Hex(kTypeOffset) + ", " + std::to_string(kPedType) + " for a ped (" +
			kStorePattern.label + ")");
		g_storeCount = reinterpret_cast<const volatile int32_t*>(globals[kStoreCount]);
		g_storeBitmap = reinterpret_cast<const volatile uint64_t*>(globals[kStoreBitmap]);
		g_storeArray = reinterpret_cast<const volatile uint64_t*>(globals[kStoreArray]);
		g_storeStride = reinterpret_cast<const volatile uint64_t*>(globals[kStoreStride]);

		// 5. the measurement, on a thread of its own
		WriteLog(kTag, "DIAGNOSTIC: the model info store at " + DescribeAddress(globals[kStoreArray]) + " (" + kStorePattern.label + "): the props per "
			"anchor of its ped models are logged after 10 seconds, then every minute while they change.");
		StartMeasurement<kMeasurement>();	// one that cannot start is logged
	}
}
