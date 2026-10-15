// TweaksV: carried over from ModkitLimitAdjusterEnhanced (Logging.h) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once
#include <string_view>
#include <fstream>
#include <string>		// TweaksV
#include "ClearLog.h"	// TweaksV: ClearLog() is shared now - its declaration moved there

namespace ModkitLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{
void Logging(std::string msg);
// TweaksV: "void ClearLog();" removed - see ClearLog.h above (plan 4.1)
} // namespace ModkitLimitAdjusterEnhanced
