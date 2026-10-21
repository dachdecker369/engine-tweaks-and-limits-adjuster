// SPDX-License-Identifier: GPL-3.0-only
// Test harness for TweaksV: pretends to be GTA5_Enhanced.exe and contains copies of the game
// code TweaksV patches: HeapAdjuster's, StraightToStoryMode's, ModkitLimit's, PackfileLimit's,
// BoxStreamer's and Decals' here, RageOpenV's in fake_game_rov.cpp.
// Prints every patch site after TweaksV.asi has loaded.
// Link it with a version resource (fake_game_version.rc) for the game build to pretend to be.
// Usage: GTA5_Enhanced.exe [TweaksV.asi] [options...]
//   prepatched     StraightToStoryMode's patches are already applied (an old .asi ran first)
//   badsplash      unexpected bytes at the splash site
//   badlegal       unexpected bytes at the legal site
//   jnzlegal       a jnz instead of the game's jz at the legal site
//   hookedlanding  the landing page function starts with a jmp (another mod's hook)
//   heapraised     the heap site already holds 2000 MB (another heap adjuster ran first)
//   modkitmoved / modkitsplit / modkitmismatch   see main()
//   packmoved      one packfile array reference points outside the game (another mod moved it)
//   packhooked     the streaming configuration call already leads outside the game
//   packbadsize    the direct size limit holds an implausible 70000
//   packindirect   the indirect size variable holds the array's size itself, 4500, not 4 less (a changed margin)
//   twor31a        a second, identical copy of the 1158.13+ packfile reference (its pattern matches twice)
//   junkr31a       the game's obfuscated code that the old 1158.13+ reference pattern also matched
//   boxmoved       every reference to the box streamer's traversal list points outside the game (another mod moved it)
//   boxsplit       one reference to the list points somewhere else in the game
//   boxninth       a ninth lea loads the list (a changed game)
//   boxmismatch    one capacity of the list holds 1001 instead of 1000
//   boxbadsize     every capacity of the list holds an implausible 200000
//   boxcallhooked  the second tree-search call of the box streamer already leads outside the game (another mod's hook)
//   boxcallsplit   the third tree-search call leads to the other tree search than the first
//   decalmoved     every reference to the decal definitions array points outside the game (another mod moved it)
//   decalsplit     one reference to the array points somewhere else in the game
//   decalextra     a 32nd lea loads the array (a changed game)
//   decalprefix    one lea of the array has a 49 prefix instead of 48
//   decalmismatch  FindById's second compare of the limit holds 261 instead of 260
//   decaloffbyone  the parser's check of the limit holds 260 instead of 259
//   decalbadsize   the parser's check of the limit holds an implausible 131071
//   decalfull      260 decal definitions are registered (the game's own array is full)
//   decalmany      300 decal definitions are registered
//   dstorage       loads the stand-in dstorage.dll (fake_dstorage.cpp) before TweaksV, calls it after
//   dsotherinterface  with dstorage: first asks for a DirectStorage interface TweaksV does not know
//   fileopens      opens files in its folder (and one outside) after TweaksV has started
//   nodyncode      no code may be changed or made once TweaksV loads (Windows' dynamic code policy)
//   lockedlog      while TweaksV starts, holds the TweaksV.log in its folder open as another program
//                  might: for reading, shared for reading only
//   wait           stays 11 seconds longer before it reports
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

// RageOpenV's part of the fake game (fake_game_rov.cpp; its options are listed there).
void RovSetup(int argc, wchar_t** argv);
void RovFirstTimeRequest();
void RovReport();
void RovForEachDataSite(void (*fn)(const volatile unsigned char*, size_t));
void RovForEachCallTarget(void (*fn)(const unsigned char*));

// The copies of the game code live in an executable section, as in the game: TweaksV only searches
// the game's code (build.cmd links this section as ERW).
#pragma data_seg(".fakesit")

// HeapAdjuster: mov edx, 648 MiB / xor r8d,r8d / mov r9b,1 / call ... / lea rcx,...
volatile unsigned char g_heapSite[] = { 0xBA, 0x00, 0x00, 0x80, 0x28, 0x45, 0x31, 0xC0, 0x41, 0xB1, 0x01,
	0xE8, 0x11, 0x22, 0x33, 0x44, 0x48, 0x8D, 0x0D };

// StraightToStoryMode splash: "jz +0x185" at match - 0x0D, then "0F 85 A9 00 00 00 48 8D 54"
volatile unsigned char g_splashSite[] = {
	0x0F, 0x84, 0x85, 0x01, 0x00, 0x00,					// jz rel32 - the patch site
	0x48, 0x8B, 0xCB, 0xE8, 0x55, 0x66, 0x77,			// 7 other bytes
	0x0F, 0x85, 0xA9, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x54 };	// the pattern

// legal: "jz rel32" at match - 0x0F (the game's bytes on 1.0.1158.16), then "00 E9 ? 05 00 00 E8 ? 07"
volatile unsigned char g_legalSite[] = {
	0x0F, 0x84, 0x07, 0x05, 0x00, 0x00,					// jz rel32 - the patch site
	0x48, 0x8D, 0x4C, 0x24, 0x20, 0x33, 0xD2, 0x41, 0xB8,	// 9 other bytes
	0x00, 0xE9, 0x3C, 0x05, 0x00, 0x00, 0xE8, 0x9A, 0x07 };	// the pattern

// landing page: call <callee> / mov ecx, [rip+x] / add ecx, imm8 / test al, al
volatile unsigned char g_landingSite[] = { 0xE8, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x0D, 0x10, 0x20, 0x30, 0x40,
	0x83, 0xC1, 0x07, 0x84, 0xC0 };
volatile unsigned char g_landingCallee[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20 };

// ModkitLimit: the game's own modkit array (1024 entries here), the six
// "lea reg, [rip+disp32]" references to it and the eight hard-coded limits.
uint16_t g_oldModkitArray[1024];
volatile unsigned char g_modkitRef1[] = { 0x31, 0xC9, 0x48, 0x8D, 0x05, 0, 0, 0, 0, 0xEB };
volatile unsigned char g_modkitRef2[] = { 0x89, 0xC2, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0xEB };
volatile unsigned char g_modkitRef3[] = { 0x41, 0x0F, 0xB7, 0xC2, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x0F, 0xB7, 0x04, 0x41 };
volatile unsigned char g_modkitRef4[] = { 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x41, 0xB8 };
volatile unsigned char g_modkitRef5[] = { 0x48, 0x8D, 0x3D, 0, 0, 0, 0, 0x0F, 0xB7, 0x3C, 0x47 };
volatile unsigned char g_modkitRef6[] = { 0x77, 0x10, 0x48, 0x8D, 0x15, 0, 0, 0, 0, 0x0F, 0xB7, 0x14, 0x42 };
struct Site { volatile unsigned char* bytes; int offset; size_t size; };
#define SITE(a, off) { a, off, sizeof(a) }
static Site g_modkitRefs[] = { SITE(g_modkitRef1, 5), SITE(g_modkitRef2, 5), SITE(g_modkitRef3, 7),
	SITE(g_modkitRef4, 8), SITE(g_modkitRef5, 3), SITE(g_modkitRef6, 5) };

volatile unsigned char g_modkitLimit1[] = { 0x0F, 0xB7, 0xC0, 0x66, 0xBA, 0xFF, 0xFF, 0x3D, 0xFF, 0x03, 0x00, 0x00, 0x77 };	// size-1
volatile unsigned char g_modkitLimit2[] = { 0x48, 0x83, 0xC5, 0x04, 0x48, 0x83, 0xC0, 0x04, 0x48, 0x83, 0xC7, 0x04, 0x48, 0x81, 0xFD, 0x03, 0x04, 0x00, 0x00 };	// size+3
volatile unsigned char g_modkitLimit3[] = { 0x74, 0x10, 0x48, 0x83, 0xC5, 0x04, 0x48, 0x83, 0xC3, 0x04, 0x48, 0x83, 0xC0, 0x04, 0x48, 0x3D, 0x03, 0x04, 0x00, 0x00 };	// size+3
volatile unsigned char g_modkitLimit4[] = { 0x48, 0x83, 0xC7, 0x04, 0x48, 0x83, 0xC2, 0x04, 0x48, 0x83, 0xC5, 0x04, 0x48, 0x81, 0xFD, 0x03, 0x04, 0x00, 0x00 };	// size+3
volatile unsigned char g_modkitLimit5[] = { 0x48, 0x83, 0xC7, 0x04, 0x48, 0x83, 0xC2, 0x04, 0x48, 0x83, 0xC6, 0x04, 0x48, 0x81, 0xFE, 0x03, 0x04, 0x00, 0x00 };	// size+3
volatile unsigned char g_modkitLimit6[] = { 0x41, 0xB8, 0x00, 0x08, 0x00, 0x00, 0xB2, 0x00, 0xE8, 0x11, 0x22, 0x33, 0x44, 0x66, 0x0F, 0x76, 0xC0 };	// size*2
volatile unsigned char g_modkitLimit7[] = { 0x0F, 0xB7, 0x41, 0x10, 0x66, 0xBF, 0xFF, 0xFF, 0x48, 0x3D, 0xFF, 0x03, 0x00, 0x00 };	// size-1
volatile unsigned char g_modkitLimit8[] = { 0x41, 0x0F, 0xB7, 0xCA, 0x66, 0xB8, 0xFF, 0xFF, 0x81, 0xF9, 0xFF, 0x03, 0x00, 0x00 };	// size-1
static Site g_modkitLimits[] = { SITE(g_modkitLimit1, 8), SITE(g_modkitLimit2, 15), SITE(g_modkitLimit3, 16), SITE(g_modkitLimit4, 15),
	SITE(g_modkitLimit5, 15), SITE(g_modkitLimit6, 2), SITE(g_modkitLimit7, 10), SITE(g_modkitLimit8, 10) };

