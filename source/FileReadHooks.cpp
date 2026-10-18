// TweaksV: carried over from RageOpenV (src/hooks/FileReadHooks.cpp) by Chiheb-Bacha, based on
// ClosedIV by martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "main.h"
#include <cstring>				// TweaksV: strnlen, memcpy
#include <string_view>			// TweaksV
#include <unordered_set>		// TweaksV: the mods folder index (was an unordered_map cache)
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVSettings.h"	// TweaksV: MODS_FOLDER, DRY_RUN, DIAGNOSTIC_MODE
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

// TweaksV: the mods folder is read once, before the hooks go on: every file and folder in it, as a
// path relative to it in lower case with '/' between the names. The hooks only look paths up in
// this set, which never changes afterwards - no file system call, no memory allocation and no lock
// on the game's file calls (upstream built a string, took a lock and asked the file system for every
// new path). Bounded: at most kMaxModsEntries entries, kMaxModsDepth folders deep, paths of at most
// kMaxModsPath characters. A path with ".." or a drive is never in the set, so nothing outside the
// mods folder is ever used.
static constexpr size_t kMaxModsEntries = 200000;
static constexpr size_t kMaxModsDepth = 32;
static constexpr size_t kMaxModsPath = 259;

struct ModsPathHash
{
    using is_transparent = void;
    size_t operator()(std::string_view path) const noexcept { return std::hash<std::string_view>{}(path); }
};
static std::unordered_set<std::string, ModsPathHash, std::equal_to<>> g_modsIndex;

// TweaksV: a game path the way the index holds it: lower case (ASCII), '/' between the names, no
// leading or doubled '/'. False if it is empty or longer than kMaxModsPath.
static bool NormalizeModsPath(const char* path, char (&out)[kMaxModsPath + 1], size_t& length)
{
    length = 0;
    for (const char* p = path; *p; ++p)
    {
        char c = *p == '\\' ? '/' : *p;
        if (c == '/' && (length == 0 || out[length - 1] == '/'))
            continue;
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (length == kMaxModsPath)
            return false;
        out[length++] = c;
    }
    out[length] = '\0';
    return length > 0;
}

