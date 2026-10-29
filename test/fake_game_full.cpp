// SPDX-License-Identifier: GPL-3.0-only
// Test harness for TweaksV: pretends to be GTA5_Enhanced.exe and contains copies of the game
// code TweaksV patches: HeapAdjuster's, StraightToStoryMode's, ModkitLimit's, PackfileLimit's,
// BoxStreamer's and Decals' here, with the script programs', the Blips part's and the sites of the parts
// measured first, RageOpenV's in fake_game_rov.cpp.
// Prints every patch site after TweaksV.asi has loaded.
// Link it with a version resource (fake_game_version.rc) for the game build to pretend to be.
// Usage: GTA5_Enhanced.exe [TweaksV.asi] [options...]
//   prepatched     StraightToStoryMode's patches are already applied (an old .asi ran first)
//   badsplash      unexpected bytes at the splash site
//   badlegal       unexpected bytes at the legal site
//   jnzlegal       a jnz instead of the game's jz at the legal site
//   hookedlanding  the landing page function starts with a jmp (another mod's hook)
//   twos3          a second copy of the landing page's site (S-E3 matches twice)
//   heapraised     the heap site already holds 2000 MB (another heap adjuster ran first)
//   heaphuge       the heap site already holds 3072 MB, more than HEAP_SIZE allows (another heap adjuster ran first)
//   heapodd        the heap site holds a value that is not a whole number of MB
//   modkitmoved / modkitsplit / modkitmismatch   see main()
//   modkitextra    a seventh lea of the modkit array, which no pattern names (a changed game)
//   packmoved      one packfile array reference points outside the game (another mod moved it)
//   packhooked     the streaming configuration call already leads outside the game
//   packbadsize    the direct size limit holds an implausible 70000
//   packindirect   the indirect size variable holds the array's size itself, 4500, not 4 less (a changed margin)
//   packbigconfig  the streaming configuration the report hands the hooked call holds 20000 archives, not 3000
//   twor31a        a second, identical copy of the 1158.13+ packfile reference (its pattern matches twice)
//   junkr31a       the game's obfuscated code that the old 1158.13+ reference pattern also matched
//   packextra      a 34th reference to the packfile array, a mov no pattern names (a changed game)
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
//   dstorage       loads the stand-in dstorage.dll (fake_dstorage.cpp) before TweaksV, calls it after: the
//                  configuration, the factory and its OpenFile for 19 paths, the files it holds closed again
//   dsotherinterface  with dstorage: first asks for a DirectStorage interface TweaksV does not know, then for the known one
//   dsotherlater   with dstorage: asks for an interface TweaksV does not know after the known one and its files
//   dsotheronly    with dstorage: asks only for an interface TweaksV does not know - no factory TweaksV knows, no file;
//                  stays 1.5 s longer after "wait", for the line TweaksV writes 10 s after that request
//   dsnofactory    with dstorage: never asks for a factory
//   dsflood        with dstorage: 20,001 more OpenFile calls, after "wait" or "waitlong"
//   dsshare        with dstorage: other programs' opens of files the stand-in holds open (ShareThroughStandIn)
//   ownshare       other programs' opens next to the fake's own opens in its folder (ShareOwnOpens)
//   dstypes        read-only opens of 17 files of 17 types in its folder that are not there
//   dsopenflood    opens 100,001 files in its folder that are not there, each by another name, after TweaksV has started
//   dsreplace      with dstorage: a file the stand-in holds open written the four ways from inside the game - the
//                  classic replace, a POSIX rename, ReplaceFile, a write in place - and by the classic replace from
//                  another program, renamer.exe (ReplaceHeldFile)
//   dsrenames      with dstorage: the renames TweaksV's NtSetInformationFile hook passes on and turns, most of them
//                  against a file the stand-in holds open (RenameAsked)
//   dsrenameflood  20,001 classic replace-renames in its folder after TweaksV has started
//   fileopens      opens files in its folder (and one outside, in %TEMP%) after TweaksV has started
//   nodyncode      no code may be changed or made once TweaksV loads (Windows' dynamic code policy)
//   lockedlog      while TweaksV starts, holds the TweaksV.log in its folder open as another program
//                  might: for reading, shared for reading only
//   unload         frees TweaksV.asi once it has started (an ASI loader that unloads it) until it is gone or
//                  8 times, says whether it is still loaded, and reports as usual - which calls the hooked
//                  functions
//   wait           stays 11 seconds longer before it reports
//   waitlong       stays 31 seconds longer before it reports (TweaksV's DirectStorage factory check comes 30 s in)
//   script         places the script program blip_controller once, once TweaksV has started, as the game does: on
//                  a thread of its own, through the call at the hook site - TweaksV's stub when the call is
//                  redirected, the fake's own resolution function when it is not - and writes what it placed to
//                  script_placed_<n>.bin, n counting blip_controller's placements, for the suite to compare with
//   scripttwice    places it twice, the second time with a native the fake's resolution function does not know
//   scriptother    places another program, other_script, before it
//   scripthash     places a program with blip_controller's hash and another name, blip_imposter, twice instead
//   scriptpage     the second page of its code cannot be read
//   scriptmany     places it 17 times
//   scripthuge     places it twice: its code length says 16 MB and a byte the first time, its native count 65537 the
//                  second
//   scripthooked   the call at the hook site already leads outside the game (another mod redirected it); nothing
//                  is placed
//   blipfused      blip_controller's guard (the Blips part) with the fused ILT_JZ in the place of its comparison and
//                  JZ, two NOPs of room before the push, its body behind the branch and its way out a jump: the
//                  polarity the other way round
//   blippadded     the guard with two NOPs after the count
//   bliproom       the guard with IGE and JZ, no NOT: three bytes before the branch
//   blip240        the guard with the threshold 240
//   blipnative     the guard calling WAIT through native 3, not below the program's native count
//   bliptwice      a second guard at 0x800
//   blipjump       a jump at 0x100 into the guard's span
//   blipcall       a CALL at 0x100 into the guard's span
//   blipswitch     a SWITCH at 0x100 whose one case leads into the guard's span
//   blipcatch      a CATCH, whose target is computed while the script runs, at 0x200
//   blipchanged    the guard's threshold changed to 151 while the game resolves the natives, as another mod could
//   blipbadbyte    the byte 0xA0, no instruction, at 0x200
// The blip pool's two sites are always there, the pool with 162 slots in use below its high-water mark, 170, and
// two beyond it.
// The parts measured first (TweaksV's SPECIFICATIONS.md 11.10), one block per limit: its options, and what is always there.
// [weapon infos]
// The weapon infos (WeaponLimits): the game's initializer of the weapon info blob array - its lea of the array, then
// W-E-I1: "mov edx, 160", the call to the allocation, the block's store, "mov word [capacity], 160" -, the allocation's
// first bytes, and the loader, a NOP before its start and W-E-I2 0x18 bytes into it, are always there; the array, in the
// fake's data, is empty while TweaksV starts. Once TweaksV has started, the fake sets the array up as the game's
// initializer does - a block on its heap of as many blobs of 248 bytes as its site's "mov edx" says, the room its
// capacity's store says - and its loader's count at 152.
//   wisetup        the array is set up before TweaksV starts (an initializer that ran before it)
//   wiraised       the array gets room for 512, as if another mod had raised both immediates after TweaksV checked
//                  them, and 170 files are loaded
//   wipacks        packs mounted while the game runs: once TweaksV.log holds the first weapon infos summary, the count
//                  becomes 160 (the array full), once it holds the second, 165 (past its end); then 2.5 s of nothing
//                  new (for the test build's TWEAKSV_TEST_SUMMARY_MS=1000)
//   wimismatch     the capacity holds 161, the number allocated 160
//   wizero         both hold 0
//   wihooked       the call to the allocation leads outside the game (another mod's hook)
//   wibytes        the allocation begins with a jump (E9) instead of its own bytes
//   wioutside      the block's store leads outside the game
//   wicapacity     the capacity's store leads to the array + 0xC instead of + 0xA
//   wicount        the loader's read of the count leads to the array + 0x10 instead of + 8
//   wisize         the loader's blob size is 0x100, the allocation's element size 0xF8
//   winotstart     the byte before the loader's start is 48, so that it is no function start
//   wiextra        a third reference to the array, a second store of a block
// [weapon components]
// The weapon components' sites are always there (the WeaponLimits part's measurement): the rebuild of the list of every
// weapon component definition, its sort and its update of the count, the pool's size call to a stand-in for the game's
// size lookup, and the list's 15 other leas, 17 in all; the list of 470 entries with its count right after it, 412
// definitions from 23 weaponcomponents.meta files, and the files' count beyond it.
//   wcpack         11 seconds after the other actions begin, a pack's weaponcomponents.meta loads: the list is rebuilt
//                  with 430 definitions from 24 files (with "wait", the run then stays 11 seconds more)
//   wcfull         470 definitions from 25 files: the list is full
//   wcover         the list has overflowed: the 471st definition's pointer lies over its count, which reads its low half
//   wcmoved        every lea of the list points outside the game (another mod moved it)
//   wcsplit        one lea of the list points elsewhere in the game: only 16 load it
//   wcextra        an 18th lea loads the list (a changed game)
//   wcprefix       one lea of the list has a 49 prefix instead of 4C
//   wccount        the update's store of the count leads 8 bytes past the count: the count's references disagree
//   wcgap          the count's four references lead 4 bytes past it: not a whole number of entries after the list
//   wchooked       the pool size call already leads outside the game (another mod's hook)
// [siren ids]
// The siren setting IDs (SirenSettings): their 14 sites are always there - the parser registrations of the siren settings
// and the vehicle variations with their tables and the two ID members' data (type 0x01), the XML reader, the base and a
// pack's loops and builders with the four lea of the ID table, the dedupe and its loop, the three constructors, the init's
// last fill of the table and its call of the base loader, the getter - and so are the carcols data with 24 siren settings of
// 0x5E8 bytes and the ID table of 256 entries with their 24 IDs; the options, all for the measurement with DIAGNOSTIC_MODE:
//   sipack        12 s after TweaksV has loaded, a pack's 6 settings appended and their IDs entered; the fake stays to 15 s
//   sinone        the carcols pointer null - no settings loaded yet - until 12 s after TweaksV has loaded; stays to 15 s
//   simany        300 settings, given their IDs as the game's builder gives them: the table full, the ID 255 to the last 45
//   siunreadable  the settings' array in memory that cannot be read
//   siwidened     the base loop's first ID load a word load already (0F B7), as another mod would leave it
//   sitype        the variations' ID member's type 0x03, a word, as another mod would leave it
//   simembers     the variations' registration with an eighth member
//   sioffsets     the siren settings' member 2 at +0x8, not +0x4
//   sidataout     the siren settings' ID member's data pointer leads outside the game
//   simoved       the four lea of the ID table lead outside the game (another mod moved it)
//   sisplit       a pack's builder's lea of the table leads 16 bytes into it
//   sififth       a fifth lea of the table (a changed game)
//   sijz          the dedupe loop's jz leads outside the game
//   sifill        the init's last fill stores 16 bytes below the table's last 16
//   sicallhooked  the base loader call leads outside the game (another mod redirected it)
//   sicarcols     the getter reads another carcols pointer than a pack's builder
// [siren count]
// [ped props]
// [texture budget]
// [streaming memory]
#include <windows.h>
#include <winternl.h>
#include <cstddef>
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
// a second copy of the landing page's site, its call to the same function: its first byte becomes E8 only
// with the option "twos3"
volatile unsigned char g_landingSiteCopy[] = { 0xCC, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x0D, 0x10, 0x20, 0x30, 0x40,
	0x83, 0xC1, 0x07, 0x84, 0xC0, 0xCC };

// ModkitLimit: the six "lea reg, [rip+disp32]" references to the game's own modkit array (below, outside this
// section) and the eight hard-coded limits.
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
// a seventh lea, which the game does not have: its first byte becomes 48 only with the option "modkitextra"
volatile unsigned char g_modkitExtra[] = { 0xCC, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };	// rel32 at +3
static Site g_modkitExtraSite = SITE(g_modkitExtra, 3);

volatile unsigned char g_modkitLimit1[] = { 0x0F, 0xB7, 0xC0, 0x66, 0xBA, 0xFF, 0xFF, 0x3D, 0xFF, 0x03, 0x00, 0x00, 0x77 };	// size-1
volatile unsigned char g_modkitLimit2[] = { 0x48, 0x83, 0xC5, 0x04, 0x48, 0x83, 0xC0, 0x04, 0x48, 0x83, 0xC7, 0x04, 0x48, 0x81, 0xFF, 0x03, 0x04, 0x00, 0x00 };	// size+3
volatile unsigned char g_modkitLimit3[] = { 0x74, 0x10, 0x48, 0x83, 0xC5, 0x04, 0x48, 0x83, 0xC3, 0x04, 0x48, 0x83, 0xC0, 0x04, 0x48, 0x3D, 0x03, 0x04, 0x00, 0x00 };	// size+3
volatile unsigned char g_modkitLimit4[] = { 0x48, 0x83, 0xC7, 0x04, 0x48, 0x83, 0xC2, 0x04, 0x48, 0x83, 0xC5, 0x04, 0x48, 0x81, 0xFD, 0x03, 0x04, 0x00, 0x00 };	// size+3
volatile unsigned char g_modkitLimit5[] = { 0x48, 0x83, 0xC7, 0x04, 0x48, 0x83, 0xC2, 0x04, 0x48, 0x83, 0xC6, 0x04, 0x48, 0x81, 0xFE, 0x03, 0x04, 0x00, 0x00 };	// size+3
volatile unsigned char g_modkitLimit6[] = { 0x41, 0xB8, 0x00, 0x08, 0x00, 0x00, 0xB2, 0x00, 0xE8, 0x11, 0x22, 0x33, 0x44, 0x66, 0x0F, 0x76, 0xC0 };	// size*2
volatile unsigned char g_modkitLimit7[] = { 0x0F, 0xB7, 0x41, 0x10, 0x66, 0xBF, 0xFF, 0xFF, 0x48, 0x3D, 0xFF, 0x03, 0x00, 0x00 };	// size-1
volatile unsigned char g_modkitLimit8[] = { 0x41, 0x0F, 0xB7, 0xCA, 0x66, 0xB8, 0xFF, 0xFF, 0x81, 0xF9, 0xFF, 0x03, 0x00, 0x00 };	// size-1
static Site g_modkitLimits[] = { SITE(g_modkitLimit1, 8), SITE(g_modkitLimit2, 15), SITE(g_modkitLimit3, 16), SITE(g_modkitLimit4, 15),
	SITE(g_modkitLimit5, 15), SITE(g_modkitLimit6, 2), SITE(g_modkitLimit7, 10), SITE(g_modkitLimit8, 10) };

// PackfileLimit: the two size limits of the game's own packfile array (below, outside this section), the
// call that reads the streaming configuration and the references to the array. Every site ends in CC so no
// pattern can match across two neighbouring arrays.
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
// the two more the game has: the load in the function that hashes a collection's entries, "lea r15" (+8),
// and the one in P-E-R20's function, behind its int3 trap ("lea rcx": the pattern at +3, its rel32 at +9)
volatile unsigned char g_packR32[] = { 0x48, 0x89, 0xCF, 0x31, 0xF6, 0x4C, 0x8D, 0x3D, 0, 0, 0, 0, 0xEB, 0x10, 0xCC };
volatile unsigned char g_packR33[] = { 0x48, 0x85, 0xC9, 0x75, 0x0C, 0xCC, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8B, 0x0C, 0xD1,
	0x48, 0x8B, 0x19, 0xCC };
// the reference that depends on the build. A game build has only its own: main makes the other build's one
// no 64-bit lea (its 4C becomes 44), which TweaksV's scan for every reference to the array does not count
volatile unsigned char g_packR31a[] = { 0xBF, 0x08, 0x00, 0x00, 0x00, 0xBE, 0x10, 0x00, 0x00, 0x00, 0x4C, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };	// >= 1.0.1158.13, +13
volatile unsigned char g_packR31b[] = { 0x41, 0xB8, 0x00, 0x10, 0x00, 0x00, 0xBA, 0x01, 0x00, 0x00, 0x00, 0x41, 0xBB, 0xFF, 0xFF, 0x00, 0x00,
	0x4C, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };																		// older builds, +20
// a second, identical copy of the 1158.13+ reference: its first byte becomes BF and its lea a 64-bit one
// (44 becomes 4C) only with the option "twor31a"; its reference points at the array too, so either match
// moves the same array. With the option "junkr31a" it holds the game's obfuscated code that the old pattern
// "BF ? ? ? ? BE ? ? ? ? 4C 8D 05" also matched (1.0.1158.16, RVA 0x582A227) instead.
volatile unsigned char g_packR31aCopy[] = { 0xCC, 0x08, 0x00, 0x00, 0x00, 0xBE, 0x10, 0x00, 0x00, 0x00, 0x44, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };
// a 34th reference, which the game does not have, "mov [rip+X], rax": its first byte becomes 48 only with
// the option "packextra"
volatile unsigned char g_packExtra[] = { 0xCC, 0x89, 0x05, 0, 0, 0, 0, 0xCC };	// rel32 at +3
static Site g_packRefs[] = { SITE(g_packR01, 3), SITE(g_packR02, 3), SITE(g_packR03, 3), SITE(g_packR04, 5), SITE(g_packR05, 5),
	SITE(g_packR06, 3), SITE(g_packR07, 3), SITE(g_packR08, 3), SITE(g_packR09, 39), SITE(g_packR10, 7), SITE(g_packR11, 3),
	SITE(g_packR12, 3), SITE(g_packR13, 3), SITE(g_packR14, 3), SITE(g_packR15, 3), SITE(g_packR16, 3), SITE(g_packR17, 41),
	SITE(g_packR18, 3), SITE(g_packR19, 3), SITE(g_packR20, 3), SITE(g_packR21, 3), SITE(g_packR22, 3), SITE(g_packR23, 3),
	SITE(g_packR24, 3), SITE(g_packR25, 3), SITE(g_packR26, 3), SITE(g_packR27, 3), SITE(g_packR28, 3), SITE(g_packR29, 3),
	SITE(g_packR30, 3), SITE(g_packR32, 8), SITE(g_packR33, 9) };