// PackfileLimit: the game's own packfile array (4500 entries here), its two size
// limits, the call that reads the streaming configuration and the references to the array. Every
// site ends in CC so no pattern can match across two neighbouring arrays.
void* g_oldPackfileArray[4500];
volatile uint32_t g_packIndirectSize = 4496;	// the variable "cmp eax, [rip+X]" reads: the array's size minus 4, as the game's own
// direct size "cmp rcx, 4500", one per game build
volatile unsigned char g_packD3[] = { 0x48, 0x83, 0xC6, 0x08, 0x48, 0x81, 0xF9, 0x94, 0x11, 0x00, 0x00, 0x75, 0xE0, 0xCC };			// >= 1.0.1158.13, +7
volatile unsigned char g_packD2[] = { 0x49, 0x83, 0xC3, 0x08, 0x48, 0x81, 0xF9, 0x94, 0x11, 0x00, 0x00, 0x75, 0xE0, 0xEB, 0x05, 0xCC };	// >= 1.0.1013.17, +7
volatile unsigned char g_packD1[] = { 0x48, 0x81, 0xF9, 0x94, 0x11, 0x00, 0x00, 0x74, 0x10, 0x48, 0x83, 0x3C, 0xCA, 0x00, 0xCC };		// older, +3
static Site g_packDirect[] = { SITE(g_packD3, 7), SITE(g_packD2, 7), SITE(g_packD1, 3) };
// indirect size: cmp eax, [rip+X] / jge rel32 / mov eax, eax
volatile unsigned char g_packI1[] = { 0x3B, 0x05, 0, 0, 0, 0, 0x0F, 0x8D, 0x20, 0x01, 0x00, 0x00, 0x89, 0xC0, 0xCC };
// the streaming configuration call: mov [rax+8], ecx / lea rcx, [rip+x] / mov [rax], rcx / mov rcx, rax / call rel32
volatile unsigned char g_packH1[] = { 0x89, 0x48, 0x08, 0x48, 0x8D, 0x0D, 0x11, 0x22, 0x33, 0x44, 0x48, 0x89, 0x08, 0x48, 0x89, 0xC1,
	0xE8, 0, 0, 0, 0, 0xCC };
// "data" references (reads of the array)
volatile unsigned char g_packR01[] = { 0x48, 0x8D, 0x15, 0, 0, 0, 0, 0x48, 0x8B, 0x3C, 0xCA, 0x48, 0x8B, 0x2F, 0xCC };
volatile unsigned char g_packR02[] = { 0x48, 0x8D, 0x3D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0xCF, 0x48, 0x85, 0xC9, 0xCC };
volatile unsigned char g_packR03[] = { 0x48, 0x8D, 0x3D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0x39, 0x48, 0x85, 0xC9, 0xCC };
volatile unsigned char g_packR04[] = { 0x89, 0xC0, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0xC1, 0x48, 0x85, 0xC9, 0x74, 0xCC };
volatile unsigned char g_packR05[] = { 0x89, 0xC0, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0xC1, 0x48, 0x85, 0xC9, 0x0F, 0x84, 0xCC };
volatile unsigned char g_packR06[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x4A, 0x8B, 0x0C, 0xC9, 0x48, 0x85, 0xC9, 0x0F, 0x84, 0x10, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x01, 0xCC };
volatile unsigned char g_packR07[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x1C, 0x08, 0x48, 0x8B, 0x03, 0xCC };
volatile unsigned char g_packR08[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x4A, 0x8B, 0x0C, 0xC1, 0x48, 0x85, 0xC9, 0xCC };
volatile unsigned char g_packR09[] = { 0x48, 0x83, 0xEC, 0x20, 0x48, 0x89, 0xCE, 0x48, 0x8D, 0x05, 0x10, 0x20, 0x30, 0x00, 0x48, 0x89, 0x01, 0xC6, 0x81,
	0x00, 0x01, 0x00, 0x00, 0x00,											// 19..23 rest of the mov byte
	0x48, 0xC7, 0x81, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90,	// 24..35
	0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0xCC };									// 36 lea, rel32 at +39
volatile unsigned char g_packR10[] = { 0x48, 0xC1, 0xE8, 0x10, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0xC1, 0x48, 0x8B, 0x01, 0xCC };
volatile unsigned char g_packR11[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x83, 0x3C, 0xC1, 0x00, 0x40, 0x0F, 0x95, 0xC6, 0xEB, 0xCC };
volatile unsigned char g_packR12[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0x08, 0x48, 0x8B, 0x01, 0xFF, 0x90, 0x10, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x48, 0xCC };
volatile unsigned char g_packR13[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0x08, 0x48, 0x8B, 0x01, 0x4C, 0x8D, 0x44, 0x24, 0xCC };
volatile unsigned char g_packR14[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x4C, 0x8B, 0x24, 0x08, 0x49, 0x8B, 0x04, 0x24, 0x4C, 0x8D, 0x44, 0x24, 0xCC };
volatile unsigned char g_packR15[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0x08, 0x48, 0x85, 0xC9, 0x75, 0xCC };
volatile unsigned char g_packR16[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0xC1, 0x48, 0x85, 0xC9, 0x0F, 0x84, 0x10, 0x01, 0x00, 0x00, 0x48, 0x89, 0xF5, 0xCC };
volatile unsigned char g_packR17[] = { 0x48, 0x83, 0xEC, 0x20, 0x89, 0xD7, 0x48, 0x89, 0xCE, 0x48, 0x8D, 0x05, 0x10, 0x20, 0x30, 0x00, 0x48, 0x89, 0x01, 0xC6, 0x81, 0xC1,
	0x01, 0x00, 0x00, 0x00,													// 22..25 rest of the mov byte
	0x48, 0xC7, 0x81, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90,	// 26..37
	0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0xCC };									// 38 lea, rel32 at +41
volatile unsigned char g_packR18[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x3C, 0x08, 0x48, 0x8B, 0x07, 0x48, 0x89, 0xF9, 0xCC };
volatile unsigned char g_packR19[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0x08, 0x48, 0x8B, 0x01, 0xFF, 0x90, 0x10, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x40, 0xCC };
volatile unsigned char g_packR20[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0xD1, 0x48, 0x85, 0xC9, 0x75, 0xCC };
volatile unsigned char g_packR21[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x4C, 0x8B, 0x24, 0xC1, 0x4D, 0x85, 0xE4, 0x0F, 0x84, 0xCC };
volatile unsigned char g_packR22[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0xC1, 0x48, 0x8B, 0x01, 0xFF, 0x90, 0x10, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x38, 0xCC };
// "write" references: mov [rip+X], reg / mov ecx, imm32 / mov edx, imm32 / call rel32 / mov ecx, imm32 / ...
#define PACK_WRITE(reg) 0x48, 0x89, reg, 0, 0, 0, 0, 0xB9, 0x01, 0, 0, 0, 0xBA, 0x02, 0, 0, 0, 0xE8, 0x11, 0x22, 0x33, 0x44, 0xB9, 0x03, 0, 0, 0
volatile unsigned char g_packR23[] = { PACK_WRITE(0x2D), 0x48, 0xC7, 0x44, 0x08, 0xCC };
volatile unsigned char g_packR24[] = { PACK_WRITE(0x35), 0x0F, 0x1F, 0x44, 0x00, 0x00, 0xCC };
volatile unsigned char g_packR25[] = { PACK_WRITE(0x3D), 0x66, 0x90, 0xCC };
volatile unsigned char g_packR26[] = { PACK_WRITE(0x35), 0x66, 0x66, 0x66, 0x2E, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCC };
volatile unsigned char g_packR27[] = { PACK_WRITE(0x2D), 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCC };
volatile unsigned char g_packR28[] = { PACK_WRITE(0x3D), 0x66, 0x66, 0x66, 0x66, 0x2E, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCC };
volatile unsigned char g_packR29[] = { PACK_WRITE(0x3D), 0x0F, 0x1F, 0x40, 0x00, 0xCC };
volatile unsigned char g_packR30[] = { PACK_WRITE(0x35), 0x48, 0xC7, 0x44, 0x08, 0xCC };
// the reference that depends on the build
volatile unsigned char g_packR31a[] = { 0xBF, 0x08, 0x00, 0x00, 0x00, 0xBE, 0x10, 0x00, 0x00, 0x00, 0x4C, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };	// >= 1.0.1158.13, +13
volatile unsigned char g_packR31b[] = { 0x41, 0xB8, 0x00, 0x10, 0x00, 0x00, 0xBA, 0x01, 0x00, 0x00, 0x00, 0x41, 0xBB, 0xFF, 0xFF, 0x00, 0x00,
	0x4C, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };																		// older builds, +20
