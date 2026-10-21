// SPDX-License-Identifier: GPL-3.0-only
// RageOpenV part of the fake game: copies of the code RageOpenV patches, laid out so its patterns
// match. The functions it hooks with MinHook (and the game functions it calls) are real code in an
// executable section, so the hooks can be called end to end; the call sites it redirects are only
// rewritten, never run.
// Options:
//   rovjump            GetFileTime 1 already starts with a jump (another mod's hook)
//   rovnotstart        the byte before GetFileTime 2 is not padding (the computed start is wrong)
//   rovmissing         GetFileSize 2's pattern is missing
//   rovdecrypt2        the two DecryptHeader2 calls lead to different functions
//   rovredirected      the DecryptHeader call already leads outside the game
//   rovcachepatched    the RPF cache check function already starts with a ret
//   rovflood           20,000 file requests before the usual ones: more than the diagnostic lines' cap lets through
//   rovopenfail        the fake OpenBulk functions fail for a path under mods/, so the hook must fall back to the game's own file
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#pragma section(".fakefn", read, write, execute)
__declspec(allocate(".fakefn")) unsigned char g_fakeCode[0x2000] = { 0xCC };

// What the fake game functions were called with, and what they return: 1, or 0 for a path under mods/
// with the option rovopenfail, as a game function that could not open the file.
static std::vector<std::string> g_fileCalls;
static bool g_openFail = false;
static uint64_t RecordFile(const char* p)
{
	g_fileCalls.push_back(p ? p : "(null)");
	return g_openFail && p && strncmp(p, "mods/", 5) == 0 ? 0 : 1;
}
static uint64_t (*volatile g_recordFile)(const char*) = RecordFile;

// The game's own functions behind the redirected calls.
static int g_decryptCalls = 0, g_decrypt2Calls = 0, g_parseCalls = 0;
static __declspec(noinline) bool FakeFindEncryption(uint32_t) { return false; }
static __declspec(noinline) void FakeDecryptHeader(uint32_t, char*, int) { g_decryptCalls++; }
static __declspec(noinline) void FakeDecryptHeader2(uint32_t, uint32_t, char*, int) { g_decrypt2Calls++; }
static __declspec(noinline) void FakeDecryptHeader2Other(uint32_t, uint32_t, char*, int) { g_decrypt2Calls += 100; }
static __declspec(noinline) bool FakeParseHeader(void*, const char*, bool, void*) { g_parseCalls++; return true; }

// Call sites: only rewritten, never run. They live in the executable section of the other game code
// copies (fake_game_full.cpp), because TweaksV only searches the game's code.
#pragma data_seg(".fakesit")
volatile unsigned char g_rovConsole[] = { 0xFF, 0x15, 0x11, 0x22, 0x33, 0x44, 0x48, 0x85, 0xC0, 0x74, 0x08, 0x48, 0x89, 0xC1, 0x31, 0xD2,
	0xFF, 0x15, 0x55, 0x66, 0x77, 0x08, 0xCC };
volatile unsigned char g_rovCacheSite[] = { 0xE8, 0, 0, 0, 0, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8D, 0x15, 0, 0, 0, 0, 0x48, 0x8D, 0x35, 0xCC };
volatile unsigned char g_rovCacheCallee[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0xCC };
volatile unsigned char g_rovFindEnc[] = { 0xE8, 0, 0, 0, 0, 0x80, 0x7C, 0x24, 0x2E, 0x01, 0x74, 0x05, 0x48, 0x8B, 0x56, 0xCC };
volatile unsigned char g_rovDecrypt[] = { 0xE8, 0, 0, 0, 0, 0x8B, 0x46, 0x10, 0x85, 0xC0, 0x74, 0x05, 0xB9, 0xCC };
volatile unsigned char g_rovDecrypt2a[] = { 0xE8, 0, 0, 0, 0, 0x48, 0x8B, 0x46, 0x10, 0x66, 0xC7, 0x00, 0x00, 0x00, 0xEB, 0x05, 0xCC };
volatile unsigned char g_rovDecrypt2b[] = { 0xE8, 0, 0, 0, 0, 0x48, 0x8B, 0x46, 0x10, 0x8B, 0x4C, 0x24, 0x20, 0x48, 0x8D, 0x14, 0x08, 0xCC };
volatile unsigned char g_rovParse[] = { 0xC6, 0x86, 0xB8, 0x00, 0x00, 0x00, 0x01, 0x48, 0x89, 0xF1, 0x4C, 0x89, 0xFA, 0x45, 0x89, 0xF0,
	0x45, 0x31, 0xC9, 0xE8, 0, 0, 0, 0, 0xCC };												// the call is at +19
