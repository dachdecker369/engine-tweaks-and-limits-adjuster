// TweaksV: carried over from PackfileLimitAdjusterEnhanced (Logging.h) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once
#include <string>		// TweaksV
// TweaksV: <string_view>, <fstream> and ClearLog.h removed - unused (TweaksV.log is cleared once, by DllMain)

// TweaksV: "std::string_view LOGFILE = ..." removed - a variable defined in a header breaks the
// link once two .cpp files include it, and it was never used

namespace PackfileLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{
void Logging(std::string msg);
// TweaksV: "void ClearLog();" removed - the one shared ClearLog() is declared in ClearLog.h
} // namespace PackfileLimitAdjusterEnhanced
