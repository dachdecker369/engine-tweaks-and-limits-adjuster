// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - failure collection and the one start-up summary message box.

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
		constexpr int kMaxLateBoxes = 3;	// a part that keeps failing cannot fill the screen with boxes

		SRWLOCK g_lock = SRWLOCK_INIT;
		std::vector<std::string> g_items;
		size_t g_dropped = 0;
		LONG g_shown = 0;
		// Problems reported after the start-up summary was shown: a thread of their own shows them
		std::vector<std::string> g_late;	// under g_lock
		bool g_lateThread = false;			// under g_lock: that thread runs
		int g_lateBoxes = 0;				// only that thread uses it

		// Starts thread with an extra reference to this module, which the thread drops when it ends, so
		// TweaksV.asi cannot be unloaded while the thread still runs its code. False if it cannot start.
		bool StartModuleThread(LPTHREAD_START_ROUTINE thread, const char* what)
		{
			HMODULE module = nullptr;
			if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(thread), &module))
			{
				const DWORD error = GetLastError();
				WriteLog("TweaksV", std::string("The ") + what + " cannot be shown (" + DescribeError(error) + ").");
				return false;
			}
			// Allowed inside DllMain as long as DllMain does not wait for the thread: it starts
			// running once the loader lock is released.
			const HANDLE handle = CreateThread(nullptr, 0, thread, module, 0, nullptr);
			if (!handle)
			{
				const DWORD error = GetLastError();
				FreeLibrary(module);
				WriteLog("TweaksV", std::string("The ") + what + " thread could not be started (" + DescribeError(error) + ").");
				return false;
			}
			CloseHandle(handle);
			return true;
		}

		void ShowBox(const std::wstring& heading, const std::vector<std::string>& items, size_t dropped)
		{
#ifdef TWEAKSV_TEST_BUILD
			// test builds only: TWEAKSV_TEST_NO_BOXES=1 in the environment keeps the box off the screen, for
			// unattended test runs - also those in which TweaksV.ini is not read and cannot turn boxes off
			wchar_t noBoxes[4]{};
			if (GetEnvironmentVariableW(L"TWEAKSV_TEST_NO_BOXES", noBoxes, 4) == 1 && noBoxes[0] == L'1')
			{
				WriteLog("TweaksV", "TEST BUILD: message box not shown (TWEAKSV_TEST_NO_BOXES=1): " + ToUtf8(heading));
				return;
			}
#endif
			std::wstring text = heading + L"\n\n";
			for (const auto& item : items)
				text += L"• " + FromUtf8(item) + L"\n\n";
			if (dropped > 0)
				text += L"• ... and " + std::to_wstring(dropped) + L" more.\n\n";
			text += L"The game continues without the affected parts. Details are in TweaksV.log:\n" + GetLogPathW() +
				L"\n\nTo stop this message, set ERROR_POPUP=0 in TweaksV.ini.";
			MessageBoxW(nullptr, text.c_str(), L"TweaksV", MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
		}

		DWORD WINAPI SummaryThread(LPVOID module)
		{
			ShowStartupSummary();
			FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
		}

		// Shows the problems reported after the start-up summary, a second after the first of them
		// (more usually follow at once), until none is left.
		DWORD WINAPI LateThread(LPVOID module)
		{
			for (;;)
			{
				Sleep(1000);
				AcquireSRWLockExclusive(&g_lock);
				std::vector<std::string> items;
				items.swap(g_late);
				if (items.empty())
					g_lateThread = false;	// under the lock, so a new problem starts a new thread
				ReleaseSRWLockExclusive(&g_lock);
				if (items.empty())
					break;

				if (!IsOn(kErrorPopup))
					WriteLog("TweaksV", "Problem after the start-up summary not shown (ERROR_POPUP=0): " +
						std::to_string(items.size()) + " problem(s), see above.");
				else if (g_lateBoxes >= kMaxLateBoxes)
					WriteLog("TweaksV", "Problem after the start-up summary not shown: " + std::to_string(kMaxLateBoxes) +
						" message boxes were shown already this session.");
				else
				{
					++g_lateBoxes;
					WriteLog("TweaksV", "Showing a problem after the start-up summary (" + std::to_string(items.size()) +
						" problem(s)).");
					ShowBox(L"TweaksV: a part stopped working after the start-up:", items, 0);
				}
			}
			FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
		}
	}

	void ReportFailure(const char* plugin, const std::string& what)
	{
		WriteLog(plugin, "PROBLEM: " + what);

		bool startLate = false;
		AcquireSRWLockExclusive(&g_lock);
		if (InterlockedCompareExchange(&g_shown, 0, 0) == 0)	// the start-up summary is still to come
		{
			if (g_items.size() < kMaxItems)
				g_items.push_back(std::string("[") + plugin + "] " + what);
			else
				++g_dropped;
		}
		else if (g_late.size() < kMaxItems)
		{
			g_late.push_back(std::string("[") + plugin + "] " + what);
			startLate = !g_lateThread;
			g_lateThread = true;
		}
		ReleaseSRWLockExclusive(&g_lock);

		if (startLate && !StartModuleThread(LateThread, "problem after the start-up summary"))
		{
			AcquireSRWLockExclusive(&g_lock);
			g_lateThread = false;
			ReleaseSRWLockExclusive(&g_lock);
		}
	}

	void ShowStartupSummary()
	{
		// From here on, a reported problem is shown on its own (LateThread): the list is taken under
		// the same lock ReportFailure adds to it with, so no problem is lost in between.
		AcquireSRWLockExclusive(&g_lock);
		const bool first = InterlockedExchange(&g_shown, 1) == 0;
		const std::vector<std::string> items = g_items;
		const size_t dropped = g_dropped;
		ReleaseSRWLockExclusive(&g_lock);

		if (!first || items.empty())
			return;
		if (!IsOn(kErrorPopup))
		{
			WriteLog("TweaksV", "Start-up summary not shown (ERROR_POPUP=0): " + std::to_string(items.size() + dropped) +
				" problem(s), see above.");
			return;
		}

		WriteLog("TweaksV", "Showing the start-up summary (" + std::to_string(items.size() + dropped) + " problem(s)).");
		ShowBox(L"TweaksV could not apply everything:", items, dropped);
	}

	void ShowStartupSummaryLater()
	{
		StartModuleThread(SummaryThread, "start-up summary");
	}
}
