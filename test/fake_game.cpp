// SPDX-License-Identifier: GPL-3.0-only
// Test harness: pretends to be the game and loads an .asi the way an ASI loader does.
// Usage: GTA5_Enhanced.exe [path\to\TweaksV.asi]
#include <windows.h>
#include <cstdio>
#include <cwchar>

int wmain(int argc, wchar_t** argv)
{
	const wchar_t* asi = argc > 1 ? argv[1] : L"TweaksV.asi";

	HMODULE module = LoadLibraryW(asi);
	if (!module)
	{
		wprintf(L"LoadLibrary(%ls) failed: %lu\n", asi, GetLastError());
		return 1;
	}
	FILETIME now{};
	GetSystemTimeAsFileTime(&now);	// as the game does early on: RageOpenV's one-time start-up runs
	Sleep(1500); // the start-up summary thread runs
	wprintf(L"harness: %ls loaded\n", asi);
	return 0;
}
