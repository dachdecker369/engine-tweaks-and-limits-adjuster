// Test harness for TweaksV: pretends to be GTA5_Enhanced.exe and contains copies of the game
// code TweaksV patches (HeapAdjusterEnhanced, StraightToStoryMode, ModkitLimitAdjusterEnhanced
// and PackfileLimitAdjusterEnhanced so far). Prints every patch site after TweaksV.asi has loaded.
// Link it with a version resource (fake_game_version.rc) for the game build to pretend to be.
// Usage: GTA5_Enhanced.exe [TweaksV.asi] [options...]
//   prepatched     StraightToStoryMode's patches are already applied (an old .asi ran first)
//   badsplash      unexpected bytes at the splash site
//   badlegal       unexpected bytes at the legal site
//   jnzlegal       a jnz instead of the game's jz at the legal site
//   hookedlanding  the landing page function starts with a jmp (another mod's hook)
//   modkitmoved / modkitsplit / modkitmismatch   see main()
//   packmoved      one packfile array reference points outside the game (another mod moved it)
//   packhooked     the streaming configuration call already leads outside the game
//   packbadsize    the direct size limit holds an implausible 70000
//   twor31a        a second, identical copy of the 1158.13+ packfile reference (its pattern matches twice)
//   junkr31a       the game's obfuscated code that the old 1158.13+ reference pattern also matched
//   dstorage       loads the stand-in dstorage.dll (fake_dstorage.cpp) before TweaksV, calls it after
//   fileopens      opens files in its folder (and one outside) after TweaksV has started
//   nodyncode      no code may be changed or made once TweaksV loads (Windows' dynamic code policy)
//   wait           stays 11 seconds longer before it reports
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

// RageOpenV's part of the fake game (fake_game_rov.cpp; its options are listed there).
void RovSetup(int argc, wchar_t** argv);
void RovPhase1();
void RovReport();
void RovForEachDataSite(void (*fn)(const volatile unsigned char*, size_t));
void RovForEachCallTarget(void (*fn)(const unsigned char*));

// The copies of the game code live in an executable section, as in the game: TweaksV only searches
// the game's code (build.cmd links this section as ERW).
#pragma data_seg(".fakesit")

// HeapAdjusterEnhanced: mov edx, 648 MiB / xor r8d,r8d / mov r9b,1 / call ... / lea rcx,...
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

// ModkitLimitAdjusterEnhanced: the game's own modkit array (1024 entries here), the six
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

// PackfileLimitAdjusterEnhanced: the game's own packfile array (4500 entries here), its two size
// limits, the call that reads the streaming configuration and the references to the array. Every
// site ends in CC so no pattern can match across two neighbouring arrays.
void* g_oldPackfileArray[4500];
volatile uint32_t g_packIndirectSize = 4500;	// the variable "cmp eax, [rip+X]" reads
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
#pragma data_seg()

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
		SITE(g_packR31aCopy, 0) };
	for (const Site& site : singles) fn(site);
	for (const Site& site : g_modkitRefs) fn(site);
	for (const Site& site : g_modkitLimits) fn(site);
	for (const Site& site : g_packDirect) fn(site);
	for (const Site& site : g_packRefs) fn(site);
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

// DirectStorage (TweaksV's UNLOCK_FILES and LOG_CALLS): the calls the game makes into dstorage.dll,
// and the factory's OpenFile for 3 archives (one with a full path) and 3 other files.
static wchar_t g_dstorageResult[400];
static void CallDStorage(HMODULE dstorage)
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
	// outside the game folder, like the game's saves (Documents\...\Profiles\...\SGTA50000) and its
	// cloud files in %TEMP%: these must always reach DirectStorage
	wchar_t save[MAX_PATH + 64] = {};
	wchar_t temp[MAX_PATH + 64] = {};
	GetTempPathW(MAX_PATH, save);
	wcscpy_s(temp, save);
	wcscat_s(save, L"tweaksv_profile\\SGTA50000");
	wcscat_s(temp, L"cloud_tweaksv.dat");
	const wchar_t* paths[8] = { L"x64a.rpf", absolute, L"rpf.cache", L"locktest.txt", L"TweaksV.asi", L"noextension", save, temp };
	unsigned long results[8];
	for (int i = 0; i < 8; i++)
	{
		void* file = nullptr;
		results[i] = static_cast<unsigned long>(openFile(factory, paths[i], factoryId, &file));
	}
	swprintf(g_dstorageResult + length, 400 - length, L"open: rpf 0x%lX, RPF 0x%lX, cache 0x%lX, txt 0x%lX, asi 0x%lX, "
		L"none 0x%lX, save 0x%lX, temp 0x%lX, reached %ld | ", results[0], results[1], results[2], results[3], results[4],
		results[5], results[6], results[7], openFileCalls());
}

// File opens in the game folder (TweaksV's LOG_FILE_OPENS): one file 6 times with the same sharing,
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
	HMODULE module = LoadLibraryW(asi);
	if (!module)
	{
		wprintf(L"LoadLibrary(%ls) failed: %lu\n", asi, GetLastError());
		return 1;
	}
	RovPhase1();	// the game asks for the time: RageOpenV's hook runs its one-time start-up
	Sleep(1500); // the start-up summary thread runs
	if (dstorage)
		CallDStorage(dstorage);
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
	ForEachSite(CheckSite);
	RovForEachDataSite(CheckData);
	PrintPackfile(argc, argv);
	RovReport();
	wprintf(L"%ls%ls", g_dstorageResult, g_fileOpensResult);
	CheckStub(Rel32Target(g_packCallSite));
	RovForEachCallTarget(CheckStub);
	wprintf(L"stubs: %d, %ls | ", g_stubs, g_writableStubs == 0 ? L"all RX" : L"NOT all RX");
	wprintf(L"protect: %ls %ls\n", g_notRestored == 0 ? L"all restored" : L"NOT restored",
		g_siteProtect == PAGE_EXECUTE_READ ? L"(RX)" : L"(not RX)");
	return 0;
}
