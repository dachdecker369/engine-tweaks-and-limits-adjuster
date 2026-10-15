// TweaksV: carried over from RageOpenV (src/utils/log.cpp) by Chiheb-Bacha, based on ClosedIV by
// martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "main.h"
#include <iomanip>
#include <sstream>
#include <string>
#include <cstring>					// TweaksV: strcmp
#include "TweaksVLog.h"	// TweaksV: TweaksV.log

namespace RageOpenV // TweaksV: one namespace per original plugin
{

// TweaksV: logger::init() removed - it deleted RageOpenV.log; the shared ClearLog() clears
// TweaksV.log once per start (plan 5.7)

void logger::vwrite(const char* type, const char* msg, va_list args)
{
    // TweaksV: the switch is checked before anything is formatted, so a type that is off costs
    // one comparison (B-ROV-11/12); "error" lines are always written (plan 5.7)
    if (strcmp(type, "error") != 0 && !config::get_log(type))
        return;

    // TweaksV: was a 256-byte buffer, printf on every line and RageOpenV.log opened per line
    // (B-ROV-11). The line now goes to TweaksV.log, which adds the time and mirrors it to the
    // console when CONSOLE=1; nothing may throw back into the game.
    try
    {
        va_list measure;
        va_copy(measure, args);
        const int length = vsnprintf(nullptr, 0, msg, measure);
        va_end(measure);
        if (length < 0)
            return;

        std::string buffer(static_cast<size_t>(length), '\0');
        vsnprintf(buffer.data(), buffer.size() + 1, msg, args);

        TweaksV::WriteLog("RageOpenV", std::string("[") + type + "]" + buffer);
    }
    catch (...)
    {
    }
}

// This function will log things that happen rarely
void logger::write(const char* type, const char* msg, ...)
{
    va_list args;
    va_start(args, msg);
    vwrite(type, msg, args);
    va_end(args);
}

// TweaksV: lines inside hooks on hot paths follow the same switches at runtime - the separate
// _LOG build ("ReleaseWithLogs") is gone, and a switch that is off costs one comparison (plan 5.7)
void logger::log(const char* type, const char* msg, ...)
{
    va_list args;
    va_start(args, msg);
    vwrite(type, msg, args);
    va_end(args);
}

} // namespace RageOpenV
