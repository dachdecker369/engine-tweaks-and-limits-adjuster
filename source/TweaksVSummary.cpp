// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - failure collection and the one start-up summary message box (plan 5.8).

#include "TweaksVSummary.h"

#include <windows.h>

#include <string>
#include <vector>

#include "TweaksVLog.h"
#include "TweaksVSettings.h"

namespace TweaksV
{
	namespace
	{
		constexpr size_t kMaxItems = 40;

		SRWLOCK g_lock = SRWLOCK_INIT;
		std::vector<std::string> g_items;
		size_t g_dropped = 0;
		LONG g_shown = 0;

		DWORD WINAPI SummaryThread(LPVOID module)
		{
			ShowStartupSummary();
			// Drops the reference taken in ShowStartupSummaryLater(), so TweaksV.asi
			// cannot be unloaded while this thread still runs its code.
			FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
		}
	}

	void ReportFailure(const char* plugin, const std::string& what)
	{
		WriteLog(plugin, "PROBLEM: " + what);

		AcquireSRWLockExclusive(&g_lock);
		if (g_items.size() < kMaxItems)
			g_items.push_back(std::string("[") + plugin + "] " + what);
		else
			++g_dropped;
		ReleaseSRWLockExclusive(&g_lock);
	}

	void ShowStartupSummary()
	{
		if (InterlockedExchange(&g_shown, 1) != 0)
			return;

		AcquireSRWLockShared(&g_lock);
		const std::vector<std::string> items = g_items;
		const size_t dropped = g_dropped;
		ReleaseSRWLockShared(&g_lock);

		if (items.empty())
			return;
		if (!IsOn(kShowErrorPopups))
		{
			WriteLog("TweaksV", "Start-up summary not shown (SHOW_ERROR_POPUPS=0): " + std::to_string(items.size() + dropped) +
				" problem(s), see above.");
			return;
		}

		std::wstring text = L"TweaksV could not apply everything:\n\n";
		for (const auto& item : items)
			text += L"• " + FromUtf8(item) + L"\n\n";
		if (dropped > 0)
			text += L"• ... and " + std::to_wstring(dropped) + L" more.\n\n";
		text += L"The game continues without the affected parts. Details are in TweaksV.log:\n" + GetLogPathW() +
			L"\n\nTo stop this message, set SHOW_ERROR_POPUPS=0 in TweaksV.ini.";

		WriteLog("TweaksV", "Showing the start-up summary (" + std::to_string(items.size() + dropped) + " problem(s)).");
		MessageBoxW(nullptr, text.c_str(), L"TweaksV", MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
	}

	void ShowStartupSummaryLater()
	{
		// An extra reference to this module, released by the thread when it ends.
		HMODULE module = nullptr;
		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&SummaryThread), &module))
		{
			const DWORD error = GetLastError();
			WriteLog("TweaksV", "The start-up summary cannot be shown (" + DescribeError(error) + ").");
			return;
		}

		// Allowed inside DllMain as long as DllMain does not wait for the thread: it
		// starts running once the loader lock is released.
		const HANDLE thread = CreateThread(nullptr, 0, SummaryThread, module, 0, nullptr);
		if (thread)
		{
			CloseHandle(thread);
		}
		else
		{
			const DWORD error = GetLastError();
			WriteLog("TweaksV", "The start-up summary thread could not be started (" + DescribeError(error) + ").");
		}
	}
}
