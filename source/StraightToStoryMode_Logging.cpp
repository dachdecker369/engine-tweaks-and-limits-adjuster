// TweaksV: carried over from StraightToStoryMode (Logging.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/StraightToStoryMode (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "StraightToStoryMode_Logging.h"				// TweaksV: was #include "pch.h"
#include "TweaksVLog.h"	// TweaksV

namespace StraightToStoryMode // TweaksV: one namespace per original plugin
{

void Logging(std::string msg) {
	// TweaksV: was std::ofstream on "StraightToStoryMode.log"; the line now goes to TweaksV.log
	TweaksV::WriteLog("StraightToStoryMode", msg);
}

// TweaksV: ClearLog() removed - the one shared ClearLog() clears TweaksV.log once per
// start (plan 5.7); clearing it here would erase the other plugins' lines

} // namespace StraightToStoryMode
