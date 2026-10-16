// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - TweaksV.ini: location, generation and readers (plan 5.6 and 4.7).

#include "TweaksVIni.h"

#include <windows.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

#include "TweaksVIniDefault.h"
#include "TweaksVLog.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		struct IniKey
		{
			const char* section;
			const char* key;
			const char* defaultValue;
		};

		// Every key of TweaksV.ini with its default, in file order. Keep in sync with
		// kDefaultIni (TweaksVIniDefault.h).
		constexpr IniKey kKeys[] = {
			{ "TWEAKSV", "ENABLED", "1" },
			{ "TWEAKSV", "LOG_TO_FILE", "1" },
			{ "TWEAKSV", "CONSOLE", "0" },
			{ "TWEAKSV", "SHOW_ERROR_POPUPS", "1" },
			{ "TWEAKSV", "CHECK_FOR_CONFLICTS", "1" },
			{ "TWEAKSV", "DIAGNOSTIC_MODE", "0" },
			{ "TWEAKSV", "DRY_RUN", "0" },
			{ "HEAP_SETTINGS", "ENABLED", "1" },
			{ "HEAP_SETTINGS", "HEAP_SIZE", "2000" },
			{ "MODKIT_SETTINGS", "ENABLED", "1" },
			{ "MODKIT_SETTINGS", "MODKITARRAY_SIZE", "65536" },
			{ "PACKFILE_SETTINGS", "ENABLED", "1" },
			{ "PACKFILE_SETTINGS", "PACKFILEARRAY_SIZE", "13000" },
			{ "RAGEOPENV_SETTINGS", "ENABLED", "1" },
			{ "RAGEOPENV_SETTINGS", "MODS_FOLDER", "1" },
			{ "RAGEOPENV_SETTINGS", "OPEN_RPF", "1" },
			{ "RAGEOPENV_SETTINGS", "SKIP_RPF_CACHE_CHECK", "1" },
			{ "RAGEOPENV_SETTINGS", "NEWMODS", "0" },
			{ "RAGEOPENV_SETTINGS", "LOG_INFO", "0" },
			{ "RAGEOPENV_SETTINGS", "LOG_MODS", "0" },
			{ "RAGEOPENV_SETTINGS", "LOG_DEVICE", "0" },
			{ "STRAIGHTTOSTORYMODE_SETTINGS", "ENABLED", "1" },
			{ "STRAIGHTTOSTORYMODE_SETTINGS", "SKIP_SPLASH_SCREEN", "1" },
			{ "STRAIGHTTOSTORYMODE_SETTINGS", "SKIP_LEGAL_SCREEN", "1" },
			{ "STRAIGHTTOSTORYMODE_SETTINGS", "SKIP_LANDING_PAGE", "1" },
			{ "DIRECTSTORAGE_SETTINGS", "ENABLED", "1" },
			{ "DIRECTSTORAGE_SETTINGS", "LOG_CALLS", "0" },
			{ "DIRECTSTORAGE_SETTINGS", "LOG_FILE_OPENS", "0" },
		};

		SRWLOCK g_lock = SRWLOCK_INIT;
		std::vector<std::string> g_disabledSections;	// switched off for this session
		std::vector<std::string> g_reportedKeys;		// invalid values already logged
		bool g_iniAvailable = false;
		std::wstring g_iniPath;							// the TweaksV.ini in use
		std::string g_iniPathA;							// the same path for the ...A functions
		bool g_iniInFallback = false;					// it is the one in %LOCALAPPDATA%\TweaksV

		// Section and key names are ASCII.
		std::wstring Widen(const char* text)
		{
			std::wstring result;
			for (const char* p = text; *p; ++p)
				result += static_cast<wchar_t>(static_cast<unsigned char>(*p));
			return result;
		}

		bool ContainsNoCase(const std::vector<std::string>& list, const std::string& value)
		{
			for (const auto& entry : list)
				if (_stricmp(entry.c_str(), value.c_str()) == 0)
					return true;
			return false;
		}

		// True the first time a problem with this key is seen (so it is logged once).
		bool FirstReport(const char* section, const char* key)
		{
			const std::string id = std::string(section) + "/" + key;
			AcquireSRWLockExclusive(&g_lock);
			const bool first = !ContainsNoCase(g_reportedKeys, id);
			if (first)
				g_reportedKeys.push_back(id);
			ReleaseSRWLockExclusive(&g_lock);
			return first;
		}

		// The value as written in TweaksV.ini, trimmed; empty if the key is missing.
		std::wstring ReadRaw(const char* section, const char* key)
		{
			wchar_t buffer[256]{};
			GetPrivateProfileStringW(Widen(section).c_str(), Widen(key).c_str(), L"", buffer,
				static_cast<DWORD>(std::size(buffer)), GetIniPathW());
			const std::wstring value = buffer;
			const size_t first = value.find_first_not_of(L" \t");
			if (first == std::wstring::npos)
				return {};
			const size_t last = value.find_last_not_of(L" \t");
			return value.substr(first, last - first + 1);
		}

		// Names of the keys in one section of TweaksV.ini.
		std::vector<std::wstring> ReadKeyNames(const std::wstring& section)
		{
			std::vector<wchar_t> buffer(4096);
			for (;;)
			{
				const DWORD length = GetPrivateProfileStringW(section.c_str(), nullptr, L"", buffer.data(),
					static_cast<DWORD>(buffer.size()), GetIniPathW());
				if (length + 2 < buffer.size())
					break;
				buffer.resize(buffer.size() * 2);
			}

			std::vector<std::wstring> names;
			for (const wchar_t* name = buffer.data(); *name; name += wcslen(name) + 1)
				names.emplace_back(name);
			return names;
		}

		bool ContainsKey(const std::vector<std::wstring>& names, const std::wstring& key)
		{
			for (const auto& name : names)
				if (_wcsicmp(name.c_str(), key.c_str()) == 0)
					return true;
			return false;
		}

		// Converts a path to the ANSI code page for the ...A INI functions. If the path
		// has characters the code page cannot hold, falls back to the 8.3 short path.
		std::string ToAnsiPath(const wchar_t* path, bool& lossy)
		{
			BOOL usedDefault = FALSE;
			const int size = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, path, -1, nullptr, 0, nullptr, &usedDefault);
			if (size <= 0)
			{
				lossy = true;
				return {};
			}
			std::string result(static_cast<size_t>(size), '\0');
			WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, path, -1, result.data(), size, nullptr, &usedDefault);
			result.resize(static_cast<size_t>(size) - 1);
			lossy = usedDefault != FALSE;
			return result;
		}

		std::string MakeAnsiPath(const std::wstring& path)
		{
			bool lossy = false;
			std::string result = ToAnsiPath(path.c_str(), lossy);
			if (lossy)
			{
				wchar_t shortPath[MAX_PATH]{};
				const DWORD length = GetShortPathNameW(path.c_str(), shortPath, MAX_PATH);
				bool shortLossy = true;
				std::string shortResult;
				if (length > 0 && length < MAX_PATH)
					shortResult = ToAnsiPath(shortPath, shortLossy);
				if (!shortLossy)
					result = shortResult;
				else
					WriteLog("TweaksV", "The path of TweaksV.ini has characters the system code page cannot "
						"represent. Settings read through it fall back to their defaults.");
			}
			return result;
		}

		bool FileExists(const std::wstring& path)
		{
			const DWORD attributes = GetFileAttributesW(path.c_str());
			return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
		}

		std::wstring GameFolderIni()
		{
			return GetGameFolder() + L"\\TweaksV.ini";
		}

		std::wstring FallbackIni()
		{
			const std::wstring folder = GetFallbackFolder();
			return folder.empty() ? std::wstring() : folder + L"\\TweaksV.ini";
		}

		void SetIniPath(const std::wstring& path, bool inFallback)
		{
			g_iniPath = path;
			g_iniInFallback = inFallback;
			g_iniPathA = MakeAnsiPath(path);
		}

		// Writes the full default TweaksV.ini (Appendix E.1, CRLF line endings) as a new
		// file. Returns ERROR_SUCCESS or the Windows error.
		DWORD WriteDefaultIni(const std::wstring& path)
		{
			std::string text;
			for (const char* p = kDefaultIni; *p; ++p)
			{
				if (*p == '\r')
					continue;
				if (*p == '\n')
					text += '\r';
				text += *p;
			}

			const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
				return GetLastError();

			DWORD written = 0;
			const BOOL ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
			DWORD error = ok ? ERROR_SUCCESS : GetLastError();
			if (ok && written != text.size())
				error = ERROR_WRITE_FAULT;
			CloseHandle(file);
			if (error != ERROR_SUCCESS)
				DeleteFileW(path.c_str());
			return error;
		}

		// Adds any missing key of the TweaksV.ini in use, with its default. Values are
		// never changed.
		void AddMissingKeys()
		{
			std::wstring section;
			std::vector<std::wstring> names;
			bool added = false;
			for (const IniKey& entry : kKeys)
			{
				const std::wstring entrySection = Widen(entry.section);
				if (entrySection != section)
				{
					section = entrySection;
					names = ReadKeyNames(section);
				}

				const std::wstring key = Widen(entry.key);
				if (ContainsKey(names, key))
					continue;

				const std::string name = std::string("[") + entry.section + "] " + entry.key;
				if (WritePrivateProfileStringW(section.c_str(), key.c_str(), Widen(entry.defaultValue).c_str(), g_iniPath.c_str()))
				{
					added = true;
					WriteLog("TweaksV", "TweaksV.ini: added the missing key " + name + "=" + entry.defaultValue + ".");
				}
				else
				{
					const DWORD error = GetLastError();
					const std::string text = "TweaksV.ini: the missing key " + name + " could not be added (" +
						DescribeError(error) + "). Its default (" + entry.defaultValue + ") is used.";
					if (error == ERROR_ACCESS_DENIED)
						WriteLog("TweaksV", text);	// a read-only TweaksV.ini is not a problem
					else
						ReportFailure("TweaksV", text);
				}
			}

			if (added)
				WritePrivateProfileStringW(nullptr, nullptr, nullptr, g_iniPath.c_str()); // flush
		}
	}

	const std::wstring& GetGameFolder()
	{
		static const std::wstring folder = []
		{
			std::wstring path(MAX_PATH, L'\0');
			for (;;)
			{
				const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
				if (length == 0)
					return std::wstring(L".");
				if (length < path.size())
				{
					path.resize(length);
					break;
				}
				path.resize(path.size() * 2);
			}
			const size_t slash = path.find_last_of(L"\\/");
			return slash == std::wstring::npos ? std::wstring(L".") : path.substr(0, slash);
		}();
		return folder;
	}

	std::wstring GetFallbackFolder()
	{
		wchar_t localAppData[MAX_PATH]{};
		const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
		if (length == 0 || length >= MAX_PATH)
			return {};
		return std::wstring(localAppData) + L"\\TweaksV";
	}

	const wchar_t* GetIniPathW()
	{
		if (g_iniPath.empty()) // only before CreateOrUpdateIni(): the file that would be read
		{
			const std::wstring fallback = FallbackIni();
			const bool inFallback = !FileExists(GameFolderIni()) && !fallback.empty() && FileExists(fallback);
			SetIniPath(inFallback ? fallback : GameFolderIni(), inFallback);
		}
		return g_iniPath.c_str();
	}

	const char* GetIniPathA()
	{
		GetIniPathW(); // makes sure a file was chosen
		return g_iniPathA.c_str();
	}

	void CreateOrUpdateIni()
	{
		const std::wstring gameIni = GameFolderIni();
		const std::wstring fallbackIni = FallbackIni();

		// An existing TweaksV.ini: the game folder's always wins, else the one in
		// %LOCALAPPDATA%\TweaksV. Only missing keys are added.
		if (FileExists(gameIni) || (!fallbackIni.empty() && FileExists(fallbackIni)))
		{
			const bool inFallback = !FileExists(gameIni);
			SetIniPath(inFallback ? fallbackIni : gameIni, inFallback);
			g_iniAvailable = true;
			AddMissingKeys();
			return;
		}

		// None yet: create it in the game folder - or, if that folder is read-only (e.g.
		// under C:\Program Files), in %LOCALAPPDATA%\TweaksV (plan 4.7). Not a problem,
		// so it is only logged.
		const DWORD gameError = WriteDefaultIni(gameIni);
		if (gameError == ERROR_SUCCESS || gameError == ERROR_FILE_EXISTS)
		{
			SetIniPath(gameIni, false);
			g_iniAvailable = true;
			if (gameError == ERROR_SUCCESS)
				WriteLog("TweaksV", "Created TweaksV.ini with the default settings.");
			else
				AddMissingKeys(); // created by someone else in the meantime
			return;
		}

		DWORD fallbackError = ERROR_PATH_NOT_FOUND;
		if (!fallbackIni.empty())
		{
			CreateDirectoryW(GetFallbackFolder().c_str(), nullptr);
			fallbackError = WriteDefaultIni(fallbackIni);
			if (fallbackError == ERROR_SUCCESS)
			{
				SetIniPath(fallbackIni, true);
				g_iniAvailable = true;
				WriteLog("TweaksV", "The game folder is read-only (" + DescribeError(gameError) + "), so TweaksV.ini "
					"was created with the default settings in " + ToUtf8(GetFallbackFolder()) + ".");
				return;
			}
		}

		SetIniPath(gameIni, false); // every reader returns its built-in default
		ReportFailure("TweaksV", "TweaksV.ini could not be created in the game folder (" + DescribeError(gameError) +
			") or in %LOCALAPPDATA%\\TweaksV (" + DescribeError(fallbackError) + "). TweaksV uses its built-in defaults.");
	}

	bool IsIniAvailable()
	{
		return g_iniAvailable;
	}

	std::string DescribeIniLocation()
	{
		const std::string path = ToUtf8(GetIniPathW());
		if (!g_iniAvailable)
			return path + " (could not be created - built-in defaults are used)";
		if (g_iniInFallback)
			return path + " (the game folder has no TweaksV.ini)";
		const std::wstring fallback = FallbackIni();
		if (!fallback.empty() && FileExists(fallback))
			return path + " (wins over the unused copy in " + ToUtf8(GetFallbackFolder()) + ")";
		return path;
	}

	bool GetIniBool(const char* section, const char* key, bool defaultValue)
	{
		std::wstring value = ReadRaw(section, key);
		if (value.empty())
			return defaultValue;

		for (auto& c : value)
			c = static_cast<wchar_t>(towlower(c));

		if (value == L"1" || value == L"true" || value == L"yes" || value == L"on")
			return true;
		if (value == L"0" || value == L"false" || value == L"no" || value == L"off")
			return false;

		if (FirstReport(section, key))
			WriteLog("TweaksV", std::string("TweaksV.ini: [") + section + "] " + key + "=" + ToUtf8(value) +
				" is not a valid on/off value - using " + (defaultValue ? "1" : "0") + ".");
		return defaultValue;
	}

	int GetIniInt(const char* section, const char* key, int defaultValue, int minValue, int maxValue)
	{
		const std::wstring value = ReadRaw(section, key);
		if (value.empty())
			return defaultValue;

		wchar_t* end = nullptr;
		errno = 0;
		const long long parsed = wcstoll(value.c_str(), &end, 10);
		if (end == value.c_str() || *end != L'\0' || errno == ERANGE)
		{
			if (FirstReport(section, key))
				WriteLog("TweaksV", std::string("TweaksV.ini: [") + section + "] " + key + "=" + ToUtf8(value) +
					" is not a number - using " + std::to_string(defaultValue) + ".");
			return defaultValue;
		}

		long long clamped = parsed;
		if (clamped < minValue)
			clamped = minValue;
		if (clamped > maxValue)
			clamped = maxValue;
		if (clamped != parsed && FirstReport(section, key))
			WriteLog("TweaksV", std::string("TweaksV.ini: [") + section + "] " + key + "=" + std::to_string(parsed) +
				" is outside " + std::to_string(minValue) + ".." + std::to_string(maxValue) + " - using " +
				std::to_string(clamped) + ".");
		return static_cast<int>(clamped);
	}

	std::string GetIniString(const char* section, const char* key, const char* defaultValue)
	{
		const std::wstring value = ReadRaw(section, key);
		return value.empty() ? std::string(defaultValue) : ToUtf8(value);
	}

	bool IsFeatureOn(const char* section)
	{
		AcquireSRWLockShared(&g_lock);
		const bool disabled = ContainsNoCase(g_disabledSections, section);
		ReleaseSRWLockShared(&g_lock);
		return !disabled && GetIniBool(section, "ENABLED", true);
	}

	void DisableFeatureForSession(const char* section)
	{
		AcquireSRWLockExclusive(&g_lock);
		if (!ContainsNoCase(g_disabledSections, section))
			g_disabledSections.emplace_back(section);
		ReleaseSRWLockExclusive(&g_lock);
	}

	bool IsDryRun()
	{
		static const bool dryRun = GetIniBool("TWEAKSV", "DRY_RUN", false);
		return dryRun;
	}

	bool IsDiagnosticMode()
	{
		static const bool diagnosticMode = GetIniBool("TWEAKSV", "DIAGNOSTIC_MODE", false);
		return diagnosticMode;
	}
}
