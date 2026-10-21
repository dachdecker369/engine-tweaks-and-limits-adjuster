// SPDX-License-Identifier: GPL-3.0-only
// Test harness: pretends to be the game and loads an .asi the way an ASI loader does.
// Usage: GTA5_Enhanced.exe [path\to\TweaksV.asi] [unload]
#include <windows.h>
#include <cstdio>
#include <cwchar>

int wmain(int argc, wchar_t** argv)
{
	const wchar_t* asi = argc > 1 ? argv[1] : L"TweaksV.asi";
	const bool unload = argc > 2 && wcscmp(argv[2], L"unload") == 0;

	HMODULE module = LoadLibraryW(asi);
	if (!module)
	{
		wprintf(L"LoadLibrary(%ls) failed: %lu\n", asi, GetLastError());
		return 1;
	}
	FILETIME now{};
	GetSystemTimeAsFileTime(&now);	// as the game does early on: RageOpenV's one-time start-up runs
	Sleep(1500); // the start-up summary thread runs
	if (unload)
	{
		FreeLibrary(module);
		Sleep(300);
	}
	wprintf(L"harness: %ls %ls\n", asi, unload ? L"loaded and unloaded" : L"loaded");
	return 0;
}
