//
// Copyright (C) 2015 crosire & kagikn & contributors
// Copyright (C) 2025 Chiheb-Bacha
// License: https://github.com/scripthookvdotnet/scripthookvdotnet#license
// Source: https://github.com/Chiheb-Bacha/ScriptHookVDotNetEnhanced/blob/main/source/core/MemScanner.cs
//
// TweaksV: carried over from ModkitLimitAdjusterEnhanced, PackfileLimitAdjusterEnhanced
// and StraightToStoryMode by Chiheb-Bacha (identical copies; this is the one shared copy),
// https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// This is an ALTERED version - modified for TweaksV. Every change is marked
// "TweaksV:". A miss now logs and returns nullptr instead of calling FatalErrorExit().
//
// TweaksV: the search is TweaksV's own; the pattern parsing is the original's. The original's
// Boyer-Moore-Horspool search went through the whole image almost byte by byte (a wildcard near the
// end of a pattern limits its skips), about 0.6 s at every game start. This one searches only the
// game's executable sections, and only the parts that can be read, so a page the game protects is
// skipped instead of crashing the game. It compares a pattern's two rarest bytes at 16 positions at a
// time (SSE2) and checks the whole pattern only where both match - about ten times faster.
//

// TweaksV: removed #include "pch.h" (no precompiled header)
#include "MemScanner.h"
#include <windows.h>
#include <psapi.h>
#include <emmintrin.h>	// TweaksV: SSE2
#include <intrin.h>		// TweaksV: _BitScanForward
#include <algorithm>
#include <array>
#include <cstdio>		// TweaksV: snprintf for the DIAGNOSTIC_MODE output
#include <cstring>		// TweaksV: _stricmp (test builds)
#include <sstream>
#include <string>
#include <vector>
#include "TweaksVLog.h" // TweaksV: was "Logging.h" (each plugin had its own log file)
#include "TweaksVSettings.h" // TweaksV: IsDiagnosticMode(), TEST_BREAK_PATTERN

namespace MemScanner
{
    namespace // TweaksV: everything below except Find is internal
    {
        struct Range
        {
            uint8_t* begin;
            uint8_t* end;
        };

        // TweaksV: GTA5_Enhanced.exe in memory and its executable sections, read once. If the
        // headers show no executable section, the whole image is searched, as the original did.
        struct Image
        {
            uint8_t* base = nullptr;
            uint64_t size = 0;
            std::vector<Range> code;
        };

        const Image& GameImage()
        {
            static const Image image = []
            {
                Image result;
                const HMODULE module = GetModuleHandleW(nullptr);
                MODULEINFO info{};
                if (!module || !GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
                    return result;
                const auto base = reinterpret_cast<uint8_t*>(info.lpBaseOfDll);
                result.base = base;
                result.size = info.SizeOfImage;

                // the headers are read only where they lie inside the image
                const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
                const bool dosOk = result.size >= sizeof(IMAGE_DOS_HEADER) && dos->e_magic == IMAGE_DOS_SIGNATURE &&
                    dos->e_lfanew > 0 && static_cast<uint64_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS) <= result.size;
                const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + (dosOk ? dos->e_lfanew : 0));
                const IMAGE_SECTION_HEADER* section = dosOk ? IMAGE_FIRST_SECTION(nt) : nullptr;
                if (dosOk && nt->Signature == IMAGE_NT_SIGNATURE &&
                    reinterpret_cast<const uint8_t*>(section + nt->FileHeader.NumberOfSections) <= base + result.size)
                {
                    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
                    {
                        if (!(section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE))
                            continue;
                        const uint64_t start = section[i].VirtualAddress;
                        const uint64_t length = section[i].Misc.VirtualSize ? section[i].Misc.VirtualSize : section[i].SizeOfRawData;
                        if (start >= result.size || length == 0)
                            continue;
                        result.code.push_back({ base + start, base + std::min<uint64_t>(start + length, result.size) });
                    }
                }
                if (result.code.empty())
                    result.code.push_back({ base, base + result.size });
                std::sort(result.code.begin(), result.code.end(), [](const Range& a, const Range& b) { return a.begin < b.begin; });
                return result;
            }();
            return image;
        }

