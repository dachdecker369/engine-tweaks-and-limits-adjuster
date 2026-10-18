// TweaksV: carried over from RageOpenV (src/main.cpp) by Chiheb-Bacha, based on ClosedIV by
// martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "main.h"
#include <atomic>						// TweaksV
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVIni.h"			// TweaksV: the game folder
#include "TweaksVSettings.h"	// TweaksV: SKIP_RPF_CACHE_CHECK, DRY_RUN
#include "TweaksVLog.h"			// TweaksV
#include "TweaksVPatch.h"		// TweaksV: DescribeAddress, CallTarget
#include "TweaksVSummary.h"		// TweaksV: start-up summary

namespace RageOpenV // TweaksV: one namespace per original plugin
{

// TweaksV: atomic, and claimed with exchange() BEFORE the init runs, because the init can end up
// calling this function again (plan 4.2) - it was a plain bool (B-ROV-10)
static std::atomic<bool> bInited{ false };
void (*origGetSystemTimeAsFileTime)(LPFILETIME lpSystemTimeAsFileTime);
static PVOID g_timeHookTarget = nullptr; // TweaksV: GetSystemTimeAsFileTime itself, to unhook it after the start-up

// TweaksV: RageOpenV looks for mods\ and newmods\ in the current directory, as the original did;
// says so in the log when that is not the game folder (B-ROV-21)
static void LogCurrentDirectory()
{
	wchar_t current[MAX_PATH] = {};
	const DWORD length = GetCurrentDirectoryW(MAX_PATH, current);
	if (length == 0 || length >= MAX_PATH)
		return;
	std::wstring game = TweaksV::GetGameFolder();
	std::wstring here = current;
	while (!game.empty() && (game.back() == L'\\' || game.back() == L'/'))
		game.pop_back();
	while (!here.empty() && (here.back() == L'\\' || here.back() == L'/'))
		here.pop_back();
	if (_wcsicmp(game.c_str(), here.c_str()) != 0)
		TweaksV::WriteLog("RageOpenV", "The current directory is " + TweaksV::ToUtf8(here) +
			", not the game folder - the mods and newmods folders are looked for there, as in the original.");
}

void HookGetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
{
	if (!bInited.load(std::memory_order_relaxed) && !bInited.exchange(true))
	{
		try // TweaksV: nothing may throw back into the game (plan 5.8)
		{
			LogCurrentDirectory(); // TweaksV

			// Don't hide the console
			// TweaksV: only with CONSOLE=1 - upstream applied it without a console as well (B-ROV-04);
			// the other edition's patterns are removed (F-ROV-1)
			if (config::get_config<bool>("console")) {
				auto addr = memory::scan("ff 15 ? ? ? ? 48 85 c0 74 ? 48 89 c1 31 d2", true);
				if (addr.address != 0) {
					// TweaksV: the 6 bytes must be the "call [rip+x]" (FF 15) that hides the console
					const uint8_t* call = addr.add(16).as<const uint8_t*>();
					if (call[0] != 0xFF || call[1] != 0x15)
						TweaksV::WriteLog("RageOpenV", "Console: unexpected bytes at " + TweaksV::DescribeAddress(call) +
							" - left as they are, so the game may hide the console.");
					else if (TweaksV::IsDryRun())
						TweaksV::WriteLog("RageOpenV", "DRY_RUN: would keep the console visible (" + TweaksV::DescribeAddress(call) + ").");
					else
						addr.add(16).nop(6);
				}
			}

			// rpf cache check skip
			// TweaksV: the SKIP_RPF_CACHE_CHECK switch - the block was "only on Enhanced" (F-ROV-2)
			if (TweaksV::IsOn(TweaksV::kSkipRpfCacheCheck)) {
				auto addr = memory::scan("e8 ? ? ? ? 48 8d 0d ? ? ? ? 48 8d 15 ? ? ? ? 48 8d 35");
				// TweaksV: the call must lead into the game, and a ret already there means another
				// mod has done this (plan 5.9 R2, 4.4)
				const uint8_t* callee = TweaksV::CallTarget((void*)addr.address);
				if (addr.address == 0)
					TweaksV::ReportFailure("RageOpenV", "The RPF cache check is not skipped this session: its call was not "
						"found in the game code.");
				else if (!callee || !TweaksV::IsInsideGameImage(callee))
					TweaksV::ReportFailure("RageOpenV", "The RPF cache check is not skipped this session: the call at " +
						TweaksV::DescribeAddress((void*)addr.address) + " does not lead into the game.");
				else if (callee[0] == 0xC3)
					TweaksV::WriteLog("RageOpenV", "The RPF cache check is already skipped (another mod did it) - left as it is.");
				else if (TweaksV::IsDryRun())
					TweaksV::WriteLog("RageOpenV", "DRY_RUN: would skip the RPF cache check (" + TweaksV::DescribeAddress(callee) + ").");
				else if (addr.add(1).rip().put<uint16_t>(0x90c3))
					TweaksV::WriteLog("RageOpenV", "The RPF cache check is skipped (" + TweaksV::DescribeAddress(callee) + ").");
				else
					TweaksV::ReportFailure("RageOpenV", "The RPF cache check is not skipped this session: the game code "
						"could not be made writable.");
			}
			else
				TweaksV::WriteLog("RageOpenV", "SKIP_RPF_CACHE_CHECK=0 - the RPF cache check is left on.");

			memory::InitFuncs::run();

			logger::write("info", " RageOpenV Inited!");
		}
		catch (...)
		{
			TweaksV::ReportFailure("RageOpenV", "Unexpected exception during start-up - the rest of RageOpenV is skipped "
				"for this session.");
		}

		// TweaksV: one start-up summary for both moments (plan 5.5) - from a worker thread, because
		// this hook runs on whichever thread first asks for the time and must not block it
		TweaksV::ShowStartupSummaryLater();

		// TweaksV: the hook's one job is done, so the game's later calls - it asks for the time
		// constantly - go straight to Windows again. Disabled, not removed: a thread that is inside
		// this function right now still calls the original through the trampoline, which stays.
		if (g_timeHookTarget && MH_DisableHook(g_timeHookTarget) != MH_OK)
			TweaksV::WriteLog("RageOpenV", "The GetSystemTimeAsFileTime hook could not be switched off - it stays and only "
				"passes the calls on.");
	}
	origGetSystemTimeAsFileTime(lpSystemTimeAsFileTime);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	(void)hModule;		// TweaksV: no longer used
	(void)lpReserved;	// TweaksV
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		// TweaksV: DisableThreadLibraryCalls removed - TweaksV links the C runtime statically, which
		// needs the thread notifications (thread_local is used, too)

		config::load(); // TweaksV: reads TweaksV.ini, which TweaksV's DllMain has created

		// TweaksV: logger::init() removed - TweaksV's DllMain clears TweaksV.log once (plan 5.7)

		if (!memory::init()) { // TweaksV: checked
			TweaksV::ReportFailure("RageOpenV", "RageOpenV could not start: MinHook could not be initialised (details "
				"in TweaksV.log). The mods folder, OPEN archives and newmods are off this session.");
			return FALSE;
		}

		// TweaksV: the console is opened by TweaksV's DllMain (CONSOLE=1, TweaksV::OpenConsole) -
		// this code moved there (plan 5.5 step 5)

		// NOTE: using a thread doesn't work consistently. Sometimes some hooks are executed before init is finished.
		// Calling HookGetSystemTimeAsFileTime directly inside DllMain doesn't work (loader lock shenanigans)
		// memory::IAT doesn't work on Enhanced since imports are obfuscated
		// (TweaksV: the other edition's mention is dropped from the first line - F-ROV-3)
		if (!memory::HookApi(L"kernel32.dll", "GetSystemTimeAsFileTime", (PVOID)HookGetSystemTimeAsFileTime, (PVOID*)&origGetSystemTimeAsFileTime,
			&g_timeHookTarget)) { // TweaksV: + the target, to unhook it after the start-up
			logger::write("error", "Hooking failed error (%ld)", GetLastError()); // TweaksV: "error" - was "info"
			TweaksV::ReportFailure("RageOpenV", "RageOpenV could not start: the GetSystemTimeAsFileTime hook failed "
				"(details in TweaksV.log). The mods folder, OPEN archives and newmods are off this session.");
			return FALSE; // TweaksV: TweaksV's DllMain shows the start-up summary instead
		}
	}
	return TRUE;
}

} // namespace RageOpenV
