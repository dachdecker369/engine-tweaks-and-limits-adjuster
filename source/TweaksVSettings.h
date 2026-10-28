// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - every setting of TweaksV.ini, defined once in kSettings: its section, key, kind, default
// and limits. The default TweaksV.ini, the keys added back when they are missing, the log header and
// every reader come from this table, so a new setting is one new entry in it (and a named reference
// below, for the code that reads it). Each key of the default file carries a short comment from the
// table after its value, and a comment is never part of a value; README.md explains every key in full.
#pragma once

#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace TweaksV
{
	// A section of TweaksV.ini and the part of TweaksV it belongs to.
	struct SettingsSection
	{
		const char* name;		// "HEAP_ADJUSTER"
		const char* feature;	// "HeapAdjuster" - for the log and the start-up summary
	};

	inline constexpr SettingsSection kTweaksVSection{ "TWEAKSV", "TweaksV" };
	inline constexpr SettingsSection kHeapSection{ "HEAP_ADJUSTER", "HeapAdjuster" };
	inline constexpr SettingsSection kModkitSection{ "MODKIT_LIMIT", "ModkitLimit" };
	inline constexpr SettingsSection kPackfileSection{ "PACKFILE_LIMIT", "PackfileLimit" };
	inline constexpr SettingsSection kBoxStreamerSection{ "BOXSTREAMER", "BoxStreamer" };
	inline constexpr SettingsSection kDecalsSection{ "DECALS", "Decals" };
	inline constexpr SettingsSection kRageOpenVSection{ "RAGEOPENV", "RageOpenV" };
	inline constexpr SettingsSection kStoryModeSection{ "STRAIGHTTOSTORYMODE", "StraightToStoryMode" };
	inline constexpr SettingsSection kDirectStorageSection{ "DIRECTSTORAGE", "DirectStorage" };
	inline constexpr SettingsSection kBlipsSection{ "BLIPS", "Blips" };

	// The sections in file order; every one but the first is a part of TweaksV with an ENABLED key.
	inline constexpr const SettingsSection* kSections[] = { &kTweaksVSection, &kHeapSection, &kModkitSection,
		&kPackfileSection, &kBoxStreamerSection, &kDecalsSection, &kRageOpenVSection, &kStoryModeSection, &kDirectStorageSection,
		&kBlipsSection };

	// The layout of TweaksV.ini this build writes and reads: VERSION, the last key of [TWEAKSV]. A file
	// without VERSION has the layout of TweaksV 1.x and is replaced (TweaksVIni.h); a file with a higher
	// number comes from a newer TweaksV and is reported. It goes up with every new key in the file (the
	// static_assert on IniKeyCount below), and when a key changes its meaning or moves. An older file is
	// carried forward: from kIniFirstLayout on, it gets the keys it lacks and then the current number
	// written back (TweaksVIni.h). Layout 2 came with TweaksV 2.0.0, layout 3 with the Blips part.
	inline constexpr int kIniLayoutVersion = 3;
	inline constexpr int kIniFirstLayout = 2;		// the first layout with VERSION, the oldest one carried forward

	enum class SettingKind { Switch, Number, Text };

	struct Setting
	{
		const SettingsSection* section;
		const char* key;
		SettingKind kind;
		int defaultValue;			// a Switch: 1 = on, 0 = off
		int minValue;				// a Number: the lowest value TweaksV accepts
		int maxValue;				// a Number: the highest
		bool testBuildOnly;			// not in TweaksV.ini; only test builds read it
		const char* comment;		// after the value in the default file, "" for none; README.md says more
	};

	constexpr Setting Switch(const SettingsSection& section, const char* key, bool on, const char* comment = "")
	{
		return { &section, key, SettingKind::Switch, on ? 1 : 0, 0, 1, false, comment };
	}

	constexpr Setting Number(const SettingsSection& section, const char* key, int value, int minValue, int maxValue,
		const char* comment = "")
	{
		return { &section, key, SettingKind::Number, value, minValue, maxValue, false, comment };
	}

	constexpr Setting TestText(const SettingsSection& section, const char* key)
	{
		return { &section, key, SettingKind::Text, 0, 0, 0, true, "" };
	}

	// Every setting, in file order - the order of the default TweaksV.ini.
	inline constexpr Setting kSettings[] = {
		Switch(kTweaksVSection, "ENABLED", true),
		Switch(kTweaksVSection, "LOG", true, "Writes to TweaksV.log"),
		Switch(kTweaksVSection, "DIAGNOSTIC_MODE", false, "TESTING ONLY: logs patterns, sites and measurements"),
		Switch(kTweaksVSection, "DRY_RUN", false, "TESTING ONLY: turns off game patching"),
		Switch(kTweaksVSection, "CONSOLE", false, "Console window showing .log while the game is running"),
		Switch(kTweaksVSection, "ERROR_POPUP", true, "Shows error message popups"),
		Switch(kTweaksVSection, "CONFLICT_CHECK", true, "Turns off TweaksV parts while other plugins that do the same thing are installed"),
		Number(kTweaksVSection, "VERSION", kIniLayoutVersion, kIniFirstLayout, 1000000, "This file's version - leave this line alone!"),
		TestText(kTweaksVSection, "TEST_BREAK_PATTERN"),
		TestText(kTweaksVSection, "TEST_LOG_LIMIT_KB"),
		TestText(kTweaksVSection, "TEST_DUMP_CODE"),
		TestText(kTweaksVSection, "TEST_DUMP_SCRIPT"),

		Switch(kHeapSection, "ENABLED", true),
		Number(kHeapSection, "HEAP_SIZE", 2000, 1, 2047, "Heap size in MB - Maximum 2047"),

		Switch(kModkitSection, "ENABLED", true),
		Number(kModkitSection, "MODKITARRAY_SIZE", 65536, 1, 65536, "Number of modkit IDs - Maximum 65536"),

		Switch(kPackfileSection, "ENABLED", true),
		Number(kPackfileSection, "PACKFILEARRAY_SIZE", 13000, 1, 65535, "Number of archives - Maximum 65535"),

		Switch(kBoxStreamerSection, "ENABLED", true),
		Number(kBoxStreamerSection, "TRAVERSAL_LIST_SIZE", 10000, 1000, 100000, "Number of map boxes - Maximum 100000"),

		Switch(kDecalsSection, "ENABLED", true),
		Number(kDecalsSection, "DECALDEFS_SIZE", 2048, 260, 16384, "Number of decal definitions - Maximum 16384"),

		Switch(kRageOpenVSection, "ENABLED", true),
		Switch(kRageOpenVSection, "MODS_FOLDER", true, "Loads \"mods\" folder in game directory"),
		Switch(kRageOpenVSection, "OPEN_RPF", true, "Allows unencrypted (\"OPEN\") RPF archives"),
		Switch(kRageOpenVSection, "SKIP_RPF_CACHE_CHECK", true, "Skips game RPF cache check"),

		Switch(kStoryModeSection, "ENABLED", true),
		Switch(kStoryModeSection, "SKIP_SPLASH_SCREEN", true, "Skips Splash Screen"),
		Switch(kStoryModeSection, "SKIP_LEGAL_SCREEN", true, "Skips Legal Screen"),
		Switch(kStoryModeSection, "SKIP_LANDING_PAGE", true, "Skips Landing Page"),

		Switch(kDirectStorageSection, "ENABLED", true),
		Switch(kDirectStorageSection, "UNLOCK_FILES", true, "Keeps game directory files writable (not .rpf/.cache)"),

		Switch(kBlipsSection, "ENABLED", true),
		Number(kBlipsSection, "ACTIVE_BLIP_LIMIT", 1900, 150, 1997, "Active blips up to which the game adds its own - Maximum 1997"),
	};

	// The keys the default TweaksV.ini holds: layout 3 has 30.
	consteval size_t IniKeyCount()
	{
		size_t keys = 0;
		for (const Setting& setting : kSettings)
			if (!setting.testBuildOnly)
				keys++;
		return keys;
	}
	static_assert(IniKeyCount() == 30, "TweaksV: layout 3 of TweaksV.ini has 30 keys - a new key means a new layout");

	// The column at which a key's comment starts in the default file, the same for every key.
	inline constexpr size_t kIniCommentColumn = 28;

	// Every KEY=default that carries a comment ends short of the comment column, so that at least one
	// blank stands before the comment; a key too long for it does not build.
	consteval bool CommentsFitTheColumn()
	{
		for (const Setting& setting : kSettings)
		{
			if (setting.testBuildOnly || *setting.comment == '\0')
				continue;
			size_t length = std::string_view(setting.key).size() + 1;	// KEY=
			for (int value = setting.defaultValue; ; value /= 10)		// the default's digits; no default is negative
			{
				length++;
				if (value < 10)
					break;
			}
			if (length >= kIniCommentColumn)
				return false;
		}
		return true;
	}
	static_assert(CommentsFitTheColumn(), "TweaksV: a KEY=default of the default TweaksV.ini reaches its comment column");

	// The entry for a section and key - checked when TweaksV is compiled: a name that is not in
	// kSettings does not build.
	consteval const Setting& FindSetting(const SettingsSection& section, std::string_view key)
	{
		for (const Setting& setting : kSettings)
			if (setting.section == &section && key == setting.key)
				return setting;
		throw "TweaksV: this setting is not in kSettings";
	}

	inline constexpr const Setting& kEnabled = FindSetting(kTweaksVSection, "ENABLED");
	inline constexpr const Setting& kLog = FindSetting(kTweaksVSection, "LOG");
	inline constexpr const Setting& kDiagnosticMode = FindSetting(kTweaksVSection, "DIAGNOSTIC_MODE");
	inline constexpr const Setting& kDryRun = FindSetting(kTweaksVSection, "DRY_RUN");
	inline constexpr const Setting& kConsole = FindSetting(kTweaksVSection, "CONSOLE");
	inline constexpr const Setting& kErrorPopup = FindSetting(kTweaksVSection, "ERROR_POPUP");
	inline constexpr const Setting& kConflictCheck = FindSetting(kTweaksVSection, "CONFLICT_CHECK");
	inline constexpr const Setting& kIniVersion = FindSetting(kTweaksVSection, "VERSION");
	inline constexpr const Setting& kTestBreakPattern = FindSetting(kTweaksVSection, "TEST_BREAK_PATTERN");
	inline constexpr const Setting& kTestLogLimitKb = FindSetting(kTweaksVSection, "TEST_LOG_LIMIT_KB");
	inline constexpr const Setting& kTestDumpCode = FindSetting(kTweaksVSection, "TEST_DUMP_CODE");
	inline constexpr const Setting& kTestDumpScript = FindSetting(kTweaksVSection, "TEST_DUMP_SCRIPT");
	inline constexpr const Setting& kHeapSize = FindSetting(kHeapSection, "HEAP_SIZE");
	inline constexpr const Setting& kModkitArraySize = FindSetting(kModkitSection, "MODKITARRAY_SIZE");
	inline constexpr const Setting& kPackfileArraySize = FindSetting(kPackfileSection, "PACKFILEARRAY_SIZE");
	inline constexpr const Setting& kTraversalListSize = FindSetting(kBoxStreamerSection, "TRAVERSAL_LIST_SIZE");
	inline constexpr const Setting& kDecalDefsSize = FindSetting(kDecalsSection, "DECALDEFS_SIZE");
	inline constexpr const Setting& kModsFolder = FindSetting(kRageOpenVSection, "MODS_FOLDER");
	inline constexpr const Setting& kOpenRpf = FindSetting(kRageOpenVSection, "OPEN_RPF");
	inline constexpr const Setting& kSkipRpfCacheCheck = FindSetting(kRageOpenVSection, "SKIP_RPF_CACHE_CHECK");
	inline constexpr const Setting& kSkipSplashScreen = FindSetting(kStoryModeSection, "SKIP_SPLASH_SCREEN");
	inline constexpr const Setting& kSkipLegalScreen = FindSetting(kStoryModeSection, "SKIP_LEGAL_SCREEN");
	inline constexpr const Setting& kSkipLandingPage = FindSetting(kStoryModeSection, "SKIP_LANDING_PAGE");
	inline constexpr const Setting& kUnlockFiles = FindSetting(kDirectStorageSection, "UNLOCK_FILES");
	inline constexpr const Setting& kActiveBlipLimit = FindSetting(kBlipsSection, "ACTIVE_BLIP_LIMIT");

	// Reads every setting once, after CreateOrUpdateIni() has chosen TweaksV.ini. A value that is not
	// valid, or outside its limits, is reported for the start-up summary; its default (or the nearest
	// limit) is used. Every reader below returns these values - none of them reads the file again.
	void LoadSettings();

	bool IsOn(const Setting& setting);					// a Switch
	int GetNumber(const Setting& setting);				// a Number, within its limits
	const std::string& GetText(const Setting& setting);	// a Text

	// The section's ENABLED switch, unless the part was switched off for this session
	// (DisableFeatureForSession, e.g. by CheckForConflicts); IsEnabledInIni is the switch alone.
	bool IsFeatureOn(const SettingsSection& section);
	bool IsEnabledInIni(const SettingsSection& section);
	void DisableFeatureForSession(const SettingsSection& section);

	inline bool IsDryRun() { return IsOn(kDryRun); }

	// DIAGNOSTIC_MODE is the one testing switch: every part that is on logs every site it finds, the
	// value it holds and every write, and runs its measurement where it has one; the scanner logs its
	// line per pattern. A part that is off does nothing, so it logs nothing.
	inline bool IsDiagnosticMode() { return IsOn(kDiagnosticMode); }

	// The default TweaksV.ini, with CRLF line endings, and a setting's default as it is written there.
	std::string DefaultIniText();
	std::string DefaultValueText(const Setting& setting);

	// The settings lines of the log header: which parts are on, then one line per part with its values.
	std::vector<std::string> DescribeSettings();
}
