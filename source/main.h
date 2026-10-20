// TweaksV: carried over from RageOpenV (src/main.h) by Chiheb-Bacha, based on ClosedIV by
// martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once

#include <Windows.h>
// TweaksV: <algorithm>, <iostream> and <thread> removed - nothing uses them
#include <string>
#include <vector>

#include "log.h"
#include "memory.h"
#include "rpf.h"
#include "config.h"

#include "fiDevice.h"
#include "fiPackfile.h"

// TweaksV: IsEnhanced() moved to IsEnhanced.h - TweaksV's DllMain checks it once for
// everything, so RageOpenV no longer calls it

namespace RageOpenV // TweaksV: one namespace per original plugin
{
// TweaksV: no longer the DLL's entry point - TweaksV's DllMain starts it from the feature table
// (TweaksVFeatures.cpp). Returns TRUE when the GetSystemTimeAsFileTime hook is installed; that hook
// then shows the start-up summary.
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved);
} // namespace RageOpenV