#pragma data_seg()

struct CallSite { volatile unsigned char* bytes; int offset; const char* name; };	// offset of the E8
static CallSite g_rovCalls[] = { { g_rovFindEnc, 0, "FindEncryption" }, { g_rovDecrypt, 0, "DecryptHeader" },
	{ g_rovDecrypt2a, 0, "DecryptHeader2 #1" }, { g_rovDecrypt2b, 0, "DecryptHeader2 #2" }, { g_rovParse, 19, "ParseHeader" } };

static void SetCallTarget(volatile unsigned char* site, const void* target)
{
	auto code = const_cast<unsigned char*>(site);
	code[0] = 0xE8;
	const int32_t rel = static_cast<int32_t>(reinterpret_cast<intptr_t>(target) - (reinterpret_cast<intptr_t>(code) + 5));
	memcpy(code + 1, &rel, 4);
}

static const unsigned char* CallTargetOf(const volatile unsigned char* site)
{
	int32_t rel = 0;
	memcpy(&rel, const_cast<const unsigned char*>(site) + 1, 4);
	return const_cast<const unsigned char*>(site) + 5 + rel;
}

// A call stub written by TweaksV::RedirectCalls: jmp [rip+0], the hook function's address right after.
static const void* StubTarget(const unsigned char* code)
{
	if (memcmp(code, "\xFF\x25\x00\x00\x00\x00", 6) != 0)
		return nullptr;
	uint64_t target = 0;
	memcpy(&target, code + 6, 8);
	return reinterpret_cast<const void*>(target);
}

static bool RovInImage(const void* p)
{
	const auto base = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
	return p >= base && p < base + nt->OptionalHeader.SizeOfImage;
}

static const unsigned char* RovOutside()	// an address past the end of the image: "another mod"
{
	const auto base = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
	return base + nt->OptionalHeader.SizeOfImage + 0x30000;
}

// ---- the executable fake functions -------------------------------------------------------------
struct Asm
{
	size_t pos;
	void b(std::initializer_list<int> bytes) { for (int x : bytes) g_fakeCode[pos++] = static_cast<unsigned char>(x); }
	void hex(const char* text)	// "66 c7 01 00 00 ..."
	{
		for (const char* p = text; *p;)
		{
			if (*p == ' ') { ++p; continue; }
			char* end = nullptr;
			g_fakeCode[pos++] = static_cast<unsigned char>(strtoul(p, &end, 16));
			p = end;
		}
	}
	template <typename F> void callPtr(F* volatile* pointer)	// FF 15 disp32 -> call [pointer]
	{
		b({ 0xFF, 0x15 });
		const int32_t disp = static_cast<int32_t>((intptr_t)pointer - (intptr_t)(g_fakeCode + pos + 4));
		memcpy(g_fakeCode + pos, &disp, 4);
		pos += 4;
	}
};

enum : size_t { F_OPENBULK1, F_GETFILETIME1, F_GETFILESIZE1, F_GETATTRIBS1, F_OPENBULK2, F_GETFILETIME2, F_GETFILESIZE2,
	F_GETATTRIBS2 };
static unsigned char* FnStart(size_t index) { return g_fakeCode + index * 0x200 + 0x10; }

