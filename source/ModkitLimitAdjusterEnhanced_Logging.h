// TweaksV: carried over from ModkitLimitAdjusterEnhanced (Logging.h) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once
#include <string>		// TweaksV
// TweaksV: <string_view>, <fstream> and ClearLog.h removed - unused (TweaksV.log is cleared once, by DllMain)

namespace ModkitLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{
void Logging(std::string msg);
// TweaksV: "void ClearLog();" removed - the one shared ClearLog() is declared in ClearLog.h
} // namespace ModkitLimitAdjusterEnhanced
