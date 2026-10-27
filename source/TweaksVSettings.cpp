// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the settings of TweaksV.ini, read once from the table in TweaksVSettings.h.

#include "TweaksVSettings.h"

#include <windows.h>

#include <array>
#include <cerrno>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

#include "TweaksVIni.h"
#include "TweaksVLog.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr size_t kCount = std::size(kSettings);

		struct Value
		{
			int number = 0;			// a Switch (1 or 0) or a Number
			std::string text;		// a Text
		};

		std::array<Value, kCount> g_values{};
		bool g_loaded = false;
		SRWLOCK g_lock = SRWLOCK_INIT;
		std::vector<const SettingsSection*> g_disabled;	// parts switched off for this session

		// Every setting is an entry of kSettings (FindSetting), so its place there is its index.
		size_t IndexOf(const Setting& setting)
		{
			return static_cast<size_t>(&setting - kSettings);
		}

		const Value& ValueOf(const Setting& setting)
		{
			if (!g_loaded)	// only if something reads a setting before DllMain has loaded them
				LoadSettings();
			return g_values[IndexOf(setting)];
		}

		std::string Where(const Setting& setting)
		{
			return std::string("TweaksV.ini: [") + setting.section->name + "] " + setting.key;
		}

		// 1/0, true/false, yes/no and on/off, in any case
		bool ParseSwitch(std::wstring value, int& out)
		{
			for (auto& c : value)
				c = static_cast<wchar_t>(std::towlower(c));
			if (value == L"1" || value == L"true" || value == L"yes" || value == L"on")
				out = 1;
			else if (value == L"0" || value == L"false" || value == L"no" || value == L"off")
				out = 0;
			else
				return false;
			return true;
		}

		void Load(const Setting& setting, Value& value)
		{
			value.number = setting.defaultValue;
			value.text.clear();
#ifndef TWEAKSV_TEST_BUILD
			if (setting.testBuildOnly)
				return;
#endif
			const std::wstring raw = ReadIniValue(setting.section->name, setting.key);
			if (raw.empty())
				return;

			switch (setting.kind)
			{
			case SettingKind::Switch:
				if (!ParseSwitch(raw, value.number))
					ReportFailure("TweaksV", Where(setting) + "=" + ToUtf8(raw) + " is not a valid on/off value - using " +
						(setting.defaultValue ? "1" : "0") + ".");
				break;

			case SettingKind::Number:
			{
				wchar_t* end = nullptr;
				errno = 0;
				const long long parsed = wcstoll(raw.c_str(), &end, 10);
				if (end == raw.c_str() || *end != L'\0' || errno == ERANGE)
				{
					ReportFailure("TweaksV", Where(setting) + "=" + ToUtf8(raw) + " is not a number - using " +
						std::to_string(setting.defaultValue) + ".");
					break;
				}
				const long long clamped = parsed < setting.minValue ? setting.minValue : parsed > setting.maxValue ? setting.maxValue : parsed;
				if (clamped != parsed)
					ReportFailure("TweaksV", Where(setting) + "=" + std::to_string(parsed) + " is outside " +
						std::to_string(setting.minValue) + ".." + std::to_string(setting.maxValue) + " - using " + std::to_string(clamped) + ".");
				value.number = static_cast<int>(clamped);
				break;
			}

			case SettingKind::Text:
				value.text = ToUtf8(raw);
				break;
			}
		}

		std::string ValueText(const Setting& setting)
		{
			return setting.kind == SettingKind::Text ? GetText(setting) : std::to_string(ValueOf(setting).number);
		}

		// A key's line in the default file: KEY=default, then its comment at the comment column, which
		// every KEY=default ends short of (CommentsFitTheColumn).
		std::string DefaultLine(const Setting& setting)
		{
			std::string line = std::string(setting.key) + "=" + DefaultValueText(setting);
			if (*setting.comment != '\0')
				line += std::string(kIniCommentColumn - line.size(), ' ') + "//" + setting.comment;
			return line;
		}
	}

	void LoadSettings()
	{
		for (size_t i = 0; i < kCount; i++)
			Load(kSettings[i], g_values[i]);
		g_loaded = true;
		ReleaseIniFile();	// every setting is read: TweaksV.ini is not read again this session

		const int layout = g_values[IndexOf(kIniVersion)].number;
		if (layout > kIniLayoutVersion)
			ReportFailure("TweaksV", "TweaksV.ini comes from a newer TweaksV (layout " + std::to_string(layout) +
				"; this TweaksV knows layout " + std::to_string(kIniLayoutVersion) + "). Settings it does not know are "
				"ignored, and some may mean something else here.");
	}

	bool IsOn(const Setting& setting)
	{
		return ValueOf(setting).number != 0;
	}

	int GetNumber(const Setting& setting)
	{
		return ValueOf(setting).number;
	}

	const std::string& GetText(const Setting& setting)
	{
		return ValueOf(setting).text;
	}

	bool IsFeatureOn(const SettingsSection& section)
	{
		AcquireSRWLockShared(&g_lock);
		bool disabled = false;
		for (const SettingsSection* off : g_disabled)
			disabled = disabled || off == &section;
		ReleaseSRWLockShared(&g_lock);
		if (disabled)
			return false;
		return IsEnabledInIni(section);
	}

	bool IsEnabledInIni(const SettingsSection& section)
	{
		for (const Setting& setting : kSettings)
			if (setting.section == &section && std::string_view(setting.key) == "ENABLED")
				return IsOn(setting);
		return true;
	}

	void DisableFeatureForSession(const SettingsSection& section)
	{
		AcquireSRWLockExclusive(&g_lock);
		bool known = false;
		for (const SettingsSection* off : g_disabled)
			known = known || off == &section;
		if (!known)
			g_disabled.push_back(&section);
		ReleaseSRWLockExclusive(&g_lock);
	}

	std::string DefaultValueText(const Setting& setting)
	{
		return setting.kind == SettingKind::Text ? std::string() : std::to_string(setting.defaultValue);
	}

	std::string DefaultIniText()
	{
		// the header, and a short comment after each key's value: README.md explains the keys in full
		std::string text =
			"; ===========================================================================\r\n"
			"; TweaksV.asi Configuration File (GTA V Enhanced)\r\n"
			"; 1 = on, 0 = off (true/false also work)\r\n"
			"; TweaksV.ini in the game folder takes priority over %LOCALAPPDATA%\\TweaksV\\\r\n"
			"; Delete this file to get a fresh one with default values on next game boot\r\n"
			"; Missing settings are added back by TweaksV with the default value\r\n"
			"; For more details on what each setting does, check the README.md\r\n"
			"; ===========================================================================\r\n";
		for (const SettingsSection* section : kSections)
		{
			text += "\r\n[" + std::string(section->name) + "]\r\n";
			for (const Setting& setting : kSettings)
				if (setting.section == section && !setting.testBuildOnly)
					text += DefaultLine(setting) + "\r\n";
		}
		return text;
	}

	std::vector<std::string> DescribeSettings()
	{
		std::vector<std::string> lines;
		std::string features = "Features:";
		for (size_t i = 1; i < std::size(kSections); i++)	// the parts - [TWEAKSV] is TweaksV itself
			features += std::string(i > 1 ? ", " : " ") + kSections[i]->feature + (IsFeatureOn(*kSections[i]) ? " on" : " off");
		lines.push_back(features);

		for (const SettingsSection* section : kSections)
		{
			std::string line = std::string(section->feature) + ":";
			bool any = false;
			for (const Setting& setting : kSettings)
			{
				if (setting.section != section || setting.testBuildOnly || std::string_view(setting.key) == "ENABLED")
					continue;
				line += " " + std::string(setting.key) + "=" + ValueText(setting);
				any = true;
			}
			if (any)
				lines.push_back(line);
		}
#ifdef TWEAKSV_TEST_BUILD
		const std::string& broken = GetText(kTestBreakPattern);
		const std::string& logLimit = GetText(kTestLogLimitKb);
		const std::string& dumpCode = GetText(kTestDumpCode);
		const std::string& dumpScript = GetText(kTestDumpScript);
		lines.push_back("TEST BUILD - not for release. TEST_BREAK_PATTERN=" + (broken.empty() ? std::string("(none)") : broken) +
			" TEST_LOG_LIMIT_KB=" + (logLimit.empty() ? std::string("(none)") : logLimit) +
			" TEST_DUMP_CODE=" + (dumpCode.empty() ? std::string("(none)") : dumpCode) +
			" TEST_DUMP_SCRIPT=" + (dumpScript.empty() ? std::string("(none)") : dumpScript));
#endif
		return lines;
	}
}