static Site g_packR31aSite = SITE(g_packR31a, 13);
static Site g_packR31bSite = SITE(g_packR31b, 20);
static Site g_packR31aCopySite = SITE(g_packR31aCopy, 13);
static Site g_packExtraSite = SITE(g_packExtra, 3);
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

// The script programs: the call in a program's resource constructor that resolves its natives ("mov rcx, r12",
// the call, then "mov r14d, 0x3FFF" and "mov eax, [r12+0x70]"), the constructor's loop over the native table,
// which reads the code length at +0x1C and the native table at +0x40, and the script interpreter's prologue,
// which reads the code page table at +0x10. The program itself and the fake's resolution function are below,
// outside this section, as the game's are. Every site ends in CC.
volatile unsigned char g_scriptHookSite[] = { 0x4C, 0x89, 0xE1, 0xE8, 0, 0, 0, 0, 0x41, 0xBE, 0xFF, 0x3F, 0x00, 0x00,
	0x41, 0x8B, 0x44, 0x24, 0x70, 0xCC };																	// the call at +3
volatile unsigned char g_scriptNativeLoop[] = { 0x41, 0x8B, 0x4C, 0x24, 0x1C, 0x01, 0xC1, 0x49, 0x8B, 0x54, 0x24, 0x40, 0x48, 0xD3,
	0x04, 0xC2, 0xCC };
volatile unsigned char g_scriptInterpreter[] = { 0x4D, 0x8B, 0x50, 0x10, 0x49, 0x63, 0x41, 0x1C, 0x4C, 0x8D, 0x7C, 0xC1, 0xF8, 0x49,
	0x63, 0x41, 0x18, 0x4C, 0x8D, 0x0C, 0xC1, 0xCC };
static Site g_scriptCallSite = SITE(g_scriptHookSite, 4);	// the rel32 of the E8 at +3

// The blip pool (the Blips part's measurement): its allocator's "lea rbx, [rip+disp32]" of the pool, the rel32 at
// +10, and its update of the high-water mark, "cmp [rip+disp32], esi" with the disp32 at +5 - then "mov [rip+disp32],
// eax" of the mark, its disp32 at +16. The pool and the mark are below, outside this section, as the game's are.
volatile unsigned char g_blipPoolSite[] = { 0xBA, 0x04, 0x00, 0x00, 0x00, 0x31, 0xC0, 0x48, 0x8D, 0x1D, 0, 0, 0, 0, 0x31, 0xF6, 0xCC };
volatile unsigned char g_blipMarkSite[] = { 0x48, 0x89, 0x3B, 0x39, 0x35, 0, 0, 0, 0, 0x7F, 0x09, 0x8D, 0x46, 0x01, 0x89, 0x05,
	0, 0, 0, 0, 0xCC };
static Site g_blipPoolRef = SITE(g_blipPoolSite, 10);
static Site g_blipMarkRefs[] = { SITE(g_blipMarkSite, 5), SITE(g_blipMarkSite, 16) };

// The parts measured first: the game code each limit's module finds and checks, one block per limit. What its
// sites reference lies below, outside this section, as the game's does.
// [weapon infos]
// The weapon infos: the game's initializer of the weapon info blob array - its "lea rcx" of the array (0x58 bytes before
// W-E-I1 in the game), then W-E-I1: a field of the manager zeroed, "mov edx, 160", the call to the allocation, the block's
// "mov [rip+disp32], rax", the capacity's "mov word [rip+disp32], 160" - whose disp32 counts from the instruction's end,
// after its imm16 - and the "lea rcx" of the manager's exit destructor; the allocation's first bytes, "push rsi" to
// "imul rcx, rdi, 0xF8", and "mov edx, 8"; the loader, a NOP before its start (padding, as TweaksV's function-start check
// takes it - a CC would add a line to the RageOpenV suite's count of computed starts), its prologue and two moves, then
// W-E-I2: the count read, "lea ecx, [rax+1]", the count written, "imul r13, rax, 0xF8" and "add r13, [block]"; and a third
// reference to the array, a store of a block once "wiextra" makes its first byte 48. Every site ends in CC.
volatile unsigned char g_weaponInfosLea[] = { 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0xCC };
volatile unsigned char g_weaponInfosInit[] = { 0xC7, 0x05, 0, 0, 0, 0, 0x00, 0x00, 0x00, 0x00, 0xBA, 0xA0, 0x00, 0x00, 0x00, 0xE8, 0, 0,
	0, 0, 0x48, 0x89, 0x05, 0, 0, 0, 0, 0x66, 0xC7, 0x05, 0, 0, 0, 0, 0xA0, 0x00, 0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0xCC };
volatile unsigned char g_weaponInfosAllocation[] = { 0x56, 0x57, 0x48, 0x83, 0xEC, 0x28, 0x89, 0xD6, 0x48, 0x63, 0xFA, 0x48, 0x69,
	0xCF, 0xF8, 0x00, 0x00, 0x00, 0xBA, 0x08, 0x00, 0x00, 0x00, 0xCC };
volatile unsigned char g_weaponInfosLoader[] = { 0x90, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x56, 0x57, 0x53, 0x48, 0x81,
	0xEC, 0x80, 0x01, 0x00, 0x00, 0x48, 0x89, 0xCE, 0x48, 0x89, 0xD7, 0x0F, 0xB7, 0x05, 0, 0, 0, 0, 0x8D, 0x48, 0x01, 0x66, 0x89, 0x0D,
	0, 0, 0, 0, 0x4C, 0x69, 0xE8, 0xF8, 0x00, 0x00, 0x00, 0x4C, 0x03, 0x2D, 0, 0, 0, 0, 0xCC };	// W-E-I2 at +25
volatile unsigned char g_weaponInfosExtra[] = { 0xCC, 0x89, 0x05, 0, 0, 0, 0, 0xCC };
static Site g_weaponInfosLeaRef = SITE(g_weaponInfosLea, 3);
static Site g_weaponInfosCall = SITE(g_weaponInfosInit, 16);
static Site g_weaponInfosBlockRef = SITE(g_weaponInfosInit, 23);
static Site g_weaponInfosLoaderRefs[] = { SITE(g_weaponInfosLoader, 28), SITE(g_weaponInfosLoader, 38), SITE(g_weaponInfosLoader, 52) };
static Site g_weaponInfosExtraRef = SITE(g_weaponInfosExtra, 3);
static Site g_weaponInfosSites[] = { SITE(g_weaponInfosLea, 0), SITE(g_weaponInfosInit, 0), SITE(g_weaponInfosAllocation, 0),
	SITE(g_weaponInfosLoader, 0), SITE(g_weaponInfosExtra, 0) };
// [weapon components]
// The weapon components (the WeaponLimits part's measurement): the list's rebuild - "mov dword [rip+disp32], 0" of its
// count, the disp32 at +2 and the instruction 10 bytes long, "movzx r8d, word [rip+disp32]" of the files' count, the
// disp32 at +14, and "lea r10, [rip+disp32]" of the list, the rel32 at +34 - its sort - "movsxd rdx, dword [rip+disp32]"
// of the count (+3), "lea rcx, [rip+disp32]" of the list (+14), the comparison's lea (+21) and "mov r8d, 8" - its update
// of the count (+3 and +12), the pool's size - "mov rcx, [rip+disp32]" of the game's configuration (+3), the pool's name,
// its default 470 and the call of the size lookup (+19) - and the list's 15 other leas, with the registers the game's
// have (rel32 at +3 of each), which only the scan finds. Every site ends in CC.
volatile unsigned char g_weaponComponentsRebuild[] = { 0xC7, 0x05, 0, 0, 0, 0, 0x00, 0x00, 0x00, 0x00, 0x44, 0x0F, 0xB7, 0x05, 0, 0, 0, 0,
	0x66, 0x45, 0x85, 0xC0, 0x0F, 0x84, 0x96, 0x00, 0x00, 0x00, 0x45, 0x31, 0xC9, 0x4C, 0x8D, 0x15, 0, 0, 0, 0, 0xCC };
volatile unsigned char g_weaponComponentsSort[] = { 0x48, 0x63, 0x15, 0, 0, 0, 0, 0xEB, 0x02, 0x31, 0xD2, 0x48, 0x8D, 0x0D, 0, 0, 0, 0,
	0x4C, 0x8D, 0x0D, 0, 0, 0, 0, 0x41, 0xB8, 0x08, 0x00, 0x00, 0x00, 0xCC };
volatile unsigned char g_weaponComponentsUpdate[] = { 0x48, 0x63, 0x15, 0, 0, 0, 0, 0x8D, 0x4A, 0x01, 0x89, 0x0D, 0, 0, 0, 0, 0x4D, 0x89,
	0x04, 0xD2, 0xCC };
volatile unsigned char g_weaponComponentsPool[] = { 0x48, 0x8B, 0x0D, 0, 0, 0, 0, 0xBA, 0xCF, 0x3C, 0xE3, 0xBA, 0x41, 0xB8, 0xD6, 0x01,
	0x00, 0x00, 0xE8, 0, 0, 0, 0, 0x89, 0xC3, 0xCC };
#define WEAPON_COMPONENTS_LEA(rex, modrm) { rex, 0x8D, modrm, 0, 0, 0, 0, 0xCC }
volatile unsigned char g_weaponComponentsLeas[15][8] = {
	WEAPON_COMPONENTS_LEA(0x4C, 0x05), WEAPON_COMPONENTS_LEA(0x4C, 0x3D), WEAPON_COMPONENTS_LEA(0x4C, 0x2D), WEAPON_COMPONENTS_LEA(0x48, 0x2D),
	WEAPON_COMPONENTS_LEA(0x4C, 0x2D), WEAPON_COMPONENTS_LEA(0x4C, 0x1D), WEAPON_COMPONENTS_LEA(0x4C, 0x0D), WEAPON_COMPONENTS_LEA(0x4C, 0x05),
	WEAPON_COMPONENTS_LEA(0x4C, 0x0D), WEAPON_COMPONENTS_LEA(0x4C, 0x05), WEAPON_COMPONENTS_LEA(0x48, 0x0D), WEAPON_COMPONENTS_LEA(0x4C, 0x05),
	WEAPON_COMPONENTS_LEA(0x48, 0x15), WEAPON_COMPONENTS_LEA(0x4C, 0x0D), WEAPON_COMPONENTS_LEA(0x4C, 0x0D) };
// an 18th lea, which the game does not have: its first byte becomes 48 only with the option "wcextra"
volatile unsigned char g_weaponComponentsLeaExtra[] = { 0xCC, 0x8D, 0x05, 0, 0, 0, 0, 0xCC };
// every site but the 15 leas, whole, for the protection check and the report; the size lookup's call
static Site g_weaponComponentsSites[] = { SITE(g_weaponComponentsRebuild, 0), SITE(g_weaponComponentsSort, 0), SITE(g_weaponComponentsUpdate, 0),
	SITE(g_weaponComponentsPool, 0), SITE(g_weaponComponentsLeaExtra, 0) };
static Site g_weaponComponentsSizeCall = SITE(g_weaponComponentsPool, 19);
// [siren ids]
// The siren setting IDs: each array one match of its pattern with the bytes the module checks at their offsets, from
// I-E-P1 to I-E-M1 (the constructors' C2 and C3 from 15 bytes before their match), and a fifth lea of the ID table that is
// one only with "sififth" - filled with CC and written by SirenIdsSetup before TweaksV loads.
volatile unsigned char g_sirenIdsP1[42] = { 0xCC };
volatile unsigned char g_sirenIdsP2[42] = { 0xCC };
volatile unsigned char g_sirenIdsP3[15] = { 0xCC };
volatile unsigned char g_sirenIdsT1[0xCD] = { 0xCC };
volatile unsigned char g_sirenIdsT2[0xDA] = { 0xCC };
volatile unsigned char g_sirenIdsT3[0xC6] = { 0xCC };
volatile unsigned char g_sirenIdsT4[0xD6] = { 0xCC };
volatile unsigned char g_sirenIdsD1[26] = { 0xCC };
volatile unsigned char g_sirenIdsD2[35] = { 0xCC };
volatile unsigned char g_sirenIdsC1[0xCD] = { 0xCC };
volatile unsigned char g_sirenIdsC2[31] = { 0xCC };
volatile unsigned char g_sirenIdsC3[35] = { 0xCC };
volatile unsigned char g_sirenIdsR1[21] = { 0xCC };
volatile unsigned char g_sirenIdsM1[32] = { 0xCC };
volatile unsigned char g_sirenIdsLeaExtra[8] = { 0xCC };
// [siren count]
// [ped props]
// [texture budget]
// [streaming memory]
#pragma data_seg()

// The game's own modkit array (1024 entries of 2 bytes) and packfile array (4500 entries of 8 bytes). The
// harness reaches them only through these pointers, set when the program is loaded: none of its own code
// may load an array with a lea, which TweaksV's scan for every reference to the array would count.
static uint16_t g_oldModkitArrayStorage[1024];
static uint16_t* volatile g_oldModkitArray = g_oldModkitArrayStorage;
static void* g_oldPackfileArrayStorage[4500];
static void** volatile g_oldPackfileArray = g_oldPackfileArrayStorage;

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

// The game's blip pool, 2000 slots of 8 bytes - a blip's pointer, or 0 - and its high-water mark, a dword. The harness
// reaches them only through these pointers, set when the program is loaded, as it does the arrays above.
static uint64_t g_blipPoolStorage[2000];
static uint64_t* volatile g_blipPool = g_blipPoolStorage;
static int32_t g_blipMarkStorage = 0;
static int32_t* volatile g_blipMark = &g_blipMarkStorage;

// The parts measured first: the game's data their sites reference and what stands in for the game's code they
// call, one block per limit. The harness reaches the data only through pointers, as it does the arrays above.
// [weapon infos]
// The weapon info manager's array of blobs - the block's pointer, the count at +8, the capacity at +0xA - in the fake's
// data, as the game's is in its own: zero while TweaksV starts. The block the fake's initializer allocates, the sites'
// bytes as laid before TweaksV starts, and TweaksV.log as the fake reads it for "wipacks".
struct WeaponInfosArray { void* block; uint16_t count; uint16_t capacity; uint32_t unused; };
static_assert(offsetof(WeaponInfosArray, count) == 8 && offsetof(WeaponInfosArray, capacity) == 0xA, "the game's array layout");
static WeaponInfosArray g_weaponInfosArrayStorage;
static WeaponInfosArray* volatile g_weaponInfosArray = &g_weaponInfosArrayStorage;
static void* g_weaponInfosBlock = nullptr;
static unsigned char g_weaponInfosLaid[sizeof(g_weaponInfosLea) + sizeof(g_weaponInfosInit) + sizeof(g_weaponInfosAllocation) +
	sizeof(g_weaponInfosLoader) + sizeof(g_weaponInfosExtra)];
static char g_weaponInfosLog[4 << 20];

// The capacity's store: its disp32 counts from the instruction's end, the imm16 after it
static void WeaponInfosPointCapacityAt(const void* target)
{
	const int32_t rel = static_cast<int32_t>(reinterpret_cast<intptr_t>(target) - reinterpret_cast<intptr_t>(g_weaponInfosInit + 36));
	memcpy(const_cast<unsigned char*>(g_weaponInfosInit + 30), &rel, sizeof(rel));
}

// The sites' bytes as laid, kept before TweaksV starts, and whether they are still so: TweaksV writes none of them.
static void WeaponInfosKeepSites()
{
	size_t at = 0;
	for (const Site& site : g_weaponInfosSites)
		for (size_t i = 0; i < site.size; i++)
			g_weaponInfosLaid[at++] = site.bytes[i];
}
static bool WeaponInfosSitesAsLaid()
{
	size_t at = 0;
	for (const Site& site : g_weaponInfosSites)
		for (size_t i = 0; i < site.size; i++)
			if (g_weaponInfosLaid[at++] != site.bytes[i])
				return false;
	return true;
}

// The game's initializer of the array, as the fake runs it: a block on its heap of as many blobs of 248 bytes as the
// site's "mov edx" says, the block into the array and the room the site's capacity says - or room for raisedTo, as if
// another mod had raised both immediates ("wiraised") - then the loader's count of the files loaded.
static void WeaponInfosInitialize(unsigned raisedTo, unsigned loaded)
{
	uint32_t number = raisedTo;
	uint16_t capacity = static_cast<uint16_t>(raisedTo);
	if (raisedTo == 0)
	{
		memcpy(&number, const_cast<const unsigned char*>(g_weaponInfosInit + 11), sizeof(number));
		memcpy(&capacity, const_cast<const unsigned char*>(g_weaponInfosInit + 34), sizeof(capacity));
	}
	g_weaponInfosBlock = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, static_cast<SIZE_T>(number) * 248 + 1);
	g_weaponInfosArray->block = g_weaponInfosBlock;
	g_weaponInfosArray->capacity = capacity;
	g_weaponInfosArray->count = static_cast<uint16_t>(loaded);
}

