// TweaksV: carried over from RageOpenV (src/utils/config.h) by Chiheb-Bacha, based on ClosedIV by
// martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#pragma once
#include <cstring>					// TweaksV: strcmp
#include "TweaksVIni.h"	// TweaksV: TweaksV.ini - was #include "inicpp.h" (D9, B-ROV-07)

namespace RageOpenV // TweaksV: one namespace per original plugin
{

class config
{
public:
	// TweaksV: get_ini() removed - the inicpp object. TweaksV.ini is read with the Win32 INI API,
	// which does not throw on ";" comment lines the way inicpp did (D9, B-ROV-07).

	static void load()
	{
		// TweaksV: TweaksV's DllMain has already created TweaksV.ini, so nothing is written here
		// (was: load RageOpenV.ini, or write one with the defaults). The log switches are read
		// once now, so later log calls never touch the INI (B-ROV-11).
		get_log("info");
	}

	// TweaksV: save() removed - TweaksV writes TweaksV.ini

	template<typename T>
	static T get_config(const char* key)
	{
		// TweaksV: [config] keys live in [TWEAKSV] now; the only one, "console", is CONSOLE
		// (Appendix D - Win32 INI key names ignore case)
		return static_cast<T>(TweaksV::GetIniBool("TWEAKSV", key, false));
	}

	static bool get_log(const char* key)
	{
		// TweaksV: [log] info/mods/device are LOG_INFO/LOG_MODS/LOG_DEVICE in [RAGEOPENV_SETTINGS]
		// (Appendix D), read once and kept; "rpf" was never read and is gone
		static const bool info = TweaksV::GetIniBool("RAGEOPENV_SETTINGS", "LOG_INFO", false);
		static const bool mods = TweaksV::GetIniBool("RAGEOPENV_SETTINGS", "LOG_MODS", false);
		static const bool device = TweaksV::GetIniBool("RAGEOPENV_SETTINGS", "LOG_DEVICE", false);
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
