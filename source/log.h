// TweaksV: carried over from RageOpenV (src/utils/log.h) by Chiheb-Bacha, based on ClosedIV by
// martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once

#include <cstdarg>	// TweaksV: va_list

namespace RageOpenV // TweaksV: one namespace per original plugin
{
namespace logger
{
	// TweaksV: "void init();" removed - the shared ClearLog() clears TweaksV.log once per start
	// TweaksV: the type "error" is always written; "info", "mods" and "device" follow the LOG_INFO,
	// LOG_MODS and LOG_DEVICE switches in TweaksV.ini (plan 5.7)
	void write(const char* type, const char* msg, ...);
	void log(const char* type, const char* msg, ...);
	void vwrite(const char* type, const char* msg, va_list args);
}
} // namespace RageOpenV