// How often TweaksV.log, read whole and shared as TweaksV shares it while it writes, holds text.
static int WeaponInfosLogHas(const char* text)
{
	const HANDLE file = CreateFileW(L"TweaksV.log", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return 0;
	DWORD read = 0;
	const BOOL ok = ReadFile(file, g_weaponInfosLog, static_cast<DWORD>(sizeof(g_weaponInfosLog) - 1), &read, nullptr);
	CloseHandle(file);
	if (!ok)
		return 0;
	g_weaponInfosLog[read] = '\0';
	int found = 0;
	for (const char* at = strstr(g_weaponInfosLog, text); at; at = strstr(at + 1, text))
		found++;
	return found;
}

// Waits, 100 ms at a time and ms at most, until TweaksV.log holds n weapon infos summaries.
static bool WeaponInfosAwaitSummaries(int n, DWORD ms)
{
	for (DWORD waited = 0; WeaponInfosLogHas(" weapons.meta files loaded (") < n; waited += 100)
	{
		if (waited >= ms)
			return false;
		Sleep(100);
	}
	return true;
}

// "wipacks": packs mounted while the game runs, as the loader counts their files - once the first summary is logged, 8
// more, the array full; once the second is, 5 more, past its end, where the game's loader would write without a check
// (the fake writes nothing there); then 2.5 s of nothing new.
static void WeaponInfosMountPacks()
{
	if (!WeaponInfosAwaitSummaries(1, 20000))
		return;
	g_weaponInfosArray->count = 160;
	if (!WeaponInfosAwaitSummaries(2, 10000))
		return;
	g_weaponInfosArray->count = 165;
	if (WeaponInfosAwaitSummaries(3, 10000))
		Sleep(2500);
}
// [weapon components]
// The game's weapon component list, 470 entries of 8 bytes - a definition's pointer each - with its count, an int32, right
// after it, then the array of the files' blobs and their count, a uint16, as the game lays them out; what the game's
// configuration pointer leads to; and the game's pool size lookup behind the pool's size call, which gives a pool the size
// the configuration names for it or - as the game's own configuration does for this pool - the default. The harness keeps
// what it placed, the store and the sites' bytes, to tell afterwards that TweaksV wrote nothing.
struct WeaponComponentsStore { uint64_t list[470]; int32_t count; uint32_t unused; uint64_t blobs; uint16_t files; uint16_t capacity; };
static_assert(offsetof(WeaponComponentsStore, count) == 470 * 8, "the list's count lies right after it");
static WeaponComponentsStore g_weaponComponentsStoreStorage;
static WeaponComponentsStore* volatile g_weaponComponentsStore = &g_weaponComponentsStoreStorage;
static WeaponComponentsStore g_weaponComponentsPlacedStore;
static unsigned char g_weaponComponentsPlacedSites[512];
static uint64_t g_weaponComponentsConfigurationStorage[4];
static void* volatile g_weaponComponentsConfiguration = g_weaponComponentsConfigurationStorage;
static uint32_t WeaponComponentsPoolSize(const void* configuration, uint32_t hash, uint32_t defaultSize)
{
	return configuration && hash == 0x0BADC0DE ? 1 : defaultSize;
}

// The list as the game leaves it after a rebuild: count definitions from files files - their pointers, as on the heap - the
// rest of it empty; kept as placed
static void WeaponComponentsLoad(int count, int files)
{
	WeaponComponentsStore* const store = g_weaponComponentsStore;
	for (int i = 0; i < 470; i++)
		store->list[i] = i < count ? 0x000001D4A3C00000ull + 0xC0ull * static_cast<uint64_t>(i) : 0;
	store->count = count;
	store->unused = 0;
	store->blobs = 0x000001D4A3B00000ull;
	store->files = static_cast<uint16_t>(files);
	store->capacity = 32;
	g_weaponComponentsPlacedStore = *store;
}

// A rip-relative operand set to lead to target: its disp32 at field, the instruction ending at end
static void WeaponComponentsPoint(volatile unsigned char* bytes, int field, int end, const void* target)
{
	const auto at = const_cast<unsigned char*>(bytes);
	const int32_t rel = static_cast<int32_t>(reinterpret_cast<intptr_t>(target) - reinterpret_cast<intptr_t>(at + end));
	memcpy(at + field, &rel, sizeof(rel));
}

// Every byte of the sites in a row, at most room: the copy before TweaksV loads, and the comparison after it ran
static size_t WeaponComponentsCopySites(unsigned char* to, size_t room)
{
	size_t n = 0;
	const auto copy = [&n, to, room](const volatile unsigned char* bytes, size_t size) {
		for (size_t i = 0; i < size && n < room; i++)
			to[n++] = bytes[i];
	};
	for (const Site& site : g_weaponComponentsSites)
		copy(site.bytes, site.size);
	for (auto& row : g_weaponComponentsLeas)
		copy(row, sizeof(row));
	return n;
}
// [siren ids]
// The siren setting IDs: the game's data their sites lead to, at the game's sizes and offsets (TweaksV's PATTERNS.md,
// "SirenSettings") - the members' data the two parser registrations' tables lead to, the ID members of type 0x01 and
// every member's offset at +0x08 as the game writes it when it registers the structure; the members' offsets; the ID
// table, per ID the position of its setting, 0xFF free; the carcols data with the siren settings' array at +0x48, their
// count at +0x50 and capacity at +0x52; the game's pointer to it - then the set-up, the late actions and the report field.
static void SetRel32(const Site& site, const void* target);
static const unsigned char* ImageEnd();
static bool HasOption(int argc, wchar_t** argv, const wchar_t* option);
struct SirenIdsMember { uint64_t nameHash; uint64_t offset; unsigned char type; unsigned char rest[15]; };
static SirenIdsMember g_sirenIdsMembersStorage[28];			// the siren settings' 20, the variations' 7 and an eighth
static SirenIdsMember* volatile g_sirenIdsMembers = g_sirenIdsMembersStorage;
static SirenIdsMember* g_sirenIdsSettingTableStorage[21];	// the siren settings' member data pointers, then null
static SirenIdsMember** volatile g_sirenIdsSettingTable = g_sirenIdsSettingTableStorage;
static SirenIdsMember* g_sirenIdsVariationTableStorage[8];	// the variations', then null
static SirenIdsMember** volatile g_sirenIdsVariationTable = g_sirenIdsVariationTableStorage;
static uint32_t g_sirenIdsSettingOffsetsStorage[20] = { 0x0, 0x5E0, 0x4, 0x8, 0xC, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34,
	0x35, 0x36, 0x37, 0x38, 0x3C };
static uint32_t* volatile g_sirenIdsSettingOffsets = g_sirenIdsSettingOffsetsStorage;
static uint32_t g_sirenIdsVariationOffsetsStorage[7] = { 0x0, 0x8, 0x18, 0x28, 0x38, 0x4A, 0x4B };
static uint32_t* volatile g_sirenIdsVariationOffsets = g_sirenIdsVariationOffsetsStorage;
static unsigned char g_sirenIdsTableStorage[256];
static unsigned char* volatile g_sirenIdsTable = g_sirenIdsTableStorage;
static unsigned char g_sirenIdsCarcolsStorage[0x160];
static unsigned char* volatile g_sirenIdsCarcols = g_sirenIdsCarcolsStorage;
static unsigned char* g_sirenIdsPointerStorage[2];			// the game's pointer to the carcols data, and another one (sicarcols)
static unsigned char** volatile g_sirenIdsPointer = g_sirenIdsPointerStorage;
constexpr size_t kSirenIdsSettingSize = 0x5E8;
static unsigned char* g_sirenIdsSettings = nullptr;			// the siren settings' array, allocated by SirenIdsSetup
static size_t g_sirenIdsCapacity = 0;
static bool g_sirenIdsPack = false, g_sirenIdsNone = false, g_sirenIdsUnreadable = false;

// The sites, each with a hash of its bytes as placed, and one of the data, for the report
struct SirenIdsSite { volatile unsigned char* bytes; size_t size; uint64_t placed; };
static SirenIdsSite g_sirenIdsSites[] = { { g_sirenIdsP1, sizeof(g_sirenIdsP1), 0 }, { g_sirenIdsP2, sizeof(g_sirenIdsP2), 0 },
	{ g_sirenIdsP3, sizeof(g_sirenIdsP3), 0 }, { g_sirenIdsT1, sizeof(g_sirenIdsT1), 0 }, { g_sirenIdsT2, sizeof(g_sirenIdsT2), 0 },
	{ g_sirenIdsT3, sizeof(g_sirenIdsT3), 0 }, { g_sirenIdsT4, sizeof(g_sirenIdsT4), 0 }, { g_sirenIdsD1, sizeof(g_sirenIdsD1), 0 },
	{ g_sirenIdsD2, sizeof(g_sirenIdsD2), 0 }, { g_sirenIdsC1, sizeof(g_sirenIdsC1), 0 }, { g_sirenIdsC2, sizeof(g_sirenIdsC2), 0 },
	{ g_sirenIdsC3, sizeof(g_sirenIdsC3), 0 }, { g_sirenIdsR1, sizeof(g_sirenIdsR1), 0 }, { g_sirenIdsM1, sizeof(g_sirenIdsM1), 0 },
	{ g_sirenIdsLeaExtra, sizeof(g_sirenIdsLeaExtra), 0 } };
static uint64_t g_sirenIdsDataPlaced = 0;
constexpr uint64_t kSirenIdsHashStart = 0xCBF29CE484222325ull;
static uint64_t SirenIdsHash(uint64_t hash, const volatile unsigned char* bytes, size_t size)
{
	for (size_t i = 0; i < size; i++)
		hash = (hash ^ bytes[i]) * 0x100000001B3ull;
	return hash;
}
static uint64_t SirenIdsData()
{
	uint64_t hash = SirenIdsHash(kSirenIdsHashStart, g_sirenIdsTable, sizeof(g_sirenIdsTableStorage));
	hash = SirenIdsHash(hash, g_sirenIdsCarcols, sizeof(g_sirenIdsCarcolsStorage));
	hash = SirenIdsHash(hash, reinterpret_cast<const unsigned char*>(g_sirenIdsMembers), sizeof(g_sirenIdsMembersStorage));
	hash = SirenIdsHash(hash, reinterpret_cast<const unsigned char*>(g_sirenIdsSettingTable), sizeof(g_sirenIdsSettingTableStorage));
	hash = SirenIdsHash(hash, reinterpret_cast<const unsigned char*>(g_sirenIdsVariationTable), sizeof(g_sirenIdsVariationTableStorage));
	hash = SirenIdsHash(hash, reinterpret_cast<const unsigned char*>(g_sirenIdsSettingOffsets), sizeof(g_sirenIdsSettingOffsetsStorage));
	hash = SirenIdsHash(hash, reinterpret_cast<const unsigned char*>(g_sirenIdsVariationOffsets), sizeof(g_sirenIdsVariationOffsetsStorage));
	hash = SirenIdsHash(hash, reinterpret_cast<const unsigned char*>(g_sirenIdsPointer), sizeof(g_sirenIdsPointerStorage));
	return g_sirenIdsUnreadable ? hash : SirenIdsHash(hash, g_sirenIdsSettings, g_sirenIdsCapacity * kSirenIdsSettingSize);
}

// Writes the bytes a text gives ("0F B6 2C 0F") at an offset of a site
static unsigned SirenIdsDigit(char c)
{
	return c <= '9' ? static_cast<unsigned>(c - '0') : static_cast<unsigned>((c | 0x20) - 'a' + 10);
}
static void SirenIdsPut(volatile unsigned char* site, size_t offset, const char* hex)
{
	for (size_t i = 0; hex[i] != 0 && hex[i + 1] != 0; i += hex[i + 2] != 0 ? 3 : 2)
		site[offset++] = static_cast<unsigned char>(SirenIdsDigit(hex[i]) * 16 + SirenIdsDigit(hex[i + 1]));
}
static void SirenIdsRel32(volatile unsigned char* site, int offset, const void* target)
{
	const Site field = { site, offset, 0 };
	SetRel32(field, target);
}

// The game's builder: a setting whose ID is none (0xFF) or taken gets the first free one - none when none is free - and the
// table the setting's position, a byte; then the settings' count
static void SirenIdsGive(size_t position, unsigned char wanted)
{
	unsigned char id = wanted;
	if (id == 0xFF || g_sirenIdsTable[id] != 0xFF)
	{
		id = 0xFF;
		for (unsigned candidate = 0; candidate < 256; candidate++)
			if (g_sirenIdsTable[candidate] == 0xFF)
			{
				id = static_cast<unsigned char>(candidate);
				break;
			}
	}
	g_sirenIdsSettings[position * kSirenIdsSettingSize] = id;
	g_sirenIdsTable[id] = static_cast<unsigned char>(position);
}
static void SirenIdsCount(size_t count)
{
	const uint16_t value = static_cast<uint16_t>(count);
	memcpy(g_sirenIdsCarcols + 0x50, &value, sizeof(value));
}

// The sites as the game has them, as the options change them, and the data: 24 siren settings with the IDs 1 to 24 (simany: 300,
// the IDs 0 to 254, then none)
static void SirenIdsSetup(int argc, wchar_t** argv)
{
	for (const SirenIdsSite& site : g_sirenIdsSites)
		for (size_t i = 0; i < site.size; i++)
			site.bytes[i] = 0xCC;
	SirenIdsPut(g_sirenIdsP1, 0, "C7 05 00 00 00 00 0D 9B EB 84 0F 57 C0 0F 11 05 00 00 00 00 48 8D 05 00 00 00 00 48 89 05 00 00 00 00 48 8D 05");
	SirenIdsPut(g_sirenIdsP2, 0, "C7 05 00 00 00 00 A9 FE EA 18 0F 57 C0 0F 11 05 00 00 00 00 48 8D 05 00 00 00 00 48 89 05 00 00 00 00 48 8D 05");
	SirenIdsPut(g_sirenIdsP3, 0, "48 8B 41 08 0F B6 48 10 48 83 F9 21 0F 87");
	SirenIdsPut(g_sirenIdsT1, 0, "B9 4B 00 00 00 31 D2 4C 8D 05 00 00 00 00 4C 8D 0D 00 00 00 00");
	SirenIdsPut(g_sirenIdsT1, 0x93, HasOption(argc, argv, L"siwidened") ? "0F B7 2C 0F 48 81 FD FF 00 00 00" : "0F B6 2C 0F 48 81 FD FF 00 00 00");
	SirenIdsPut(g_sirenIdsT1, 0xC4, "0F B6 1C 2B 88 1C 0F");
	SirenIdsPut(g_sirenIdsT2, 0, "41 0F B7 46 50 48 85 C0 0F 84 00 00 00 00 49 8B 4E 48 31 D2 48 8D 1D 00 00 00 00");
	SirenIdsPut(g_sirenIdsT2, 0x29, "40 88 31");
	SirenIdsPut(g_sirenIdsT2, 0x30, "0F B6 2C 39");
	SirenIdsPut(g_sirenIdsT2, 0x4B, "0F B6 2C 39 48 81 FD FF 00 00 00");
	SirenIdsPut(g_sirenIdsT2, 0x85, "48 81 FE 00 01 00 00");
	SirenIdsPut(g_sirenIdsT2, 0xCD, "48 81 FE 00 01 00 00");
	SirenIdsPut(g_sirenIdsT2, 0xD6, "40 B6 FF");
	SirenIdsPut(g_sirenIdsT3, 0, "B8 4B 00 00 00 31 C9 4C 8D 05 00 00 00 00 48 8D 2D 00 00 00 00");
	SirenIdsPut(g_sirenIdsT3, 0x8D, "0F B6 3C 03 48 81 FF FF 00 00 00");
	SirenIdsPut(g_sirenIdsT3, 0xBE, "0F B6 14 17 88 14 03");
	SirenIdsPut(g_sirenIdsT4, 0, "4C 8B 05 00 00 00 00 49 8B 58 48 89 CA 48 8D 2D 00 00 00 00");
	SirenIdsPut(g_sirenIdsT4, 0x21, "40 88 33");
	SirenIdsPut(g_sirenIdsT4, 0x28, "0F B6 0C 3B");
	SirenIdsPut(g_sirenIdsT4, 0x42, "0F B6 34 3B 48 81 FE FF 00 00 00");
	SirenIdsPut(g_sirenIdsT4, 0x81, "48 81 FE 00 01 00 00");
	SirenIdsPut(g_sirenIdsT4, 0xC9, "48 81 FE 00 01 00 00");
	SirenIdsPut(g_sirenIdsT4, 0xD2, "40 B6 FF");
	SirenIdsPut(g_sirenIdsD1, 0, "4D 8B 6F 48 4D 69 FE E8 05 00 00 43 0F B6 54 3D 00 0F B7 7B 50 80 FA FF 74");
	SirenIdsPut(g_sirenIdsD2, 0, "31 F6 66 2E 0F 1F 84 00 00 00 00 00 38 54 35 00 0F 84 00 00 00 00 48 81 C6 E8 05 00 00 48 39 F3 75 EA");
	SirenIdsPut(g_sirenIdsC1, 0, "56 48 83 EC 20 48 89 CE C7 41 1C 00 00 00 00 B8 83 00 00 00 0F 57 C0");
	SirenIdsPut(g_sirenIdsC1, 0xA0, "C6 06 FF");
	SirenIdsPut(g_sirenIdsC1, 0xC9, "89 46 1C");
	SirenIdsPut(g_sirenIdsC2, 0, "C7 41 30 00 00 00 00 48 C7 41 38 00 00 00 00 C7 41 40 00 00 00 00 C7 41 48 00 00 FF FF C3");
	SirenIdsPut(g_sirenIdsC3, 0, "C7 40 30 00 00 00 00 48 C7 40 38 00 00 00 00 C7 40 40 00 00 00 00 C7 40 48 00 00 FF FF 48 83 C4 28 C3");
	SirenIdsPut(g_sirenIdsR1, 0, "66 0F 7F 05 00 00 00 00 E8 00 00 00 00 E8 00 00 00 00 80 3D");
	SirenIdsPut(g_sirenIdsM1, 0, "0F B6 81 3B 05 00 00 48 85 C0 74 13 48 8B 0D 00 00 00 00 48 69 C0 E8 05 00 00 48 03 41 48 C3");
	SirenIdsPut(g_sirenIdsLeaExtra, 0, HasOption(argc, argv, L"sififth") ? "48 8D 05 00 00 00 00" : "CC 8D 05 00 00 00 00");

	// the parser registrations' tables and the members' data
	for (size_t i = 0; i < 20; i++)
	{
		g_sirenIdsMembers[i].offset = g_sirenIdsSettingOffsets[i];
		g_sirenIdsMembers[i].type = i == 0 ? 0x01 : 0x07;
		g_sirenIdsSettingTable[i] = &g_sirenIdsMembers[i];
	}
	for (size_t i = 0; i < 7; i++)
	{
		g_sirenIdsMembers[20 + i].offset = g_sirenIdsVariationOffsets[i];
		g_sirenIdsMembers[20 + i].type = i >= 5 ? 0x01 : 0x07;	// the light and the siren setting ID, a byte each
		g_sirenIdsVariationTable[i] = &g_sirenIdsMembers[20 + i];
	}
	g_sirenIdsMembers[27].type = 0x01;
	if (HasOption(argc, argv, L"simembers"))
		g_sirenIdsVariationTable[7] = &g_sirenIdsMembers[27];
	if (HasOption(argc, argv, L"sitype"))
		g_sirenIdsMembers[26].type = 0x03;
	if (HasOption(argc, argv, L"sioffsets"))
		g_sirenIdsSettingOffsets[2] = 0x8;
	if (HasOption(argc, argv, L"sidataout"))
		g_sirenIdsSettingTable[0] = reinterpret_cast<SirenIdsMember*>(const_cast<unsigned char*>(ImageEnd() + 0x73000));

	// the siren settings, given their IDs as the game gives them
	const bool many = HasOption(argc, argv, L"simany");
	g_sirenIdsCapacity = many ? 304 : 32;
	g_sirenIdsSettings = static_cast<unsigned char*>(VirtualAlloc(nullptr, g_sirenIdsCapacity * kSirenIdsSettingSize, MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE));
	memset(g_sirenIdsTable, 0xFF, sizeof(g_sirenIdsTableStorage));
	const size_t count = many ? 300 : 24;
	for (size_t i = 0; i < count; i++)
		SirenIdsGive(i, static_cast<unsigned char>(many ? (i < 255 ? i : 0xFF) : 1 + i));
	SirenIdsCount(count);
	const uint16_t capacity = static_cast<uint16_t>(g_sirenIdsCapacity);
	memcpy(g_sirenIdsCarcols + 0x52, &capacity, sizeof(capacity));
	memcpy(g_sirenIdsCarcols + 0x48, &g_sirenIdsSettings, sizeof(g_sirenIdsSettings));
	g_sirenIdsUnreadable = HasOption(argc, argv, L"siunreadable");
	DWORD old = 0;
	if (g_sirenIdsUnreadable)
		VirtualProtect(g_sirenIdsSettings, g_sirenIdsCapacity * kSirenIdsSettingSize, PAGE_NOACCESS, &old);
	g_sirenIdsPack = HasOption(argc, argv, L"sipack");
	g_sirenIdsNone = HasOption(argc, argv, L"sinone");
	g_sirenIdsPointer[0] = g_sirenIdsNone ? nullptr : g_sirenIdsCarcols;
	g_sirenIdsPointer[1] = g_sirenIdsCarcols;

	// where the sites lead
	SirenIdsRel32(g_sirenIdsP1, 23, g_sirenIdsSettingTable);
	SirenIdsRel32(g_sirenIdsP1, 37, g_sirenIdsSettingOffsets);
	SirenIdsRel32(g_sirenIdsP2, 23, g_sirenIdsVariationTable);
	SirenIdsRel32(g_sirenIdsP2, 37, g_sirenIdsVariationOffsets);
	const unsigned char* const table = HasOption(argc, argv, L"simoved") ? ImageEnd() + 0x70000 : g_sirenIdsTable;
	SirenIdsRel32(g_sirenIdsT1, 10, table);
	SirenIdsRel32(g_sirenIdsT2, 23, table);
	SirenIdsRel32(g_sirenIdsT3, 10, table);
	SirenIdsRel32(g_sirenIdsT4, 16, HasOption(argc, argv, L"sisplit") ? table + 16 : table);
	SirenIdsRel32(g_sirenIdsLeaExtra, 3, table);
	SirenIdsRel32(g_sirenIdsT4, 3, &g_sirenIdsPointer[0]);
	SirenIdsRel32(g_sirenIdsM1, 15, &g_sirenIdsPointer[HasOption(argc, argv, L"sicarcols") ? 1 : 0]);
	SirenIdsRel32(g_sirenIdsD2, 18, HasOption(argc, argv, L"sijz") ? ImageEnd() + 0x71000 : const_cast<const unsigned char*>(g_sirenIdsD1));
	SirenIdsRel32(g_sirenIdsR1, 4, table + (HasOption(argc, argv, L"sifill") ? 0xE0 : 0xF0));
	SirenIdsRel32(g_sirenIdsR1, 14, HasOption(argc, argv, L"sicallhooked") ? ImageEnd() + 0x72000 : const_cast<const unsigned char*>(g_sirenIdsT1));

	for (SirenIdsSite& site : g_sirenIdsSites)
		site.placed = SirenIdsHash(kSirenIdsHashStart, site.bytes, site.size);
	g_sirenIdsDataPlaced = SirenIdsData();
}

// What the game does with its siren settings once TweaksV has started, 12 s after it loaded - after the measurement's first
// summary: a pack's 6 settings appended with the IDs 40 to 45 (sipack), or the base files read (sinone); then the fake stays
// to 15 s, for the summary after it
static void SirenIdsLater()
{
	if (!g_sirenIdsPack && !g_sirenIdsNone)
		return;
	Sleep(10500);
	if (g_sirenIdsPack)
	{
		for (size_t i = 24; i < 30; i++)
			SirenIdsGive(i, static_cast<unsigned char>(16 + i));
		SirenIdsCount(30);
	}
	if (g_sirenIdsNone)
		g_sirenIdsPointer[0] = g_sirenIdsCarcols;
	g_sirenIdsDataPlaced = SirenIdsData();
	Sleep(3000);
}

// "siren ids: sites as placed 15 of 15, data as placed | ": TweaksV wrote nothing into the sites or the data
static void SirenIdsReport()
{
	int asPlaced = 0;
	for (const SirenIdsSite& site : g_sirenIdsSites)
		if (SirenIdsHash(kSirenIdsHashStart, site.bytes, site.size) == site.placed)
			asPlaced++;
	wprintf(L"siren ids: sites as placed %d of %d, data %ls | ", asPlaced, static_cast<int>(sizeof(g_sirenIdsSites) / sizeof(g_sirenIdsSites[0])),
		SirenIdsData() == g_sirenIdsDataPlaced ? L"as placed" : L"NOT as placed");
}
// [siren count]
// [ped props]
// [texture budget]
// [streaming memory]

// The game's two tree searches behind the box streamer's four calls: the first and the third call lead
// to the first search, the second and the fourth to the second. Each writes the number of entries the
// harness asks for - at most 16 short of the capacity it is handed, where the game's searches stop - and
// returns it, and checks that it was given the harness's own arguments, the capacity its call site
// holds among them. The two differ, so that the linker cannot fold them into one.
using BoxTreeSearch = int (*)(void* tree, const float* min, const float* max, uint64_t* list, int capacity);
static uint64_t g_boxSearchList[10000];
static int g_boxSearchWanted = 0, g_boxSearchCapacity = 0, g_boxSearchCallsA = 0, g_boxSearchCallsB = 0, g_boxSearchArgsBad = 0;
static int FakeTreeSearch(void* tree, const float* min, const float* max, uint64_t* list, int capacity)
{
	if (tree != nullptr || !min || !max || list != g_boxSearchList || capacity != g_boxSearchCapacity)
		g_boxSearchArgsBad++;
	const int found = g_boxSearchWanted < capacity - 16 ? g_boxSearchWanted : capacity - 16;
	for (int i = 0; i < found; i++)
		list[i] = static_cast<uint64_t>(i);
	return found;
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

// A script program, laid out at the offsets of the game's (PATTERNS.md, "Script programs"); the fields the game
// has there and TweaksV does not read hold recognisable values, so that the copy of the object shows them.
struct FakeScriptProgram
{
	uint64_t vtable;				// +0x00
	uint64_t unknown08;
	unsigned char** codePages;		// +0x10: one pointer per page of 16 KB
	uint32_t unknown18;
	uint32_t codeLength;			// +0x1C
	uint32_t unknown20[3];
	uint32_t nativeCount;			// +0x2C
	uint64_t unknown30[2];
	uint64_t* natives;				// +0x40
	uint64_t unknown48[2];
	uint32_t nameHash;				// +0x58
	uint32_t unknown5C;
	const char* name;				// +0x60
	unsigned char** stringPages;	// +0x68
	uint32_t stringLength;			// +0x70
	uint32_t unknown74[3];
};
static_assert(offsetof(FakeScriptProgram, codePages) == 0x10 && offsetof(FakeScriptProgram, codeLength) == 0x1C &&
	offsetof(FakeScriptProgram, nativeCount) == 0x2C && offsetof(FakeScriptProgram, natives) == 0x40 &&
	offsetof(FakeScriptProgram, nameHash) == 0x58 && offsetof(FakeScriptProgram, name) == 0x60 &&
	offsetof(FakeScriptProgram, stringPages) == 0x68 && offsetof(FakeScriptProgram, stringLength) == 0x70 &&
	sizeof(FakeScriptProgram) == 0x80, "the fake's script program must have the game's layout");

// The natives the fake's programs call: their hashes, and the handler the fake's resolution function puts in
// their place - a function of the fake's own, each a different one so that the linker cannot fold them; a hash
// it does not know gets the "unknown" handler, as the game's does, and the function returns 0.
static int g_nativeCalls = 0;
static __declspec(noinline) void FakeNativeA() { g_nativeCalls += 1; }
static __declspec(noinline) void FakeNativeB() { g_nativeCalls += 2; }
static __declspec(noinline) void FakeNativeC() { g_nativeCalls += 3; }
static __declspec(noinline) void FakeNativeUnknown() { g_nativeCalls += 4; }
struct FakeNative { uint64_t hash; void (*handler)(); };
static const FakeNative g_fakeNatives[] = { { 0x4A5B6C7D8E9F0011ull, &FakeNativeA }, { 0x1122334455667788ull, &FakeNativeB },
	{ 0x99AABBCCDDEEFF00ull, &FakeNativeC } };
constexpr size_t kFakeNatives = sizeof(g_fakeNatives) / sizeof(g_fakeNatives[0]);
constexpr uint64_t kUnknownNativeHash = 0x0123456789ABCDEFull;
constexpr DWORD kResolveLastError = 0x2A1B;	// the last error the fake's resolution function leaves

// The game's own function behind the call at the hook site: the program's native table, hashes until now,
// gets the handlers; 1 when every hash was known. It reads no more entries than the fake's tables have, whatever
// the program's native count says. With the option blipchanged it also changes blip_controller's threshold, at
// 0x403 of its code, to 151 - between the Blips part's look at the program and its write, as another mod could.
static int g_resolveCalls = 0;
static bool g_blipChanged = false;
static __declspec(noinline) unsigned char FakeResolveNatives(unsigned char* program)
{
	g_resolveCalls++;
	const auto p = reinterpret_cast<FakeScriptProgram*>(program);
	if (g_blipChanged && p->name && strcmp(p->name, "blip_controller") == 0)
		p->codePages[0][0x403] = 0x97;
	unsigned char every = 1;
	for (uint32_t i = 0; i < p->nativeCount && i < kFakeNatives; i++)
	{
		uint64_t handler = reinterpret_cast<uint64_t>(&FakeNativeUnknown);
		for (const FakeNative& native : g_fakeNatives)
			if (p->natives[i] == native.hash)
				handler = reinterpret_cast<uint64_t>(native.handler);
		if (handler == reinterpret_cast<uint64_t>(&FakeNativeUnknown))
			every = 0;
		p->natives[i] = handler;
	}
	SetLastError(kResolveLastError);
	return every;
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

// The game build this program pretends to be, from its own version resource (fake_game_version.rc):
// FILEVERSION 1,0,C,D gives (C << 16) | D; 0 without one.
static uint32_t OwnBuild()
{
	const HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(VS_VERSION_INFO), RT_VERSION);
	const HGLOBAL loaded = resource ? LoadResource(nullptr, resource) : nullptr;
	const auto data = static_cast<const unsigned char*>(loaded ? LockResource(loaded) : nullptr);
	const DWORD size = resource ? SizeofResource(nullptr, resource) : 0;
	for (DWORD at = 0; data && at + sizeof(VS_FIXEDFILEINFO) <= size; at += 4)
	{
		VS_FIXEDFILEINFO info{};
		memcpy(&info, data + at, sizeof(info));
		if (info.dwSignature == VS_FFI_SIGNATURE)
			return info.dwFileVersionLS;
	}
	return 0;
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
		static_cast<const unsigned char*>(info.BaseAddress) + info.RegionSize < first + sizeof(g_oldModkitArrayStorage))
	{
		wprintf(L"modkit copy: unreadable | ");
		return;
	}
	wprintf(L"modkit copy: %ls | ", memcmp(first, g_oldModkitArray, sizeof(g_oldModkitArrayStorage)) == 0 ? L"ok" : L"MISSING");
}

// "pack refs: 32 new, R31a new, R31b game (RW, 512 KB)" - "new" is the common target of the
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
		// Call it the way the game would, with a streaming configuration whose archive count is 3000 - or,
		// with the option "packbigconfig", 20000, more than the array is given.
		alignas(8) char si[64] = {};
		const uint32_t configCount = HasOption(argc, argv, L"packbigconfig") ? 20000 : 3000;
		memcpy(si + 24, &configCount, 4);
		reinterpret_cast<void (*)(char*)>(const_cast<unsigned char*>(callTarget))(si);
		uint32_t after = 0;
		memcpy(&after, si + 24, 4);
		wprintf(L"hook: config %u -> %u, original called %dx with %u | ", configCount, after, g_origCalls, g_origSawCount);
	}
}

