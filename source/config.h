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
		// TweaksV: [log] info/mods/device are LOG_INFO/LOG_MODS/LOG_DEVICE in [RAGEOPENV_SETTINGS],
		// read once and kept; "rpf" was never read and is gone
		static const bool info = TweaksV::IsOn(TweaksV::kLogInfo);
		static const bool mods = TweaksV::IsOn(TweaksV::kLogMods);
		static const bool device = TweaksV::IsOn(TweaksV::kLogDevice);
		if (strcmp(key, "info") == 0)
			return info;
		if (strcmp(key, "mods") == 0)
			return mods;
		if (strcmp(key, "device") == 0)
			return device;
		return false;
	}
};

} // namespace RageOpenV
