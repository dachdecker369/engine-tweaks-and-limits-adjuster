// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the relocation core (see TweaksVRelocation.h).

#include "TweaksVRelocation.h"

#include <windows.h>

#include <cstring>
#include <string>

#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVScanner.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr size_t kLeaSize = 7;	// REX.W, 8D, ModRM, disp32

		// "the modkit array" -> "The modkit array", for the start of a line
		std::string Capitalised(std::string text)
		{
			if (!text.empty() && text[0] >= 'a' && text[0] <= 'z')
				text[0] = static_cast<char>(text[0] - 'a' + 'A');
			return text;
		}

		// "N", "N-1", "N+3", "2N": how a limit encodes the array's size N
		std::string EncodingText(int mul, int add)
		{
			std::string text = mul == 1 ? "N" : std::to_string(mul) + "N";
			if (add < 0)
				text += std::to_string(add);
			else if (add > 0)
				text += "+" + std::to_string(add);
			return text;
		}

		// "48 or 4C": the REX bytes a scanned lea may start with
		std::string PrefixText(const RelocationSpec& spec)
		{
			std::string text;
			for (size_t i = 0; i < spec.leaPrefixCount; i++)
				text += (i ? " or " : "") + HexBytes(spec.leaPrefixes + i, 1);
			return text;
		}

		bool AllowedPrefix(const RelocationSpec& spec, uint8_t prefix)
		{
			if (!spec.leaPrefixes)
				return true;
			for (size_t i = 0; i < spec.leaPrefixCount; i++)
				if (static_cast<uint8_t>(spec.leaPrefixes[i]) == prefix)
					return true;
			return false;
		}
	}

	void RelocationFailed(const RelocationSpec& spec, const std::string& why)
	{
		ReportFailure(spec.tag, std::string(spec.job) + " this session: " + why);
	}

	bool FindRelocation(const RelocationSpec& spec, RelocationPlan& plan)
	{
		const bool diagnostic = IsDiagnosticMode();
		const std::string name = spec.name;
		plan = RelocationPlan{};
		plan.matches.resize(spec.anchors.size());
		std::vector<uint8_t*> anchorReferences;
		bool sizeKnown = false;

		// 1. every anchor: as many matches as it must have, its reference, its limits
		for (size_t a = 0; a < spec.anchors.size(); a++)
		{
			const Anchor& anchor = spec.anchors[a];
			const std::string label = anchor.pattern->label;
			std::vector<uint8_t*>& matches = plan.matches[a];
			matches = Scanner::FindAll(*anchor.pattern, anchor.matches + 1);
			if (matches.size() != anchor.matches)
			{
				if (matches.empty())
					RelocationFailed(spec, label + " was not found.");
				else
					RelocationFailed(spec, "the game code holds " + label + " " + std::to_string(matches.size()) +
						(matches.size() > anchor.matches ? " or more" : "") + " times, not the expected " + std::to_string(anchor.matches) + ".");
				return false;
			}

			for (uint8_t* match : matches)
			{
				if (anchor.hasReference)
				{
					const ReferenceSite& site = anchor.reference;
					uint8_t* const field = match + site.offset;
					uint8_t* const instruction = field - site.opcodeSize;
					if (!IsReadable(instruction, site.opcodeSize + sizeof(int32_t)))
					{
						RelocationFailed(spec, "the reference to the " + name + " at " + DescribeAddress(field) + " is not readable memory (" + label + ").");
						return false;
					}
					if (site.opcode && memcmp(instruction, site.opcode, site.opcodeSize) != 0)
					{
						RelocationFailed(spec, "the reference to the " + name + " at " + DescribeAddress(instruction) +
							" is not the expected instruction (" + HexBytes(instruction, site.opcodeSize + sizeof(int32_t)) + ") (" + label + ").");
						return false;
					}
					const uint8_t* const target = Rel32Target(field);
					if (diagnostic)
						WriteLog(spec.tag, "DIAGNOSTIC: reference at " + DescribeAddress(field) + " -> " + DescribeAddress(target) + " (" + label + ")");
					if (!plan.array)
					{
						if (!IsInsideGameImage(target))	// the game's own array lies inside the game
						{
							RelocationFailed(spec, "the " + name + " is not inside the game: " + DescribeAddress(target) +
								" (another mod may have moved it already).");
							return false;
						}
						plan.array = target;
					}
					else if (target != plan.array)
					{
						RelocationFailed(spec, "the references to the " + name + " do not all point to the game's own array: " +
							DescribeAddress(plan.array) + " and " + DescribeAddress(target) + " (" + label + ").");
						return false;
					}
					anchorReferences.push_back(field);
				}

				for (size_t i = 0; i < anchor.limitCount; i++)
				{
					const LimitSite& site = anchor.limits[i];
					uint8_t* const instruction = match + site.offset;
					uint8_t* value = match + site.valueOffset;
					if (!IsReadable(instruction, site.valueOffset - site.offset + sizeof(uint32_t)))
					{
						RelocationFailed(spec, "the limit of the " + name + " at " + DescribeAddress(instruction) + " is not readable memory (" + label + ").");
						return false;
					}
					if (site.opcode && memcmp(instruction, site.opcode, site.opcodeSize) != 0)
					{
						RelocationFailed(spec, "the limit of the " + name + " at " + DescribeAddress(instruction) +
							" is not the expected instruction (" + HexBytes(instruction, site.opcodeSize) + ") (" + label + ").");
						return false;
					}
					const bool variable = site.kind == LimitSite::Kind::Variable;
					if (variable)
					{
						value = const_cast<uint8_t*>(Rel32Target(value));
						if (!IsInsideGameImage(value) || !IsReadable(value, sizeof(uint32_t)))
						{
							RelocationFailed(spec, "the variable that holds the limit of the " + name + " is not inside the game: " +
								DescribeAddress(value) + " (" + label + ").");
							return false;
						}
					}
					uint32_t holds = 0;
					memcpy(&holds, value, sizeof(holds));

					// N from imm32 = N x mul + add
					const int64_t numerator = static_cast<int64_t>(holds) - site.add;
					if (site.mul <= 0 || numerator < 0 || numerator % site.mul != 0)
					{
						RelocationFailed(spec, "the limit of the " + name + " at " + DescribeAddress(value) + " holds " + std::to_string(holds) +
							", which is " + EncodingText(site.mul, site.add) + " for no size (" + label + ").");
						return false;
					}
					const uint64_t decoded = static_cast<uint64_t>(numerator / site.mul);
					if (diagnostic)
						WriteLog(spec.tag, std::string("DIAGNOSTIC: limit") + (variable ? " variable" : "") + " at " + DescribeAddress(value) + " holds " +
							std::to_string(holds) + " (" + EncodingText(site.mul, site.add) + ", " + label + ") -> game size " + std::to_string(decoded));
					if (!sizeKnown)
					{
						if (decoded == 0 || decoded > spec.maxEntries)
						{
							RelocationFailed(spec, std::string("the game's own ") + spec.sizeName + ", " + std::to_string(decoded) + ", is not plausible.");
							return false;
						}
						plan.gameSize = static_cast<uint32_t>(decoded);
						sizeKnown = true;
					}
					else if (decoded != plan.gameSize)
					{
						RelocationFailed(spec, "the limits of the " + name + " disagree about the game's own size: " + std::to_string(plan.gameSize) +
							" and " + std::to_string(decoded) + " (" + label + ").");
						return false;
					}
					plan.limits.push_back({ value, site.mul, site.add });
				}
			}
		}
		if (!plan.array || !sizeKnown)	// a table without a reference or a limit: the part's own mistake
		{
			RelocationFailed(spec, "its tables name no " + std::string(plan.array ? "limit" : "reference") + ".");
			return false;
		}
		if (diagnostic)
			WriteLog(spec.tag, "DIAGNOSTIC: the " + name + " is at " + DescribeAddress(plan.array) + " - " + std::to_string(plan.gameSize) +
				" entries of " + std::to_string(spec.entrySize) + " bytes");

		// 2. the references that are moved: the anchors', or every lea that loads the array - exactly as
		// many as the game has (fewer means another mod has changed some, more that the game has changed)
		if (spec.scannedReferences == 0)
		{
			plan.references = anchorReferences;
		}
		else
		{
			const std::vector<uint8_t*> loads = Scanner::FindRipRelativeLeas(plan.array, spec.scannedReferences + 1);
			if (diagnostic)
				for (const uint8_t* load : loads)
					WriteLog(spec.tag, "DIAGNOSTIC: reference at " + DescribeAddress(load) + " -> " + DescribeAddress(plan.array) + " (lea scan)");
			if (loads.size() != spec.scannedReferences)
			{
				RelocationFailed(spec, "the game code loads the " + name + " in " + std::to_string(loads.size()) +
					(loads.size() > spec.scannedReferences ? " or more" : "") + " places, not the expected " + std::to_string(spec.scannedReferences) +
					" (another mod may have moved some of them, or the game has changed).");
				return false;
			}
			for (uint8_t* load : loads)
			{
				if (!AllowedPrefix(spec, load[0]))
				{
					RelocationFailed(spec, "the load of the " + name + " at " + DescribeAddress(load) + " is not a \"lea r64, [rip+disp32]\" with a " +
						PrefixText(spec) + " prefix: " + HexBytes(load, kLeaSize) + ".");
					return false;
				}
				plan.references.push_back(load + kLeaSize - sizeof(int32_t));
			}
		}

		// 3. the size to use: the game's own size..maxEntries - smaller would shrink the array
		const uint32_t requested = static_cast<uint32_t>(GetNumber(*spec.setting));
		uint32_t size = requested;
		if (size < plan.gameSize)
			size = plan.gameSize;
		if (size > spec.maxEntries)
			size = spec.maxEntries;
		if (size != requested)
			WriteLog(spec.tag, std::string("TweaksV.ini: ") + spec.setting->key + "=" + std::to_string(requested) + " is outside " +
				std::to_string(plan.gameSize) + ".." + std::to_string(spec.maxEntries) + " - using " + std::to_string(size) + ".");
		plan.size = size;
		return true;
	}

	bool ApplyRelocation(const RelocationSpec& spec, const RelocationPlan& plan, std::vector<CodeWrite> alsoWrite,
		const std::vector<CallRedirect>& redirects, const std::string& also)
	{
		const std::string name = spec.name;
		const std::string change = "from " + std::to_string(plan.gameSize) + " to " + std::to_string(plan.size);
		if (IsDryRun())	// everything found and checked, nothing written
		{
			WriteLog(spec.tag, "DRY_RUN: would move the " + name + " (" + std::to_string(plan.references.size()) + " references) and raise its limit " +
				change + " (" + std::to_string(plan.limits.size()) + " sites)" + also + ".");
			return true;
		}

		// the new array: always the full maxEntries, read/write data near the game (a lea reaches +-2 GB),
		// zeroed by Windows
		uint8_t* const newArray = static_cast<uint8_t*>(AllocateNearGame(spec.entrySize * spec.maxEntries));
		if (!newArray)
		{
			RelocationFailed(spec, "the new " + name + " could not be allocated near the game.");
			return false;
		}

		// one write, all or nothing: every reference to the new array, every limit to the new size, the
		// part's own writes, and the redirected calls if it has any
		std::vector<CodeWrite> writes;
		writes.reserve(plan.references.size() + plan.limits.size() + alsoWrite.size());
		for (uint8_t* field : plan.references)
		{
			const intptr_t distance = newArray - (field + sizeof(int32_t));
			if (distance < INT32_MIN || distance > INT32_MAX)
			{
				VirtualFree(newArray, 0, MEM_RELEASE);
				RelocationFailed(spec, "the new " + name + " is out of reach of the game code at " + DescribeAddress(field) + ".");
				return false;
			}
			writes.push_back({ field, BytesOf(static_cast<int32_t>(distance)), name + " reference" });
		}
		for (const FoundLimit& limit : plan.limits)
		{
			const int64_t encoded = static_cast<int64_t>(plan.size) * limit.mul + limit.add;
			writes.push_back({ limit.value, BytesOf(static_cast<uint32_t>(encoded)), name + " limit" });
		}
		writes.insert(writes.end(), alsoWrite.begin(), alsoWrite.end());
		const bool written = redirects.empty() ? WriteCode(writes, spec.tag) : RedirectCalls(redirects, writes, spec.tag);
		if (!written)
		{
			VirtualFree(newArray, 0, MEM_RELEASE);	// nothing points at it: nothing was written
			RelocationFailed(spec, "the game code could not be changed (details in TweaksV.log).");
			return false;
		}
		WriteLog(spec.tag, Capitalised("the " + name) + " moved and its limit raised " + change + " entries.");
		return true;
	}
}