// Calls each of the four tree-search calls' targets the way the game would - TweaksV's stub or the
// game's own tree search, with the capacity its site holds; "another mod's" address (option
// "boxcallhooked") not at all - asking for 100, 400, 700 and 1000 entries of a 10000-entry list.
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
		g_boxSearchCapacity = static_cast<int>(Imm32(g_boxCaps[i]));
		returns[i] = reinterpret_cast<BoxTreeSearch>(const_cast<unsigned char*>(target))(nullptr, lower, upper, g_boxSearchList,
			g_boxSearchCapacity);
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
		SITE(g_landingCallee, 0), SITE(g_landingSiteCopy, 0), SITE(g_modkitExtra, 0), SITE(g_packI1, 0), SITE(g_packH1, 0),
		SITE(g_packR31a, 0), SITE(g_packR31b, 0), SITE(g_packR31aCopy, 0), SITE(g_packExtra, 0), SITE(g_boxRef9, 0),
		SITE(g_decalFull, 0), SITE(g_decalParserLea, 0), SITE(g_decalFindById, 0), SITE(g_decalPickVariant, 0), SITE(g_decalOverrideOne, 0), SITE(g_decalOverrideLoop, 0), SITE(g_decalOverrideCmp1, 0),
		SITE(g_decalOverrideCmpEbp, 0), SITE(g_decalOverrideCmp2, 0), SITE(g_decalLoopStart, 0), SITE(g_decalLoopEnd, 0),
		SITE(g_decalAddR8d, 0), SITE(g_decalAddEsi, 0), SITE(g_decalAddRel8, 0), SITE(g_decalAddEbx, 0), SITE(g_decalLeaExtra, 0),
		SITE(g_scriptHookSite, 0), SITE(g_scriptNativeLoop, 0), SITE(g_scriptInterpreter, 0), SITE(g_blipPoolSite, 0), SITE(g_blipMarkSite, 0) };
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
	// the parts measured first: every site of each limit
	// [weapon infos]
	for (const Site& site : g_weaponInfosSites) fn(site);
	// [weapon components]
	for (const Site& site : g_weaponComponentsSites) fn(site);
	for (auto& row : g_weaponComponentsLeas) { Site site = { row, 0, sizeof(row) }; fn(site); }
	// [siren ids]
	for (const SirenIdsSite& sirenIds : g_sirenIdsSites)
	{
		const Site site = { sirenIds.bytes, 0, sirenIds.size };
		fn(site);
	}
	// [siren count]
	// [ped props]
	// [texture budget]
	// [streaming memory]
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

// DirectStorage (TweaksV's UNLOCK_FILES and DIAGNOSTIC_MODE): the calls the game makes into the stand-in
// dstorage.dll, which opens its files for real and holds them open, as DirectStorage does, and the factory's
// OpenFile for 19 paths: 4 archives in the game folder (one with a full path, one .CACHE), 7 other files in
// it (one with '/', one with a \\?\ prefix, one named x.rpf.txt, one whose extension is too long for
// TweaksV's table of types), 7 paths outside it (a save, temp files - one an archive - a sibling folder whose
// name starts with the game folder's, a path that leaves the game folder through "..", a network path in its
// \\?\UNC\ form, its prefix in upper and in lower case) and one too long to check. The files in the game
// folder are made first and deleted again afterwards; the others are not there.
enum class OtherInterface { None, First, Later, Only };
static const GUID g_factoryId = { 0x6924EA0C, 0xC3CD, 0x4826, { 0xB1, 0x0A, 0xF6, 0x4F, 0x4E, 0xD9, 0x27, 0xC1 } };
static const GUID g_otherId = { 0x12345678, 0x1234, 0x5678, { 0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78 } };
using OpenFileFn = HRESULT(STDMETHODCALLTYPE*)(void*, const wchar_t*, REFIID, void**);
using FileHandleFn = HANDLE(WINAPI*)(void*);
using CloseFileFn = void(WINAPI*)(void*);
static void* g_factory = nullptr;			// for dsshare and dsflood
static OpenFileFn g_openFile = nullptr;
static FileHandleFn g_fileHandle = nullptr;	// the handle a file the stand-in opened holds
static CloseFileFn g_closeFile = nullptr;	// lets go of a file the stand-in opened
static wchar_t g_dstorageResult[1024];

