// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - TweaksV.ini, the file: where it is, creating it, adding keys that are missing, and
// replacing a file that has the layout of TweaksV 1.x.

#include "TweaksVIni.h"

#include <windows.h>

#include <cwchar>
#include <iterator>
#include <string>
#include <vector>

#include "TweaksVFiles.h"
#include "TweaksVLog.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const wchar_t* kIniName = L"TweaksV.ini";
		constexpr const wchar_t* kOldIniName = L"TweaksV.old.ini";	// what a file with the layout of 1.x becomes

		bool g_iniAvailable = false;
		std::wstring g_iniPath;							// the TweaksV.ini in use
		bool g_iniInFallback = false;					// it is the one in %LOCALAPPDATA%\TweaksV
		HANDLE g_iniHold = INVALID_HANDLE_VALUE;		// holds it from CreateOrUpdateIni() to ReleaseIniFile()
		bool g_iniReadable = true;						// false: it failed its check and is not read at all
		bool g_iniReplaced = false;						// it is the fresh file written in place of a 1.x file
		std::string g_iniNotUsed;						// why it is not used, for the log header

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

		std::wstring GameFolderIni()
		{
			return GetGameFolder() + L"\\" + kIniName;
		}

		std::wstring FallbackIni()
		{
			const std::wstring folder = GetFallbackFolder();
			return folder.empty() ? std::wstring() : folder + L"\\" + kIniName;
		}

		void SetIniPath(const std::wstring& path, bool inFallback)
		{
			g_iniPath = path;
			g_iniInFallback = inFallback;
		}

		// The folder of the TweaksV.ini in use, and which kind of folder it is for the own-file checks.
		std::wstring IniFolder()
		{
			return g_iniInFallback ? GetFallbackFolder() : GetGameFolder();
		}

		FileFolder IniFolderKind()
		{
			return g_iniInFallback ? FileFolder::Own : FileFolder::Game;
		}

		// Writes the full default TweaksV.ini as a new file in folder, only where it is what its path says
		// (TweaksVFiles.h). Comes back without a handle: error is ERROR_SUCCESS once it is written, and
		// unsafe says why nothing was written where a link is in the way.
		OwnFile WriteDefaultIni(const std::wstring& folder, FileFolder where)
		{
			OwnFile file = OpenOwnFile(folder, kIniName, where, CREATE_NEW, false);
			if (file.handle == INVALID_HANDLE_VALUE)
				return file;

			const std::string text = DefaultIniText();
			DWORD written = 0;
			const BOOL ok = WriteFile(file.handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
			file.error = ok ? ERROR_SUCCESS : GetLastError();
			if (ok && written != text.size())
				file.error = ERROR_WRITE_FAULT;
			if (file.error != ERROR_SUCCESS)	// not left half written
			{
				FILE_DISPOSITION_INFO remove{ TRUE };
				SetFileInformationByHandle(file.handle, FileDispositionInfo, &remove, sizeof(remove));
			}
			CloseHandle(file.handle);
			file.handle = INVALID_HANDLE_VALUE;
			return file;
		}

		// Holds the TweaksV.ini in use (g_iniPath) once it has passed its check (TweaksVFiles.h), until
		// ReleaseIniFile(): whatever TweaksV reads and writes by its path then reaches this very file. A
		// file that fails the check is not read at all, and TweaksV uses its built-in defaults.
		bool HoldIni()
		{
			const OwnFile held = HoldOwnFile(IniFolder(), kIniName, IniFolderKind());
			if (held.handle != INVALID_HANDLE_VALUE)
			{
				g_iniHold = held.handle;
				g_iniAvailable = true;
				return true;
			}
			g_iniReadable = false;
			g_iniNotUsed = held.Reason();
			ReportFailure("TweaksV", "TweaksV.ini (" + ToUtf8(g_iniPath) + ") is not used: " + g_iniNotUsed +
				". TweaksV uses its built-in defaults this session.");
			return false;
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

		// True if the TweaksV.ini in use has the layout of TweaksV 1.x: no VERSION under [TWEAKSV]. Only
		// the key names of that section are read - no value of a 1.x file is ever used.
		bool HasLayout1()
		{
			return !ContainsKey(ReadKeyNames(Widen(kTweaksVSection.name)), Widen(kIniVersion.key));
		}

		// Replaces the 1.x TweaksV.ini in use: it is renamed to TweaksV.old.ini through the own-file
		// checks (an older TweaksV.old.ini that passes them is replaced) and the fresh default file is
		// written in its place; the start-up summary says so. A file that cannot be renamed stays, is
		// reported, and TweaksV runs on its built-in defaults until the file is gone.
		void ReplaceLayout1Ini()
		{
			ReleaseIniFile();	// the hold shares no delete: the rename needs a handle of its own
			const OwnFile renamed = RenameOwnFile(IniFolder(), kIniName, kOldIniName, IniFolderKind());
			if (!renamed.unsafe.empty() || renamed.error != ERROR_SUCCESS)
			{
				g_iniReadable = false;
				g_iniNotUsed = "it has the layout of TweaksV 1.x and could not be renamed to TweaksV.old.ini (" + renamed.Reason() + ")";
				ReportFailure("TweaksV", "TweaksV.ini (" + ToUtf8(g_iniPath) + ") has the layout of TweaksV 1.x and could not be renamed "
					"to TweaksV.old.ini (" + renamed.Reason() + "). Delete or rename it yourself - TweaksV uses its built-in defaults "
					"until then.");
				return;
			}

			const OwnFile fresh = WriteDefaultIni(IniFolder(), IniFolderKind());
			if (!fresh.unsafe.empty() || fresh.error != ERROR_SUCCESS)
			{
				g_iniReadable = false;
				g_iniNotUsed = "the 1.x file was renamed to TweaksV.old.ini, but a fresh TweaksV.ini could not be written (" +
					fresh.Reason() + ")";
				ReportFailure("TweaksV", "TweaksV.ini had the layout of TweaksV 1.x and was renamed to TweaksV.old.ini, but a fresh "
					"TweaksV.ini could not be written in " + ToUtf8(IniFolder()) + " (" + fresh.Reason() + "). TweaksV uses its "
					"built-in defaults this session.");
				return;
			}

			if (!HoldIni())	// reported there
				return;
			g_iniReplaced = true;
			ReportFailure("TweaksV", "TweaksV.ini had the layout of TweaksV 1.x. It was renamed to TweaksV.old.ini and a fresh "
				"TweaksV.ini written with the default settings - set your values again.");
		}

		// The TweaksV.ini in use exists and has passed its check: a 1.x file is replaced, a layout-2
		// file gets the keys it is missing.
		void UpdateIni()
		{
			if (HasLayout1())
				ReplaceLayout1Ini();
			else
				AddMissingKeys();
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
		// %LOCALAPPDATA%\TweaksV. It is held while TweaksV reads it; a 1.x file is replaced, otherwise
		// only missing keys are added.
		if (FileExists(gameIni) || (!fallbackIni.empty() && FileExists(fallbackIni)))
		{
			const bool inFallback = !FileExists(gameIni);
			SetIniPath(inFallback ? fallbackIni : gameIni, inFallback);
			if (HoldIni())
				UpdateIni();
			return;
		}

		// None yet: create it in the game folder - or, if that fails, in %LOCALAPPDATA%\TweaksV. A
		// read-only game folder (e.g. under C:\Program Files) is normal, so it is only logged; any
		// other reason, such as a link in the way, is a problem.
		const OwnFile game = WriteDefaultIni(GetGameFolder(), FileFolder::Game);
		if (game.unsafe.empty() && (game.error == ERROR_SUCCESS || game.error == ERROR_FILE_EXISTS))
		{
			SetIniPath(gameIni, false);
			if (HoldIni())
			{
				if (game.error == ERROR_SUCCESS)
					WriteLog("TweaksV", "Created TweaksV.ini with the default settings.");
				else
					UpdateIni(); // created by someone else in the meantime
			}
			return;
		}

		OwnFile fallback{};
		fallback.error = ERROR_ENVVAR_NOT_FOUND;	// no LOCALAPPDATA
		if (!fallbackIni.empty())
		{
			CreateDirectoryW(GetFallbackFolder().c_str(), nullptr);
			fallback = WriteDefaultIni(GetFallbackFolder(), FileFolder::Own);
			if (fallback.unsafe.empty() && fallback.error == ERROR_SUCCESS)
			{
				SetIniPath(fallbackIni, true);
				if (!HoldIni())
					return;
				if (game.ReadOnly())
					WriteLog("TweaksV", "The game folder is read-only (" + DescribeError(game.error) + "), so TweaksV.ini "
						"was created with the default settings in " + ToUtf8(GetFallbackFolder()) + ".");
				else
					ReportFailure("TweaksV", "TweaksV.ini was not created in the game folder: " + game.Reason() + ". It was "
						"created with the default settings in " + ToUtf8(GetFallbackFolder()) + " instead.");
				return;
			}
		}

		SetIniPath(gameIni, false);
		g_iniReadable = false; // every reader returns its built-in default
		ReportFailure("TweaksV", "TweaksV.ini could not be created in the game folder (" + game.Reason() +
			") or in %LOCALAPPDATA%\\TweaksV (" + fallback.Reason() + "). TweaksV uses its built-in defaults.");
	}

	void ReleaseIniFile()
	{
		if (g_iniHold != INVALID_HANDLE_VALUE)
		{
			CloseHandle(g_iniHold);
			g_iniHold = INVALID_HANDLE_VALUE;
		}
	}

	std::string DescribeIniLocation()
	{
		const std::string path = ToUtf8(GetIniPathW());
		if (!g_iniNotUsed.empty())
			return path + " (not used: " + g_iniNotUsed + " - built-in defaults are used)";
		if (!g_iniAvailable)
			return path + " (could not be created - built-in defaults are used)";

		std::string notes;
		if (g_iniReplaced)
			notes = "the 1.x file was renamed to TweaksV.old.ini and this fresh file written";
		if (g_iniInFallback)
			notes += std::string(notes.empty() ? "" : "; ") + "the game folder has no TweaksV.ini";
		else
		{
			const std::wstring fallback = FallbackIni();
			if (!fallback.empty() && FileExists(fallback))
				notes += std::string(notes.empty() ? "" : "; ") + "wins over the unused copy in " + ToUtf8(GetFallbackFolder());
		}
		return notes.empty() ? path : path + " (" + notes + ")";
	}

	std::wstring ReadIniValue(const char* section, const char* key)
	{
		if (!g_iniReadable)	// a TweaksV.ini that failed its check is not read: every setting keeps its default
			return {};
		wchar_t buffer[256]{};
		GetPrivateProfileStringW(Widen(section).c_str(), Widen(key).c_str(), L"", buffer,
			static_cast<DWORD>(std::size(buffer)), GetIniPathW());
		std::wstring value = buffer;
		// a comment after the value - "//", as the default file writes one, or ";" - is not part of it
		const size_t slashes = value.find(L"//");
		const size_t semicolon = value.find(L';');
		const size_t comment = slashes < semicolon ? slashes : semicolon;
		if (comment != std::wstring::npos)
			value.resize(comment);
		const size_t first = value.find_first_not_of(L" \t");
		if (first == std::wstring::npos)
			return {};
		const size_t last = value.find_last_not_of(L" \t");
		return value.substr(first, last - first + 1);
	}
}
