// TweaksV: carried over from RageOpenV (src/hooks/CustomDevice.cpp) by Chiheb-Bacha, based on
// ClosedIV by martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "main.h"
#include "TweaksVConflicts.h"	// TweaksV: IsInsideGameImage
#include "TweaksVIni.h"		// TweaksV: NEWMODS, DRY_RUN, DIAGNOSTIC_MODE
#include "TweaksVLog.h"		// TweaksV
#include "TweaksVPatch.h"	// TweaksV: DescribeAddress, CallTarget, StartsWithJump, MakeWritable
#include "TweaksVSummary.h"	// TweaksV: start-up summary

namespace RageOpenV // TweaksV: one namespace per original plugin
{

/*
	List of stuff you can replace with this: https://gist.github.com/martonp96/59f731446c7f17db3f400c2be458c4a4

	How to do it:
	Find something from the list, eg. common:/data/dlclist.xml

	Copy the original dlclist.xml from the files and place it in [your GTA5 folder]/newmods/common/data/dlclist.xml
	Edit the file, add a new entry like <Item>dlcpacks:/mrpd/</Item>
	Place the single player dlc.rpf like [your GTA5 folder]/newmods/dlcpacks/mrpd/dlc.rpf

	Start the game and the modded stuff should be loaded automatically.

	I used this for the example: https://hu.gta5-mods.com/maps/community-mission-row-pd
*/

rage::fiDeviceLocal* platformDevice;
rage::fiDeviceLocal* platformDeviceCRC;
rage::fiDeviceLocal* commonDevice;
rage::fiDeviceLocal* commonDeviceCRC;
rage::fiDeviceLocal* dlcDevice;

bool IsCustomDevice(rage::fiDeviceLocal* dev)
{
	if (dev == platformDevice || dev == platformDeviceCRC || dev == commonDevice || dev == commonDeviceCRC || dev == dlcDevice)
		return true;
	return false;
}

void(*InitialMountOrig)();
void InitialMountHook()
{
	InitialMountOrig();

	// TweaksV: nothing may throw back into the game (B-ROV-21)
	try
	{
	std::error_code error; // TweaksV: the current_path() that does not throw (B-ROV-21)
	std::filesystem::path cwd = std::filesystem::current_path(error) / "newmods/";
	if (error)
	{
		TweaksV::WriteLog("RageOpenV", "newmods: the current directory could not be read - nothing mounted.");
		return;
	}
	cwd.make_preferred();

	logger::write("device", "[%s] Mods path: %s", __FUNCTION__, cwd.string().c_str());

	rage::fiDeviceLocal* rootDevice = new rage::fiDeviceLocal();
	rootDevice->SetPath(cwd.string().c_str(), true, nullptr);

	if (rootDevice->Mount("mods:/"))
		logger::write("device", "[%s] Root device mounted!", __FUNCTION__);

	platformDevice = new rage::fiDeviceLocal();
	platformDeviceCRC = new rage::fiDeviceLocal();
	commonDevice = new rage::fiDeviceLocal();
	commonDeviceCRC = new rage::fiDeviceLocal();
	dlcDevice = new rage::fiDeviceLocal();

	platformDevice->SetPath("mods:/platform/", true, nullptr);
	platformDevice->Mount("platform:/");

	platformDeviceCRC->SetPath("mods:/platform/", true, nullptr);
	platformDeviceCRC->Mount("platformcrc:/");

	commonDevice->SetPath("mods:/common/", true, nullptr);
	commonDevice->Mount("common:/");

	commonDeviceCRC->SetPath("mods:/common/", true, nullptr);
	commonDeviceCRC->Mount("commoncrc:/");

	dlcDevice->SetPath("mods:/dlcpacks/", true, nullptr);
	dlcDevice->Mount("dlcpacks:/");
	}
	catch (...)
	{
		TweaksV::WriteLog("RageOpenV", "newmods: the devices could not be mounted (unexpected exception).");
	}
}

// TweaksV: only the GTA V Enhanced pattern is kept (F-ROV-5)
static memory::func<rage::fiDevice*, const char*, bool> GetDeviceHook("41 57 41 56 41 55 41 54 56 57 55 53 48 81 ec ? ? ? ? 89 d6 49 89 ce");

char (*OpenArchiveOrig)(rage::fiPackfile* a1, const char* path, char smth, int32_t type, __int64 a5);
char OpenArchiveHook(rage::fiPackfile* a1, const char* path, char smth, int32_t type, __int64 a5)
{
	// TweaksV: nothing may throw back into the game; the original is always called
	try
	{
	auto device = GetDeviceHook(path, true);
	if (device && IsCustomDevice((rage::fiDeviceLocal*)device))
	{
		// TweaksV: INVALID_FILE_ATTRIBUTES has every bit set, the directory bit too - a path the
		// device does not have is not a directory
		const uint32_t attributes = device->GetAttributes(path);
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			logger::write("device", "[%s] Archive %s is a directory!", __FUNCTION__, path);
			type = 2;
		}
	}

	logger::write("device", "[%s] Opening archive %s %d %d", __FUNCTION__, path, smth, type);
	}
	catch (...)
	{
	}

	return OpenArchiveOrig(a1, path, smth, type, a5);
}