// A file of the fake's own with text in it (one write open).
static void MakeFile(const wchar_t* path, const char* text = "test")
{
	const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return;
	DWORD written = 0;
	WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
	CloseHandle(file);
}

static void CallDStorage(HMODULE dstorage, OtherInterface other)
{
	using GetFactory = HRESULT(WINAPI*)(REFIID, void**);
	using SetConfiguration = HRESULT(WINAPI*)(const void*);
	const auto setConfiguration = reinterpret_cast<SetConfiguration>(GetProcAddress(dstorage, "DStorageSetConfiguration"));
	const auto setConfiguration1 = reinterpret_cast<SetConfiguration>(GetProcAddress(dstorage, "DStorageSetConfiguration1"));
	const auto getFactory = reinterpret_cast<GetFactory>(GetProcAddress(dstorage, "DStorageGetFactory"));
	using FakeOpenFileCalls = long(WINAPI*)();
	const auto openFileCalls = reinterpret_cast<FakeOpenFileCalls>(GetProcAddress(dstorage, "FakeOpenFileCalls"));
	g_fileHandle = reinterpret_cast<FileHandleFn>(GetProcAddress(dstorage, "FakeFileHandle"));
	g_closeFile = reinterpret_cast<CloseFileFn>(GetProcAddress(dstorage, "FakeCloseFile"));
	const size_t size = sizeof(g_dstorageResult) / sizeof(g_dstorageResult[0]);
	if (!setConfiguration || !setConfiguration1 || !getFactory || !openFileCalls || !g_fileHandle || !g_closeFile)
	{
		swprintf(g_dstorageResult, size, L"dstorage: exports missing | ");
		return;
	}
	const uint32_t configuration[8] = { 4, 0xFFFFFFFF, 0, 1, 0, 0, 1, 1 };
	const HRESULT a = setConfiguration(configuration);
	const HRESULT b = setConfiguration1(configuration);
	void* otherFactory = nullptr;
	if (other == OtherInterface::Only)	// dsotheronly: an interface TweaksV does not know, and nothing else
	{
		const HRESULT c = getFactory(g_otherId, &otherFactory);
		swprintf(g_dstorageResult, size, L"dstorage: configuration 0x%lX/0x%lX, other interface only 0x%lX | ", static_cast<unsigned long>(a),
			static_cast<unsigned long>(b), static_cast<unsigned long>(c));
		return;
	}
	if (other == OtherInterface::First)	// dsotherinterface: first an interface TweaksV does not know
		getFactory(g_otherId, &otherFactory);
	void* factory = nullptr;
	const HRESULT c = getFactory(g_factoryId, &factory);
	int length = swprintf(g_dstorageResult, size, L"dstorage: configuration 0x%lX/0x%lX, factory 0x%lX %ls | ",
		static_cast<unsigned long>(a), static_cast<unsigned long>(b), static_cast<unsigned long>(c), factory ? L"set" : L"null");
	if (FAILED(c) || !factory || length < 0)
		return;

	const auto openFile = reinterpret_cast<OpenFileFn>((*reinterpret_cast<void***>(factory))[4]);
	g_factory = factory;
	g_openFile = openFile;
	wchar_t folder[MAX_PATH + 1] = {};
	GetCurrentDirectoryW(MAX_PATH, folder);
	wchar_t absolute[MAX_PATH + 64] = {}, prefixed[MAX_PATH + 64] = {}, sibling[MAX_PATH + 64] = {}, dotdot[MAX_PATH + 64] = {};
	swprintf(absolute, MAX_PATH + 64, L"%ls\\ds_update\\update.RPF", folder);
	swprintf(prefixed, MAX_PATH + 64, L"\\\\?\\%ls\\prefixed.txt", folder);		// in the game folder after all
	swprintf(sibling, MAX_PATH + 64, L"%ls2\\sibling.txt", folder);			// "...\TestRun2": not in the game folder
	swprintf(dotdot, MAX_PATH + 64, L"%ls\\..\\outside.txt", folder);		// out of it again
	// outside the game folder, like the game's saves (Documents\...\Profiles\...\SGTA50000) - here in the
	// folder above it, outside %TEMP% too - its cloud files in %TEMP% and a network path in its
	// \\?\UNC\ form: none of them is there
	wchar_t save[MAX_PATH + 64] = {};
	wcscpy_s(save, folder);
	if (wchar_t* const slash = wcsrchr(save, L'\\'))
		*slash = L'\0';
	wcscat_s(save, L"\\tweaksv_profile\\SGTA50000");
	wchar_t tempFolder[MAX_PATH + 1] = {}, temp[MAX_PATH + 64] = {}, tempArchive[MAX_PATH + 64] = {};
	GetTempPathW(MAX_PATH, tempFolder);
	swprintf(temp, MAX_PATH + 64, L"%lscloud_tweaksv.dat", tempFolder);
	swprintf(tempArchive, MAX_PATH + 64, L"%lsoutside.rpf", tempFolder);
	static wchar_t tooLong[3001];	// more than the 2047 characters TweaksV checks
	wmemset(tooLong, L'a', 3000);
	tooLong[3000] = L'\0';
	const struct { const wchar_t* name; const wchar_t* path; } files[] = { { L"rpf", L"x64a.rpf" }, { L"RPF", absolute },
		{ L"cache", L"rpf.cache" }, { L"txt", L"locktest.txt" }, { L"asi", L"TweaksV.asi" }, { L"none", L"noextension" },
		{ L"save", save }, { L"temp", temp }, { L"unc", L"\\\\?\\UNC\\localhost\\tweaksv_share\\cloud.dat" },
		{ L"slash", L"ds_dir/slash.txt" }, { L"prefixed", prefixed }, { L"sibling", sibling }, { L"dotdot", dotdot },
		{ L"rpftxt", L"x.rpf.txt" }, { L"CACHE", L"y.CACHE" }, { L"outrpf", tempArchive },
		{ L"unc2", L"\\\\?\\unc\\localhost\\tweaksv_share\\cloud2.dat" }, { L"long", tooLong }, { L"manifest", L"other.manifest" } };
	// the files in the game folder (TweaksV.asi is there already)
	const wchar_t* const made[] = { L"x64a.rpf", L"ds_update\\update.RPF", L"rpf.cache", L"locktest.txt", L"noextension",
		L"ds_dir\\slash.txt", L"prefixed.txt", L"x.rpf.txt", L"y.CACHE", L"other.manifest" };
	CreateDirectoryW(L"ds_update", nullptr);
	CreateDirectoryW(L"ds_dir", nullptr);
	for (const wchar_t* path : made)
		MakeFile(path);
	void* opened[sizeof(files) / sizeof(files[0])] = {};
	length += swprintf(g_dstorageResult + length, size - length, L"open:");
	for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
	{
		const HRESULT result = openFile(factory, files[i].path, g_factoryId, &opened[i]);
		length += swprintf(g_dstorageResult + length, size - length, L" %ls 0x%lX,", files[i].name, static_cast<unsigned long>(result));
	}
	swprintf(g_dstorageResult + length, size - length, L" reached %ld | ", openFileCalls());
	for (void* file : opened)
		g_closeFile(file);	// nullptr for a file not opened
	for (const wchar_t* path : made)
		DeleteFileW(path);
	RemoveDirectoryW(L"ds_update");
	RemoveDirectoryW(L"ds_dir");
	if (other == OtherInterface::Later)	// dsotherlater: an interface TweaksV does not know, after the known one
		getFactory(g_otherId, &otherFactory);
}

// dsflood: 20,001 more OpenFile calls - with DIAGNOSTIC_MODE, more than the 20,000 OpenFile lines TweaksV writes.
static wchar_t g_floodResult[64];
static void FloodOpenFile()
{
	if (!g_openFile)
		return;
	int calls = 0;
	for (int i = 0; i < 20001; i++)
	{
		void* file = nullptr;
		g_openFile(g_factory, L"locktest.txt", g_factoryId, &file);
		g_closeFile(file);
		calls++;
	}
	swprintf(g_floodResult, sizeof(g_floodResult) / sizeof(g_floodResult[0]), L"flood: %d calls | ", calls);
}

