// TweaksV: carried over from PackfileLimitAdjusterEnhanced (Logging.h) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once
#include <string_view>
#include <fstream>
#include <string>		// TweaksV
#include "ClearLog.h"	// TweaksV: ClearLog() is shared now - its declaration moved there

// TweaksV: "std::string_view LOGFILE = ..." removed - a variable defined in a header breaks the
// link once two .cpp files include it, and it was never used (B-PACK-08)

namespace PackfileLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{
void Logging(std::string msg);
// TweaksV: "void ClearLog();" removed - see ClearLog.h above (plan 4.1)
} // namespace PackfileLimitAdjusterEnhanced