static memory::InitFuncs CustomDevice([] {
	// TweaksV: the NEWMODS switch (off by default); only the GTA V Enhanced patterns are kept (F-ROV-5)
	if (!TweaksV::GetIniBool("RAGEOPENV_SETTINGS", "NEWMODS", false)) {
		TweaksV::WriteLog("RageOpenV", "NEWMODS=0 - the newmods folder is not mounted.");
		return;
	}

	// TweaksV: without a newmods folder there is nothing to mount - and no reason for hooks that
	// would look for every archive and file in it (the devices are mounted from the current
	// directory, as upstream). DIAGNOSTIC_MODE still checks the patterns (plan Phase E2).
	std::error_code error;
	const std::filesystem::path folder = std::filesystem::current_path(error) / "newmods";
	const bool haveFolder = !error && std::filesystem::is_directory(folder, error);
	if (!haveFolder && !TweaksV::IsDiagnosticMode()) {
		TweaksV::WriteLog("RageOpenV", "No newmods folder in the current directory - nothing to mount.");
		return;
	}

	// TweaksV: pass 1 - the mount call, the archive function and the three game functions the
	// devices use are found and checked before anything is written (B-ROV-01, plan 5.9 R1/R2)
	memory mountCall = memory::scan("e8 ? ? ? ? 48 8d 05 ? ? ? ? 66 c7 44 58 02 00 00 66 89 3d").add(26);
	memory openArchive = memory::scan("41 57 41 56 41 55 41 54 56 57 55 53 48 81 ec ? ? ? ? 45 89 cd 45 89 c6 49 89 d7 48 89 ce 8b 05");
	const uint8_t* mountCallee = TweaksV::CallTarget((void*)mountCall.address);

	std::string problem;
	if (!mountCall.address)
		problem = "the initial mount call was not found in the game code";
	else if (!mountCallee)
		problem = "the initial mount call at " + TweaksV::DescribeAddress((void*)mountCall.address) + " is not a call";
	// TweaksV: a call that leads out of the game has been redirected by another mod (plan 4.4)
	else if (!TweaksV::IsInsideGameImage(mountCallee))
		problem = "the initial mount call at " + TweaksV::DescribeAddress((void*)mountCall.address) +
			" leads outside the game (another mod has redirected it)";
	else if (!openArchive.address)
		problem = "the archive function was not found in the game code";
	else if (!GetDeviceHook.run()) // TweaksV: was searched for by an init function of its own
		problem = "the game's GetDevice was not found in the game code";
	else if (!rage::fiDevice::FindGameFunctions()) // TweaksV: were searched for lazily, inside the game's calls
		problem = "the game's fiDevice::Mount or fiDevice::SetPath was not found in the game code";
	else if (memory::virtual_mem().address == 0) // TweaksV: B-ROV-08
		problem = "no trampoline memory could be allocated near the game";
	if (!problem.empty()) {
		TweaksV::ReportFailure("RageOpenV", "The newmods folder is not mounted this session: " + problem + ".");
		return;
	}
	// TweaksV: a function that already starts with a jump has probably been hooked by another mod -
	// said in the log, and hooked on top, which chains to it (plan 4.4)
	if (TweaksV::StartsWithJump((void*)openArchive.address))
		TweaksV::WriteLog("RageOpenV", "The archive function at " + TweaksV::DescribeAddress((void*)openArchive.address) +
			" already starts with a jump - another mod may have hooked it; RageOpenV's hook goes on top.");
	if (TweaksV::IsDiagnosticMode())
		TweaksV::WriteLog("RageOpenV", "DIAGNOSTIC: initial mount call at " + TweaksV::DescribeAddress((void*)mountCall.address) +
			" -> " + TweaksV::DescribeAddress(mountCallee) + ", archive function at " + TweaksV::DescribeAddress((void*)openArchive.address) +
			", GetDevice at " + TweaksV::DescribeAddress(GetDeviceHook.get()));
	if (!haveFolder) { // TweaksV: only DIAGNOSTIC_MODE gets here without one
		TweaksV::WriteLog("RageOpenV", "No newmods folder in the current directory - the patterns were checked "
			"(DIAGNOSTIC_MODE), nothing is mounted.");
		return;
	}

	if (TweaksV::IsDryRun()) { // TweaksV: DRY_RUN stops after pass 1
		TweaksV::WriteLog("RageOpenV", "DRY_RUN: would hook the initial mount call and the archive function for newmods.");
		return;
	}

	// TweaksV: pass 2 - the original writes. The archive hook is made (nothing written yet) and
	// the mount call made writable before either is switched on; if a step fails, both are undone.
	if (!openArchive.hook(OpenArchiveHook, &OpenArchiveOrig)) {
		TweaksV::ReportFailure("RageOpenV", "The newmods folder is not mounted this session: the archive function "
			"could not be hooked (details in TweaksV.log).");
		return;
	}
	std::vector<uint8_t*> site = { (uint8_t*)mountCall.address };
	std::vector<DWORD> oldProtect;
	if (!TweaksV::MakeWritable(site, 5, oldProtect, "RageOpenV")) {
		openArchive.unhook();
		TweaksV::ReportFailure("RageOpenV", "The newmods folder is not mounted this session: the game code could "
			"not be made writable (details in TweaksV.log).");
		return;
	}
	uint8_t saved[5];
	memcpy(saved, site[0], 5);

	bool ok = memory::apply_hooks();
	if (ok) {
		InitialMountOrig = mountCall.add(1).rip().as<decltype(InitialMountOrig)>();
		ok = mountCall.set_call(InitialMountHook);
		if (!ok)
			memcpy(site[0], saved, 5); // TweaksV: the call is put back as it was
	}
	TweaksV::RestoreProtection(site, 5, oldProtect);
	if (!ok) {
		openArchive.unhook();
		TweaksV::ReportFailure("RageOpenV", "The newmods folder is not mounted this session: the hooks could not be "
			"installed (details in TweaksV.log).");
		return;
	}
	TweaksV::WriteLog("RageOpenV", "The newmods folder is mounted when the game mounts its own files (2 hooks).");
});

} // namespace RageOpenV