// TweaksV: reads the mods folder (in the current directory, as upstream) into g_modsIndex. A name
// the ANSI code page cannot hold is skipped - the game, which asks with ANSI paths, could not ask
// for it either. Returns the number of entries; complete is false if a limit was reached.
static size_t BuildModsIndex(bool& complete)
{
    struct Folder { std::wstring path; std::string relative; size_t depth; };
    std::vector<Folder> todo{ { L"mods", std::string(), 1 } };
    complete = true;
    while (!todo.empty() && complete)
    {
        const Folder folder = std::move(todo.back());
        todo.pop_back();
        WIN32_FIND_DATAW found{};
        const HANDLE find = FindFirstFileExW((folder.path + L"\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch,
            nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (find == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            if (wcscmp(found.cFileName, L".") == 0 || wcscmp(found.cFileName, L"..") == 0)
                continue;
            char name[kMaxModsPath + 1];
            BOOL lossy = FALSE;
            const int size = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, found.cFileName, -1, name, sizeof(name), nullptr, &lossy);
            if (size <= 1 || lossy)
                continue;
            std::string relative = folder.relative.empty() ? std::string(name) : folder.relative + "/" + name;
            if (relative.size() > kMaxModsPath)
                continue;
            for (char& c : relative)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            if (g_modsIndex.size() >= kMaxModsEntries)
            {
                complete = false;
                break;
            }
            const bool isFolder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (isFolder && folder.depth >= kMaxModsDepth)
                complete = false;
            else if (isFolder)
                todo.push_back({ folder.path + L"\\" + found.cFileName, relative, folder.depth + 1 });
            g_modsIndex.insert(std::move(relative));
        } while (FindNextFileW(find, &found));
        FindClose(find);
    }
    return g_modsIndex.size();
}

// TweaksV: the path to pass on for a file or folder the mods folder has, into out: "mods/" and the
// game's own spelling of the path, as upstream passed it on. False if the mods folder does not have it.
static bool TryModsPath(const char* path, char (&out)[kMaxModsPath + 6])
{
    char normalized[kMaxModsPath + 1];
    size_t length = 0;
    if (!path || !NormalizeModsPath(path, normalized, length) || !g_modsIndex.contains(std::string_view(normalized, length)))
        return false;
    const size_t original = strnlen(path, kMaxModsPath + 1);
    if (original > kMaxModsPath)
        return false;
    memcpy(out, "mods/", 5);
    memcpy(out + 5, path, original);
    out[5 + original] = '\0';
    return true;
}

static HANDLE OpenBulkImpl(void* device, const char* path, __int64* a3, OpenBulkFn orig)
{
    logger::log("info", "[%s] Called with path /%s", __FUNCTION__, path);

    char modsPath[kMaxModsPath + 6]; // TweaksV: on the stack - was a std::string per call
    if (TryModsPath(path, modsPath))
    {
        logger::log("mods", "[%s] Redirecting to /%s", __FUNCTION__, modsPath);
        HANDLE result = orig(device, modsPath, a3);
        if (result && result != INVALID_HANDLE_VALUE)
            return result;
    }
    return orig(device, path, a3);
}

static FILETIME GetFileTimeImpl(void* device, const char* path, GetFileTimeFn orig)
{
    logger::log("info", "[%s] Called with path /%s", __FUNCTION__, path);

    char modsPath[kMaxModsPath + 6]; // TweaksV: on the stack - was a std::string per call
    if (TryModsPath(path, modsPath))
    {
        logger::log("mods", "[%s] Redirecting to /%s", __FUNCTION__, modsPath);
        return orig(device, modsPath);
    }
    return orig(device, path);
}

static uint64_t GetFileSizeImpl(void* device, const char* path, GetFileSizeFn orig)
{
    logger::log("info", "[%s] Called with path /%s", __FUNCTION__, path);

    char modsPath[kMaxModsPath + 6]; // TweaksV: on the stack - was a std::string per call
    if (TryModsPath(path, modsPath))
    {
        logger::log("mods", "[%s] Redirecting to /%s", __FUNCTION__, modsPath);
        return orig(device, modsPath);
    }
    return orig(device, path);
}

static uint64_t GetAttribsImpl(void* device, const char* path, GetAttrsFn orig)
{
    logger::log("info", "[%s] Called with path /%s", __FUNCTION__, path);

    char modsPath[kMaxModsPath + 6]; // TweaksV: on the stack - was a std::string per call
    if (TryModsPath(path, modsPath))
    {
        logger::log("mods", "[%s] Redirecting to /%s", __FUNCTION__, modsPath);
        return orig(device, modsPath);
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
    // TweaksV: the MODS_FOLDER switch; only the GTA V Enhanced patterns are kept
    if (!TweaksV::IsOn(TweaksV::kModsFolder)) {
        TweaksV::WriteLog("RageOpenV", "MODS_FOLDER=0 - files in the mods folder are not used.");
        return;
    }

    // TweaksV: without a mods folder there is nothing to redirect, so the game's file functions are not
    // hooked at all (the folder is looked for in the current directory, as upstream). DIAGNOSTIC_MODE
    // still checks their patterns.
    const DWORD modsAttributes = GetFileAttributesW(L"mods");
    const bool haveFolder = modsAttributes != INVALID_FILE_ATTRIBUTES && (modsAttributes & FILE_ATTRIBUTE_DIRECTORY);
    if (!haveFolder && !TweaksV::IsDiagnosticMode()) {
        TweaksV::WriteLog("RageOpenV", "No mods folder in the current directory - the file functions are not hooked.");
        return;
    }

    // TweaksV: pass 1 - all eight functions are found and checked before anything is hooked, so
    // they are hooked together or not at all
    memory openBulk1 = memory::scan(TweaksV::Pattern("R-E-F1"));
    memory getFileTime1 = memory::scan(TweaksV::Pattern("R-E-F2")).add(-0x10d);
    memory getFileSize1 = memory::scan(TweaksV::Pattern("R-E-F3")).add(-0x10d);
    memory getAttribs1 = memory::scan(TweaksV::Pattern("R-E-F4"));
    memory openBulk2 = memory::scan(TweaksV::Pattern("R-E-F5"));
    memory getFileTime2 = memory::scan(TweaksV::Pattern("R-E-F6")).add(-0x15d);
    memory getFileSize2 = memory::scan(TweaksV::Pattern("R-E-F7")).add(-0x15d);
    memory getAttribs2 = memory::scan(TweaksV::Pattern("R-E-F8"));

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
        // TweaksV: a start computed back from a pattern must really be where a function starts
        else if (target.computed && !TweaksV::IsFunctionStart(address, detail))
            problem = std::string(target.name) + " at " + TweaksV::DescribeAddress(address) + " is not a function start (" + detail + ")";
        if (!problem.empty())
            break;
        // TweaksV: a function that already starts with a jump has probably been hooked by another
        // mod - said in the log, and hooked on top, which chains to it
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
    if (!haveFolder) { // TweaksV: only DIAGNOSTIC_MODE gets here without one
        TweaksV::WriteLog("RageOpenV", "No mods folder in the current directory - the patterns were checked "
            "(DIAGNOSTIC_MODE), the file functions are not hooked.");
        return;
    }

    if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN stops after pass 1
        TweaksV::WriteLog("RageOpenV", "DRY_RUN: would hook the 8 file functions for the mods folder.");
        return;
    }

    // TweaksV: the mods folder is read now, before the hooks go on (see g_modsIndex)
    bool complete = true;
    const size_t entries = BuildModsIndex(complete);
    TweaksV::WriteLog("RageOpenV", "The mods folder has " + std::to_string(entries) + " files and folders.");
    if (!complete)
        TweaksV::ReportFailure("RageOpenV", "Only part of the mods folder is used this session: it has more than " +
            std::to_string(kMaxModsEntries) + " files and folders, or folders more than " + std::to_string(kMaxModsDepth) +
            " levels deep.");

    // TweaksV: pass 2 - the original hooks, queued and then switched on together; if any
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
