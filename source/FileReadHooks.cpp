// TweaksV: carried over from RageOpenV (src/hooks/FileReadHooks.cpp) by Chiheb-Bacha, based on
// ClosedIV by martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "main.h"
#include <unordered_map>
#include <shared_mutex>
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVIni.h"		// TweaksV: MODS_FOLDER, DRY_RUN, DIAGNOSTIC_MODE
#include "TweaksVLog.h"		// TweaksV
#include "TweaksVPatch.h"	// TweaksV: DescribeAddress, IsFunctionStart, StartsWithJump
#include "TweaksVSummary.h"	// TweaksV: start-up summary

namespace RageOpenV // TweaksV: one namespace per original plugin
{

using OpenBulkFn = HANDLE(*)(void*, const char*, __int64*);
using GetFileTimeFn = FILETIME(*)(void*, const char*);
using GetFileSizeFn = uint64_t(*)(void*, const char*);
using GetAttrsFn = uint64_t(*)(void*, const char*);

inline OpenBulkFn g_origOpenBulk1 = nullptr, g_origOpenBulk2 = nullptr;
inline GetFileTimeFn g_origGetFileTime1 = nullptr, g_origGetFileTime2 = nullptr;
inline GetFileSizeFn g_origGetFileSize1 = nullptr, g_origGetFileSize2 = nullptr;
inline GetAttrsFn g_origGetAttribs1 = nullptr, g_origGetAttribs2 = nullptr;

static const uint8_t EXISTS = 1;
static const uint8_t NOT_EXISTS = 2;
static std::unordered_map<std::string, uint8_t> g_modsCache;
static std::shared_mutex g_modsCacheMutex;

static bool TryModsPath(const char* path, std::string& outModsPath)
{
    // TweaksV: nothing may throw back into the game - on any error the file is not redirected
    try
    {
    outModsPath = std::string("mods/") + path;

    // Used a hashmap for lookups so that we don't call GetFileAttributeA every time
    {
        std::shared_lock lock(g_modsCacheMutex);
        auto it = g_modsCache.find(outModsPath);
        if (it != g_modsCache.end())
            return it->second == EXISTS;
    }

    bool exists = GetFileAttributesA(outModsPath.c_str()) != INVALID_FILE_ATTRIBUTES;
    {
        std::unique_lock lock(g_modsCacheMutex);
        g_modsCache[outModsPath] = exists ? EXISTS : NOT_EXISTS;
    }
    return exists;
    }
    catch (...)
    {
        return false;
    }
}

static HANDLE OpenBulkImpl(void* device, const char* path, __int64* a3, OpenBulkFn orig)
{
    logger::log("info", "[%s] Called with path /%s", __FUNCTION__, path);

    std::string modsPath;
    if (path && TryModsPath(path, modsPath))
    {
        logger::log("mods", "[%s] Redirecting to /%s", __FUNCTION__, modsPath.c_str());
        HANDLE result = orig(device, modsPath.c_str(), a3);
        if (result && result != INVALID_HANDLE_VALUE)
            return result;
    }
    return orig(device, path, a3);
}

static FILETIME GetFileTimeImpl(void* device, const char* path, GetFileTimeFn orig)
{
    logger::log("info", "[%s] Called with path /%s", __FUNCTION__, path);

    std::string modsPath;
    if (path && TryModsPath(path, modsPath))
    {
        logger::log("mods", "[%s] Redirecting to /%s", __FUNCTION__, modsPath.c_str());
        return orig(device, modsPath.c_str());
    }
    return orig(device, path);
}

static uint64_t GetFileSizeImpl(void* device, const char* path, GetFileSizeFn orig)
{
    logger::log("info", "[%s] Called with path /%s", __FUNCTION__, path);

    std::string modsPath;
    if (path && TryModsPath(path, modsPath))
    {
        logger::log("mods", "[%s] Redirecting to /%s", __FUNCTION__, modsPath.c_str());
        return orig(device, modsPath.c_str());
    }
    return orig(device, path);
}

static uint64_t GetAttribsImpl(void* device, const char* path, GetAttrsFn orig)
{
    logger::log("info", "[%s] Called with path /%s", __FUNCTION__, path);

    std::string modsPath;
    if (path && TryModsPath(path, modsPath))
    {
        logger::log("mods", "[%s] Redirecting to /%s", __FUNCTION__, modsPath.c_str());
        return orig(device, modsPath.c_str());
    }
    return orig(device, path);
}

// Vtable 1 (non-DS)
HANDLE OpenBulkHook1(void* d, const char* p, __int64* a3) {
    return OpenBulkImpl(d, p, a3, g_origOpenBulk1);
}

FILETIME GetFileTimeHook1(void* d, const char* p) {
    return GetFileTimeImpl(d, p, g_origGetFileTime1);
}

uint64_t GetFileSizeHook1(void* d, const char* p) {
    return GetFileSizeImpl(d, p, g_origGetFileSize1);
}

uint64_t GetAttribsHook1(void* d, const char* p) {
    return GetAttribsImpl(d, p, g_origGetAttribs1);
}

// Vtable 2 (DS)
HANDLE OpenBulkHook2(void* d, const char* p, __int64* a3) {
    return OpenBulkImpl(d, p, a3, g_origOpenBulk2);
}

FILETIME GetFileTimeHook2(void* d, const char* p) {
    return GetFileTimeImpl(d, p, g_origGetFileTime2);
}

uint64_t GetFileSizeHook2(void* d, const char* p) {
    return GetFileSizeImpl(d, p, g_origGetFileSize2);
}

uint64_t GetAttribsHook2(void* d, const char* p) {
    return GetAttribsImpl(d, p, g_origGetAttribs2);
}

static memory::InitFuncs FileReadHooks([] {
    // TweaksV: the MODS_FOLDER switch; only the GTA V Enhanced patterns are kept (F-ROV-6)
    if (!TweaksV::GetIniBool("RAGEOPENV_SETTINGS", "MODS_FOLDER", true)) {
        TweaksV::WriteLog("RageOpenV", "MODS_FOLDER=0 - files in the mods folder are not used.");
        return;
    }

    // TweaksV: pass 1 - all eight functions are found and checked before anything is hooked, so
    // they are hooked together or not at all (B-ROV-01, plan 5.9 R1)
    memory openBulk1 = memory::scan("41 56 56 57 53 48 81 ec ? ? ? ? 4c 89 c7 66 c7 ? ? ? ? ? 4c 8d 74 24");
    memory getFileTime1 = memory::scan("66 c7 01 ? ? 29 d9 d1 e9 66 89 4c 24 ? 31 ff 4c 8d 44 ? ? 48 89 d9 31 d2 ff 15 ? ? ? ? 85 c0").add(-0x10d);
    memory getFileSize1 = memory::scan("66 c7 01 ? ? 29 d9 d1 e9 66 89 4c 24 ? 31 ff 4c 8d 44 24 ? 48 89 d9 31 d2 ff 15 ? ? ? ? 8b 4c 24").add(-0x10d);
    memory getAttribs1 = memory::scan("41 56 56 57 53 48 81 ec ? ? ? ? 66 c7 ? ? ? ? ? 4c 8d 74 24");
    memory openBulk2 = memory::scan("41 57 41 56 56 57 53 48 81 ec 60 02 00 00 48 89 d3 48 89 cf");
    memory getFileTime2 = memory::scan("66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 ? ? ? ? 85 c0 74 14 8b 44 24 64 8b 4c 24 68 48 c1 e0 20 48 09 c1 48 89 4c 24 38").add(-0x15d);
    memory getFileSize2 = memory::scan("66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 ? ? ? ? 85 c0 74 0a 48 8b 44 24 5c 48 89 44 24 38").add(-0x15d);
    memory getAttribs2 = memory::scan("56 57 53 48 81 ec 60 02 00 00 48 89 d7 48 89 ce c7 44 24 3c ff ff ff ff 48 89 d1 ba 3a 00 00 00");

    struct Target { memory* mem; const char* name; bool computed; }; // computed = pattern inside the function
    const Target targets[] = {
        { &openBulk1, "OpenBulk 1", false }, { &getFileTime1, "GetFileTime 1", true },
        { &getFileSize1, "GetFileSize 1", true }, { &getAttribs1, "GetAttributes 1", false },
        { &openBulk2, "OpenBulk 2", false }, { &getFileTime2, "GetFileTime 2", true },
        { &getFileSize2, "GetFileSize 2", true }, { &getAttribs2, "GetAttributes 2", false } };

    std::string problem;
    for (const Target& target : targets)
    {
        void* address = (void*)target.mem->address;
        std::string detail;
        if (!address)
            problem = std::string(target.name) + " was not found in the game code";
        else if (!TweaksV::IsInsideGameImage(address))
            problem = std::string(target.name) + " is not inside the game (" + TweaksV::DescribeAddress(address) + ")";
        // TweaksV: a start computed back from a pattern must really be where a function starts (plan 5.9 R9)
        else if (target.computed && !TweaksV::IsFunctionStart(address, detail))
            problem = std::string(target.name) + " at " + TweaksV::DescribeAddress(address) + " is not a function start (" + detail + ")";
        if (!problem.empty())
            break;
        // TweaksV: a function that already starts with a jump has probably been hooked by another
        // mod - said in the log, and hooked on top, which chains to it (plan 4.4)
        if (TweaksV::StartsWithJump(address))
            TweaksV::WriteLog("RageOpenV", std::string(target.name) + " at " + TweaksV::DescribeAddress(address) +
                " already starts with a jump - another mod may have hooked it; RageOpenV's hook goes on top.");
        if (TweaksV::IsDiagnosticMode())
            TweaksV::WriteLog("RageOpenV", "DIAGNOSTIC: " + std::string(target.name) + " at " + TweaksV::DescribeAddress(address) +
                (target.computed ? " - " + detail : std::string()));
    }
    if (!problem.empty()) {
        TweaksV::ReportFailure("RageOpenV", "Files in the mods folder are not used this session: " + problem + ".");
        return;
    }

    if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN stops after pass 1
        TweaksV::WriteLog("RageOpenV", "DRY_RUN: would hook the 8 file functions for the mods folder.");
        return;
    }

    // TweaksV: pass 2 - the original hooks, queued and then switched on together (B-ROV-09); if any
    // of them fails, the ones already made are removed again
    const bool hooked =
        openBulk1.hook(OpenBulkHook1, &g_origOpenBulk1) &&
        getFileTime1.hook(GetFileTimeHook1, &g_origGetFileTime1) &&
        getFileSize1.hook(GetFileSizeHook1, &g_origGetFileSize1) &&
        getAttribs1.hook(GetAttribsHook1, &g_origGetAttribs1) &&
        openBulk2.hook(OpenBulkHook2, &g_origOpenBulk2) &&
        getFileTime2.hook(GetFileTimeHook2, &g_origGetFileTime2) &&
        getFileSize2.hook(GetFileSizeHook2, &g_origGetFileSize2) &&
        getAttribs2.hook(GetAttribsHook2, &g_origGetAttribs2) &&
        memory::apply_hooks();
    if (!hooked) {
        for (const Target& target : targets)
            target.mem->unhook();
        TweaksV::ReportFailure("RageOpenV", "Files in the mods folder are not used this session: the file functions "
            "could not be hooked (details in TweaksV.log).");
        return;
    }
    TweaksV::WriteLog("RageOpenV", "Files in the mods folder are used (8 file functions hooked).");
});

} // namespace RageOpenV
