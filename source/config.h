// TweaksV: carried over from RageOpenV (src/utils/config.h) by Chiheb-Bacha, based on ClosedIV by
// martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once
#include <cstring>					// TweaksV: strcmp
#include "TweaksVSettings.h"	// TweaksV: TweaksV.ini's settings - was #include "inicpp.h"

namespace RageOpenV // TweaksV: one namespace per original plugin
{

class config
{
public:
	// TweaksV: get_ini() removed - the inicpp object. TweaksV.ini is read with the Win32 INI API,
	// which does not throw on ";" comment lines the way inicpp did.

	static void load()
	{
		// TweaksV: TweaksV's DllMain has already created TweaksV.ini, so nothing is written here
		// (was: load RageOpenV.ini, or write one with the defaults). The log switches are read
		// once now, so later log calls never touch the INI.
		get_log("info");
	}

	// TweaksV: save() removed - TweaksV writes TweaksV.ini

	template<typename T>
	static T get_config(const char* key)
	{
		// TweaksV: [config] keys live in [TWEAKSV] now; the only one, "console", is CONSOLE
		return static_cast<T>(strcmp(key, "console") == 0 && TweaksV::IsOn(TweaksV::kConsole));
	}

	static bool get_log(const char* key)
	{
		// TweaksV: [log] info and mods follow DIAGNOSTIC_MODE under [TWEAKSV], the one testing switch of
		// 2.0.0, read once and kept; "device" went with the newmods folder, "rpf" was never read
		static const bool diagnostic = TweaksV::IsDiagnosticMode();
		if (strcmp(key, "info") == 0 || strcmp(key, "mods") == 0)
			return diagnostic;
		return false;
	}
};

} // namespace RageOpenV
