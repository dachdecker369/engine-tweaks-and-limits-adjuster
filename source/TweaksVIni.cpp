// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - TweaksV.ini, the file: where it is, creating it and adding keys that are missing.

#include "TweaksVIni.h"

#include <windows.h>

#include <cwchar>
#include <iterator>
#include <string>
#include <vector>

#include "TweaksVLog.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		bool g_iniAvailable = false;
		std::wstring g_iniPath;							// the TweaksV.ini in use
		bool g_iniInFallback = false;					// it is the one in %LOCALAPPDATA%\TweaksV

		// Section and key names are ASCII.
		std::wstring Widen(const char* text)
		{
			std::wstring result;
			for (const char* p = text; *p; ++p)
				result += static_cast<wchar_t>(static_cast<unsigned char>(*p));
			return result;
		}

		// Names of the keys in one section of TweaksV.ini.
		std::vector<std::wstring> ReadKeyNames(const std::wstring& section)
		{
			std::vector<wchar_t> buffer(4096);
			for (;;)
			{
				const DWORD length = GetPrivateProfileStringW(section.c_str(), nullptr, L"", buffer.data(),
					static_cast<DWORD>(buffer.size()), GetIniPathW());
				if (length + 2 < buffer.size() || buffer.size() >= 1024 * 1024)	// at most 1 M characters of names
					break;
				buffer.resize(buffer.size() * 2);
			}
			buffer.back() = L'\0';
			if (buffer.size() >= 2)
				buffer[buffer.size() - 2] = L'\0';

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
		}

		// Writes the full default TweaksV.ini as a new file. Returns ERROR_SUCCESS or the Windows error.
		DWORD WriteDefaultIni(const std::wstring& path)
		{
			const std::string text = DefaultIniText();
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

		// Adds every key of the table that the TweaksV.ini in use is missing, with its default.
		// Values are never changed.
		void AddMissingKeys()
		{
			std::wstring section;
			std::vector<std::wstring> names;
			bool added = false;
			for (const Setting& setting : kSettings)
			{
				if (setting.testBuildOnly)
					continue;
				const std::wstring settingSection = Widen(setting.section->name);
				if (settingSection != section)
				{
					section = settingSection;
					names = ReadKeyNames(section);
				}

				const std::wstring key = Widen(setting.key);
				if (ContainsKey(names, key))
					continue;

				const std::string value = DefaultValueText(setting);
				const std::string name = std::string("[") + setting.section->name + "] " + setting.key;
				if (WritePrivateProfileStringW(section.c_str(), key.c_str(), Widen(value.c_str()).c_str(), g_iniPath.c_str()))
				{
					added = true;
					WriteLog("TweaksV", "TweaksV.ini: added the missing key " + name + "=" + value + ".");
				}
				else
				{
					const DWORD error = GetLastError();
					const std::string text = "TweaksV.ini: the missing key " + name + " could not be added (" +
						DescribeError(error) + "). Its default (" + value + ") is used.";
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
				if (path.size() >= 32768)	// longer than any Windows path
					return std::wstring(L".");
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
		// under C:\Program Files), in %LOCALAPPDATA%\TweaksV. Not a problem, so it is only logged.
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

	std::wstring ReadIniValue(const char* section, const char* key)
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
}