// a second, identical copy of the 1158.13+ reference: its first byte becomes BF only with the option
// "twor31a", its reference points at the array too, so either match moves the same array. With the
// option "junkr31a" it holds the game's obfuscated code that the old pattern "BF ? ? ? ? BE ? ? ? ?
// 4C 8D 05" also matched (1.0.1158.16, RVA 0x582A227) instead.
volatile unsigned char g_packR31aCopy[] = { 0xCC, 0x08, 0x00, 0x00, 0x00, 0xBE, 0x10, 0x00, 0x00, 0x00, 0x4C, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };
static Site g_packRefs[] = { SITE(g_packR01, 3), SITE(g_packR02, 3), SITE(g_packR03, 3), SITE(g_packR04, 5), SITE(g_packR05, 5),
	SITE(g_packR06, 3), SITE(g_packR07, 3), SITE(g_packR08, 3), SITE(g_packR09, 39), SITE(g_packR10, 7), SITE(g_packR11, 3),
	SITE(g_packR12, 3), SITE(g_packR13, 3), SITE(g_packR14, 3), SITE(g_packR15, 3), SITE(g_packR16, 3), SITE(g_packR17, 41),
	SITE(g_packR18, 3), SITE(g_packR19, 3), SITE(g_packR20, 3), SITE(g_packR21, 3), SITE(g_packR22, 3), SITE(g_packR23, 3),
	SITE(g_packR24, 3), SITE(g_packR25, 3), SITE(g_packR26, 3), SITE(g_packR27, 3), SITE(g_packR28, 3), SITE(g_packR29, 3),
	SITE(g_packR30, 3) };
static Site g_packR31aSite = SITE(g_packR31a, 13);
static Site g_packR31bSite = SITE(g_packR31b, 20);
static Site g_packR31aCopySite = SITE(g_packR31aCopy, 13);
static Site g_packIndirectSite = SITE(g_packI1, 2);
static Site g_packCallSite = SITE(g_packH1, 17);	// the rel32 of the E8 at +0x10

// BoxStreamer: the four sites that hand the capacity of the traversal list to the tree search
// ("mov dword ptr [rsp+20h], 1000"), three of them followed by the "lea r13, [rip+disp32]" that loads
// the list, each followed by the "call rel32" to the tree search (DIAGNOSTIC_MODE redirects it), and the
// other five leas that load the list (two "lea r14", three "lea r13"). g_boxRef4 and g_boxRef8 are the
// same bytes, as in the game, where two of the query functions are near-identical copies: only a scan
// for every lea that loads the list finds both. The list itself and the two tree searches are below,
// outside this section, as the game's are.
volatile unsigned char g_boxCap1[] = { 0x48, 0x8B, 0x44, 0x24, 0x40, 0x48, 0x8B, 0x48, 0x08, 0xC7, 0x44, 0x24, 0x20, 0xE8, 0x03, 0x00, 0x00,
	0x48, 0x8D, 0x54, 0x24, 0x60, 0x4C, 0x8D, 0x44, 0x24, 0x70, 0x4D, 0x89, 0xF1, 0xE8, 0, 0, 0, 0, 0xCC };					// imm32 at +13, call at +30
volatile unsigned char g_boxCap2[] = { 0x48, 0x8B, 0x81, 0x08, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x48, 0x08, 0xC7, 0x44, 0x24, 0x20, 0xE8, 0x03, 0x00, 0x00,
	0x4C, 0x8D, 0x2D, 0, 0, 0, 0, 0x48, 0x89, 0x6C, 0x24, 0x38, 0x4D, 0x89, 0xE9, 0xE8, 0, 0, 0, 0, 0xCC };					// imm32 at +15, rel32 at +22, call at +34
volatile unsigned char g_boxCap3[] = { 0x0F, 0x29, 0x44, 0x24, 0x40, 0x48, 0x8B, 0x48, 0x08, 0xC7, 0x44, 0x24, 0x20, 0xE8, 0x03, 0x00, 0x00,
	0x4C, 0x8D, 0x2D, 0, 0, 0, 0, 0x48, 0x8D, 0x54, 0x24, 0x40, 0x48, 0x89, 0x6C, 0x24, 0x30, 0x4C, 0x8D, 0x44, 0x24, 0x50,
	0x4D, 0x89, 0xE9, 0xE8, 0, 0, 0, 0, 0xCC };																				// imm32 at +13, rel32 at +20, call at +42
volatile unsigned char g_boxCap4[] = { 0x48, 0x8B, 0x01, 0x48, 0x8B, 0x48, 0x08, 0xC7, 0x44, 0x24, 0x20, 0xE8, 0x03, 0x00, 0x00,
	0x4C, 0x8D, 0x2D, 0, 0, 0, 0, 0x48, 0x89, 0x6C, 0x24, 0x38, 0x4D, 0x89, 0xE9, 0xE8, 0, 0, 0, 0, 0xCC };					// imm32 at +11, rel32 at +18, call at +30
volatile unsigned char g_boxRef1[] = { 0x31, 0xC9, 0x4C, 0x8D, 0x35, 0, 0, 0, 0, 0x4C, 0x89, 0x64, 0x24, 0x50, 0x4C, 0x89, 0x4C, 0x24, 0x38, 0xEB, 0x14, 0xCC };	// rel32 at +5
volatile unsigned char g_boxRef2[] = { 0x66, 0x89, 0x5F, 0x0A, 0x48, 0x89, 0x2F, 0x0F, 0xB7, 0x77, 0x08, 0x4C, 0x8D, 0x35, 0, 0, 0, 0, 0x8B, 0x54, 0x24, 0x2C, 0xCC };	// rel32 at +14
volatile unsigned char g_boxRef4[] = { 0x44, 0x0F, 0xB6, 0x64, 0x24, 0x37, 0x4C, 0x8D, 0x2D, 0, 0, 0, 0, 0x48, 0x8B, 0x5C, 0x24, 0x40, 0x4C, 0x8B, 0x44, 0x24, 0x38,
	0xE9, 0xEB, 0xFD, 0xFF, 0xFF, 0xCC };	// rel32 at +9
volatile unsigned char g_boxRef6[] = { 0x44, 0x0F, 0xB6, 0x64, 0x24, 0x2F, 0x4C, 0x8D, 0x2D, 0, 0, 0, 0, 0x48, 0x8B, 0x6C, 0x24, 0x38, 0x4C, 0x8B, 0x44, 0x24, 0x30,
	0xE9, 0xEA, 0xFD, 0xFF, 0xFF, 0xCC };	// rel32 at +9
volatile unsigned char g_boxRef8[] = { 0x44, 0x0F, 0xB6, 0x64, 0x24, 0x37, 0x4C, 0x8D, 0x2D, 0, 0, 0, 0, 0x48, 0x8B, 0x5C, 0x24, 0x40, 0x4C, 0x8B, 0x44, 0x24, 0x38,
	0xE9, 0xEB, 0xFD, 0xFF, 0xFF, 0xCC };	// rel32 at +9
// a ninth lea, which the game does not have: its first byte becomes 4C only with the option "boxninth"
volatile unsigned char g_boxRef9[] = { 0xCC, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };	// rel32 at +3
static Site g_boxCaps[] = { SITE(g_boxCap1, 13), SITE(g_boxCap2, 15), SITE(g_boxCap3, 13), SITE(g_boxCap4, 11) };
static Site g_boxRefs[] = { SITE(g_boxRef1, 5), SITE(g_boxRef2, 14), SITE(g_boxCap2, 22), SITE(g_boxRef4, 9), SITE(g_boxCap3, 20),
	SITE(g_boxRef6, 9), SITE(g_boxCap4, 18), SITE(g_boxRef8, 9) };
static Site g_boxRef9Site = SITE(g_boxRef9, 3);
static Site g_boxCalls[] = { SITE(g_boxCap1, 31), SITE(g_boxCap2, 35), SITE(g_boxCap3, 43), SITE(g_boxCap4, 31) };	// the rel32 of each call