        // TweaksV: the readable parts of the code, neighbouring readable regions joined. Read for every
        // search: TweaksV itself changes protections between two searches.
        std::vector<Range> ReadableCode()
        {
            constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
            std::vector<Range> runs;
            for (const Range& range : GameImage().code)
            {
                for (uint8_t* at = range.begin; at < range.end;)
                {
                    MEMORY_BASIC_INFORMATION info{};
                    if (VirtualQuery(at, &info, sizeof(info)) != sizeof(info))
                        break;
                    uint8_t* next = static_cast<uint8_t*>(info.BaseAddress) + info.RegionSize;
                    if (next <= at)
                        break;
                    if (next > range.end)
                        next = range.end;
                    if (info.State == MEM_COMMIT && (info.Protect & kReadable) && !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    {
                        if (!runs.empty() && runs.back().end == at)
                            runs.back().end = next;
                        else
                            runs.push_back({ at, next });
                    }
                    at = next;
                }
            }
            return runs;
        }

        // TweaksV: how often each byte occurs in the first 256 KB of the game's code. The search compares
        // a pattern's two rarest bytes first; this only changes how fast it is, never what it finds.
        const std::array<uint32_t, 256>& ByteCounts()
        {
            static const std::array<uint32_t, 256> counts = []
            {
                std::array<uint32_t, 256> result{};
                const std::vector<Range> code = ReadableCode();
                if (!code.empty())
                {
                    const size_t n = std::min<size_t>(static_cast<size_t>(code[0].end - code[0].begin), 256 * 1024);
                    for (size_t i = 0; i < n; i++)
                        result[code[0].begin[i]]++;
                }
                return result;
            }();
            return counts;
        }

        // TweaksV: returns false on a malformed token instead of letting std::stoi throw
        // inside DllMain (the caller treats that as "pattern not found")
        bool ParsePattern(const std::string& pat, std::vector<uint8_t>& outPattern, std::string& outMask)
        {
            outPattern.clear();
            outMask.clear();

            std::istringstream ss(pat);
            std::string token;

            while (ss >> token)
            {
                if (token == "?" || token == "??")
                {
                    outPattern.push_back(0);
                    outMask.push_back('?');
                }
                else
                {
                    // TweaksV: catch parse errors and reject values above 0xFF
                    int value = -1;
                    try
                    {
                        size_t used = 0;
                        value = std::stoi(token, &used, 16);
                        if (used != token.size())
                            value = -1;
                    }
                    catch (...)
                    {
                        value = -1;
                    }
                    if (value < 0 || value > 0xFF)
                        return false;

                    uint8_t val = (uint8_t)value;
                    outPattern.push_back(val);
                    outMask.push_back('x');
                }
            }
            return true; // TweaksV
        }

        // TweaksV: a parsed pattern and the positions of its two rarest fixed bytes
        struct Pattern
        {
            std::vector<uint8_t> bytes;
            std::vector<uint8_t> fixed;	// 1: the byte must match; 0: a wildcard
            size_t first = 0;
            size_t second = 0;
        };

        // False for a malformed pattern and for one that is only wildcards (it would match everywhere).
        bool Compile(const std::string& text, Pattern& pattern)
        {
            std::string mask;
            if (!ParsePattern(text, pattern.bytes, mask))
                return false;
            std::vector<size_t> fixedAt;
            pattern.fixed.assign(pattern.bytes.size(), 0);
            for (size_t i = 0; i < pattern.bytes.size(); i++)
            {
                if (mask[i] != 'x')
                    continue;
                pattern.fixed[i] = 1;
                fixedAt.push_back(i);
            }
            if (fixedAt.empty())
                return false;
            const auto& counts = ByteCounts();
            std::stable_sort(fixedAt.begin(), fixedAt.end(),
                [&](size_t a, size_t b) { return counts[pattern.bytes[a]] < counts[pattern.bytes[b]]; });
            pattern.first = fixedAt[0];
            pattern.second = fixedAt.size() > 1 ? fixedAt[1] : fixedAt[0];
            return true;
        }

        bool Matches(const uint8_t* at, const Pattern& pattern)
        {
            for (size_t i = 0; i < pattern.bytes.size(); i++)
                if (pattern.fixed[i] && at[i] != pattern.bytes[i])
                    return false;
            return true;
        }

        // The first match that starts in [begin, end) and ends inside it. Every load stays below end.
        uint8_t* FindIn(const Pattern& pattern, uint8_t* begin, uint8_t* end)
        {
            const size_t length = pattern.bytes.size();
            const size_t available = static_cast<size_t>(end - begin);
            if (available < length)
                return nullptr;
            const size_t lastStart = available - length;
            const __m128i a = _mm_set1_epi8(static_cast<char>(pattern.bytes[pattern.first]));
            const __m128i b = _mm_set1_epi8(static_cast<char>(pattern.bytes[pattern.second]));
            size_t at = 0;
            for (; at + 15 <= lastStart; at += 16)
            {
                const __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(begin + at + pattern.first));
                const __m128i y = _mm_loadu_si128(reinterpret_cast<const __m128i*>(begin + at + pattern.second));
                unsigned mask = static_cast<unsigned>(_mm_movemask_epi8(_mm_and_si128(_mm_cmpeq_epi8(x, a), _mm_cmpeq_epi8(y, b))));
                while (mask)
                {
                    unsigned long bit = 0;
                    _BitScanForward(&bit, mask);
                    if (Matches(begin + at + bit, pattern))
                        return begin + at + bit;
                    mask &= mask - 1;
                }
            }
            for (; at <= lastStart; at++)
                if (Matches(begin + at, pattern))
                    return begin + at;
            return nullptr;
        }

