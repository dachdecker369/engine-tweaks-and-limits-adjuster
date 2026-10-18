// TweaksV: carried over from RageOpenV (src/hooks/PackfileEncryptionHooks.cpp) by Chiheb-Bacha,
// based on ClosedIV by martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "main.h"
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVSettings.h"	// TweaksV: OPEN_RPF, DRY_RUN, DIAGNOSTIC_MODE
#include "TweaksVLog.h"		// TweaksV
#include "TweaksVPatch.h"	// TweaksV: DescribeAddress, CallTarget, RedirectCalls
#include "TweaksVSummary.h"	// TweaksV: start-up summary

namespace RageOpenV // TweaksV: one namespace per original plugin
{

static const int OPEN = 0x4E45504F;

// TweaksV: more entries than this in one archive means the header was not read right (B-ROV-06)
static const int32_t kMaxRpfEntries = 0x100000;

// TweaksV: thread_local - FindEncryptionHook sets it and the hooks below read it on the thread
// that opens the archive; one global mixed up archives opened on two threads at once (B-ROV-06)
thread_local uint32_t currentEncryption;
bool FindEncryptionHook(uint32_t encryption)
{
	currentEncryption = encryption;
	return (encryption & 0xFF00000) == 0xFE00000;
}

void(*DecryptHeaderOrig)(uint32_t, char*, int);
void DecryptHeaderHook(uint32_t salt, char* entryTable, int size)
{
	if (currentEncryption == OPEN)
	{
		logger::log("mods", "[%s] not encrypted RPF found", __FUNCTION__);
		return;
	}
	logger::log("mods", "[%s] called", __FUNCTION__);
	DecryptHeaderOrig(salt, entryTable, size);
}

void(*DecryptHeader2Orig)(uint32_t, uint32_t, char*, int);
void DecryptHeader2Hook(uint32_t encryption, uint32_t salt, char* header, int nameTableLen)
{
	if (encryption == OPEN)
	{
		logger::log("mods", "[%s] not encrypted RPF found", __FUNCTION__);
		return;
	}
	logger::log("mods", "[%s] called", __FUNCTION__);
	DecryptHeader2Orig(encryption, salt, header, nameTableLen);
}

bool(*ParseHeaderOrig)(rage::fiPackfile*, const char*, bool, void*);
bool ParseHeaderHook(rage::fiPackfile* a1, const char* name, bool readHeader, void* customHeader)
{
	bool ret = ParseHeaderOrig(a1, name, readHeader, customHeader);
	logger::log("mods", "[%s] parsed header /%s", __FUNCTION__, name);
	if (ret && a1) // TweaksV: + null check
	{
		// fiPackfile on Enhanced is shifted by 0x08
		// TweaksV: always - the edition check around the shift is removed (F-ROV-7)
		a1 = reinterpret_cast<rage::fiPackfile*>(reinterpret_cast<uintptr_t>(a1) + 0x8);
		logger::log("mods", "[%s] fileCount /%d", __FUNCTION__, a1->filesCount);
		// TweaksV: only an OPEN archive is changed, so the entry table of an encrypted one - every
		// archive of the game - is no longer walked (upstream walked it and changed nothing)
		if (currentEncryption != OPEN)
			return ret;
		// TweaksV: only a plausible entry table is walked (B-ROV-06)
		if (!a1->entryTable || a1->filesCount < 0 || a1->filesCount > kMaxRpfEntries)
		{
			logger::write("error", "[%s] %s: implausible entry table (%d entries) - left as it is", __FUNCTION__,
				name ? name : "?", a1->filesCount);
			return ret;
		}
		for (int i = 0; i < a1->filesCount; ++i)
		{
			Entry* v21 = (Entry*)(a1->entryTable + 16 * i);
			if (v21->IsBinary() && v21->bin.nameOffset > 0 && v21->bin.isEncrypted)
				v21->bin.isEncrypted = 0xFEFFFFF;
		}

		a1->currentFileOffset = 0xFEFFFFF;
	}
	return ret;
}

static memory::InitFuncs PackfileEncryptionHooks([] {
	// Allow unencrypted RPFs
	// TweaksV: the OPEN_RPF switch; only the GTA V Enhanced patterns are kept (F-ROV-7)
	if (!TweaksV::IsOn(TweaksV::kOpenRpf)) {
		TweaksV::WriteLog("RageOpenV", "OPEN_RPF=0 - unencrypted (OPEN) RPF archives are not enabled.");
		return;
	}

	// TweaksV: pass 1 - the five call sites are found and checked before anything is written, so
	// they are redirected together or not at all (B-ROV-01/05, plan 5.9 R1/R2)
	memory findEncryption = memory::scan("e8 ? ? ? ? 80 7c 24 2e ? 74 ? 48 8b 56");
	memory decryptHeader = memory::scan("e8 ? ? ? ? 8b 46 ? 85 c0 74 ? b9");
	memory decryptHeader2 = memory::scan("e8 ? ? ? ? 48 8b 46 ? 66 c7 00 00 ? eb");
	memory decryptHeader2b = memory::scan("e8 ? ? ? ? 48 8b 46 ? 8b 4c 24 ? 48 8d 14 08");
	memory parseHeader = memory::scan("c6 86 ? ? ? ? ? 48 89 f1 4c 89 fa 45 89 f0 45 31 c9").add(19);

	// TweaksV: each call with the hook it gets and the variable that keeps the function it called
	// (upstream read it with add(1).rip()); the second DecryptHeader2 call shares the first one's
	struct Site { memory* mem; const char* name; const void* hook; void** original; };
	const Site sites[] = {
		{ &findEncryption, "FindEncryption call", (const void*)&FindEncryptionHook, nullptr },
		{ &decryptHeader, "DecryptHeader call", (const void*)&DecryptHeaderHook, (void**)&DecryptHeaderOrig },
		{ &decryptHeader2, "first DecryptHeader2 call", (const void*)&DecryptHeader2Hook, (void**)&DecryptHeader2Orig },
		{ &decryptHeader2b, "second DecryptHeader2 call", (const void*)&DecryptHeader2Hook, nullptr },
		{ &parseHeader, "ParseHeader call", (const void*)&ParseHeaderHook, (void**)&ParseHeaderOrig } };

	std::string problem;
	for (const Site& site : sites)
	{
		void* address = (void*)site.mem->address;
		const uint8_t* callee = TweaksV::CallTarget(address);
		if (!address)
			problem = std::string("the ") + site.name + " was not found in the game code";
		else if (!callee)
			problem = std::string("the ") + site.name + " at " + TweaksV::DescribeAddress(address) + " is not a call";
		// TweaksV: a call that leads out of the game has been redirected by another mod (plan 4.4)
		else if (!TweaksV::IsInsideGameImage(callee))
			problem = std::string("the ") + site.name + " at " + TweaksV::DescribeAddress(address) +
				" leads outside the game (another mod has redirected it)";
		if (!problem.empty())
			break;
		if (TweaksV::IsDiagnosticMode())
			TweaksV::WriteLog("RageOpenV", "DIAGNOSTIC: the " + std::string(site.name) + " at " + TweaksV::DescribeAddress(address) +
				" -> " + TweaksV::DescribeAddress(callee));
	}
	// TweaksV: the second DecryptHeader2 call is given the first one's original, so both must
	// call the same function (B-ROV-05)
	if (problem.empty() && TweaksV::CallTarget((void*)decryptHeader2.address) != TweaksV::CallTarget((void*)decryptHeader2b.address))
		problem = "the two DecryptHeader2 calls lead to different functions (" +
			TweaksV::DescribeAddress(TweaksV::CallTarget((void*)decryptHeader2.address)) + " and " +
			TweaksV::DescribeAddress(TweaksV::CallTarget((void*)decryptHeader2b.address)) + ")";
	if (!problem.empty()) {
		TweaksV::ReportFailure("RageOpenV", "Unencrypted (OPEN) RPF archives cannot be loaded this session: " + problem + ".");
		return;
	}

	if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN stops after pass 1
		TweaksV::WriteLog("RageOpenV", "DRY_RUN: would redirect the 5 RPF encryption calls.");
		return;
	}

	// TweaksV: pass 2 - the original writes (set_call on each site), now through TweaksV's one call
	// redirector: all five calls or none, each original set before its call can reach a hook
	// (plan 5.9 R1/R3)
	std::vector<TweaksV::CallRedirect> redirects;
	for (const Site& site : sites)
		redirects.push_back({ (uint8_t*)site.mem->address, site.hook, site.name, site.original });
	if (!TweaksV::RedirectCalls(redirects, {}, "RageOpenV")) {
		TweaksV::ReportFailure("RageOpenV", "Unencrypted (OPEN) RPF archives cannot be loaded this session: the calls "
			"could not be redirected (details in TweaksV.log).");
		return;
	}
	TweaksV::WriteLog("RageOpenV", "Unencrypted (OPEN) RPF archives can be loaded (5 calls redirected).");
});

} // namespace RageOpenV