// Decals: the game's code that carries the decal definitions limit and loads the array (the array
// itself, with its count 16 bytes before it, is below, outside this section, as the game's is): the
// parser's "full" check, whose mov reads the count; the parser's lea; FindById's head, with three constant
// sites and a lea beyond the pattern; PickVariant's head; the texture override's five sites, two of them
// with a lea; the start-up loop's start (with a lea) and end; the seven identical "add decal" checks and
// the four that differ; and the other 26 leas of the array, which only the scan for its address finds.
// Every site ends in CC.
#define NOP8 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
volatile unsigned char g_decalFull[] = { 0x8B, 0x35, 0, 0, 0, 0, 0x48, 0x81, 0xFE, 0x03, 0x01, 0x00, 0x00, 0x77, 0xAF, 0x48, 0x89, 0xF9, 0xFF, 0x15, 0xCC };	// disp32 at +2 (the count), imm32 at +9
volatile unsigned char g_decalParserLea[] = { 0x4C, 0x8D, 0x2D, 0, 0, 0, 0, 0xF3, 0x44, 0x0F, 0x10, 0x0D, 0, 0, 0, 0, 0xF3, 0x44, 0x0F, 0x10, 0x15, 0xCC };	// rel32 at +3
volatile unsigned char g_decalFindById[] = {
	0x56, 0x85, 0xC9, 0x74, 0x66, 0xC7, 0x02, 0x04, 0x01, 0x00, 0x00, 0x41, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x8B, 0x1D,	// 0: the pattern, imm32 at +7
	0, 0, 0, 0, NOP8,																													// 21: the mov's disp32, then filler
	0x4C, 0x8D, 0x15, 0, 0, 0, 0,																										// 33 (+0x21): lea r10, rel32 at +36
	NOP8, NOP8, NOP8, NOP8, NOP8, 0x90,																									// 40: filler
	0x81, 0x3A, 0x04, 0x01, 0x00, 0x00,																									// 81 (+0x51): cmp dword ptr [rdx], imm32 at +0x53
	NOP8, 0x90,																															// 87: filler
	0x81, 0x3A, 0x04, 0x01, 0x00, 0x00,																									// 96 (+0x60): imm32 at +0x62
	0x0F, 0x92, 0xC0, 0x5E, 0xC3, 0xCC };																								// 102: setc al / pop rsi / ret
volatile unsigned char g_decalPickVariant[] = { 0xB8, 0x04, 0x01, 0x00, 0x00, 0x81, 0xF9, 0x03, 0x01, 0x00, 0x00, 0x77, 0x2E, 0x41, 0x89, 0xD0, 0x8B, 0x05, 0, 0, 0, 0, 0xCC };	// imm32 at +1 (N) and +7 (N-1)
volatile unsigned char g_decalOverrideOne[] = { 0x45, 0x31, 0xC9, 0x41, 0xBB, 0x04, 0x01, 0x00, 0x00, 0x31, 0xC0, 0x45, 0x31, 0xD2, 0xEB, 0x75, 0xCC };	// imm32 at +5
volatile unsigned char g_decalOverrideLoop[] = { 0x41, 0xBB, 0x04, 0x01, 0x00, 0x00, 0x48, 0x8D, 0x3D, 0, 0, 0, 0, 0x31, 0xC0, 0x45, 0x31, 0xD2, 0xCC };	// imm32 at +2, lea rel32 at +9
volatile unsigned char g_decalOverrideCmp1[] = { 0x41, 0x81, 0xFB, 0x04, 0x01, 0x00, 0x00, 0x89, 0xC3, 0x41, 0x0F, 0x44, 0xD9, 0xCC };	// imm32 at +3
volatile unsigned char g_decalOverrideCmpEbp[] = { 0x81, 0xFD, 0x04, 0x01, 0x00, 0x00, 0x89, 0xD8, 0x41, 0x0F, 0x44, 0xC1, 0xCC };	// imm32 at +2
volatile unsigned char g_decalOverrideCmp2[] = { 0x41, 0x81, 0xFB, 0x04, 0x01, 0x00, 0x00, 0x41, 0x0F, 0x44, 0xC1, 0x48, 0x8D, 0x3D, 0, 0, 0, 0, 0xCC };	// imm32 at +3, lea rel32 at +14
volatile unsigned char g_decalLoopStart[] = { 0xB8, 0xD8, 0x07, 0x00, 0x00, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x0F, 0x57, 0xC0, 0xCC };	// the start offset at +1, lea rel32 at +8
volatile unsigned char g_decalLoopEnd[] = { 0x48, 0x05, 0x20, 0x08, 0x00, 0x00, 0x48, 0x3D, 0x18, 0x59, 0x00, 0x00, 0x0F, 0x85, 0xF3, 0xFD, 0xFF, 0xFF, 0xCC };	// the pass at +2, the end at +8
#define DECAL_ADD_SITE { 0xE8, 0, 0, 0, 0, 0x3D, 0x03, 0x01, 0x00, 0x00, 0x0F, 0x8F, 0x58, 0x01, 0x00, 0x00, 0xCC }	// imm32 at +6
volatile unsigned char g_decalAdd[7][17] = { DECAL_ADD_SITE, DECAL_ADD_SITE, DECAL_ADD_SITE, DECAL_ADD_SITE, DECAL_ADD_SITE, DECAL_ADD_SITE, DECAL_ADD_SITE };
volatile unsigned char g_decalAddR8d[] = { 0x44, 0x89, 0xC3, 0x41, 0x81, 0xF8, 0x03, 0x01, 0x00, 0x00, 0x77, 0x95, 0xCC };	// imm32 at +6
volatile unsigned char g_decalAddEsi[] = { 0x89, 0xC6, 0x81, 0xFE, 0x03, 0x01, 0x00, 0x00, 0x0F, 0x87, 0xE6, 0x04, 0x00, 0x00, 0xCC };	// imm32 at +4
volatile unsigned char g_decalAddRel8[] = { 0xE8, 0, 0, 0, 0, 0x3D, 0x03, 0x01, 0x00, 0x00, 0x7F, 0x32, 0xCC };	// imm32 at +6
volatile unsigned char g_decalAddEbx[] = { 0x89, 0xC3, 0x31, 0xC0, 0x81, 0xFB, 0x03, 0x01, 0x00, 0x00, 0x0F, 0x8F, 0x60, 0x01, 0x00, 0x00, 0xCC };	// imm32 at +6
// the other 26 leas of the array, with the registers the game uses (rel32 at +3 of each)
#define DECAL_LEA(rex, modrm) { rex, 0x8D, modrm, 0, 0, 0, 0, 0xCC }
volatile unsigned char g_decalLeas[26][8] = {
	DECAL_LEA(0x48, 0x0D), DECAL_LEA(0x48, 0x05), DECAL_LEA(0x48, 0x0D), DECAL_LEA(0x48, 0x05), DECAL_LEA(0x48, 0x1D), DECAL_LEA(0x48, 0x05),
	DECAL_LEA(0x48, 0x05), DECAL_LEA(0x48, 0x05), DECAL_LEA(0x48, 0x05), DECAL_LEA(0x48, 0x0D), DECAL_LEA(0x48, 0x05), DECAL_LEA(0x48, 0x0D),
	DECAL_LEA(0x48, 0x15), DECAL_LEA(0x48, 0x0D), DECAL_LEA(0x48, 0x35), DECAL_LEA(0x48, 0x1D), DECAL_LEA(0x4C, 0x2D), DECAL_LEA(0x48, 0x1D),
	DECAL_LEA(0x48, 0x15), DECAL_LEA(0x48, 0x15), DECAL_LEA(0x48, 0x05), DECAL_LEA(0x48, 0x05), DECAL_LEA(0x4C, 0x3D), DECAL_LEA(0x48, 0x05),
	DECAL_LEA(0x4C, 0x25), DECAL_LEA(0x4C, 0x1D) };
// a 32nd lea, which the game does not have: its first byte becomes 48 only with the option "decalextra"
volatile unsigned char g_decalLeaExtra[] = { 0xCC, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };	// rel32 at +3
static Site g_decalCountSite = SITE(g_decalFull, 2);
// the 31 leas of the array: the five inside the sites above, then the 26 others (filled in main)
static Site g_decalRefs[31] = { SITE(g_decalParserLea, 3), SITE(g_decalFindById, 36), SITE(g_decalOverrideLoop, 9), SITE(g_decalOverrideCmp2, 14),
	SITE(g_decalLoopStart, 8) };
static Site g_decalLeaExtraSite = SITE(g_decalLeaExtra, 3);
// the 23 sites that carry the limit: 'L' holds N-1, 'N' holds N, 'E' the end of the start-up loop
struct DecalLimit { Site site; char holds; };
static DecalLimit g_decalLimits[23] = {
	{ SITE(g_decalFull, 9), 'L' }, { SITE(g_decalFindById, 7), 'N' }, { SITE(g_decalFindById, 0x53), 'N' }, { SITE(g_decalFindById, 0x62), 'N' },
	{ SITE(g_decalPickVariant, 1), 'N' }, { SITE(g_decalPickVariant, 7), 'L' }, { SITE(g_decalOverrideOne, 5), 'N' },
	{ SITE(g_decalOverrideLoop, 2), 'N' }, { SITE(g_decalOverrideCmp1, 3), 'N' }, { SITE(g_decalOverrideCmpEbp, 2), 'N' },
	{ SITE(g_decalOverrideCmp2, 3), 'N' }, { SITE(g_decalLoopEnd, 8), 'E' },
	{ SITE(g_decalAdd[0], 6), 'L' }, { SITE(g_decalAdd[1], 6), 'L' }, { SITE(g_decalAdd[2], 6), 'L' }, { SITE(g_decalAdd[3], 6), 'L' },
	{ SITE(g_decalAdd[4], 6), 'L' }, { SITE(g_decalAdd[5], 6), 'L' }, { SITE(g_decalAdd[6], 6), 'L' },
	{ SITE(g_decalAddR8d, 6), 'L' }, { SITE(g_decalAddEsi, 4), 'L' }, { SITE(g_decalAddRel8, 6), 'L' }, { SITE(g_decalAddEbx, 6), 'L' } };