        // The first match at or after from (nullptr: from the start) in the readable code.
        uint8_t* FindInCode(const Pattern& pattern, const std::vector<Range>& code, uint8_t* from)
        {
            for (const Range& range : code)
            {
                if (from && range.end <= from)
                    continue;
                uint8_t* const begin = from && from > range.begin ? from : range.begin;
                if (uint8_t* found = FindIn(pattern, begin, range.end))
                    return found;
            }
            return nullptr;
        }

        // TweaksV: DIAGNOSTIC_MODE output - RVA, number of matches and the original bytes
        // (a pattern that matches more than once should be made unique). A pattern that matches more
        // than once also gets one line per match with the bytes around it, so that a longer pattern
        // can be chosen that matches only the place it is meant for.
        void LogPatternDiagnostics(const TweaksV::GamePattern& gamePattern, const Pattern& pattern, const std::vector<Range>& code,
            uint8_t* address)
        {
            const int maxCount = 100;
            std::vector<uint8_t*> found; // every match, for the lines per match below
            for (uint8_t* cur = address; cur && (int)found.size() < maxCount; cur = FindInCode(pattern, code, cur + 1))
                found.push_back(cur);
            const int matches = (int)found.size();

            const Image& image = GameImage();
            auto rva = [&image](const uint8_t* at) // 0 outside the game
            {
                return at >= image.base && at < image.base + image.size ? (unsigned long long)(at - image.base) : 0ULL;
            };
            char text[96];
            snprintf(text, sizeof(text), " -> RVA 0x%llX, %d%s match%s, bytes:",
                rva(address), matches, matches >= maxCount ? "+" : "", matches == 1 ? "" : "es");

            std::string line = std::string("Pattern ") + gamePattern.label + " found: " + gamePattern.text + text;
            for (size_t i = 0; i < pattern.bytes.size() && i < 32; i++)
            {
                char byteText[4];
                snprintf(byteText, sizeof(byteText), " %02X", address[i]);
                line += byteText;
            }
            TweaksV::WriteLog("MemScanner", line);

            if (matches < 2)
                return;
            // the first 10 matches: 16 bytes before each and 48 from its start, "|" marks the start; never
            // outside the readable part of the code the match is in
            for (int m = 0; m < matches && m < 10; m++)
            {
                const uint8_t* match = found[m];
                const Range* in = nullptr;
                for (const Range& range : code)
                    if (match >= range.begin && match < range.end)
                        in = &range;
                if (!in)
                    continue;
                const uint8_t* from = match - in->begin >= 16 ? match - 16 : in->begin;
                const uint8_t* to = in->end - match >= 48 ? match + 48 : in->end;
                snprintf(text, sizeof(text), "  match %d of %d%s at RVA 0x%llX:", m + 1, matches,
                    matches >= maxCount ? "+" : "", rva(match));
                line = text;
                for (const uint8_t* at = from; at < to; at++)
                {
                    char byteText[6];
                    snprintf(byteText, sizeof(byteText), at == match ? " | %02X" : " %02X", *at);
                    line += byteText;
                }
                TweaksV::WriteLog("MemScanner", line);
            }
        }

#ifdef TWEAKSV_TEST_BUILD
        // TweaksV test builds only (-p:TweaksVTestBuild=true): TweaksV.ini [TWEAKSV] TEST_BREAK_PATTERN
        // names one pattern by its label in PATTERNS.md (e.g. S-E2), which is then treated as not
        // found - to see that its part is skipped cleanly and the rest still works.
        bool IsBrokenForTest(const TweaksV::GamePattern& pattern)
        {
            const std::string& broken = TweaksV::GetText(TweaksV::kTestBreakPattern);
            return !broken.empty() && _stricmp(broken.c_str(), pattern.label) == 0;
        }
#endif
    }

    uint8_t* Find(const TweaksV::GamePattern& gamePattern)
    {
        const std::string label = gamePattern.label;
#ifdef TWEAKSV_TEST_BUILD
        if (IsBrokenForTest(gamePattern)) {
            TweaksV::WriteLog("MemScanner", "TEST BUILD: pattern " + label + " is treated as not found (TEST_BREAK_PATTERN).");
            return nullptr;
        }
#endif
        Pattern pattern;
        if (!Compile(gamePattern.text, pattern)) { // TweaksV: not for a pattern of kPatterns, which are checked when TweaksV is compiled
            TweaksV::WriteLog("MemScanner", "Pattern " + label + " is not valid (treated as not found): " + gamePattern.text);
            return nullptr;
        }

        const std::vector<Range> code = ReadableCode();
        uint8_t* address = FindInCode(pattern, code, nullptr);
        if (!address) {
            // TweaksV: was Logging(...) + FatalErrorExit(); the calling plugin now decides
            TweaksV::WriteLog("MemScanner", "Pattern " + label + " not found: " + gamePattern.text);
            return nullptr;
        }

        if (TweaksV::IsDiagnosticMode()) // TweaksV
            LogPatternDiagnostics(gamePattern, pattern, code, address);

        return address;
    }
}
