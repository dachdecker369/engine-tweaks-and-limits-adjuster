// TweaksV: carried over from PackfileLimitAdjusterEnhanced (Logging.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "PackfileLimitAdjusterEnhanced_Logging.h"				// TweaksV: was #include "pch.h"
#include "TweaksVLog.h"	// TweaksV

namespace PackfileLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{

void Logging(std::string msg) {
	// TweaksV: was std::ofstream on "PackfileLimitAdjusterEnhanced.log"; the line now goes to TweaksV.log
	TweaksV::WriteLog("PackfileLimitAdjusterEnhanced", msg);
}

// TweaksV: ClearLog() removed - the one shared ClearLog() clears TweaksV.log once per
// start; clearing it here would erase the other plugins' lines

} // namespace PackfileLimitAdjusterEnhanced