// push rsi/rdi/rbx; sub rsp,0x260; record the path in rdx and return what RecordFile returns - the start
// of a computed target
static void EmitSimple(Asm& a)
{
	a.hex("56 57 53 48 81 ec 60 02 00 00");
	a.b({ 0x48, 0x89, 0xD1 });						// mov rcx, rdx
	a.callPtr(&g_recordFile);
	a.hex("48 81 c4 60 02 00 00 5b 5f 5e c3");		// add rsp; pop rbx/rdi/rsi; ret
}

static void BuildFakeCode(bool notStart, bool missing, bool jump)
{
	DWORD old = 0;	// the compiler drops the section's "write" attribute
	VirtualProtect(g_fakeCode, sizeof(g_fakeCode), PAGE_EXECUTE_READWRITE, &old);
	memset(g_fakeCode, 0xCC, sizeof(g_fakeCode));
	Asm a{};

	a.pos = FnStart(F_OPENBULK1) - g_fakeCode;		// push r14/rsi/rdi/rbx; sub rsp,0x268; mov rdi,r8; ...
	a.hex("41 56 56 57 53 48 81 ec 68 02 00 00 4c 89 c7 66 c7 44 24 20 00 00 4c 8d 74 24 30");
	a.b({ 0x48, 0x89, 0xD1 }); a.callPtr(&g_recordFile);
	a.hex("48 81 c4 68 02 00 00 5b 5f 5e 41 5e c3");

	a.pos = FnStart(F_GETATTRIBS1) - g_fakeCode;
	a.hex("41 56 56 57 53 48 81 ec 68 02 00 00 66 c7 44 24 20 00 00 4c 8d 74 24 30");
	a.b({ 0x48, 0x89, 0xD1 }); a.callPtr(&g_recordFile);
	a.hex("48 81 c4 68 02 00 00 5b 5f 5e 41 5e c3");

	a.pos = FnStart(F_OPENBULK2) - g_fakeCode;		// push r15/r14/rsi/rdi/rbx; sub rsp,0x260; mov rbx,rdx; mov rdi,rcx
	a.hex("41 57 41 56 56 57 53 48 81 ec 60 02 00 00 48 89 d3 48 89 cf");
	a.b({ 0x48, 0x89, 0xD9 }); a.callPtr(&g_recordFile);	// mov rcx, rbx
	a.hex("48 81 c4 60 02 00 00 5b 5f 5e 41 5e 41 5f c3");

	a.pos = FnStart(F_GETATTRIBS2) - g_fakeCode;		// ... mov rdi,rdx; ...; mov edx,0x3a
	a.hex("56 57 53 48 81 ec 60 02 00 00 48 89 d7 48 89 ce c7 44 24 3c ff ff ff ff 48 89 d1 ba 3a 00 00 00");
	a.b({ 0x48, 0x89, 0xF9 }); a.callPtr(&g_recordFile);	// mov rcx, rdi
	a.hex("48 81 c4 60 02 00 00 5b 5f 5e c3");

	// The four starts computed back from a pattern inside the function (-0x10D / -0x15D).
	const struct { size_t index; int offset; const char* pattern; } computed[] = {
		{ F_GETFILETIME1, 0x10D, "66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 31 ff 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 00 00 00 00 85 c0" },
		{ F_GETFILESIZE1, 0x10D, "66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 31 ff 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 00 00 00 00 8b 4c 24" },
		{ F_GETFILETIME2, 0x15D, "66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 00 00 00 00 85 c0 74 14 8b 44 24 64 8b 4c 24 68 48 c1 e0 20 48 09 c1 48 89 4c 24 38" },
		{ F_GETFILESIZE2, 0x15D, missing ? "66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 00 00 00 00 85 c0 74 0a 48 8b 44 24 5c 48 89 44 24 39"
		                                 : "66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 00 00 00 00 85 c0 74 0a 48 8b 44 24 5c 48 89 44 24 38" } };
	for (const auto& c : computed)
	{
		a.pos = FnStart(c.index) - g_fakeCode;
		if (jump && c.index == F_GETFILETIME1)
			a.b({ 0xE9, 0, 0, 0, 0 });		// another mod's hook: a jmp at the start (here to the next instruction)
		EmitSimple(a);
		a.pos = FnStart(c.index) - g_fakeCode + c.offset;
		a.hex(c.pattern);
	}
	if (notStart)
		FnStart(F_GETFILETIME2)[-1] = 0x48;

	FlushInstructionCache(GetCurrentProcess(), g_fakeCode, sizeof(g_fakeCode));
}