#pragma data_seg()

// The game's own traversal list (1000 entries of 8 bytes). The harness reaches it only through this
// pointer, set when the program is loaded: none of its own code may load the list with a lea, which
// TweaksV's scan for the game's loads of the list would count.
static uint64_t g_oldBoxListStorage[1000];
static uint64_t* volatile g_oldBoxList = g_oldBoxListStorage;

// The game's own decal definitions array (260 entries of 80 bytes) with its count 16 bytes before it. The
// harness reaches it only through this pointer, set when the program is loaded: none of its own code may
// load the array with a lea, which TweaksV's scan for the game's loads of it would count.
struct DecalStore { uint32_t count; uint32_t unused[3]; unsigned char entries[260 * 80]; };
alignas(32) static DecalStore g_oldDecalStoreStorage;
static DecalStore* volatile g_oldDecalStore = &g_oldDecalStoreStorage;

// The game's two tree searches behind the box streamer's four calls: the first and the third call lead
// to the first search, the second and the fourth to the second. Each writes the number of entries the
// harness asks for and returns it, and checks that it was given the harness's own arguments. The two
// differ, so that the linker cannot fold them into one.
using BoxTreeSearch = int (*)(void* tree, const float* min, const float* max, uint64_t* list, int capacity);
static uint64_t g_boxSearchList[10000];
static int g_boxSearchWanted = 0, g_boxSearchCallsA = 0, g_boxSearchCallsB = 0, g_boxSearchArgsBad = 0;
static int FakeTreeSearch(void* tree, const float* min, const float* max, uint64_t* list, int capacity)
{
	if (tree != nullptr || !min || !max || list != g_boxSearchList || capacity != 10000)
		g_boxSearchArgsBad++;
	for (int i = 0; i < g_boxSearchWanted && i < capacity; i++)
		list[i] = static_cast<uint64_t>(i);
	return g_boxSearchWanted;
}
static __declspec(noinline) int FakeTreeSearchA(void* tree, const float* min, const float* max, uint64_t* list, int capacity)
{
	g_boxSearchCallsA++;
	return FakeTreeSearch(tree, min, max, list, capacity);
}
static __declspec(noinline) int FakeTreeSearchB(void* tree, const float* min, const float* max, uint64_t* list, int capacity)
{
	g_boxSearchCallsB++;
	return FakeTreeSearch(tree, min, max, list, capacity);
}

// The game's own function behind the streaming configuration call.
static int g_origCalls = 0;
static uint32_t g_origSawCount = 0;
static __declspec(noinline) void OriginalSetStreamingInterface(char* si)
{
	g_origCalls++;
	g_origSawCount = *reinterpret_cast<uint32_t*>(si + 24);
}

static void SetRel32(const Site& site, const void* target)
{
	auto field = const_cast<unsigned char*>(site.bytes + site.offset);
	const int32_t rel = static_cast<int32_t>(reinterpret_cast<intptr_t>(target) - (reinterpret_cast<intptr_t>(field) + 4));
	memcpy(field, &rel, 4);
}

static const unsigned char* Rel32Target(const Site& site)
{
	auto field = const_cast<const unsigned char*>(site.bytes + site.offset);
	int32_t rel = 0;
	memcpy(&rel, field, 4);
	return field + 4 + rel;
}

static uint32_t Imm32(const Site& site)
{
	uint32_t value = 0;
	memcpy(&value, const_cast<const unsigned char*>(site.bytes + site.offset), 4);
	return value;
}

static void SetImm32(const Site& site, uint32_t value)
{
	memcpy(const_cast<unsigned char*>(site.bytes + site.offset), &value, 4);
}

static const unsigned char* ImageBase()
{
	return reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
}

static const unsigned char* ImageEnd()
{
	const auto base = ImageBase();
	const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
	return base + nt->OptionalHeader.SizeOfImage;
}

static bool InImage(const void* p)
{
	return p >= ImageBase() && p < ImageEnd();
}

// "RW, 128 KB" for a new array outside the game
static void PrintNewArray(const unsigned char* array)
{
	MEMORY_BASIC_INFORMATION info{};
	VirtualQuery(array, &info, sizeof(info));
	const SIZE_T usable = info.RegionSize - (array - static_cast<const unsigned char*>(info.BaseAddress));
	wprintf(L"%ls, %llu KB", info.Protect == PAGE_READWRITE ? L"RW" : info.Protect == PAGE_EXECUTE_READWRITE ? L"RWX" : L"other",
		static_cast<unsigned long long>(usable / 1024));
}

// "game array", "new array (...)" or "mixed: ..."
static void PrintModkitRefs()
{
	const unsigned char* first = Rel32Target(g_modkitRefs[0]);
	bool same = true;
	for (const Site& site : g_modkitRefs)
		same = same && Rel32Target(site) == first;
	if (same && first == reinterpret_cast<const unsigned char*>(g_oldModkitArray))
	{
		wprintf(L"modkit refs: game array | ");
	}
	else if (same && !InImage(first))
	{
		wprintf(L"modkit refs: new array (");
		PrintNewArray(first);
		wprintf(L") | ");
	}
	else
	{
		wprintf(L"modkit refs: mixed | ");
	}
}

static void PrintModkitLimits()
{
	wprintf(L"limits:");
	for (const Site& site : g_modkitLimits)
		wprintf(L" %u", Imm32(site));
	wprintf(L" | ");
}

