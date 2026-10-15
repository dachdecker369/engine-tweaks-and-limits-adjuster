// TweaksV: carried over from ModkitLimitAdjusterEnhanced (Logging.cpp:10-16) by Chiheb-Bacha -
// https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "ClearLog.h"
#include "TweaksVLog.h" // TweaksV

void ClearLog()
{
	// TweaksV: was std::ofstream on "ModkitLimitAdjusterEnhanced.log" (a path relative to
	// the current directory). TweaksV.log has an absolute path and an open handle owned by
	// TweaksV::WriteLog, so it is cleared there.
	TweaksV::ClearLogFile();
}
