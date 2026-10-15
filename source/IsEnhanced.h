// TweaksV: IsEnhanced(), carried over unchanged from RageOpenV (src/main.h:19-31) by
// Chiheb-Bacha - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV: moved into its own shared header. TweaksV's DllMain
// calls it once; TweaksV does nothing unless the game is GTA5_Enhanced.exe.
#pragma once

#include <Windows.h>
#include <cstring> // TweaksV: strrchr / _stricmp (RageOpenV's main.h got them through its other includes)

inline bool IsEnhanced() {
    static const bool isEnhanced = []() -> bool
        {
            char path[MAX_PATH];
            GetModuleFileNameA(GetModuleHandleA(nullptr), path, MAX_PATH);

            const char* filename = strrchr(path, '\\');
            filename = filename ? filename + 1 : path;

            return (_stricmp(filename, "GTA5_Enhanced.exe") == 0);
        }();
    return isEnhanced;
}