static bool Has(int argc, wchar_t** argv, const wchar_t* option)
{
	for (int i = 2; i < argc; i++)
		if (wcscmp(argv[i], option) == 0)
			return true;
	return false;
}

static unsigned char g_apiBefore[5];
static bool g_flood = false;

void RovSetup(int argc, wchar_t** argv)
{
	BuildFakeCode(Has(argc, argv, L"rovnotstart"), Has(argc, argv, L"rovmissing"), Has(argc, argv, L"rovjump"));
	g_flood = Has(argc, argv, L"rovflood");
	g_openFail = Has(argc, argv, L"rovopenfail");

	SetCallTarget(g_rovCacheSite, const_cast<unsigned char*>(g_rovCacheCallee));
	if (Has(argc, argv, L"rovcachepatched"))
		g_rovCacheCallee[0] = 0xC3;
	SetCallTarget(g_rovFindEnc, reinterpret_cast<const void*>(&FakeFindEncryption));
	SetCallTarget(g_rovDecrypt, Has(argc, argv, L"rovredirected") ? RovOutside() : reinterpret_cast<const void*>(&FakeDecryptHeader));
	SetCallTarget(g_rovDecrypt2a, reinterpret_cast<const void*>(&FakeDecryptHeader2));
	SetCallTarget(g_rovDecrypt2b, reinterpret_cast<const void*>(Has(argc, argv, L"rovdecrypt2") ? &FakeDecryptHeader2Other : &FakeDecryptHeader2));
	SetCallTarget(g_rovParse + 19, reinterpret_cast<const void*>(&FakeParseHeader));

	memcpy(g_apiBefore, reinterpret_cast<const void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemTimeAsFileTime")), 5);
}

// Every RageOpenV data site, for the page protection check.
void RovForEachDataSite(void (*fn)(const volatile unsigned char*, size_t))
{
	fn(g_rovConsole, sizeof(g_rovConsole)); fn(g_rovCacheSite, sizeof(g_rovCacheSite)); fn(g_rovCacheCallee, sizeof(g_rovCacheCallee));
	fn(g_rovFindEnc, sizeof(g_rovFindEnc)); fn(g_rovDecrypt, sizeof(g_rovDecrypt)); fn(g_rovDecrypt2a, sizeof(g_rovDecrypt2a));
	fn(g_rovDecrypt2b, sizeof(g_rovDecrypt2b)); fn(g_rovParse, sizeof(g_rovParse));
}

// Where every call RageOpenV redirects leads now, for the call stub check.
void RovForEachCallTarget(void (*fn)(const unsigned char*))
{
	for (const CallSite& call : g_rovCalls)
		fn(CallTargetOf(call.bytes + call.offset));
}

static const unsigned char* TimeApi()
{
	return reinterpret_cast<const unsigned char*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemTimeAsFileTime"));
}

// The first GetSystemTimeAsFileTime call after TweaksV.asi has loaded. Whether RageOpenV
// had hooked it is noted first: the hook removes itself once its one-time start-up has run.
static bool g_apiHookedAtFirstRequest = false;
void RovFirstTimeRequest()
{
	g_apiHookedAtFirstRequest = memcmp(TimeApi(), g_apiBefore, 5) != 0;
	FILETIME now{};
	GetSystemTimeAsFileTime(&now);
}

void RovReport()
{
	wprintf(L"rov api hook: %ls | ", g_apiHookedAtFirstRequest ? L"yes" : L"no");
	wprintf(L"rov api hook afterwards: %ls | ", memcmp(TimeApi(), g_apiBefore, 5) != 0 ? L"still there" : L"removed");
	wprintf(L"console: %02X %02X | ", g_rovConsole[16], g_rovConsole[17]);
	wprintf(L"rpf: %02X %02X %02X | ", g_rovCacheCallee[0], g_rovCacheCallee[1], g_rovCacheCallee[2]);

	// The file functions (MinHook writes a jmp at their start).
	int hooked = 0;
	for (size_t i = F_OPENBULK1; i <= F_GETATTRIBS2; i++)	// (rovjump's "other mod" is E9 00 00 00 00)
		if (FnStart(i)[0] == 0xE9 && memcmp(FnStart(i), "\xE9\x00\x00\x00\x00", 5) != 0)
			hooked++;
	wprintf(L"file hooks %d/8", hooked);
	if (hooked == 8)
	{
		if (g_flood)	// 20,000 requests before the usual ones: more than the cap lets through as lines
		{
			for (int i = 0; i < 20000; i++)
				reinterpret_cast<uint64_t (*)(void*, const char*, int64_t*)>(FnStart(F_OPENBULK1))(nullptr, "test/none.txt", nullptr);
			g_fileCalls.clear();
		}
		int redirected = 0, passed = 0;
		for (size_t i = F_OPENBULK1; i <= F_GETATTRIBS2; i++)
		{
			g_fileCalls.clear();
			reinterpret_cast<uint64_t (*)(void*, const char*, int64_t*)>(FnStart(i))(nullptr, "test/file.txt", nullptr);
			if (g_fileCalls.size() == 1 && g_fileCalls[0] == "mods/test/file.txt")
				redirected++;
			g_fileCalls.clear();
			reinterpret_cast<uint64_t (*)(void*, const char*, int64_t*)>(FnStart(i))(nullptr, "test/none.txt", nullptr);
			if (g_fileCalls.size() == 1 && g_fileCalls[0] == "test/none.txt")
				passed++;
		}
		wprintf(L", redirect %d/8, pass %d/8", redirected, passed);
		if (g_openFail)	// the mods file could not be opened: the hook must hand the game's own path to the game's function next
		{
			g_fileCalls.clear();
			reinterpret_cast<uint64_t (*)(void*, const char*, int64_t*)>(FnStart(F_OPENBULK1))(nullptr, "test/file.txt", nullptr);
			const bool fallback = g_fileCalls.size() == 2 && g_fileCalls[0] == "mods/test/file.txt" && g_fileCalls[1] == "test/file.txt";
			wprintf(L", fallback %ls", fallback ? L"ok" : L"no");
		}

		// The same lookup with the game's other spellings, a folder, and paths that must never be
		// redirected: one that climbs out with "..", and one longer than any path TweaksV looks up.
		const auto call = [](const char* path)
		{
			g_fileCalls.clear();
			reinterpret_cast<uint64_t (*)(void*, const char*, int64_t*)>(FnStart(F_OPENBULK1))(nullptr, path, nullptr);
			return g_fileCalls.size() == 1 ? g_fileCalls[0] : std::string("?");
		};
		const std::string longPath = std::string(300, 'a') + "/file.txt";
		const bool spellings = call("TEST\\File.TXT") == "mods/TEST\\File.TXT" && call("/test//file.txt") == "mods//test//file.txt";
		const bool folder = call("test") == "mods/test";
		const bool outside = call("test/../test/file.txt") == "test/../test/file.txt" && call(longPath.c_str()) == longPath;
		wprintf(L", spellings %ls, folder %ls, outside %ls", spellings ? L"ok" : L"no", folder ? L"ok" : L"no", outside ? L"ok" : L"no");
	}
	wprintf(L" | ");

	// The five encryption calls: redirected to a call stub outside the game?
	int redirected = 0;
	const void* hooks[5] = {};
	for (int i = 0; i < 5; i++)
	{
		const unsigned char* target = CallTargetOf(g_rovCalls[i].bytes + g_rovCalls[i].offset);
		if (!RovInImage(target) && target != RovOutside())
			if ((hooks[i] = StubTarget(target)) != nullptr)
				redirected++;
	}
	wprintf(L"open calls %d/5", redirected);
	if (redirected == 5)
	{
		auto findEncryption = reinterpret_cast<bool (*)(uint32_t)>(const_cast<void*>(hooks[0]));
		auto decrypt = reinterpret_cast<void (*)(uint32_t, char*, int)>(const_cast<void*>(hooks[1]));
		auto decrypt2 = reinterpret_cast<void (*)(uint32_t, uint32_t, char*, int)>(const_cast<void*>(hooks[2]));
		auto parse = reinterpret_cast<bool (*)(void*, const char*, bool, void*)>(const_cast<void*>(hooks[4]));
		char table[16] = {};
		bool ok = hooks[2] == hooks[3];
		ok = ok && !findEncryption(0x4E45504F);					// OPEN: not "encrypted" by NG ...
		decrypt(1, table, 16);									// ... and not decrypted
		ok = ok && g_decryptCalls == 0;
		ok = ok && findEncryption(0x0FE00000);					// an NG-encrypted archive ...
		decrypt(1, table, 16);									// ... is decrypted as before
		ok = ok && g_decryptCalls == 1;
		decrypt2(0x4E45504F, 1, table, 16);
		ok = ok && g_decrypt2Calls == 0;
		decrypt2(0x12345678, 1, table, 16);
		ok = ok && g_decrypt2Calls == 1;

		// ParseHeader: a fake fiPackfile (fields 8 bytes further on, as in the game) with four entries.
		alignas(16) unsigned char entries[4][16] = {};
		const uint32_t dirType = 0x7FFFFF00, one = 1;
		memcpy(entries[0] + 4, &dirType, 4);									// a directory
		entries[1][0] = 5; memcpy(entries[1] + 12, &one, 4);					// binary, named, encrypted
		memcpy(entries[2] + 12, &one, 4);										// binary without a name
		entries[3][0] = 7; entries[3][7] = 0x80; memcpy(entries[3] + 12, &one, 4);	// a resource
		alignas(16) unsigned char packfile[0x120] = {};
		char* table2 = reinterpret_cast<char*>(entries);
		const int32_t count = 4;
		memcpy(packfile + 0x20 + 8, &table2, 8);
		memcpy(packfile + 0x28 + 8, &count, 4);
		findEncryption(0x4E45504F);
		ok = ok && parse(packfile, "test.rpf", true, nullptr) && g_parseCalls == 1;
		uint32_t enc1 = 0, enc2 = 0, enc3 = 0, offset = 0;
		memcpy(&enc1, entries[1] + 12, 4); memcpy(&enc2, entries[2] + 12, 4); memcpy(&enc3, entries[3] + 12, 4);
		memcpy(&offset, packfile + 0xB4 + 8, 4);
		ok = ok && enc1 == 0xFEFFFFF && enc2 == 1 && enc3 == 1 && offset == 0xFEFFFFF;
		memcpy(entries[1] + 12, &one, 4);
		memset(packfile + 0xB4 + 8, 0, 4);
		findEncryption(0x0FE00000);											// not OPEN: left alone
		ok = ok && parse(packfile, "test2.rpf", true, nullptr);
		memcpy(&enc1, entries[1] + 12, 4); memcpy(&offset, packfile + 0xB4 + 8, 4);
		ok = ok && enc1 == 1 && offset == 0;
		// an OPEN archive whose header claims more entries than plausible: left alone, said in the log
		const int32_t tooMany = 0x100001;
		memcpy(packfile + 0x28 + 8, &tooMany, 4);
		findEncryption(0x4E45504F);
		ok = ok && parse(packfile, "test3.rpf", true, nullptr);
		memcpy(&enc1, entries[1] + 12, 4); memcpy(&offset, packfile + 0xB4 + 8, 4);
		ok = ok && enc1 == 1 && offset == 0;
		wprintf(L", behaviour %ls", ok ? L"ok" : L"WRONG");
	}
	wprintf(L" | ");
}