// "modkit copy: ok" - the new array holds the game's own 1024 entries (main puts a value in the first
// and the last); "game array" when nothing was moved
static void PrintModkitCopy()
{
	const unsigned char* first = Rel32Target(g_modkitRefs[0]);
	if (first == reinterpret_cast<const unsigned char*>(g_oldModkitArray) || InImage(first))
	{
		wprintf(L"modkit copy: game array | ");
		return;
	}
	MEMORY_BASIC_INFORMATION info{};
	if (VirtualQuery(first, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
		static_cast<const unsigned char*>(info.BaseAddress) + info.RegionSize < first + sizeof(g_oldModkitArray))
	{
		wprintf(L"modkit copy: unreadable | ");
		return;
	}
	wprintf(L"modkit copy: %ls | ", memcmp(first, g_oldModkitArray, sizeof(g_oldModkitArray)) == 0 ? L"ok" : L"MISSING");
}

// "pack refs: 30 new, R31a new, R31b game (RW, 512 KB)" - "new" is the common target of the
// first reference when that is outside the game
static void PrintPackfile(int argc, wchar_t** argv);

static void SetBytes(volatile unsigned char* target, const char* bytes, size_t size)
{
	for (size_t i = 0; i < size; i++)
		target[i] = static_cast<unsigned char>(bytes[i]);
}

static void PrintBytes(const char* name, const volatile unsigned char* bytes, size_t size)
{
	wprintf(L"%hs:", name);
	for (size_t i = 0; i < size; i++)
		wprintf(L" %02X", bytes[i]);
	wprintf(L" | ");
}

static bool HasOption(int argc, wchar_t** argv, const wchar_t* option)
{
	for (int i = 2; i < argc; i++)
		if (wcscmp(argv[i], option) == 0)
			return true;
	return false;
}

static const char* RefState(const unsigned char* target, const unsigned char* newArray)
{
	if (target == reinterpret_cast<const unsigned char*>(g_oldPackfileArray))
		return "game";
	if (newArray && target == newArray)
		return "new";
	return "other";
}

static void PrintPackfile(int argc, wchar_t** argv)
{
	const unsigned char* first = Rel32Target(g_packRefs[0]);
	const unsigned char* newArray = InImage(first) ? nullptr : first;
	int moved = 0, game = 0, other = 0;
	for (const Site& site : g_packRefs)
	{
		const unsigned char* target = Rel32Target(site);
		if (target == reinterpret_cast<const unsigned char*>(g_oldPackfileArray))
			game++;
		else if (newArray && target == newArray)
			moved++;
		else
			other++;
	}
	wprintf(L"pack refs: %d new %d game %d other, R31a %hs, R31b %hs", moved, game, other,
		RefState(Rel32Target(g_packR31aSite), newArray), RefState(Rel32Target(g_packR31bSite), newArray));
	if (newArray)
	{
		wprintf(L" (");
		PrintNewArray(newArray);
		wprintf(L")");
	}
	wprintf(L" | pack sizes: D3=%u D2=%u D1=%u I=%u | ", Imm32(g_packDirect[0]), Imm32(g_packDirect[1]), Imm32(g_packDirect[2]),
		g_packIndirectSize);

	// The streaming configuration call: still the game's function, another mod's, or TweaksV's stub.
	const unsigned char* callTarget = Rel32Target(g_packCallSite);
	if (callTarget == reinterpret_cast<const unsigned char*>(&OriginalSetStreamingInterface))
	{
		wprintf(L"hook: not set | ");
	}
	else if (HasOption(argc, argv, L"packhooked") && callTarget == ImageEnd() + 0x20000)
	{
		wprintf(L"hook: other mod's | ");
	}
	else
	{
		// Call it the way the game would, with a streaming configuration whose archive count is 3000.
		alignas(8) char si[64] = {};
		const uint32_t configCount = 3000;
		memcpy(si + 24, &configCount, 4);
		reinterpret_cast<void (*)(char*)>(const_cast<unsigned char*>(callTarget))(si);
		uint32_t after = 0;
		memcpy(&after, si + 24, 4);
		wprintf(L"hook: config 3000 -> %u, original called %dx with %u | ", after, g_origCalls, g_origSawCount);
	}
}

// Calls each of the four tree-search calls' targets the way the game would - TweaksV's stub or the
// game's own tree search; "another mod's" address (option "boxcallhooked") not at all - asking for
// 100, 400, 700 and 1000 entries of a 10000-entry list.
// "box calls: 4 redirected 0 game 0 other, originals 2x+2x, args ok, returns 100 400 700 1000 | "
static wchar_t g_boxCallsResult[160];
static void CallBoxSearches()
{
	const unsigned char* const otherMod = ImageEnd() + 0x40000;
	int redirected = 0, game = 0, other = 0;
	int returns[4] = { -1, -1, -1, -1 };
	const float lower[4] = { -10.0f, -10.0f, -10.0f, 0.0f };
	const float upper[4] = { 10.0f, 10.0f, 10.0f, 0.0f };
	for (int i = 0; i < 4; i++)
	{
		const unsigned char* target = Rel32Target(g_boxCalls[i]);
		if (target == reinterpret_cast<const unsigned char*>(&FakeTreeSearchA) ||
			target == reinterpret_cast<const unsigned char*>(&FakeTreeSearchB))
		{
			game++;
		}
		else if (target == otherMod || InImage(target))
		{
			other++;
			continue;
		}
		else
		{
			redirected++;
		}
		g_boxSearchWanted = 100 + 300 * i;
		returns[i] = reinterpret_cast<BoxTreeSearch>(const_cast<unsigned char*>(target))(nullptr, lower, upper, g_boxSearchList, 10000);
	}
	swprintf(g_boxCallsResult, 160, L"box calls: %d redirected %d game %d other, originals %dx+%dx, args %ls, returns %d %d %d %d | ",
		redirected, game, other, g_boxSearchCallsA, g_boxSearchCallsB, g_boxSearchArgsBad == 0 ? L"ok" : L"BAD",
		returns[0], returns[1], returns[2], returns[3]);
}

// "box refs: 8 new 0 game 0 other (RW, 784 KB) | box caps: 10000 10000 10000 10000 | " - "new" is the
// common target of the first reference when that is outside the game and not the other mod's address
// (option "boxmoved") - then the tree-search calls (above)
static void PrintBoxStreamer()
{
	const unsigned char* const oldList = reinterpret_cast<const unsigned char*>(g_oldBoxList);
	const unsigned char* const otherMod = ImageEnd() + 0x30000;
	const unsigned char* first = Rel32Target(g_boxRefs[0]);
	const unsigned char* newList = InImage(first) || first == otherMod ? nullptr : first;
	int moved = 0, game = 0, other = 0;
	for (const Site& site : g_boxRefs)
	{
		const unsigned char* target = Rel32Target(site);
		if (target == oldList)
			game++;
		else if (newList && target == newList)
			moved++;
		else
			other++;
	}
	wprintf(L"box refs: %d new %d game %d other", moved, game, other);
	if (newList)
	{
		wprintf(L" (");
		PrintNewArray(newList);
		wprintf(L")");
	}
	wprintf(L" | box caps: %u %u %u %u | %ls", Imm32(g_boxCaps[0]), Imm32(g_boxCaps[1]), Imm32(g_boxCaps[2]), Imm32(g_boxCaps[3]),
		g_boxCallsResult);
}

// "decal refs: 31 new 0 game 0 other (RW, 1280 KB) | decal limits: 2047 2048 0x28198, 23 agree | decal count: 0 | " -
// "new" is the common target of the first reference when that is outside the game and not the other
// mod's address (option "decalmoved"); the limits are what the first site of each kind holds (N-1, N,
// the end of the start-up loop) and how many sites differ from the first of their kind
static void PrintDecals()
{
	const unsigned char* const oldArray = g_oldDecalStore->entries;
	const unsigned char* const otherMod = ImageEnd() + 0x50000;
	const unsigned char* first = Rel32Target(g_decalRefs[0]);
	const unsigned char* newArray = InImage(first) || first == otherMod ? nullptr : first;
	int moved = 0, game = 0, other = 0;
	for (const Site& site : g_decalRefs)
	{
		const unsigned char* target = Rel32Target(site);
		if (target == oldArray)
			game++;
		else if (newArray && target == newArray)
			moved++;
		else
			other++;
	}
	wprintf(L"decal refs: %d new %d game %d other", moved, game, other);
	if (newArray)
	{
		wprintf(L" (");
		PrintNewArray(newArray);
		wprintf(L")");
	}
	uint32_t firstOf[3] = {};
	bool seen[3] = {};
	int differ = 0;
	for (const DecalLimit& limit : g_decalLimits)
	{
		const int kind = limit.holds == 'L' ? 0 : limit.holds == 'N' ? 1 : 2;
		const uint32_t value = Imm32(limit.site);
		if (!seen[kind])
		{
			seen[kind] = true;
			firstOf[kind] = value;
		}
		else if (value != firstOf[kind])
			differ++;
	}
	wprintf(L" | decal limits: %u %u 0x%X, ", firstOf[0], firstOf[1], firstOf[2]);
	if (differ == 0)
		wprintf(L"23 agree");
	else
		wprintf(L"%d differ", differ);
	wprintf(L" | decal count: %u | ", g_oldDecalStore->count);
}

// Every site is written once before TweaksV loads, so all their pages are private copies alike; then
// the whole section is made read+execute, as the game's code is (MakeSitesReadExecute). TweaksV must
// make each page it patches writable and give it its own protection back.
static void TouchSite(const Site& site)
{
	for (size_t i = 0; i < site.size; i++)
		site.bytes[i] = site.bytes[i];
}

static void ForEachSite(void (*fn)(const Site&))
{
	static Site singles[] = { SITE(g_heapSite, 0), SITE(g_splashSite, 0), SITE(g_legalSite, 0), SITE(g_landingSite, 0),
		SITE(g_landingCallee, 0), SITE(g_packI1, 0), SITE(g_packH1, 0), SITE(g_packR31a, 0), SITE(g_packR31b, 0),
		SITE(g_packR31aCopy, 0), SITE(g_boxRef9, 0), SITE(g_decalFull, 0), SITE(g_decalParserLea, 0), SITE(g_decalFindById, 0),
		SITE(g_decalPickVariant, 0), SITE(g_decalOverrideOne, 0), SITE(g_decalOverrideLoop, 0), SITE(g_decalOverrideCmp1, 0),
		SITE(g_decalOverrideCmpEbp, 0), SITE(g_decalOverrideCmp2, 0), SITE(g_decalLoopStart, 0), SITE(g_decalLoopEnd, 0),
		SITE(g_decalAddR8d, 0), SITE(g_decalAddEsi, 0), SITE(g_decalAddRel8, 0), SITE(g_decalAddEbx, 0), SITE(g_decalLeaExtra, 0) };
	for (const Site& site : singles) fn(site);
	for (auto& row : g_decalAdd) { Site site = { row, 0, sizeof(row) }; fn(site); }
	for (auto& row : g_decalLeas) { Site site = { row, 0, sizeof(row) }; fn(site); }
	for (const Site& site : g_modkitRefs) fn(site);
	for (const Site& site : g_modkitLimits) fn(site);
	for (const Site& site : g_packDirect) fn(site);
	for (const Site& site : g_packRefs) fn(site);
	for (const Site& site : g_boxCaps) fn(site);
	for (const Site& site : g_boxRefs) fn(site);
	Site indirect = { reinterpret_cast<volatile unsigned char*>(&g_packIndirectSize), 0, sizeof(g_packIndirectSize) };
	fn(indirect);
}

static void MakeSitesReadExecute()
{
	const auto base = const_cast<unsigned char*>(ImageBase());
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, section++)
		if (memcmp(section->Name, ".fakesit", 8) == 0)
		{
			DWORD old = 0;
			VirtualProtect(base + section->VirtualAddress, section->Misc.VirtualSize, PAGE_EXECUTE_READ, &old);
		}
}

// The protection every site had before TweaksV loaded (read+execute); TweaksV must put every page
// it changes back to it.
static DWORD g_siteProtect = 0;
static int g_notRestored = 0;
static DWORD ProtectionOf(const volatile void* p)
{
	MEMORY_BASIC_INFORMATION info{};
	VirtualQuery(const_cast<const void*>(p), &info, sizeof(info));
	return info.Protect;
}
static void TouchData(const volatile unsigned char* bytes, size_t size)
{
	auto p = const_cast<volatile unsigned char*>(bytes);
	for (size_t i = 0; i < size; i++)
		p[i] = p[i];
}
static void CheckData(const volatile unsigned char* bytes, size_t size)
{
	if (ProtectionOf(bytes) != g_siteProtect)
		g_notRestored++;
	if (ProtectionOf(bytes + size - 1) != g_siteProtect)
		g_notRestored++;
}
static void CheckSite(const Site& site)
{
	if (ProtectionOf(site.bytes) != g_siteProtect)
		g_notRestored++;
	if (ProtectionOf(site.bytes + site.size - 1) != g_siteProtect)
		g_notRestored++;
}