// Opens path as another program would and closes it again: 0 if it opened, else Windows' error.
static DWORD TryOpen(const wchar_t* path, DWORD access, DWORD share)
{
	const HANDLE file = CreateFileW(path, access, share, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return GetLastError();
	CloseHandle(file);
	return 0;
}

// Renames from to to and back again, as another program would: 0 if it could be renamed, else Windows' error.
static DWORD TryRename(const wchar_t* from, const wchar_t* to)
{
	if (!MoveFileExW(from, to, 0))
		return GetLastError();
	MoveFileExW(to, from, 0);
	return 0;
}

// dsshare (with dstorage): while the stand-in holds a file open, as DirectStorage holds the game's, other
// programs' opens of it. A text file in the game folder, opened by a relative path and by a full one: a write
// open that shares reading and a rename succeed, a write open that shares nothing fails (the limit: the
// stand-in reads it). An archive: a write open fails. A loose file in a subfolder, the way a mods loader
// serves it (onigiri\common\data\gameconfig.xml): opened, its bytes read through the stand-in's handle,
// shared; the same file by a \\?\ path, shared. A file in %TEMP% and one reached through "..", both outside
// the game folder: a write open fails. Each open's result, then the error of the other program's open (0:
// it succeeded).
static wchar_t g_shareResult[512];
static void ShareThroughStandIn()
{
	const size_t size = sizeof(g_shareResult) / sizeof(g_shareResult[0]);
	if (!g_openFile)
	{
		swprintf(g_shareResult, size, L"share: no stand-in | ");
		return;
	}
	wchar_t folder[MAX_PATH + 1] = {}, path[MAX_PATH * 2] = {};
	GetCurrentDirectoryW(MAX_PATH, folder);
	void* file = nullptr;

	MakeFile(L"ds_lock.txt");
	const HRESULT txt = g_openFile(g_factory, L"ds_lock.txt", g_factoryId, &file);
	const DWORD txtWrite = TryOpen(L"ds_lock.txt", GENERIC_WRITE, FILE_SHARE_READ);
	const DWORD txtRename = TryRename(L"ds_lock.txt", L"ds_lock_renamed.txt");
	const DWORD txtExclusive = TryOpen(L"ds_lock.txt", GENERIC_WRITE, 0);
	g_closeFile(file);
	swprintf(path, MAX_PATH * 2, L"%ls\\ds_lock.txt", folder);
	const HRESULT full = g_openFile(g_factory, path, g_factoryId, &file);
	const DWORD fullWrite = TryOpen(L"ds_lock.txt", GENERIC_WRITE, FILE_SHARE_READ);
	g_closeFile(file);
	DeleteFileW(L"ds_lock.txt");

	MakeFile(L"ds_lock.rpf");
	const HRESULT rpf = g_openFile(g_factory, L"ds_lock.rpf", g_factoryId, &file);
	const DWORD rpfWrite = TryOpen(L"ds_lock.rpf", GENERIC_WRITE, FILE_SHARE_READ);
	g_closeFile(file);
	DeleteFileW(L"ds_lock.rpf");

	const wchar_t* const loosePath = L"onigiri\\common\\data\\gameconfig.xml";
	CreateDirectoryW(L"onigiri", nullptr);
	CreateDirectoryW(L"onigiri\\common", nullptr);
	CreateDirectoryW(L"onigiri\\common\\data", nullptr);
	MakeFile(loosePath, "<gameconfig/>");
	const HRESULT loose = g_openFile(g_factory, loosePath, g_factoryId, &file);
	char bytes[32] = {};
	DWORD read = 0;
	const bool looseBytes = SUCCEEDED(loose) && ReadFile(g_fileHandle(file), bytes, sizeof(bytes) - 1, &read, nullptr) &&
		read == 13 && memcmp(bytes, "<gameconfig/>", 13) == 0;
	const DWORD looseWrite = TryOpen(loosePath, GENERIC_WRITE, FILE_SHARE_READ);
	g_closeFile(file);
	swprintf(path, MAX_PATH * 2, L"\\\\?\\%ls\\%ls", folder, loosePath);
	const HRESULT prefixed = g_openFile(g_factory, path, g_factoryId, &file);
	const DWORD prefixedWrite = TryOpen(loosePath, GENERIC_WRITE, FILE_SHARE_READ);
	g_closeFile(file);
	DeleteFileW(loosePath);
	RemoveDirectoryW(L"onigiri\\common\\data");
	RemoveDirectoryW(L"onigiri\\common");
	RemoveDirectoryW(L"onigiri");

	wchar_t temp[MAX_PATH + 64] = {};
	GetTempPathW(MAX_PATH, temp);
	wcscat_s(temp, L"tweaksv_ds_temp.txt");
	MakeFile(temp);
	const HRESULT tempOpen = g_openFile(g_factory, temp, g_factoryId, &file);
	const DWORD tempWrite = TryOpen(temp, GENERIC_WRITE, FILE_SHARE_READ);
	g_closeFile(file);
	DeleteFileW(temp);

	// out of the game folder through "..", into the harness's %TEMP% next to it (object\TestTemp, common.sh)
	const wchar_t* const upPath = L"..\\TestTemp\\tweaksv_ds_up.txt";
	MakeFile(upPath);
	const HRESULT up = g_openFile(g_factory, upPath, g_factoryId, &file);
	const DWORD upWrite = TryOpen(upPath, GENERIC_WRITE, FILE_SHARE_READ);
	g_closeFile(file);
	DeleteFileW(upPath);

	swprintf(g_shareResult, size, L"share: txt 0x%lX write %lu rename %lu write-exclusive %lu, full 0x%lX write %lu, rpf 0x%lX write %lu, "
		L"loose 0x%lX bytes %ls write %lu, prefixed 0x%lX write %lu, temp 0x%lX write %lu, up 0x%lX write %lu | ",
		static_cast<unsigned long>(txt), txtWrite, txtRename, txtExclusive, static_cast<unsigned long>(full), fullWrite,
		static_cast<unsigned long>(rpf), rpfWrite, static_cast<unsigned long>(loose), looseBytes ? L"ok" : L"WRONG", looseWrite,
		static_cast<unsigned long>(prefixed), prefixedWrite, static_cast<unsigned long>(tempOpen), tempWrite,
		static_cast<unsigned long>(up), upWrite);
}

// NtCreateFile as a program that calls it directly: path relative to root or, without root, made full
// ("\??\C:\..."). The handle, or nullptr.
using NtCreateFileFn = LONG(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER,
	ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
static HANDLE OpenNt(const wchar_t* path, HANDLE root, ACCESS_MASK access, ULONG share, ULONG options)
{
	const auto create = reinterpret_cast<NtCreateFileFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateFile"));
	wchar_t name[MAX_PATH * 2] = {};
	if (root)
		wcscpy_s(name, path);
	else
	{
		wchar_t full[MAX_PATH] = {};
		GetFullPathNameW(path, MAX_PATH, full, nullptr);
		swprintf(name, MAX_PATH * 2, L"\\??\\%ls", full);
	}
	UNICODE_STRING string{};
	string.Length = string.MaximumLength = static_cast<USHORT>(wcslen(name) * sizeof(wchar_t));
	string.Buffer = name;
	OBJECT_ATTRIBUTES attributes{};
	InitializeObjectAttributes(&attributes, &string, OBJ_CASE_INSENSITIVE, root, nullptr);
	IO_STATUS_BLOCK io{};
	HANDLE handle = nullptr;
	if (!create || create(&handle, access | SYNCHRONIZE, &attributes, &io, nullptr, FILE_ATTRIBUTE_NORMAL, share, FILE_OPEN,
		options | FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0) < 0)
		return nullptr;
	return handle;
}

// ownshare: the fake's own opens in its folder, also without the stand-in (dstorage.dll not loaded), and
// other programs' opens next to them. A read-only open that shares reading is given write and delete sharing:
// a write open that shares reading succeeds while it is held. An exclusive read-only open stays exclusive: a
// second open fails. A folder opened as one (FILE_DIRECTORY_FILE) is left as it is: it cannot be renamed while
// held. A name relative to another folder's handle is left as it is: a write open fails. A write open is left
// as it is: a second write open fails, as its sharing leaves writing out. 9999: the fake's own open failed.
static wchar_t g_ownResult[256];
static void ShareOwnOpens()
{
	MakeFile(L"ds_own.txt");
	HANDLE held = CreateFileW(L"ds_own.txt", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	const DWORD widened = held != INVALID_HANDLE_VALUE ? TryOpen(L"ds_own.txt", GENERIC_WRITE, FILE_SHARE_READ) : 9999;
	if (held != INVALID_HANDLE_VALUE)
		CloseHandle(held);
	DeleteFileW(L"ds_own.txt");

	MakeFile(L"ds_excl.txt");
	held = CreateFileW(L"ds_excl.txt", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	const DWORD exclusive = held != INVALID_HANDLE_VALUE ? TryOpen(L"ds_excl.txt", GENERIC_READ, FILE_SHARE_READ) : 9999;
	if (held != INVALID_HANDLE_VALUE)
		CloseHandle(held);
	DeleteFileW(L"ds_excl.txt");

	CreateDirectoryW(L"ds_own_dir", nullptr);
	MakeFile(L"ds_own_dir\\inner.txt");
	const HANDLE folder = OpenNt(L"ds_own_dir", nullptr, FILE_LIST_DIRECTORY, FILE_SHARE_READ, FILE_DIRECTORY_FILE);
	const DWORD folderRename = folder ? TryRename(L"ds_own_dir", L"ds_own_dir2") : 9999;
	if (folder)
		CloseHandle(folder);
	const HANDLE root = OpenNt(L"ds_own_dir", nullptr, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_DIRECTORY_FILE);
	const HANDLE rooted = root ? OpenNt(L"inner.txt", root, GENERIC_READ, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE) : nullptr;
	const DWORD rootedWrite = rooted ? TryOpen(L"ds_own_dir\\inner.txt", GENERIC_WRITE, FILE_SHARE_READ) : 9999;
	if (rooted)
		CloseHandle(rooted);
	if (root)
		CloseHandle(root);
	DeleteFileW(L"ds_own_dir\\inner.txt");
	RemoveDirectoryW(L"ds_own_dir");

	MakeFile(L"ds_w.txt");
	held = CreateFileW(L"ds_w.txt", GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	const DWORD writeOpen = held != INVALID_HANDLE_VALUE ? TryOpen(L"ds_w.txt", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE) : 9999;
	if (held != INVALID_HANDLE_VALUE)
		CloseHandle(held);
	DeleteFileW(L"ds_w.txt");

	swprintf(g_ownResult, sizeof(g_ownResult) / sizeof(g_ownResult[0]),
		L"own: widened write %lu, exclusive %lu, folder rename %lu, rooted write %lu, write-open %lu | ",
		widened, exclusive, folderRename, rootedWrite, writeOpen);
}

// dstypes: read-only opens of 17 files in its folder that are not there, each of another type - 16 short ones
// and one too long for TweaksV's table of types: more types than its summary lists.
static void OpenTypes()
{
	for (int i = 1; i <= 17; i++)
	{
		wchar_t name[32];
		if (i <= 16)
			swprintf(name, 32, L"ds_type.e%02d", i);
		else
			wcscpy_s(name, L"ds_type.toolongx");
		const HANDLE file = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file != INVALID_HANDLE_VALUE)
			CloseHandle(file);
	}
}

// dsopenflood: 100,001 opens in the game folder of files that are not there, each by another name - more than
// the tables of TweaksV's open summary take, and more files than its UNLOCK_FILES summary tells apart.
static void FloodOpens()
{
	for (int i = 0; i < 100001; i++)
	{
		wchar_t name[32];
		swprintf(name, sizeof(name) / sizeof(name[0]), L"dsflood_%06d.txt", i);
		const HANDLE file = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file != INVALID_HANDLE_VALUE)
			CloseHandle(file);
	}
}

// The replace-renames (TweaksV's UNLOCK_FILES on NtSetInformationFile): ds_held.txt in the game folder, holding
// "old", held open by the stand-in as DirectStorage holds the game's files, and written while it is held, one of
// the ways a program saves a file at a time; ds_new.txt holds "new". What a way gave - 0 when it went through,
// else Windows' error, or NtSetInformationFile's status - then what the file's name holds afterwards and what
// the stand-in's handle reads: "new/old" is a replace, which the holder sees once it opens the file again,
// "new/new" a write in place, "old/old" nothing done.
static const wchar_t* Content(HANDLE file)
{
	char bytes[8] = {};
	DWORD read = 0;
	if (!ReadFile(file, bytes, sizeof(bytes) - 1, &read, nullptr))
		return L"none";
	return read == 3 && memcmp(bytes, "old", 3) == 0 ? L"old" : read == 3 && memcmp(bytes, "new", 3) == 0 ? L"new" : L"?";
}

static const wchar_t* ContentByName(const wchar_t* path)
{
	const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return L"none";
	const wchar_t* const content = Content(file);
	CloseHandle(file);
	return content;
}

struct HeldWrite
{
	DWORD result;			// 0: it went through; 9999: the stand-in could not hold the file
	const wchar_t* named;	// what ds_held.txt holds afterwards
	const wchar_t* held;	// what the stand-in's handle reads
};

// ds_held.txt written by write while the stand-in holds it - made read-only first with readOnly.
static HeldWrite WriteHeld(DWORD (*write)(), bool readOnly = false)
{
	HeldWrite outcome = { 9999, L"none", L"none" };
	MakeFile(L"ds_held.txt", "old");
	MakeFile(L"ds_new.txt", "new");
	void* file = nullptr;
	if (g_openFile && SUCCEEDED(g_openFile(g_factory, L"ds_held.txt", g_factoryId, &file)) && file)
	{
		if (readOnly)
			SetFileAttributesW(L"ds_held.txt", FILE_ATTRIBUTE_READONLY);
		outcome.result = write();
		outcome.named = ContentByName(L"ds_held.txt");
		const HANDLE handle = g_fileHandle(file);
		const LARGE_INTEGER start{};
		outcome.held = SetFilePointerEx(handle, start, nullptr, FILE_BEGIN) ? Content(handle) : L"none";
		g_closeFile(file);
	}
	SetFileAttributesW(L"ds_held.txt", FILE_ATTRIBUTE_NORMAL);
	DeleteFileW(L"ds_held.txt");
	DeleteFileW(L"ds_new.txt");
	return outcome;
}

// The classic replace: MoveFileExW with MOVEFILE_REPLACE_EXISTING, the usual safe way to save a file.
static DWORD ClassicReplace()
{
	return MoveFileExW(L"ds_new.txt", L"ds_held.txt", MOVEFILE_REPLACE_EXISTING) ? 0 : GetLastError();
}

// A POSIX rename: SetFileInformationByHandle with FileRenameInfoEx, replace-if-exists and POSIX semantics.
static DWORD PosixRename()
{
	const HANDLE file = CreateFileW(L"ds_new.txt", DELETE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return GetLastError();
	wchar_t target[MAX_PATH + 1] = {};
	const DWORD length = GetFullPathNameW(L"ds_held.txt", MAX_PATH + 1, target, nullptr);
	alignas(FILE_RENAME_INFO) unsigned char buffer[sizeof(FILE_RENAME_INFO) + (MAX_PATH + 1) * sizeof(wchar_t)] = {};
	FILE_RENAME_INFO* const info = reinterpret_cast<FILE_RENAME_INFO*>(buffer);
	info->Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS;
	info->FileNameLength = length * sizeof(wchar_t);
	memcpy(info->FileName, target, (length + 1) * sizeof(wchar_t));
	const DWORD error = SetFileInformationByHandle(file, FileRenameInfoEx, info, sizeof(buffer)) ? 0 : GetLastError();
	CloseHandle(file);
	return error;
}

static DWORD ReplaceHeld()
{
	return ReplaceFileW(L"ds_held.txt", L"ds_new.txt", nullptr, 0, nullptr, nullptr) ? 0 : GetLastError();
}

// A write in place: the file opened for writing, emptied and written, as the C runtime's fopen with "w" does.
static DWORD WriteInPlace()
{
	const HANDLE file = CreateFileW(L"ds_held.txt", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return GetLastError();
	DWORD written = 0;
	const DWORD error = WriteFile(file, "new", 3, &written, nullptr) ? 0 : GetLastError();
	CloseHandle(file);
	return error;
}

// The classic replace from another program, outside the game (renamer.exe, fake_renamer.cpp): its exit code, the
// error it got; 9998 if it cannot be started, 9997 if it does not end within 10 seconds.
static DWORD ClassicReplaceOutside()
{
	wchar_t command[] = L"renamer.exe ds_new.txt ds_held.txt";
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	if (!CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process))
		return 9998;
	DWORD code = 9997;
	if (WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0)
		GetExitCodeProcess(process.hProcess, &code);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return code;
}

// dsreplace (with dstorage): the four ways of writing a held file from inside the game - the classic replace, a
// POSIX rename, ReplaceFile, a write in place - and the classic replace from another program.
static wchar_t g_replaceResult[256];
static void ReplaceHeldFile()
{
	const size_t size = sizeof(g_replaceResult) / sizeof(g_replaceResult[0]);
	if (!g_openFile)
	{
		swprintf(g_replaceResult, size, L"replace: no stand-in | ");
		return;
	}
	const HeldWrite classic = WriteHeld(ClassicReplace), posix = WriteHeld(PosixRename), replace = WriteHeld(ReplaceHeld),
		inPlace = WriteHeld(WriteInPlace), other = WriteHeld(ClassicReplaceOutside);
	swprintf(g_replaceResult, size, L"replace: classic %lu %ls/%ls, posix %lu %ls/%ls, replacefile %lu %ls/%ls, inplace %lu %ls/%ls, "
		L"other %lu %ls/%ls | ", classic.result, classic.named, classic.held, posix.result, posix.named, posix.held, replace.result,
		replace.named, replace.held, inPlace.result, inPlace.named, inPlace.held, other.result, other.named, other.held);
}

// NtSetInformationFile as a program that calls it directly - ntdll's, which passes TweaksV's hook as every rename
// in the game does: from, opened for deleting, renamed to name (nameLength characters), relative to root when
// given, with the class and flags asked for (FileRenameInformation's ReplaceIfExists is the first byte of flags).
// The status, 0x9999 if from cannot be opened. The last error set before the call must be there after it.
using NtSetInformationFileFn = LONG(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
static bool g_lastErrorKept = true;
static LONG RenameNt(const wchar_t* from, const wchar_t* name, size_t nameLength, HANDLE root, ULONG informationClass, ULONG flags)
{
	const auto setInformation = reinterpret_cast<NtSetInformationFileFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
		"NtSetInformationFile"));
	const HANDLE file = CreateFileW(from, DELETE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (!setInformation || file == INVALID_HANDLE_VALUE)
		return 0x9999;
	// FILE_RENAME_INFORMATION: the flags at 0, the folder handle at 8, the name's length in bytes at 16, the name at 20
	alignas(8) static unsigned char buffer[24 + 4096 * sizeof(wchar_t)];
	const ULONG bytes = static_cast<ULONG>(nameLength * sizeof(wchar_t));
	memset(buffer, 0, 24);
	memcpy(buffer, &flags, sizeof(flags));
	memcpy(buffer + 8, &root, sizeof(root));
	memcpy(buffer + 16, &bytes, sizeof(bytes));
	memcpy(buffer + 20, name, bytes);
	IO_STATUS_BLOCK io{};
	SetLastError(0xA5A5);
	const LONG status = setInformation(file, &io, buffer, 20 + bytes < 24 ? 24 : 20 + bytes, informationClass);
	if (GetLastError() != 0xA5A5)
		g_lastErrorKept = false;
	CloseHandle(file);
	return status;
}

constexpr ULONG kRenameInformation = 10, kRenameInformationEx = 65;
static wchar_t g_heldNtName[MAX_PATH * 2];			// \??\<the game folder>\ds_held.txt
static HANDLE g_gameFolderHandle = INVALID_HANDLE_VALUE;

// dsrenames (with dstorage): the renames TweaksV's hook sees, against the held file unless said otherwise - onto a
// file outside the game folder that the fake holds with every sharing (MoveFileExW, refused if passed on: 5), one
// without replace-if-exists (MoveFileExW without MOVEFILE_REPLACE_EXISTING: 183), one that asks for POSIX semantics
// itself, then the classic ones TweaksV turns: by a bare name, by a name relative to the game folder's handle, and
// FileRenameInformationEx with replace-if-exists and FILE_RENAME_IGNORE_READONLY_ATTRIBUTE onto the held file made
// read-only, which only goes through with that flag kept; onto a file in the game folder that nothing holds; and a
// name longer than TweaksV copies (2100 characters in the game folder), passed on as made.
static wchar_t g_renamesResult[384];
static void RenameAsked()
{
	const size_t size = sizeof(g_renamesResult) / sizeof(g_renamesResult[0]);
	if (!g_openFile)
	{
		swprintf(g_renamesResult, size, L"renames: no stand-in | ");
		return;
	}
	wchar_t folder[MAX_PATH + 1] = {};
	GetCurrentDirectoryW(MAX_PATH, folder);
	swprintf(g_heldNtName, MAX_PATH * 2, L"\\??\\%ls\\ds_held.txt", folder);
	g_gameFolderHandle = CreateFileW(L".", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

	wchar_t temp[MAX_PATH + 1] = {}, outsideHeld[MAX_PATH + 64] = {}, outsideNew[MAX_PATH + 64] = {};
	GetTempPathW(MAX_PATH, temp);
	swprintf(outsideHeld, MAX_PATH + 64, L"%lstweaksv_ds_held.txt", temp);
	swprintf(outsideNew, MAX_PATH + 64, L"%lstweaksv_ds_new.txt", temp);
	MakeFile(outsideHeld, "old");
	MakeFile(outsideNew, "new");
	const HANDLE outside = CreateFileW(outsideHeld, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	DWORD outsideResult = 9999;
	if (outside != INVALID_HANDLE_VALUE)
	{
		outsideResult = MoveFileExW(outsideNew, outsideHeld, MOVEFILE_REPLACE_EXISTING) ? 0 : GetLastError();
		CloseHandle(outside);
	}
	DeleteFileW(outsideHeld);
	DeleteFileW(outsideNew);

	const HeldWrite noReplace = WriteHeld([]() -> DWORD { return MoveFileExW(L"ds_new.txt", L"ds_held.txt", 0) ? 0 : GetLastError(); });
	const HeldWrite posix = WriteHeld([]() -> DWORD {
		return static_cast<DWORD>(RenameNt(L"ds_new.txt", g_heldNtName, wcslen(g_heldNtName), nullptr, kRenameInformationEx, 0x3)); });
	const HeldWrite bare = WriteHeld([]() -> DWORD {
		return static_cast<DWORD>(RenameNt(L"ds_new.txt", L"ds_held.txt", 11, nullptr, kRenameInformation, 1)); });
	const HeldWrite relative = WriteHeld([]() -> DWORD {
		return static_cast<DWORD>(RenameNt(L"ds_new.txt", L"ds_held.txt", 11, g_gameFolderHandle, kRenameInformation, 1)); });
	const HeldWrite ex = WriteHeld([]() -> DWORD {
		return static_cast<DWORD>(RenameNt(L"ds_new.txt", g_heldNtName, wcslen(g_heldNtName), nullptr, kRenameInformationEx, 0x41)); },
		true);

	wchar_t freeName[MAX_PATH * 2] = {};
	swprintf(freeName, MAX_PATH * 2, L"\\??\\%ls\\ds_free.txt", folder);
	MakeFile(L"ds_free.txt", "old");
	MakeFile(L"ds_new.txt", "new");
	const LONG freeResult = RenameNt(L"ds_new.txt", freeName, wcslen(freeName), nullptr, kRenameInformation, 1);
	const wchar_t* const freeNamed = ContentByName(L"ds_free.txt");
	DeleteFileW(L"ds_free.txt");
	DeleteFileW(L"ds_new.txt");

	static wchar_t longName[MAX_PATH + 2200];
	const int prefix = swprintf(longName, MAX_PATH + 2200, L"\\??\\%ls\\", folder);
	LONG longResult = 0x9999;
	MakeFile(L"ds_new.txt", "new");
	if (prefix > 0)
	{
		wmemset(longName + prefix, L'a', 2100);
		longName[prefix + 2100] = L'\0';
		longResult = RenameNt(L"ds_new.txt", longName, wcslen(longName), nullptr, kRenameInformation, 1);
	}
	DeleteFileW(L"ds_new.txt");
	if (g_gameFolderHandle != INVALID_HANDLE_VALUE)
		CloseHandle(g_gameFolderHandle);

	swprintf(g_renamesResult, size, L"renames: outside %lu, noreplace %lu %ls/%ls, posix 0x%lX %ls/%ls, bare 0x%lX %ls/%ls, "
		L"relative 0x%lX %ls/%ls, ex 0x%lX %ls/%ls, free 0x%lX %ls, long 0x%lX, last error %ls | ", outsideResult, noReplace.result,
		noReplace.named, noReplace.held, posix.result, posix.named, posix.held, bare.result, bare.named, bare.held, relative.result,
		relative.named, relative.held, ex.result, ex.named, ex.held, static_cast<unsigned long>(freeResult), freeNamed,
		static_cast<unsigned long>(longResult), g_lastErrorKept ? L"kept" : L"CHANGED");
}

// dsrenameflood: 20,001 classic replace-renames in the game folder, ds_flood_a.txt onto ds_flood_b.txt and back -
// with DIAGNOSTIC_MODE, more than the 20,000 rename lines TweaksV writes.
static wchar_t g_renameFloodResult[64];
static void FloodRenames()
{
	MakeFile(L"ds_flood_a.txt");
	MakeFile(L"ds_flood_b.txt");
	int renamed = 0;
	for (int i = 0; i < 20001; i++)
	{
		const bool even = i % 2 == 0;
		if (MoveFileExW(even ? L"ds_flood_a.txt" : L"ds_flood_b.txt", even ? L"ds_flood_b.txt" : L"ds_flood_a.txt",
			MOVEFILE_REPLACE_EXISTING))
			renamed++;
	}
	DeleteFileW(L"ds_flood_a.txt");
	DeleteFileW(L"ds_flood_b.txt");
	swprintf(g_renameFloodResult, sizeof(g_renameFloodResult) / sizeof(g_renameFloodResult[0]), L"rename flood: %d renamed | ", renamed);
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

// The script programs (TweaksVScripts, TEST_DUMP_SCRIPT, the Blips part): blip_controller with its code over two
// pages of 16 KB, the second holding 0x123 bytes, three natives and 0x1F0 bytes of strings in one page; other_script
// and blip_imposter - blip_controller's hash under another name - with code and strings of their own. Every page is
// a VirtualAlloc of its own, so that one can be made unreadable. The placements run on a thread of their own,
// as the game's resource placement thread places programs, each through the call at the hook site.
constexpr uint32_t kScriptPage = 0x4000;
constexpr uint32_t kScriptCodeLength = kScriptPage + 0x123;
constexpr uint32_t kScriptStringLength = 0x1F0;
constexpr uint32_t kBlipControllerHash = 0x80CFE974;	// the game's hash of "blip_controller"
struct ScriptStorage
{
	FakeScriptProgram program{};
	unsigned char* codePages[2] = {};
	unsigned char* stringPages[1] = {};
	uint64_t natives[kFakeNatives] = {};
};
static ScriptStorage g_blipController, g_otherScript, g_blipImposter;
struct ScriptPlacement { ScriptStorage* script; bool unknownNative; uint32_t codeLength; uint32_t nativeCount; };
static ScriptPlacement g_scriptPlan[20];
static int g_scriptPlanned = 0;
static int g_scriptPlaced = 0, g_scriptResolved = 0, g_scriptErrorKept = 0;
static DWORD g_scriptThread = 0;
static bool g_scriptPageUnreadable = false;
static wchar_t g_scriptResult[256];

static void MakeScript(ScriptStorage& script, const char* name, uint32_t hash, unsigned char seed)
{
	FakeScriptProgram& p = script.program;
	p.vtable = 0x1111111111111111ull;
	p.unknown08 = 0x0808080808080808ull;
	p.unknown18 = 0x18181818;
	for (uint32_t& value : p.unknown20)
		value = 0x20202020;
	p.unknown30[0] = p.unknown30[1] = 0x3030303030303030ull;
	p.unknown48[0] = p.unknown48[1] = 0x4848484848484848ull;
	p.unknown5C = 0x5C5C5C5C;
	for (uint32_t& value : p.unknown74)
		value = 0x74747474;
	for (unsigned char*& page : script.codePages)
		page = static_cast<unsigned char*>(VirtualAlloc(nullptr, kScriptPage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	script.stringPages[0] = static_cast<unsigned char*>(VirtualAlloc(nullptr, kScriptPage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	for (uint32_t i = 0; i < kScriptPage; i++)
	{
		script.codePages[0][i] = static_cast<unsigned char>(i * 7 + seed);
		script.codePages[1][i] = static_cast<unsigned char>(i * 13 + seed + 1);
		script.stringPages[0][i] = static_cast<unsigned char>('a' + (i + seed) % 26);
	}
	p.codePages = script.codePages;
	p.codeLength = kScriptCodeLength;
	p.nativeCount = static_cast<uint32_t>(kFakeNatives);
	p.natives = script.natives;
	p.nameHash = hash;
	p.name = name;
	p.stringPages = script.stringPages;
	p.stringLength = kScriptStringLength;
}

static void PlanScript(ScriptStorage& script, bool unknownNative = false, uint32_t codeLength = kScriptCodeLength,
	uint32_t nativeCount = static_cast<uint32_t>(kFakeNatives))
{
	if (g_scriptPlanned < static_cast<int>(sizeof(g_scriptPlan) / sizeof(g_scriptPlan[0])))
		g_scriptPlan[g_scriptPlanned++] = { &script, unknownNative, codeLength, nativeCount };
}

// blip_controller's guard (the Blips part), at 0x3FE of its code: its bytes there - the guard, then its way out, a
// NATIVE that makes a blip, and LEAVE - and where its branch lies, for each guard an option asks for. The part may
// change the bytes from the end of the count's NATIVE up to the branch, and a fused branch's opcode; nothing else.
struct BlipGuard
{
	const wchar_t* option;
	unsigned char bytes[40];
	uint32_t size;
	uint32_t branch;
	bool fused;
};
constexpr uint32_t kBlipGuard = 0x3FE;
constexpr uint32_t kGuardSize = 0x15;	// the game's guard: 21 bytes, its way out at 0x413
static const BlipGuard kBlipGuards[] = {
	// as the game has it: the count (native 1), PUSH_CONST_U8 150, ILT, NOT, JZ +10; PUSH_CONST_S16 2000, WAIT (native 2), J -21
	{ L"", { 0x2C, 0x01, 0x00, 0x01, 0x25, 0x96, 0x0C, 0x06, 0x56, 0x0A, 0x00, 0x43, 0xD0, 0x07, 0x2C, 0x04, 0x00, 0x02, 0x55, 0xEB, 0xFF,
		0x2C, 0x00, 0x00, 0x00, 0x2E, 0x00, 0x00 }, 28, 0x406, false },
	{ L"blip240", { 0x2C, 0x01, 0x00, 0x01, 0x25, 0xF0, 0x0C, 0x06, 0x56, 0x0A, 0x00, 0x43, 0xD0, 0x07, 0x2C, 0x04, 0x00, 0x02, 0x55, 0xEB, 0xFF,
		0x2C, 0x00, 0x00, 0x00, 0x2E, 0x00, 0x00 }, 28, 0x406, false },
	// WAIT through native 3, which the program's table of three does not have
	{ L"blipnative", { 0x2C, 0x01, 0x00, 0x01, 0x25, 0x96, 0x0C, 0x06, 0x56, 0x0A, 0x00, 0x43, 0xD0, 0x07, 0x2C, 0x04, 0x00, 0x03, 0x55, 0xEB,
		0xFF, 0x2C, 0x00, 0x00, 0x00, 0x2E, 0x00, 0x00 }, 28, 0x406, false },
	// two NOPs after the count, JZ +10 at 0x408, J -23
	{ L"blippadded", { 0x2C, 0x01, 0x00, 0x01, 0x00, 0x00, 0x25, 0x96, 0x0C, 0x06, 0x56, 0x0A, 0x00, 0x43, 0xD0, 0x07, 0x2C, 0x04, 0x00, 0x02,
		0x55, 0xE9, 0xFF, 0x2C, 0x00, 0x00, 0x00, 0x2E, 0x00, 0x00 }, 30, 0x408, false },
	// IGE and JZ +10 at 0x405, no NOT, J -20
	{ L"bliproom", { 0x2C, 0x01, 0x00, 0x01, 0x25, 0x96, 0x0B, 0x56, 0x0A, 0x00, 0x43, 0xD0, 0x07, 0x2C, 0x04, 0x00, 0x02, 0x55, 0xEC, 0xFF,
		0x2C, 0x00, 0x00, 0x00, 0x2E, 0x00, 0x00 }, 27, 0x405, false },
	// two NOPs, PUSH_CONST_U8 150, ILT_JZ +3 at 0x406 to the body, J +12 to the way out at 0x418; the body, J -24; two NOPs
	{ L"blipfused", { 0x2C, 0x01, 0x00, 0x01, 0x00, 0x00, 0x25, 0x96, 0x5B, 0x03, 0x00, 0x55, 0x0C, 0x00, 0x43, 0xD0, 0x07, 0x2C, 0x04, 0x00,
		0x02, 0x55, 0xE8, 0xFF, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x2E, 0x00, 0x00 }, 33, 0x406, true },
};
static const BlipGuard* g_blipGuard = &kBlipGuards[0];
static bool g_blipTwice = false, g_blipJump = false, g_blipCall = false, g_blipSwitch = false, g_blipCatch = false, g_blipBadByte = false;
// What TweaksV left of what the fake placed: blip_controller's placements and those whose code is as placed but where
// the part may change it, its guard through the branch after the last one, and the other programs' placements and
// those whose code is as placed
static int g_blipPlaced = 0, g_blipAsPlaced = 0, g_otherPlaced = 0, g_otherAsPlaced = 0;
static unsigned char g_blipGuardAfter[40];
static uint32_t g_blipGuardShown = 0;
static wchar_t g_blipResult[256];

// True if a program's code is as the fake placed it, but the bytes from..to of its first page; its second page only
// while it can be read.
static bool AsPlaced(const ScriptStorage& script, const unsigned char (&placed)[kScriptCodeLength], uint32_t from, uint32_t to)
{
	for (uint32_t i = 0; i < kScriptPage; i++)
		if ((i < from || i >= to) && script.codePages[0][i] != placed[i])
			return false;
	return g_scriptPageUnreadable || memcmp(script.codePages[1], placed + kScriptPage, kScriptCodeLength - kScriptPage) == 0;
}

// blip_controller's code, decodable from its start to its end with the game's opcode lengths, as the game's own is
// (PATTERNS.md, "Script programs"): ENTER and instructions of every kind a decoder must step over - a NATIVE, the
// pushes, a CALL, a jump over three NOPs, a SWITCH whose entries hold bytes above the last opcode - then LEAVE; a
// second function whose ENTER's name holds the game's guard, which only a reader of bytes, not of instructions,
// would take for a second guard; a third function with the guard at 0x3FE - its count native index 1 and WAIT index
// 2, as the fake's program has three natives - and its way out, a NATIVE that makes a blip; NOPs to the end of each
// page, every instruction within its page, and LEAVE last. The options add a second guard at 0x800, a jump, a CALL
// or a SWITCH case at 0x100 into the guard's span, a CATCH or the byte 0xA0 at 0x200. Laid again before each
// placement, as the game makes a new program each time - its second page only while it can be read.
static void LayBlipCode(ScriptStorage& script)
{
	static const unsigned char head[] = {
		0x2D, 0x00, 0x02, 0x00, 0x00,				// ENTER, 2 locals, no name
		0x2C, 0x00, 0x00, 0x00,						// NATIVE, no argument, no result, index 0
		0x25, 0x07, 0x26, 0x01, 0x02, 0x27, 0x01, 0x02, 0x03,		// PUSH_CONST_U8, _U8_U8, _U8_U8_U8
		0x28, 0xE8, 0x03, 0x00, 0x00, 0x29, 0x00, 0x00, 0x80, 0x3F,	// PUSH_CONST_U32 1000, PUSH_CONST_F 1.0
		0x5D, 0x00, 0x00, 0x00,						// CALL 0
		0x64, 0x00, 0x01, 0x00,						// PUSH_CONST_U24 256
		0x55, 0x03, 0x00, 0x00, 0x00, 0x00,			// J +3, over three NOPs
		0x65, 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0x06, 0x00, 0x99, 0x88, 0x77, 0x66, 0x00, 0x00,	// SWITCH, two entries, both to the LEAVE
		0x2E, 0x00, 0x00,							// LEAVE
		0x2D, 0x00, 0x00, 0x00, 0x15 };				// ENTER, its name of 21 bytes the guard's
	static const unsigned char second[] = { 0x2E, 0x00, 0x00, 0x2D, 0x00, 0x00, 0x00, 0x00 };	// LEAVE; ENTER, the third function
	static const unsigned char jump[] = { 0x55, 0xFF, 0x02 };									// J +0x2FF, from 0x100 to 0x402
	static const unsigned char call[] = { 0x5D, 0x02, 0x04, 0x00 };								// CALL 0x402, from 0x100
	static const unsigned char cases[] = { 0x65, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFA, 0x02 };	// SWITCH, one case: 0x108 + 0x2FA = 0x402
	static const unsigned char leave[] = { 0x2E, 0x00, 0x00 };
	unsigned char* const code = script.codePages[0];
	memset(code, 0x00, kScriptPage);
	memcpy(code, head, sizeof(head));
	memcpy(code + sizeof(head), kBlipGuards[0].bytes, kGuardSize);
	memcpy(code + sizeof(head) + kGuardSize, second, sizeof(second));
	memcpy(code + kBlipGuard, g_blipGuard->bytes, g_blipGuard->size);
	if (g_blipTwice)
		memcpy(code + 0x800, kBlipGuards[0].bytes, kBlipGuards[0].size);
	if (g_blipJump)
		memcpy(code + 0x100, jump, sizeof(jump));
	if (g_blipCall)
		memcpy(code + 0x100, call, sizeof(call));
	if (g_blipSwitch)
		memcpy(code + 0x100, cases, sizeof(cases));
	if (g_blipCatch)
		code[0x200] = 0x6D;	// CATCH
	if (g_blipBadByte)
		code[0x200] = 0xA0;
	if (g_scriptPageUnreadable)
		return;
	memset(script.codePages[1], 0x00, kScriptPage);
	memcpy(script.codePages[1] + (kScriptCodeLength - kScriptPage) - sizeof(leave), leave, sizeof(leave));
}

// What a copy of blip_controller's n-th placement must hold after its header: the program object, the code as the
// fake placed it, before the call (the unreadable page as zeros), the natives before and after the game's resolution,
// the strings.
static void WriteExpected(int n, const ScriptStorage& script, const unsigned char (&placed)[kScriptCodeLength],
	const uint64_t (&before)[kFakeNatives])
{
	wchar_t name[48];
	swprintf(name, 48, L"script_placed_%d.bin", n);
	const HANDLE file = CreateFileW(name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return;
	DWORD written = 0;
	WriteFile(file, &script.program, sizeof(script.program), &written, nullptr);
	WriteFile(file, placed, sizeof(placed), &written, nullptr);
	WriteFile(file, before, sizeof(before), &written, nullptr);
	WriteFile(file, script.natives, sizeof(script.natives), &written, nullptr);
	WriteFile(file, script.stringPages[0], kScriptStringLength, &written, nullptr);
	CloseHandle(file);
}

static DWORD WINAPI PlaceScripts(void*)
{
	g_scriptThread = GetCurrentThreadId();
	using Resolve = unsigned char (*)(unsigned char*);
	const auto resolve = reinterpret_cast<Resolve>(const_cast<unsigned char*>(Rel32Target(g_scriptCallSite)));
	int blipPlacements = 0;
	for (int i = 0; i < g_scriptPlanned; i++)
	{
		ScriptStorage& script = *g_scriptPlan[i].script;
		FakeScriptProgram& p = script.program;
		// as the game's constructor leaves the program before the call: a new program's code, the natives' hashes in
		// the table - and what it placed, which TweaksV may change in the call
		uint64_t before[kFakeNatives] = {};
		for (size_t k = 0; k < kFakeNatives; k++)
			before[k] = script.natives[k] = g_fakeNatives[k].hash;
		if (g_scriptPlan[i].unknownNative)
			before[1] = script.natives[1] = kUnknownNativeHash;
		p.codeLength = g_scriptPlan[i].codeLength;
		p.nativeCount = g_scriptPlan[i].nativeCount;
		if (&script == &g_blipController)
			LayBlipCode(script);
		static unsigned char placed[kScriptCodeLength];
		memcpy(placed, script.codePages[0], kScriptPage);
		if (g_scriptPageUnreadable)
			memset(placed + kScriptPage, 0, kScriptCodeLength - kScriptPage);
		else
			memcpy(placed + kScriptPage, script.codePages[1], kScriptCodeLength - kScriptPage);
		SetLastError(0);
		const unsigned char resolved = resolve(reinterpret_cast<unsigned char*>(&p));
		if (GetLastError() == kResolveLastError)
			g_scriptErrorKept++;
		g_scriptPlaced++;
		if (resolved == 1)
			g_scriptResolved++;
		if (&script == &g_blipController)
		{
			blipPlacements++;
			if (p.codeLength == kScriptCodeLength && p.nativeCount == kFakeNatives)
				WriteExpected(blipPlacements, script, placed, before);
			g_blipPlaced++;
			if (AsPlaced(script, placed, kBlipGuard + 4, g_blipGuard->branch + (g_blipGuard->fused ? 1 : 0)))
				g_blipAsPlaced++;
			g_blipGuardShown = g_blipGuard->branch + 3 - kBlipGuard;
			memcpy(g_blipGuardAfter, script.codePages[0] + kBlipGuard, g_blipGuardShown);
		}
		else
		{
			g_otherPlaced++;
			if (AsPlaced(script, placed, 0, 0))
				g_otherAsPlaced++;
		}
	}
	return 0;
}

// The placements the options ask for, once TweaksV has started; then a while for a problem reported on the
// placing thread to reach its own message box (TweaksV shows such a box a second after the problem).
// "script: 2 placed on thread 1234 through a TweaksV stub, resolved 1 of 2, last error kept 2 of 2,
// image 140695... 1236992 1700000000 | " - through the game function (the fake's own resolution function), a
// TweaksV stub (outside the game), or nothing placed with a call to another mod (option "scripthooked")
static void PlaceScriptsAsAsked(int argc, wchar_t** argv)
{
	const size_t size = sizeof(g_scriptResult) / sizeof(g_scriptResult[0]);
	const unsigned char* const otherMod = ImageEnd() + 0x60000;
	const unsigned char* const target = Rel32Target(g_scriptCallSite);
	const wchar_t* const through = target == reinterpret_cast<const unsigned char*>(&FakeResolveNatives) ? L"the game function"
		: target == otherMod ? L"another mod" : InImage(target) ? L"elsewhere in the game" : L"a TweaksV stub";
	if (HasOption(argc, argv, L"scripthash"))
	{
		PlanScript(g_blipImposter);
		PlanScript(g_blipImposter);
	}
	else if (HasOption(argc, argv, L"scriptother"))
	{
		PlanScript(g_otherScript);
		PlanScript(g_blipController);
	}
	else if (HasOption(argc, argv, L"scripttwice"))
	{
		PlanScript(g_blipController);
		PlanScript(g_blipController, true);
	}
	else if (HasOption(argc, argv, L"scriptmany"))
		for (int i = 0; i < 17; i++)
			PlanScript(g_blipController);
	else if (HasOption(argc, argv, L"scripthuge"))
	{
		PlanScript(g_blipController, false, (16u << 20) + 1);
		PlanScript(g_blipController, false, kScriptCodeLength, 65537);
	}
	else if (HasOption(argc, argv, L"script") || HasOption(argc, argv, L"scriptpage") || HasOption(argc, argv, L"scripthooked"))
		PlanScript(g_blipController);
	if (g_scriptPlanned == 0)
	{
		swprintf(g_scriptResult, size, L"script: none placed, call to %ls | ", through);
		return;
	}
	if (target == otherMod || (InImage(target) && target != reinterpret_cast<const unsigned char*>(&FakeResolveNatives)))
	{
		swprintf(g_scriptResult, size, L"script: none placed, call to %ls | ", through);	// nothing is there to call
		return;
	}
	if (HasOption(argc, argv, L"scriptpage"))
	{
		DWORD old = 0;
		g_scriptPageUnreadable = VirtualProtect(g_blipController.codePages[1], kScriptPage, PAGE_NOACCESS, &old) != FALSE;
	}
	const HANDLE thread = CreateThread(nullptr, 0, PlaceScripts, nullptr, 0, nullptr);
	if (thread)
	{
		WaitForSingleObject(thread, 30000);
		CloseHandle(thread);
	}
	Sleep(1500);
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(ImageBase() + reinterpret_cast<const IMAGE_DOS_HEADER*>(ImageBase())->e_lfanew);
	swprintf(g_scriptResult, size, L"script: %d placed on thread %lu through %ls, resolved %d of %d, last error kept %d of %d, "
		L"image %llu %lu %lu | ", g_scriptPlaced, static_cast<unsigned long>(g_scriptThread), through, g_scriptResolved, g_scriptPlaced,
		g_scriptErrorKept, g_scriptPlaced, static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(ImageBase())),
		static_cast<unsigned long>(nt->OptionalHeader.SizeOfImage), static_cast<unsigned long>(nt->FileHeader.TimeDateStamp));

	// "blips: 2 placed, guard 2C 01 00 01 43 6C 07 0B 56 0A 00, the rest as placed 2 of 2, other programs as placed 0 of 0 | " -
	// blip_controller's guard from its count's NATIVE through its branch after the last placement ("-" with none)
	wchar_t guard[160] = L"-";
	size_t at = 0;
	for (uint32_t i = 0; i < g_blipGuardShown; i++)
		at += static_cast<size_t>(swprintf(guard + at, sizeof(guard) / sizeof(guard[0]) - at, i ? L" %02X" : L"%02X", g_blipGuardAfter[i]));
	swprintf(g_blipResult, sizeof(g_blipResult) / sizeof(g_blipResult[0]), L"blips: %d placed, guard %ls, the rest as placed %d of %d, "
		L"other programs as placed %d of %d | ", g_blipPlaced, guard, g_blipAsPlaced, g_blipPlaced, g_otherAsPlaced, g_otherPlaced);
}

int wmain(int argc, wchar_t** argv)
{
	const wchar_t* asi = argc > 1 ? argv[1] : L"TweaksV.asi";

	// The call at the landing site must lead to g_landingCallee.
	const int32_t rel32 = static_cast<int32_t>(reinterpret_cast<intptr_t>(g_landingCallee) -
		(reinterpret_cast<intptr_t>(g_landingSite) + 5));
	memcpy(const_cast<unsigned char*>(g_landingSite) + 1, &rel32, 4);
	if (HasOption(argc, argv, L"twos3"))	// a second site of the same shape, its call to the same function
	{
		const int32_t copyRel32 = static_cast<int32_t>(reinterpret_cast<intptr_t>(g_landingCallee) -
			(reinterpret_cast<intptr_t>(g_landingSiteCopy) + 5));
		g_landingSiteCopy[0] = 0xE8;
		memcpy(const_cast<unsigned char*>(g_landingSiteCopy) + 1, &copyRel32, 4);
	}

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
	if (HasOption(argc, argv, L"heaphuge"))		// another heap adjuster set more than HEAP_SIZE allows
	{
		const Site heap = SITE(g_heapSite, 1);
		SetImm32(heap, 3072u << 20);
	}
	if (HasOption(argc, argv, L"heapodd"))		// a value that is not a whole number of MB
	{
		const Site heap = SITE(g_heapSite, 1);
		SetImm32(heap, (648u << 20) | 1);
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
	SetRel32(g_modkitExtraSite, g_oldModkitArray);
	if (HasOption(argc, argv, L"modkitextra"))		// a seventh lea of the array, which no pattern names
		g_modkitExtra[0] = 0x48;

	// Every packfile reference points at the game's own array, the indirect size at its variable
	// and the streaming configuration call at the game's function...
	for (const Site& site : g_packRefs)
		SetRel32(site, g_oldPackfileArray);
	SetRel32(g_packR31aSite, g_oldPackfileArray);
	SetRel32(g_packR31bSite, g_oldPackfileArray);
	SetRel32(g_packR31aCopySite, g_oldPackfileArray);
	SetRel32(g_packExtraSite, g_oldPackfileArray);
	if (OwnBuild() >= ((1158u << 16) | 13))	// only this build's own reference is one: the other build's lea is a 32-bit one
		g_packR31b[17] = 0x44;
	else
		g_packR31a[10] = 0x44;
	if (HasOption(argc, argv, L"twor31a"))		// ...and the 1158.13+ pattern matches twice
	{
		g_packR31aCopy[0] = 0xBF;
		g_packR31aCopy[10] = 0x4C;
	}
	if (HasOption(argc, argv, L"packextra"))		// ...or a 34th reference no pattern names
		g_packExtra[0] = 0x48;
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

	// The call at the script programs' hook site leads to the fake's own resolution function...
	MakeScript(g_blipController, "blip_controller", kBlipControllerHash, 1);
	MakeScript(g_otherScript, "other_script", 0x0F74EC00, 50);		// the game's hash of "other_script"
	MakeScript(g_blipImposter, "blip_imposter", kBlipControllerHash, 100);
	SetRel32(g_scriptCallSite, reinterpret_cast<const void*>(&FakeResolveNatives));
	if (HasOption(argc, argv, L"scripthooked"))	// ...unless another mod redirected it already
		SetRel32(g_scriptCallSite, ImageEnd() + 0x60000);
	// blip_controller's guard as an option asks for it, and what more its code holds
	for (const BlipGuard& guard : kBlipGuards)
		if (guard.option[0] && HasOption(argc, argv, guard.option))
			g_blipGuard = &guard;
	g_blipTwice = HasOption(argc, argv, L"bliptwice");
	g_blipJump = HasOption(argc, argv, L"blipjump");
	g_blipCall = HasOption(argc, argv, L"blipcall");
	g_blipSwitch = HasOption(argc, argv, L"blipswitch");
	g_blipCatch = HasOption(argc, argv, L"blipcatch");
	g_blipChanged = HasOption(argc, argv, L"blipchanged");
	g_blipBadByte = HasOption(argc, argv, L"blipbadbyte");
	// The blip pool: 162 slots in use below its high-water mark, 170 - eight below it empty - and two in use beyond it,
	// which the part's measurement must not count; its two sites lead to it
	for (int slot = 0; slot < 170; slot++)
		g_blipPool[slot] = slot % 20 == 10 ? 0 : static_cast<uint64_t>(0x1000 + slot);
	g_blipPool[500] = g_blipPool[1999] = 0x5000;
	*g_blipMark = 170;
	SetRel32(g_blipPoolRef, g_blipPool);
	for (const Site& site : g_blipMarkRefs)
		SetRel32(site, g_blipMark);
	// The parts measured first: each limit's sites lead to its data, as its options ask for them
	// [weapon infos]
	// The weapon infos: the sites lead to the array and to the allocation, but where an option asks otherwise; the array
	// stays empty while TweaksV starts, unless "wisetup" sets it up first
	{
		const auto array = reinterpret_cast<const unsigned char*>(g_weaponInfosArray);
		const auto outside = ImageEnd() + 0x70000;
		SetRel32(g_weaponInfosLeaRef, array);
		SetRel32(g_weaponInfosExtraRef, array);
		SetRel32(g_weaponInfosCall, HasOption(argc, argv, L"wihooked") ? outside : const_cast<const unsigned char*>(g_weaponInfosAllocation));
		SetRel32(g_weaponInfosBlockRef, HasOption(argc, argv, L"wioutside") ? outside : array);
		WeaponInfosPointCapacityAt(array + (HasOption(argc, argv, L"wicapacity") ? 0xC : 0xA));
		SetRel32(g_weaponInfosLoaderRefs[0], array + (HasOption(argc, argv, L"wicount") ? 0x10 : 8));
		SetRel32(g_weaponInfosLoaderRefs[1], array + 8);
		SetRel32(g_weaponInfosLoaderRefs[2], array);
		if (HasOption(argc, argv, L"wimismatch"))	// the capacity 161
			g_weaponInfosInit[34] = 0xA1;
		if (HasOption(argc, argv, L"wizero"))		// both limits 0
		{
			g_weaponInfosInit[11] = 0;
			g_weaponInfosInit[34] = 0;
		}
		if (HasOption(argc, argv, L"wibytes"))		// a jump at the allocation's start
			g_weaponInfosAllocation[0] = 0xE9;
		if (HasOption(argc, argv, L"wisize"))		// the loader's blob size 0x100
		{
			g_weaponInfosLoader[45] = 0x00;
			g_weaponInfosLoader[46] = 0x01;
		}
		if (HasOption(argc, argv, L"winotstart"))	// no padding before the loader's start
			g_weaponInfosLoader[0] = 0x48;
		if (HasOption(argc, argv, L"wiextra"))		// a second store of a block into the array
			g_weaponInfosExtra[0] = 0x48;
		if (HasOption(argc, argv, L"wisetup"))		// the initializer ran before TweaksV
			WeaponInfosInitialize(0, 152);
		WeaponInfosKeepSites();
	}
	// [weapon components]
	// The weapon components: 412 definitions from 23 files in the list - 470 from 25 with "wcfull" and "wcover" - and every
	// site leading to the list, its count, the files' count, the configuration and the size lookup...
	WeaponComponentsLoad(HasOption(argc, argv, L"wcfull") || HasOption(argc, argv, L"wcover") ? 470 : 412,
		HasOption(argc, argv, L"wcfull") || HasOption(argc, argv, L"wcover") ? 25 : 23);
	if (HasOption(argc, argv, L"wcover"))	// ...the 471st definition stored over the count, which reads its pointer's low half
	{
		const uint64_t past = 0x000001D4A3C00000ull + 0xC0ull * 470;
		g_weaponComponentsStore->count = static_cast<int32_t>(static_cast<uint32_t>(past));
		g_weaponComponentsStore->unused = static_cast<uint32_t>(past >> 32);
		g_weaponComponentsPlacedStore = *g_weaponComponentsStore;
	}
	{
		WeaponComponentsStore* const store = g_weaponComponentsStore;
		const void* const list = HasOption(argc, argv, L"wcmoved") ? static_cast<const void*>(ImageEnd() + 0x70000) : store->list;	// ...unless another mod moved it
		const unsigned char* const count = reinterpret_cast<unsigned char*>(&store->count) + (HasOption(argc, argv, L"wcgap") ? 4 : 0);
		WeaponComponentsPoint(g_weaponComponentsRebuild, 2, 10, count);
		WeaponComponentsPoint(g_weaponComponentsRebuild, 14, 18, &store->files);
		WeaponComponentsPoint(g_weaponComponentsRebuild, 34, 38, list);
		WeaponComponentsPoint(g_weaponComponentsSort, 3, 7, count);
		WeaponComponentsPoint(g_weaponComponentsSort, 14, 18, list);
		WeaponComponentsPoint(g_weaponComponentsUpdate, 3, 7, count);
		WeaponComponentsPoint(g_weaponComponentsUpdate, 12, 16, count + (HasOption(argc, argv, L"wccount") ? 8 : 0));	// ...or the store elsewhere
		WeaponComponentsPoint(g_weaponComponentsPool, 3, 7, const_cast<void**>(&g_weaponComponentsConfiguration));
		WeaponComponentsPoint(g_weaponComponentsPool, 19, 23, HasOption(argc, argv, L"wchooked") ? static_cast<const void*>(ImageEnd() + 0x71000)
			: reinterpret_cast<const void*>(&WeaponComponentsPoolSize));	// ...or the call another mod's hook
		for (auto& row : g_weaponComponentsLeas)
			WeaponComponentsPoint(row, 3, 7, list);
		WeaponComponentsPoint(g_weaponComponentsLeaExtra, 3, 7, list);
		if (HasOption(argc, argv, L"wcsplit"))	// ...or one lea points elsewhere in the game
			WeaponComponentsPoint(g_weaponComponentsLeas[4], 3, 7, store->list + 1);
		if (HasOption(argc, argv, L"wcextra"))	// ...or an 18th loads it
			g_weaponComponentsLeaExtra[0] = 0x48;
		if (HasOption(argc, argv, L"wcprefix"))	// ...or one has a prefix the game's never have
			g_weaponComponentsLeas[0][0] = 0x49;
		WeaponComponentsCopySites(g_weaponComponentsPlacedSites, sizeof(g_weaponComponentsPlacedSites));
	}
	// [siren ids]
	SirenIdsSetup(argc, argv);
	// [siren count]
	// [ped props]
	// [texture budget]
	// [streaming memory]

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
	if (HasOption(argc, argv, L"unload"))	// TweaksV pins itself: freed, it must stay, and its hooks with it. One FreeLibrary
	{										// would not show that: TweaksV's own threads hold references of their own for a while
		int frees = 0;						// (RageOpenV's start-up check, 30 s), so it is freed until it is gone, or 8 times -
		while (frees < 8 && GetModuleHandleW(asi))	// pinned, it stays however often
		{
			FreeLibrary(module);
			frees++;
			Sleep(100);
		}
		wprintf(L"unload: %ls after %d x FreeLibrary | ", GetModuleHandleW(asi) ? L"still loaded" : L"unloaded", frees);
	}
	CallBoxSearches();	// the box streamer searches, as the game would call them
	PlaceScriptsAsAsked(argc, argv);	// the script programs, as the game's resource placement thread places them
	if (dstorage && HasOption(argc, argv, L"dsnofactory"))
		swprintf(g_dstorageResult, sizeof(g_dstorageResult) / sizeof(g_dstorageResult[0]), L"dstorage: not called | ");
	else if (dstorage)
		CallDStorage(dstorage, HasOption(argc, argv, L"dsotherinterface") ? OtherInterface::First
			: HasOption(argc, argv, L"dsotherlater") ? OtherInterface::Later
			: HasOption(argc, argv, L"dsotheronly") ? OtherInterface::Only : OtherInterface::None);
	if (HasOption(argc, argv, L"dsshare"))
		ShareThroughStandIn();
	if (HasOption(argc, argv, L"ownshare"))
		ShareOwnOpens();
	if (HasOption(argc, argv, L"dstypes"))
		OpenTypes();
	if (HasOption(argc, argv, L"fileopens"))
		OpenFiles();
	if (HasOption(argc, argv, L"dsopenflood"))
		FloodOpens();
	if (HasOption(argc, argv, L"dsreplace"))
		ReplaceHeldFile();
	if (HasOption(argc, argv, L"dsrenames"))
		RenameAsked();
	if (HasOption(argc, argv, L"dsrenameflood"))
		FloodRenames();
	// The parts measured first: what the game does with each limit once TweaksV has started, as its options ask
	// [weapon infos]
	if (!HasOption(argc, argv, L"wisetup"))	// the game's initializer of the weapon info blob array, then its loader
		WeaponInfosInitialize(HasOption(argc, argv, L"wiraised") ? 512 : 0, HasOption(argc, argv, L"wiraised") ? 170 : 152);
	if (HasOption(argc, argv, L"wipacks"))
		WeaponInfosMountPacks();
	// [weapon components]
	if (HasOption(argc, argv, L"wcpack"))	// a pack's weaponcomponents.meta loads: the list is rebuilt from 24 files
	{
		Sleep(11000);
		WeaponComponentsLoad(430, 24);
	}
	// [siren ids]
	SirenIdsLater();
	// [siren count]
	// [ped props]
	// [texture budget]
	// [streaming memory]
	if (HasOption(argc, argv, L"wait"))
		Sleep(11000);	// TweaksV's UNLOCK_FILES summary comes 10 s after the first open it counts, the OpenFile summary 10 s
						// after the first OpenFile, the interface check 10 s after the request
	if (HasOption(argc, argv, L"waitlong"))
		Sleep(31000);	// TweaksV's DirectStorage factory check comes 30 s after it started
	if (HasOption(argc, argv, L"dsotheronly"))
		Sleep(1500);	// the interface check's line comes 10 s after the request, a little after "wait" began
	if (HasOption(argc, argv, L"dsflood"))
		FloodOpenFile();

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
	wprintf(L"%ls%ls%ls%ls%ls%ls%ls%ls%ls%ls", g_dstorageResult, g_floodResult, g_shareResult, g_ownResult, g_fileOpensResult,
		g_replaceResult, g_renamesResult, g_renameFloodResult, g_scriptResult, g_blipResult);
	// The parts measured first: each limit's field of the report, and the stub of a call it redirected
	// [weapon infos]
	wprintf(L"weapon infos: sites %ls, array %u of %u%ls | ", WeaponInfosSitesAsLaid() ? L"as laid" : L"CHANGED",
		static_cast<unsigned>(g_weaponInfosArray->count), static_cast<unsigned>(g_weaponInfosArray->capacity),
		g_weaponInfosArray->block == g_weaponInfosBlock ? L"" : L", its block CHANGED");
	// [weapon components]
	{
		unsigned char sites[sizeof(g_weaponComponentsPlacedSites)];
		const size_t size = WeaponComponentsCopySites(sites, sizeof(sites));
		wprintf(L"wc: sites %ls, data %ls, size call %ls | ", memcmp(sites, g_weaponComponentsPlacedSites, size) == 0 ? L"as placed" : L"CHANGED",
			memcmp(g_weaponComponentsStore, &g_weaponComponentsPlacedStore, sizeof(WeaponComponentsStore)) == 0 ? L"as placed" : L"CHANGED",
			Rel32Target(g_weaponComponentsSizeCall) == reinterpret_cast<const unsigned char*>(&WeaponComponentsPoolSize) ? L"to the lookup" : L"elsewhere");
		CheckStub(Rel32Target(g_weaponComponentsSizeCall));
	}
	// [siren ids]
	SirenIdsReport();
	// [siren count]
	// [ped props]
	// [texture budget]
	// [streaming memory]
	CheckStub(Rel32Target(g_packCallSite));
	CheckStub(Rel32Target(g_scriptCallSite));
	for (const Site& site : g_boxCalls)
		CheckStub(Rel32Target(site));
	RovForEachCallTarget(CheckStub);
	wprintf(L"stubs: %d, %ls | ", g_stubs, g_writableStubs == 0 ? L"all RX" : L"NOT all RX");
	wprintf(L"protect: %ls %ls\n", g_notRestored == 0 ? L"all restored" : L"NOT restored",
		g_siteProtect == PAGE_EXECUTE_READ ? L"(RX)" : L"(not RX)");
	return 0;
}
