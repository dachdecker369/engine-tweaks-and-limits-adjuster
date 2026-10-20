// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - every setting of TweaksV.ini, defined once in kSettings: its section, key, kind, default,
// limits and the comment above it in the file. The default TweaksV.ini, the keys added back when they
// are missing, the log header and every reader come from this table, so a new setting is one new entry
// in it (and a named reference below, for the code that reads it).
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
		const char* name;		// "HEAP_SETTINGS"
		const char* feature;	// "HeapAdjusterEnhanced" - for the log and the start-up summary
	};

	inline constexpr SettingsSection kTweaksVSection{ "TWEAKSV", "TweaksV" };
	inline constexpr SettingsSection kHeapSection{ "HEAP_SETTINGS", "HeapAdjusterEnhanced" };
	inline constexpr SettingsSection kModkitSection{ "MODKIT_SETTINGS", "ModkitLimitAdjusterEnhanced" };
	inline constexpr SettingsSection kPackfileSection{ "PACKFILE_SETTINGS", "PackfileLimitAdjusterEnhanced" };
	inline constexpr SettingsSection kBoxStreamerSection{ "BOXSTREAMER_SETTINGS", "BoxStreamer" };
	inline constexpr SettingsSection kDecalDefsSection{ "DECALDEFS_SETTINGS", "DecalDefs" };
	inline constexpr SettingsSection kRageOpenVSection{ "RAGEOPENV_SETTINGS", "RageOpenV" };
	inline constexpr SettingsSection kStoryModeSection{ "STRAIGHTTOSTORYMODE_SETTINGS", "StraightToStoryMode" };
	inline constexpr SettingsSection kDirectStorageSection{ "DIRECTSTORAGE_SETTINGS", "DirectStorage" };

	// The sections in file order; every one but the first is a part of TweaksV with an ENABLED key.
	inline constexpr const SettingsSection* kSections[] = { &kTweaksVSection, &kHeapSection, &kModkitSection,
		&kPackfileSection, &kBoxStreamerSection, &kDecalDefsSection, &kRageOpenVSection, &kStoryModeSection, &kDirectStorageSection };

	// The layout of TweaksV.ini this build writes and reads (INI_VERSION). It goes up only when a key
	// changes its meaning or moves - new keys are simply added to an older file.
	inline constexpr int kIniLayoutVersion = 1;

	enum class SettingKind { Switch, Number, Text };

	struct Setting
	{
		const SettingsSection* section;
		const char* key;
		SettingKind kind;
		int defaultValue;			// a Switch: 1 = on, 0 = off
		int minValue;				// a Number: the lowest value TweaksV accepts
		int maxValue;				// a Number: the highest
		const char* comment;		// the comment line above the key in the default TweaksV.ini, or nullptr
		bool limitsInComment;		// a Number: ". Default <default>. Maximum <max>" is added to the comment
		bool testBuildOnly;			// not in TweaksV.ini; only test builds read it
	};

	constexpr Setting Switch(const SettingsSection& section, const char* key, bool on, const char* comment = nullptr)
	{
		return { &section, key, SettingKind::Switch, on ? 1 : 0, 0, 1, comment, false, false };
	}

	constexpr Setting Number(const SettingsSection& section, const char* key, int value, int minValue, int maxValue,
		const char* comment, bool limitsInComment = true)
	{
		return { &section, key, SettingKind::Number, value, minValue, maxValue, comment, limitsInComment, false };
	}

	constexpr Setting TestText(const SettingsSection& section, const char* key)
	{
		return { &section, key, SettingKind::Text, 0, 0, 0, nullptr, false, true };
	}

	// Every setting, in file order. The texts are the ones the default TweaksV.ini shows.
	inline constexpr Setting kSettings[] = {
		Switch(kTweaksVSection, "ENABLED", true),
		Switch(kTweaksVSection, "LOG_TO_FILE", true, "Writes TweaksV.log (in the game folder, or in %LOCALAPPDATA%\\TweaksV\\)"),
		Switch(kTweaksVSection, "CONSOLE", false, "Open a console window that shows the log while the game runs"),
		Switch(kTweaksVSection, "SHOW_ERROR_POPUPS", true, "Show a message box if a feature could not be applied or stops working"),
		Switch(kTweaksVSection, "CHECK_FOR_CONFLICTS", true, "Warn about old stand-alone plugins that TweaksV replaces"),
		Switch(kTweaksVSection, "DIAGNOSTIC_MODE", false, "TESTING ONLY: logs every memory pattern TweaksV looks for"),
		Switch(kTweaksVSection, "DRY_RUN", false, "TESTING ONLY: logs everything and turns off game patching"),
		Number(kTweaksVSection, "INI_VERSION", kIniLayoutVersion, 1, 1000000,
			"The layout of this file - TweaksV keeps it up to date, do not change it", false),
		TestText(kTweaksVSection, "TEST_BREAK_PATTERN"),
		TestText(kTweaksVSection, "TEST_LOG_LIMIT_KB"),
		TestText(kTweaksVSection, "TEST_DUMP_CODE"),

		Switch(kHeapSection, "ENABLED", true),
		Number(kHeapSection, "HEAP_SIZE", 2000, 1, 2047, "Heap size in MB"),

		Switch(kModkitSection, "ENABLED", true),
		Number(kModkitSection, "MODKITARRAY_SIZE", 65536, 1, 65536, "Number of modkit IDs"),

		Switch(kPackfileSection, "ENABLED", true),
		Number(kPackfileSection, "PACKFILEARRAY_SIZE", 13000, 1, 65535, "Number of archives"),

		Switch(kBoxStreamerSection, "ENABLED", true),
		Number(kBoxStreamerSection, "TRAVERSAL_LIST_SIZE", 10000, 1000, 100000, "Map boxes one search of the box streamer can return (the game's own list holds 1000 - too few with heavy map mods, whose LODs then go missing)"),
		Switch(kBoxStreamerSection, "LOG_SEARCHES", false, "TESTING ONLY: logs every minute how full the traversal list gets, and whether the game's own list would have overflowed (changes nothing)"),

		Switch(kDecalDefsSection, "ENABLED", true),
		Number(kDecalDefsSection, "DECALDEFS_SIZE", 2048, 260, 16384, "Decal definitions (the kinds of bullet holes, blood, scorch marks and so on from decals.dat) the game can hold - its own array holds 260, and add-on content silently loses the rest"),
		Switch(kDecalDefsSection, "LOG_DECALDEFS", false, "TESTING ONLY: logs how many decal definitions are registered, and whether the game's own array of 260 would have dropped some (changes nothing)"),

		Switch(kRageOpenVSection, "ENABLED", true),
		Switch(kRageOpenVSection, "MODS_FOLDER", true, "Load replacement \"mods\" folder in the GTA V Enhanced directory"),
		Switch(kRageOpenVSection, "OPEN_RPF", true, "Allow unencrypted (\"OPEN\") RPF archives"),
		Switch(kRageOpenVSection, "SKIP_RPF_CACHE_CHECK", true, "Skip the game's RPF cache check (needed for modified RPFs; 0 only for testing)"),
		Switch(kRageOpenVSection, "NEWMODS", false, "Loose files from the \"newmods\" folder (experimental upstream feature)"),
		Switch(kRageOpenVSection, "LOG_INFO", false, "RageOpenV log switches"),
		Switch(kRageOpenVSection, "LOG_MODS", false),
		Switch(kRageOpenVSection, "LOG_DEVICE", false),

		Switch(kStoryModeSection, "ENABLED", true),
		Switch(kStoryModeSection, "SKIP_SPLASH_SCREEN", true, "Skips the Splash Screen"),
		Switch(kStoryModeSection, "SKIP_LEGAL_SCREEN", true, "Skips the Legal Screen"),
		Switch(kStoryModeSection, "SKIP_LANDING_PAGE", true, "Skips straight into Story Mode"),

		Switch(kDirectStorageSection, "ENABLED", true),
		Switch(kDirectStorageSection, "UNLOCK_FILES", true, "DirectStorage may open only the .rpf and .cache files in the game folder, so its other files stay free while the game runs"),
		Switch(kDirectStorageSection, "LOG_CALLS", false, "TESTING ONLY: logs the game's DirectStorage calls (changes nothing)"),
		Switch(kDirectStorageSection, "LOG_FILE_OPENS", false, "TESTING ONLY: logs every file in the game folder the game opens, and how (changes nothing)"),
	};

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
	inline constexpr const Setting& kLogToFile = FindSetting(kTweaksVSection, "LOG_TO_FILE");
	inline constexpr const Setting& kConsole = FindSetting(kTweaksVSection, "CONSOLE");
	inline constexpr const Setting& kShowErrorPopups = FindSetting(kTweaksVSection, "SHOW_ERROR_POPUPS");
	inline constexpr const Setting& kCheckForConflicts = FindSetting(kTweaksVSection, "CHECK_FOR_CONFLICTS");
	inline constexpr const Setting& kDiagnosticMode = FindSetting(kTweaksVSection, "DIAGNOSTIC_MODE");
	inline constexpr const Setting& kDryRun = FindSetting(kTweaksVSection, "DRY_RUN");
	inline constexpr const Setting& kIniVersion = FindSetting(kTweaksVSection, "INI_VERSION");
	inline constexpr const Setting& kTestBreakPattern = FindSetting(kTweaksVSection, "TEST_BREAK_PATTERN");
	inline constexpr const Setting& kTestLogLimitKb = FindSetting(kTweaksVSection, "TEST_LOG_LIMIT_KB");
	inline constexpr const Setting& kTestDumpCode = FindSetting(kTweaksVSection, "TEST_DUMP_CODE");
	inline constexpr const Setting& kHeapSize = FindSetting(kHeapSection, "HEAP_SIZE");
	inline constexpr const Setting& kModkitArraySize = FindSetting(kModkitSection, "MODKITARRAY_SIZE");
	inline constexpr const Setting& kPackfileArraySize = FindSetting(kPackfileSection, "PACKFILEARRAY_SIZE");
	inline constexpr const Setting& kTraversalListSize = FindSetting(kBoxStreamerSection, "TRAVERSAL_LIST_SIZE");
	inline constexpr const Setting& kLogSearches = FindSetting(kBoxStreamerSection, "LOG_SEARCHES");
	inline constexpr const Setting& kDecalDefsSize = FindSetting(kDecalDefsSection, "DECALDEFS_SIZE");
	inline constexpr const Setting& kLogDecalDefs = FindSetting(kDecalDefsSection, "LOG_DECALDEFS");
	inline constexpr const Setting& kModsFolder = FindSetting(kRageOpenVSection, "MODS_FOLDER");
	inline constexpr const Setting& kOpenRpf = FindSetting(kRageOpenVSection, "OPEN_RPF");
	inline constexpr const Setting& kSkipRpfCacheCheck = FindSetting(kRageOpenVSection, "SKIP_RPF_CACHE_CHECK");
	inline constexpr const Setting& kNewMods = FindSetting(kRageOpenVSection, "NEWMODS");
	inline constexpr const Setting& kLogInfo = FindSetting(kRageOpenVSection, "LOG_INFO");
	inline constexpr const Setting& kLogMods = FindSetting(kRageOpenVSection, "LOG_MODS");
	inline constexpr const Setting& kLogDevice = FindSetting(kRageOpenVSection, "LOG_DEVICE");
	inline constexpr const Setting& kSkipSplashScreen = FindSetting(kStoryModeSection, "SKIP_SPLASH_SCREEN");
	inline constexpr const Setting& kSkipLegalScreen = FindSetting(kStoryModeSection, "SKIP_LEGAL_SCREEN");
	inline constexpr const Setting& kSkipLandingPage = FindSetting(kStoryModeSection, "SKIP_LANDING_PAGE");
	inline constexpr const Setting& kUnlockFiles = FindSetting(kDirectStorageSection, "UNLOCK_FILES");
	inline constexpr const Setting& kLogCalls = FindSetting(kDirectStorageSection, "LOG_CALLS");
	inline constexpr const Setting& kLogFileOpens = FindSetting(kDirectStorageSection, "LOG_FILE_OPENS");

	// Reads every setting once, after CreateOrUpdateIni() has chosen TweaksV.ini. A value that is not
	// valid, or outside its limits, is reported for the start-up summary; its default (or the nearest
	// limit) is used. Every reader below returns these values - none of them reads the file again.
	void LoadSettings();

	bool IsOn(const Setting& setting);					// a Switch
	int GetNumber(const Setting& setting);				// a Number, within its limits
	const std::string& GetText(const Setting& setting);	// a Text

	// The section's ENABLED switch, unless the part was switched off for this session
	// (DisableFeatureForSession, e.g. by CheckForConflicts).
	bool IsFeatureOn(const SettingsSection& section);
	void DisableFeatureForSession(const SettingsSection& section);

	inline bool IsDryRun() { return IsOn(kDryRun); }
	inline bool IsDiagnosticMode() { return IsOn(kDiagnosticMode); }

	// The default TweaksV.ini, with CRLF line endings, and a setting's default as it is written there.
	std::string DefaultIniText();
	std::string DefaultValueText(const Setting& setting);

	// The settings lines of the log header: which parts are on, then one line per part with its values.
	std::vector<std::string> DescribeSettings();
}