// TweaksV's call stubs: every call it redirected leads to one, and none may be writable.
static int g_stubs = 0, g_writableStubs = 0;
static void CheckStub(const unsigned char* callTarget)
{
	MEMORY_BASIC_INFORMATION info{};
	if (InImage(callTarget) || VirtualQuery(callTarget, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT)
		return;	// not redirected, or "another mod's" address, where nothing is
	g_stubs++;
	if (info.Protect != PAGE_EXECUTE_READ)
		g_writableStubs++;
}

// DirectStorage (TweaksV's UNLOCK_FILES and DIAGNOSTIC_MODE): the calls the game makes into dstorage.dll,
// and the factory's OpenFile for 3 archives (one with a full path), 3 other files in the game folder and
// 3 paths outside it.
static wchar_t g_dstorageResult[400];
static void CallDStorage(HMODULE dstorage, bool otherInterfaceFirst)
{
	using GetFactory = HRESULT(WINAPI*)(REFIID, void**);
	using SetConfiguration = HRESULT(WINAPI*)(const void*);
	const auto setConfiguration = reinterpret_cast<SetConfiguration>(GetProcAddress(dstorage, "DStorageSetConfiguration"));
	const auto setConfiguration1 = reinterpret_cast<SetConfiguration>(GetProcAddress(dstorage, "DStorageSetConfiguration1"));
	const auto getFactory = reinterpret_cast<GetFactory>(GetProcAddress(dstorage, "DStorageGetFactory"));
	using FakeOpenFileCalls = long(WINAPI*)();
	const auto openFileCalls = reinterpret_cast<FakeOpenFileCalls>(GetProcAddress(dstorage, "FakeOpenFileCalls"));
	if (!setConfiguration || !setConfiguration1 || !getFactory || !openFileCalls)
	{
		swprintf(g_dstorageResult, 400, L"dstorage: exports missing | ");
		return;
	}
	const uint32_t configuration[8] = { 4, 0xFFFFFFFF, 0, 1, 0, 0, 1, 1 };
	static const GUID factoryId = { 0x6924EA0C, 0xC3CD, 0x4826, { 0xB1, 0x0A, 0xF6, 0x4F, 0x4E, 0xD9, 0x27, 0xC1 } };
	void* factory = nullptr;
	const HRESULT a = setConfiguration(configuration);
	const HRESULT b = setConfiguration1(configuration);
	if (otherInterfaceFirst)	// dsotherinterface: first an interface TweaksV does not know
	{
		static const GUID otherId = { 0x12345678, 0x1234, 0x5678, { 0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78 } };
		void* other = nullptr;
		getFactory(otherId, &other);
	}
	const HRESULT c = getFactory(factoryId, &factory);
	int length = swprintf(g_dstorageResult, 400, L"dstorage: configuration 0x%lX/0x%lX, factory 0x%lX %ls | ",
		static_cast<unsigned long>(a), static_cast<unsigned long>(b), static_cast<unsigned long>(c), factory ? L"set" : L"null");
	if (FAILED(c) || !factory || length < 0)
		return;

	using OpenFileFn = HRESULT(STDMETHODCALLTYPE*)(void*, const wchar_t*, REFIID, void**);
	const auto openFile = reinterpret_cast<OpenFileFn>((*reinterpret_cast<void***>(factory))[4]);
	wchar_t absolute[MAX_PATH + 32] = {};
	GetCurrentDirectoryW(MAX_PATH, absolute);
	wcscat_s(absolute, L"\\update\\update.RPF");
	// outside the game folder, like the game's saves (Documents\...\Profiles\...\SGTA50000), its
	// cloud files in %TEMP% and a network path in its \\?\UNC\ form: these must always reach DirectStorage
	wchar_t save[MAX_PATH + 64] = {};
	wchar_t temp[MAX_PATH + 64] = {};
	GetTempPathW(MAX_PATH, save);
	wcscpy_s(temp, save);
	wcscat_s(save, L"tweaksv_profile\\SGTA50000");
	wcscat_s(temp, L"cloud_tweaksv.dat");
	const wchar_t* paths[9] = { L"x64a.rpf", absolute, L"rpf.cache", L"locktest.txt", L"TweaksV.asi", L"noextension", save, temp,
		L"\\\\?\\UNC\\localhost\\tweaksv_share\\cloud.dat" };
	unsigned long results[9];
	for (int i = 0; i < 9; i++)
	{
		void* file = nullptr;
		results[i] = static_cast<unsigned long>(openFile(factory, paths[i], factoryId, &file));
	}
	swprintf(g_dstorageResult + length, 400 - length, L"open: rpf 0x%lX, RPF 0x%lX, cache 0x%lX, txt 0x%lX, asi 0x%lX, "
		L"none 0x%lX, save 0x%lX, temp 0x%lX, unc 0x%lX, reached %ld | ", results[0], results[1], results[2], results[3], results[4],
		results[5], results[6], results[7], results[8], openFileCalls());
}

// File opens in the game folder (TweaksV's DIAGNOSTIC_MODE): one file 6 times with the same sharing,
// a missing file, the folder itself, a file in a subfolder and a file outside the game folder.
static wchar_t g_fileOpensResult[160];
static void OpenFiles()
{
	HANDLE file = CreateFileW(L"ds_test.txt", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	for (int i = 0; i < 5; i++)
	{
		file = CreateFileW(L"ds_test.txt", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file != INVALID_HANDLE_VALUE)
			CloseHandle(file);
	}
	SetLastError(0);
	file = CreateFileW(L"ds_missing.txt", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	const DWORD missingError = file == INVALID_HANDLE_VALUE ? GetLastError() : 0;
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	file = CreateFileW(L".", FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	CreateDirectoryW(L"ds_sub", nullptr);
	file = CreateFileW(L"ds_sub\\inner.rpf", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	wchar_t outside[MAX_PATH + 32];
	const DWORD length = GetTempPathW(MAX_PATH, outside);
	if (length > 0 && length < MAX_PATH)
	{
		wcscat_s(outside, L"tweaksv_ds_outside.txt");
		file = CreateFileW(outside, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file != INVALID_HANDLE_VALUE)
			CloseHandle(file);
		DeleteFileW(outside);
	}
	DeleteFileW(L"ds_test.txt");
	DeleteFileW(L"ds_sub\\inner.rpf");
	RemoveDirectoryW(L"ds_sub");
	swprintf(g_fileOpensResult, 160, L"file opens: missing file error %lu | ", static_cast<unsigned long>(missingError));
}

int wmain(int argc, wchar_t** argv)
{
	const wchar_t* asi = argc > 1 ? argv[1] : L"TweaksV.asi";

	// The call at the landing site must lead to g_landingCallee.
	const int32_t rel32 = static_cast<int32_t>(reinterpret_cast<intptr_t>(g_landingCallee) -
		(reinterpret_cast<intptr_t>(g_landingSite) + 5));
	memcpy(const_cast<unsigned char*>(g_landingSite) + 1, &rel32, 4);

	if (HasOption(argc, argv, L"prepatched"))
	{
		SetBytes(g_splashSite, "\xE9\x86\x01\x00\x00\x90", 6);
		SetBytes(g_legalSite, "\x90\x90\x90\x90\x90\x90", 6);
		SetBytes(g_landingCallee, "\x31\xC0\xC3", 3);
	}
	if (HasOption(argc, argv, L"badsplash"))
		SetBytes(g_splashSite, "\x48\x89", 2);
	if (HasOption(argc, argv, L"badlegal"))
		SetBytes(g_legalSite, "\x48\x89", 2);
	if (HasOption(argc, argv, L"jnzlegal"))
		SetBytes(g_legalSite, "\x0F\x85", 2);
	if (HasOption(argc, argv, L"hookedlanding"))
		SetBytes(g_landingCallee, "\xE9\x11\x22\x33\x44", 5);
	if (HasOption(argc, argv, L"heapraised"))	// another heap adjuster ran first
	{
		const Site heap = SITE(g_heapSite, 1);
		SetImm32(heap, 2000u << 20);
	}

	// The game's own modkit array holds something in its first and last entry, so the report can tell
	// whether TweaksV copied the game's entries into the new array.
	g_oldModkitArray[0] = 0x1234;
	g_oldModkitArray[1023] = 0x5678;

	// Every modkit reference points at the game's own array...
	for (const Site& site : g_modkitRefs)
		SetRel32(site, g_oldModkitArray);
	if (HasOption(argc, argv, L"modkitmoved"))		// ...unless another mod moved it already
		SetRel32(g_modkitRefs[2], ImageEnd() + 0x10000);
	if (HasOption(argc, argv, L"modkitsplit"))		// ...or one points somewhere else in the game
		SetRel32(g_modkitRefs[4], g_oldModkitArray + 8);
	if (HasOption(argc, argv, L"modkitmismatch"))	// one limit decodes to 1025 instead of 1024
		SetImm32(g_modkitLimits[5], 2050);

	// Every packfile reference points at the game's own array, the indirect size at its variable
	// and the streaming configuration call at the game's function...
	for (const Site& site : g_packRefs)
		SetRel32(site, g_oldPackfileArray);
	SetRel32(g_packR31aSite, g_oldPackfileArray);
	SetRel32(g_packR31bSite, g_oldPackfileArray);
	SetRel32(g_packR31aCopySite, g_oldPackfileArray);
	if (HasOption(argc, argv, L"twor31a"))		// ...and the 1158.13+ pattern matches twice
		g_packR31aCopy[0] = 0xBF;
	if (HasOption(argc, argv, L"junkr31a"))		// ...or the code only the old pattern matched is there
		SetBytes(g_packR31aCopy, "\xBF\x3D\xFC\xE9\xF3\xBE\x1E\x00\x41\x50\x4C\x8D\x05\x47\x06\x55\xFC\xCC", 18);
	SetRel32(g_packIndirectSite, const_cast<uint32_t*>(&g_packIndirectSize));
	SetRel32(g_packCallSite, reinterpret_cast<const void*>(&OriginalSetStreamingInterface));
	if (HasOption(argc, argv, L"packmoved"))		// ...unless another mod moved the array already
		SetRel32(g_packRefs[5], ImageEnd() + 0x10000);
	if (HasOption(argc, argv, L"packhooked"))		// ...or hooked the call
		SetRel32(g_packCallSite, ImageEnd() + 0x20000);
	if (HasOption(argc, argv, L"packbadsize"))		// an implausible direct size
		for (const Site& site : g_packDirect)
			SetImm32(site, 70000);
	if (HasOption(argc, argv, L"packindirect"))	// the variable holds the size itself, not 4 less
		g_packIndirectSize = 4500;

	// Every reference to the box streamer's traversal list points at the game's own list, and every
	// capacity holds its 1000 entries...
	for (const Site& site : g_boxRefs)
		SetRel32(site, g_oldBoxList);
	SetRel32(g_boxRef9Site, g_oldBoxList);
	if (HasOption(argc, argv, L"boxmoved"))		// ...unless another mod moved the list already
		for (const Site& site : g_boxRefs)
			SetRel32(site, ImageEnd() + 0x30000);
	if (HasOption(argc, argv, L"boxsplit"))		// ...or one points somewhere else in the game
		SetRel32(g_boxRefs[1], g_oldBoxList + 8);
	if (HasOption(argc, argv, L"boxninth"))		// ...or a ninth lea loads the list
		g_boxRef9[0] = 0x4C;
	if (HasOption(argc, argv, L"boxmismatch"))	// one capacity holds 1001 instead of 1000
		SetImm32(g_boxCaps[2], 1001);
	if (HasOption(argc, argv, L"boxbadsize"))	// an implausible capacity
		for (const Site& site : g_boxCaps)
			SetImm32(site, 200000);
	// ...and the four tree-search calls lead to the game's two tree searches, in pairs...
	SetRel32(g_boxCalls[0], reinterpret_cast<const void*>(&FakeTreeSearchA));
	SetRel32(g_boxCalls[1], reinterpret_cast<const void*>(&FakeTreeSearchB));
	SetRel32(g_boxCalls[2], reinterpret_cast<const void*>(&FakeTreeSearchA));
	SetRel32(g_boxCalls[3], reinterpret_cast<const void*>(&FakeTreeSearchB));
	if (HasOption(argc, argv, L"boxcallhooked"))	// ...unless another mod hooked one of them already
		SetRel32(g_boxCalls[1], ImageEnd() + 0x40000);
	if (HasOption(argc, argv, L"boxcallsplit"))	// ...or a pair leads to two different functions
		SetRel32(g_boxCalls[2], reinterpret_cast<const void*>(&FakeTreeSearchB));

	// Every reference to the decal definitions array points at the game's own array, 16 bytes after its
	// count, and every site that carries the limit holds the game's 260...
	for (size_t i = 0; i < 26; i++)
	{
		Site lea = { g_decalLeas[i], 3, sizeof(g_decalLeas[i]) };
		g_decalRefs[5 + i] = lea;
	}
	unsigned char* const decalArray = g_oldDecalStore->entries;
	for (const Site& site : g_decalRefs)
		SetRel32(site, decalArray);
	SetRel32(g_decalLeaExtraSite, decalArray);
	SetRel32(g_decalCountSite, &g_oldDecalStore->count);
	if (HasOption(argc, argv, L"decalmoved"))		// ...unless another mod moved the array already
		for (const Site& site : g_decalRefs)
			SetRel32(site, ImageEnd() + 0x50000);
	if (HasOption(argc, argv, L"decalsplit"))		// ...or one lea points somewhere else in the game
		SetRel32(g_decalRefs[6], decalArray + 80);
	if (HasOption(argc, argv, L"decalextra"))		// ...or a 32nd lea loads it
		g_decalLeaExtra[0] = 0x48;
	if (HasOption(argc, argv, L"decalprefix"))		// ...or one lea has a prefix the game's never have
		g_decalLeas[0][0] = 0x49;
	if (HasOption(argc, argv, L"decalmismatch"))	// FindById's second compare holds 261 instead of 260
		SetImm32(g_decalLimits[2].site, 261);
	if (HasOption(argc, argv, L"decaloffbyone"))	// the parser's check holds 260 instead of 259: N would be 261
		SetImm32(g_decalLimits[0].site, 260);
	if (HasOption(argc, argv, L"decalbadsize"))		// the parser's check implies an implausible 131072
		SetImm32(g_decalLimits[0].site, 131071);
	if (HasOption(argc, argv, L"decalfull"))		// the game's own array is full
		g_oldDecalStore->count = 260;
	if (HasOption(argc, argv, L"decalmany"))		// more than the game's own array holds
		g_oldDecalStore->count = 300;

	RovSetup(argc, argv);
	ForEachSite(TouchSite);
	RovForEachDataSite(TouchData);
	MakeSitesReadExecute();
	g_siteProtect = ProtectionOf(g_heapSite);

	// the stand-in dstorage.dll, loaded before TweaksV as the game's import is
	const HMODULE dstorage = HasOption(argc, argv, L"dstorage") ? LoadLibraryW(L"dstorage.dll") : nullptr;
	if (HasOption(argc, argv, L"nodyncode"))	// from here on no code may be changed or made (Windows' dynamic code policy)
	{
		PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy{};
		policy.ProhibitDynamicCode = 1;
		if (!SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &policy, sizeof(policy)))
			wprintf(L"dynamic code policy not set: %lu | ", GetLastError());
	}
	const HANDLE lockedLog = HasOption(argc, argv, L"lockedlog") ? CreateFileW(L"TweaksV.log", GENERIC_READ,
		FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;
	if (HasOption(argc, argv, L"lockedlog"))
		wprintf(L"log %ls | ", lockedLog != INVALID_HANDLE_VALUE ? L"held" : L"NOT held");
	HMODULE module = LoadLibraryW(asi);
	if (lockedLog != INVALID_HANDLE_VALUE)
		CloseHandle(lockedLog);
	if (!module)
	{
		wprintf(L"LoadLibrary(%ls) failed: %lu\n", asi, GetLastError());
		return 1;
	}
	RovFirstTimeRequest();	// the game asks for the time: RageOpenV's hook runs its one-time start-up
	Sleep(1500); // the start-up summary thread runs
	CallBoxSearches();	// the box streamer searches, as the game would call them
	if (dstorage)
		CallDStorage(dstorage, HasOption(argc, argv, L"dsotherinterface"));
	if (HasOption(argc, argv, L"dsotherinterface"))
		Sleep(1500);	// the problem after the start-up summary is shown (or logged) a second after it is reported
	if (HasOption(argc, argv, L"fileopens"))
		OpenFiles();
	if (HasOption(argc, argv, L"wait"))
		Sleep(11000);	// TweaksV's DirectStorage summary comes 10 s after the first OpenFile

	unsigned int heap = 0;
	for (int i = 0; i < 4; i++)
		heap |= static_cast<unsigned int>(g_heapSite[1 + i]) << (8 * i);
	wprintf(L"heap: %u MB | ", heap >> 20);
	PrintBytes("splash", g_splashSite, 6);
	PrintBytes("legal", g_legalSite, 6);
	PrintBytes("landing", g_landingCallee, 5);
	PrintModkitRefs();
	PrintModkitLimits();
	PrintModkitCopy();
	ForEachSite(CheckSite);
	RovForEachDataSite(CheckData);
	PrintPackfile(argc, argv);
	PrintBoxStreamer();
	PrintDecals();
	RovReport();
	wprintf(L"%ls%ls", g_dstorageResult, g_fileOpensResult);
	CheckStub(Rel32Target(g_packCallSite));
	for (const Site& site : g_boxCalls)
		CheckStub(Rel32Target(site));
	RovForEachCallTarget(CheckStub);
	wprintf(L"stubs: %d, %ls | ", g_stubs, g_writableStubs == 0 ? L"all RX" : L"NOT all RX");
	wprintf(L"protect: %ls %ls\n", g_notRestored == 0 ? L"all restored" : L"NOT restored",
		g_siteProtect == PAGE_EXECUTE_READ ? L"(RX)" : L"(not RX)");
	return 0;
}
