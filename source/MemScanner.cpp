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

// TweaksV: removed #include "pch.h" (no precompiled header)
#include "MemScanner.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <windows.h>
#include <psapi.h>
#include "TweaksVLog.h" // TweaksV: was "Logging.h" (each plugin had its own log file)
#include "TweaksVIni.h" // TweaksV: IsDiagnosticMode()
#include <algorithm>
#include <array>
#include <cstdio> // TweaksV: snprintf for the DIAGNOSTIC_MODE output

namespace MemScanner
{
    struct ModuleInfo
    {
        uint8_t* base = nullptr;
        uint64_t size = 0;

        ModuleInfo()
        {
            HMODULE mod = GetModuleHandleA(nullptr);
            MODULEINFO info{};
            GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info));
            base = reinterpret_cast<uint8_t*>(info.lpBaseOfDll);
            size = static_cast<uint64_t>(info.SizeOfImage);
        }
    };

    static ModuleInfo& GetModuleInfo()
    {
        static ModuleInfo cache;
        return cache;
    }

    static std::array<short, 256> CreateShiftTableForBmh(const std::vector<short>& pattern)
    {
        std::array<short, 256> skipTable;
        int lastIndex = (int)pattern.size() - 1;
        int lastWildcardIndex = -1;
        for (int i = (int)pattern.size() - 1; i >= 0; --i)
        {
            if (pattern[i] < 0)
            {
                lastWildcardIndex = i;
                break;
            }
        }

        int diff = lastIndex - std::max(lastWildcardIndex, 0); // TweaksV: std::max (NOMINMAX is defined)
        if (diff == 0) diff = 1;

        skipTable.fill((short)diff);

        for (int i = lastIndex - diff; i < lastIndex; i++)
        {
            short val = pattern[i];
            if (val >= 0)
                skipTable[val] = (short)(lastIndex - i);
        }

        return skipTable;
    }

    // TweaksV: returns false on a malformed token instead of letting std::stoi throw
    // inside DllMain (the caller treats that as "pattern not found")
    static bool ParsePattern(const std::string& pat, std::vector<uint8_t>& outPattern, std::string& outMask)
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

    uint8_t* FindPatternBmh(const std::vector<uint8_t>& pattern, const std::string& mask, uint8_t* startAddress, uint64_t size)
    {
        if (!startAddress || size == 0 || pattern.empty() || pattern.size() != mask.size())
            return nullptr;
        if (size < pattern.size()) // TweaksV: endAddr below would wrap around
            return nullptr;

        std::vector<short> patternArray(pattern.size());
        for (size_t i = 0; i < pattern.size(); i++)
        {
            patternArray[i] = (mask[i] != '?') ? (short)pattern[i] : (short)-1;
        }

        int lastIndex = (int)patternArray.size() - 1;
        auto skipTable = CreateShiftTableForBmh(patternArray);

        uint8_t* endAddr = startAddress + size - patternArray.size();

        for (uint8_t* cur = startAddress; cur <= endAddr;)
        {
            int i = lastIndex;
            while (i >= 0 && (patternArray[i] < 0 || cur[i] == patternArray[i]))
                --i;

            if (i < 0)
                return cur;

            int shift = skipTable[cur[lastIndex]];
            if (shift <= 0) shift = 1;
            cur += shift;
        }


        return nullptr;
    }

    // TweaksV: DIAGNOSTIC_MODE output - RVA, number of matches and the original bytes
    // (plan 5.9 R7: a pattern that matches more than once should be made unique). A pattern
    // that matches more than once also gets one line per match with the bytes around it, so
    // that a longer pattern can be chosen that matches only the place it is meant for (T2).
    static void LogPatternDiagnostics(const std::string& patternStr, const std::vector<uint8_t>& pattern, const std::string& mask, uint8_t* startAddress, uint64_t size, uint8_t* address)
    {
        if (!address) // only found patterns are described (code analysis C28182)
            return;
        const int maxCount = 100;
        std::vector<uint8_t*> found; // every match, for the lines per match below
        for (uint8_t* cur = address; cur && (int)found.size() < maxCount;)
        {
            found.push_back(cur);
            uint64_t used = (uint64_t)(cur + 1 - startAddress);
            if (used >= size)
                break;
            cur = FindPatternBmh(pattern, mask, cur + 1, size - used);
        }
        const int matches = (int)found.size();

        auto& module = GetModuleInfo();
        auto rva = [&module](const uint8_t* at) // 0 outside the game
        {
            return at >= module.base && at < module.base + module.size ? (unsigned long long)(at - module.base) : 0ULL;
        };
        char text[96];
        snprintf(text, sizeof(text), " -> RVA 0x%llX, %d%s match%s, bytes:",
            rva(address), matches, matches >= maxCount ? "+" : "", matches == 1 ? "" : "es");

        std::string line = "Pattern found: " + patternStr + text;
        for (size_t i = 0; i < pattern.size() && i < 32; i++)
        {
            char byteText[4];
            snprintf(byteText, sizeof(byteText), " %02X", address[i]);
            line += byteText;
        }
        TweaksV::WriteLog("MemScanner", line);

        if (matches < 2)
            return;
        // the first 10 matches: 16 bytes before each and 48 from its start, "|" marks the start
        const uint8_t* scanEnd = startAddress + size;
        for (int m = 0; m < matches && m < 10; m++)
        {
            const uint8_t* match = found[m];
            const uint8_t* from = match - startAddress >= 16 ? match - 16 : startAddress;
            const uint8_t* to = scanEnd - match >= 48 ? match + 48 : scanEnd;
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
    // TweaksV test builds only (-p:TweaksVTestBuild=true, plan 8.4 T-NEG-1): TweaksV.ini [TWEAKSV]
    // TEST_BREAK_PATTERN names one pattern, as written in PATTERNS.md, that is then treated as
    // not found - to see that its part is skipped cleanly and the rest still works.
    static std::string NormalizePattern(const std::string& text)
    {
        std::istringstream tokens(text);
        std::string out, token;
        while (tokens >> token)
        {
            if (token == "??")
                token = "?";
            for (auto& c : token)
                c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
            out += (out.empty() ? "" : " ") + token;
        }
        return out;
    }

    static bool IsBrokenForTest(const std::string& patternStr)
    {
        static const std::string broken = NormalizePattern(TweaksV::GetIniString("TWEAKSV", "TEST_BREAK_PATTERN", ""));
        return !broken.empty() && NormalizePattern(patternStr) == broken;
    }
#endif

    uint8_t* FindPatternBmh(const std::string& patternStr, std::string& maskStr, uint8_t* startAddress, uint64_t size)
    {
#ifdef TWEAKSV_TEST_BUILD
        if (IsBrokenForTest(patternStr)) {
            TweaksV::WriteLog("MemScanner", "TEST BUILD: treated as not found (TEST_BREAK_PATTERN): " + patternStr);
            return nullptr;
        }
#endif
        std::vector<uint8_t> pattern;
        std::string mask;
        if (!ParsePattern(patternStr, pattern, mask)) { // TweaksV: malformed pattern
            TweaksV::WriteLog("MemScanner", "Invalid pattern (treated as not found): " + patternStr);
            return nullptr;
        }
        maskStr = mask;

        auto address = FindPatternBmh(pattern, mask, startAddress, size);

        if (!address) {
            // TweaksV: was Logging(...) + FatalErrorExit(); the calling plugin now decides
            TweaksV::WriteLog("MemScanner", "Pattern not found: " + patternStr);
            return nullptr;
        }

        if (TweaksV::IsDiagnosticMode()) // TweaksV
            LogPatternDiagnostics(patternStr, pattern, mask, startAddress, size, address);

        return address;
    }

    uint8_t* FindPatternBmh(const std::string& patternStr, uint8_t* startAddress, uint64_t size)
    {
        std::string mask;
        return FindPatternBmh(patternStr, mask, startAddress, size);
    }

    uint8_t* FindPatternBmh(const std::string& patternStr)
    {
        auto& module = GetModuleInfo();
        return FindPatternBmh(patternStr, module.base, module.size);
    }

    uint8_t* FindPatternBmh(const std::string& patternStr, uint8_t* startAddress)
    {
        auto& module = GetModuleInfo();
        if (startAddress < module.base) return nullptr;
        if (startAddress >= module.base + module.size) return nullptr; // TweaksV: size below would wrap around
        uint64_t size = module.size - (uint64_t)(startAddress - module.base);
        return FindPatternBmh(patternStr, startAddress, size);
    }
}
