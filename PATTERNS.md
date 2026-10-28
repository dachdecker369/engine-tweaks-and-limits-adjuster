# Patterns

Every byte pattern TweaksV searches for in `GTA5_Enhanced.exe`, and the game build each one was last verified on. TweaksV searches only the game's code (its executable sections). Each pattern is defined once, in the table in `source\TweaksVPatterns.h`, under the label it has here (`H-E1`, `P-E-R31a`, ...), and is written here exactly as there; `?` matches any byte. The tests check that this file and the table list the same patterns, and `TweaksV.log` names each pattern by its label. RVA is where each pattern matched on 1.0.1158.16, counted from the start of `GTA5_Enhanced.exe` in memory. What each match holds - the reference, limit, call or function - is here; what each part does with it is in [SPECIFICATIONS.md](SPECIFICATIONS.md).

## Status

| | |
|---|---|
| **Game Build** | 1.0.1158.16 (PE timestamp 0x6AA45F10) |
| **Found** | 1.0.1158.16 uses 94 of the table's 97 patterns, and the check boot (`DIAGNOSTIC_MODE=1` and `CONSOLE=1`, every part on, with or without `DRY_RUN=1`) looks for every one: the Blips part the script programs' `Y-E-H1`, `Y-E-N1` and `Y-E-I1` on every boot it is on, and its own `L-E-P1` and `L-E-P2` with `DIAGNOSTIC_MODE=1`; a normal boot looks for fewer (`R-E-C1` only with `CONSOLE=1`, `R-E-F1..F8` only with a mods folder in the game's current directory, normally the game folder, or with `DIAGNOSTIC_MODE=1`, `L-E-P1` and `L-E-P2` only with `DIAGNOSTIC_MODE=1`) |
| **Untested** | `L-E-P1` and `L-E-P2`, found once each in the dump of 1.0.1158.16 and not yet by a boot: the first check boot with the Blips part looks for them. The script programs' three were found once each by the switch's boot of 2026-10-27, every other pattern by the check boots before. A part with `ENABLED=0` looks for none of its patterns. |
| **Matches** | All match exactly once, except `D-E-L12`, which matches exactly 7 times (the check seven decal-placing functions share). To check: boot once with `DIAGNOSTIC_MODE=1` and `CONSOLE=1`, every part on. Log lists each pattern address and number of matches, and the bytes around each match of a pattern that matches more than once. Every pattern must match exactly once (`D-E-L12`: exactly 7 times). |

The build uses 94 of the table's 97 patterns: Packfile picks one of three direct size patterns and one of two references by game build. Six more rows below are kept as removed:
- `R-E-D3` belonged to code that was never used. Removed.
- `R-E-M0`, `R-E-M1`, `R-E-M2`, `R-E-D1` and `R-E-D2` belonged to the newmods folder, which 2.0.0 drops. Removed.

The DirectStorage part uses no patterns. The hook that does its job is on `NtCreateFile` in `ntdll.dll`, which every file open of the game passes; what it relies on is how the game folder's files are locked: the game asks DirectStorage to open every file in its folder at start-up, subfolders included (`Redistributables\*.cab`, `BattlEye\*`, `update\`, `x64\` - on my 2.1.1 boot 463 files in the first 10 seconds, 184 of them `.rpf` and `.cache`, 279 others), and DirectStorage opens each with `CreateFileW` for reading while letting others only read it - `NtCreateFile` with access 0x80100080 (`GENERIC_READ`, `SYNCHRONIZE`, `FILE_READ_ATTRIBUTES`), share `R--`, options 0x48 - on the thread that called `OpenFile`, right before it returns, and keeps the handle. A relative path reaches `NtCreateFile` as a name relative to the current directory's handle, which the hook tells by the folder's identity. The diagnostic aids hook `DStorageGetFactory` in `dstorage.dll` and, on the factory the game gets for the interface `{6924EA0C-C3CD-4826-B10A-F64F4ED927C1}`, its function [4], `OpenFile` (`DStorageCore.dll+0x3F9D0` with DirectStorage 1.2.2311.1405 on 1.0.1158.16) - diagnostic facts now, nothing depends on them. The game reads more than the game folder's files through that `OpenFile`: its saves and `pc_settings.bin` (in `Documents\Rockstar Games\GTAV Enhanced\Profiles`, the saves named `SGTA500xx`) and its cloud files in `%TEMP%` (`cloud_*.dat`) too, and a mods loader hands it loose files of its own folder: NaturalVision Enhanced's `onigiri.asi` 91 of them, `onigiri\common\data\gameconfig.xml` the first.

### When Game Updates

1. Start the game with `DIAGNOSTIC_MODE=1` and `DRY_RUN=1` - and `CONSOLE=1`, with every part on, so every pattern is looked for, the script programs' three by the Blips part. A dry run does not redirect their call, so the Blips part sees no script program then: whether it still finds `blip_controller`'s guard shows only on a boot without `DRY_RUN`, where its line, or its problem, says so. The game is not patched then. `TweaksV.log` lists every pattern not found, by its label.
2. Check whether the project the pattern came from has changed it (`CREDITS.md` names them).
3. Update the pattern in `source\TweaksVPatterns.h` and in this file - the tests check that both agree - and confirm that it matches exactly once. `GTA5_Enhanced.exe` is encrypted on disk, so look for the code in a copy of the running game: a test build with `TEST_DUMP_CODE=60` saves one (README, Test Builds) - the file on disk shows random bytes where the patterns match in memory, because the code is decrypted only there. The game is built with clang, not MSVC: its code writes `31 C9` and `45 31 C0` for xor where MSVC writes `33 C9`, and the image has a `.retplne` section, two `.text` sections and a `.pdata` function table - so a pattern from a GTA V Legacy mod never carries over and is found anew here. To analyse a dump, rebuild it as a PE (the headers and the saved sections at their offsets, `.data` as zeros) and let Ghidra's headless analyser take it; the `.pdata` table gives the functions. Re-find each site from its anchor pattern rather than trusting its old RVA, and treat a site that moved as new findings, never a guess.
4. DirectStorage: with `DIAGNOSTIC_MODE=1` the log names the factory's functions and says why `OpenFile` cannot be logged (another interface, an `OpenFile` outside DirectStorage's DLLs) - diagnostic facts only; the part's job depends on how the files are opened (item 5).
5. The game facts no pattern protects, to check in the dump even when every pattern still matches, because a change in them would go wrong silently: RageOpenV's `fiPackfile` fields (counted from the object's start, 8 bytes further on than Legacy's, which are in brackets: the entry table at +0x28 [+0x20], its count at +0x30 [+0x28], the current file offset at +0xBC [+0xB4]; entries of 16 bytes with the name offset at +0, the type at +4 and the encryption at +12) and the fixed distances of its sites (GetFileTime and GetFileSize 0x10D before `R-E-F2` and `R-E-F3`, 0x15D before `R-E-F6` and `R-E-F7`; the ParseHeader call 19 bytes into `R-E-E5`; the console call 16 bytes into `R-E-C1`), and that every ParseHeader that returns true passes the FindEncryption call (`R-E-E1`) first - once the header's magic is RPF7, that call runs before any branch on the readHeader flag or on a header handed in, and the function returns true only for RPF7 (1.0.1158.16) - which RageOpenV's hooks rely on: the tag FindEncryption sees is the one of the archive being parsed; the archive count at offset 24 of the streaming configuration that `P-E-H1`'s call reads, and that call's single argument; the tree-search calls 30, 34, 42 and 30 bytes into `B-E-L1..L4`, their fifth argument on the stack, and that a search stops 16 entries short of the capacity it is handed (`kSearchHeadroom`, against which the box streamer's measurement counts the searches the game's own list could not hold); the decal manager's loop (the start offset at +1 and the lea at +5 of `D-E-L10`, the pass and the end at +2 and +8 of `D-E-L11`); each array's references are exactly the ones listed - the modkit array's 6 leas, the packfile array's 33 references (25 leas and the 8 stores of `P-E-R23..R30`), the box streamer list's 8 and the decal definitions array's 31 leas - and the scan refuses otherwise; the heap adjuster's rule that a heap of 1024 MB or more is another adjuster's (a build whose own heap is that large would be left alone, with only a log line); DirectStorage's opens - that the game still locks its folder's files through opens that read only and share reading, the opens `UNLOCK_FILES` widens - every file of its folder tree, 280 in the first 10 seconds on 1.0.1158.16, when its RPF cache check is skipped as RageOpenV's part does, and its archives alone when the check runs: a build that opened them for writing or exclusively would leave them locked without an error, so check the 10-second summary: the game folder's files among the opens given write and delete sharing, by type, and few left as they were (with `DIAGNOSTIC_MODE=1` each open is named with its access and sharing); and the script programs' (section "Script programs"): the offsets +0x2C (the native count), +0x58 (the name's hash), +0x60 (the name) and +0x68 (the string page table), the page size of 0x4000 and its rule - every instruction within one page but SWITCH's entry table - and the opcodes' values and lengths, against the game's length table (.rdata 0x2610C50, the dispatch table + 0x800), the native table stored rotated by (code length + index) mod 64, holding the natives' hashes before `Y-E-H1`'s call and their handlers' addresses after it, the function behind that call taking the program alone and returning a byte, the resource placement thread the call runs on at every placement, a save load included, and the registry the program enters only after it, and the name's hash (`blip_controller` gives 0x80CFE974); and the Blips facts (section "Blips"): the pool's 2,000 slots and the three the count leaves out, the count's answers with the pool full (1,997 to 2,000) - `L-E-P1` and `L-E-P2` give the pool's and its mark's addresses, not the number of slots, which the part's measurement takes as 2,000 - and the guard's shape in `blip_controller`: a script update can move it, change its natives' indices, its form or its threshold; the part finds it wherever it lies and in each form its match takes, and refuses one it cannot match, but the threshold 150 it raises, the room it needs and 1,997, the highest limit, are this build's. Last, the encoding of the paths the eight file functions take (`R-E-F1..F8`), settled in the dump of 1.0.1158.16 on 2026-10-27: each takes its path as a char string and turns it into UTF-16 with code page 65001, UTF-8, flags 0, before any Win32 call - `B9 E9 FD 00 00` (`mov ecx, 0xFDE9`), `31 D2`, `49 89 D8` or `49 89 F8`, then `FF 15` through the import slot at RVA 0x51215F4, with MultiByteToWideChar's six arguments - at RVA 0x10B158 (`R-E-F1`), 0x10B973 (`F2`), 0x10B823 (`F3`), 0x10BC55 (`F4`), 0x108028 (`F5`), 0x108781 (`F6`), 0x108971 (`F7`) and 0x108CA0 (`F8`); it converts at most 259 bytes of the path, into 260 wide characters, then makes every run of `/` or `\` one `\` and opens the file by its UTF-16 path, which Windows compares with the files' names by their upper case. RageOpenV's list of the mods folder and its lookups follow that: names in UTF-8 on every Windows, paths of at most 254 bytes, because the hooks hand the game `mods/` and the path, and it reads 259. A game build whose functions took their paths in the ANSI code page would leave every name outside ASCII unserved, without an error - so check the conversion at the eight sites, and the 259 bytes.

## HeapAdjuster

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| H-E1 | `BA ? ? ? ? 45 31 C0 41 B1 01 E8 ? ? ? ? 48 8D 0D` | +1: imm32 of `mov edx` - heap size in bytes; 646 MB on current build | found | 0x13F7 |

## ModkitLimit

References to modkit array (rel32 of a `lea` - moved to new array). The six are every reference the game's code has to the array (RVA 0x471E610 on the current build: 1024 entries of 2 bytes): no other instruction reaches it or into it, and no stored pointer holds its address; the relocation core checks the count with a scan for every `lea` and `mov` that reaches it. Modkit IDs are 16-bit, which caps the array at 65536 entries.

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| M-E-R1 | `31 c9 48 8d 05 ? ? ? ? eb` | +5 | found | 0x134A56F |
| M-E-R2 | `89 c2 48 8d 0d ? ? ? ? eb` | +5 | found | 0x13785B2 |
| M-E-R3 | `41 0f b7 c2 48 8d 0d ? ? ? ? 0f b7 04 41` | +7 | found | 0x7A6F4B |
| M-E-R4 | `57 48 83 ec ? 48 8d 0d ? ? ? ? 41 b8` | +8 | found | 0x1349BD1 |
| M-E-R5 | `48 8d 3d ? ? ? ? 0f b7 3c 47` | +3 | found | 0xD225CE |
| M-E-R6 | `77 ? 48 8d 15 ? ? ? ? 0f b7 14 42` | +5 | found | 0x1E50467 |

Hard-coded limits (imm32 = size x mul + add; every one MUST decode to the same game size; 1024 on current build):

| Label | Pattern | Offset, mul, add | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| M-E-L1 | `0f b7 c0 66 ba ? ? 3d ? ? ? ? 77` | 8, 1, -1 | found | 0x1E5045B |
| M-E-L2 | `48 83 c5 ? 48 83 c0 ? 48 83 c7` | 15, 1, 3 (the imm32 of `cmp rdi, imm32`, `48 81 FF` at 12) | found | 0x137863F |
| M-E-L3 | `74 ? 48 83 c5 ? 48 83 c3 ? 48 83 c0 ? 48 3d` | 16, 1, 3 | found | 0x137868D |
| M-E-L4 | `48 83 c7 ? 48 83 c2 ? 48 83 c5 ? 48 81 fd` | 15, 1, 3 | found | 0x134A603 |
| M-E-L5 | `48 83 c7 ? 48 83 c2 ? 48 83 c6 ? 48 81 fe` | 15, 1, 3 | found | 0x134A657 |
| M-E-L6 | `41 b8 ? ? ? ? b2 ? e8 ? ? ? ? 66 0f 76 c0` | 2, 2, 0 | found | 0x1349BDD |
| M-E-L7 | `0f b7 41 ? 66 bf ? ? 48 3d` | 10, 1, -1 | found | 0xD225BE |
| M-E-L8 | `41 0f b7 ca 66 b8 ? ? 81 f9` | 10, 1, -1 | found | 0x7A6F3B |

## PackfileLimit

Sizes and the streaming configuration call (the two limit sites and the call the part redirects):

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| P-E-D3 | `48 83 C6 ? 48 81 F9` | +7: direct size (builds 1.0.1158.13 and later); 7260 on current build | found | 0x10F1BF |
| P-E-D2 | `49 83 c3 ? 48 81 f9 ? ? ? ? 75 ? eb` | +7: direct size (builds 1.0.1013.17 to 1158.12) | not used on current build | - |
| P-E-D1 | `48 81 f9 ? ? ? ? 74 ? 48 83 3c ca 00` | +3: direct size (older builds) | not used on current build | - |
| P-E-I1 | `3b 05 ? ? ? ? 0f 8d ? ? ? ? 89 c0` | +2: rel32 to the indirect size variable, which holds N-4 (7256 on the current build) | found | 0x1279156 |
| P-E-H1 | `89 48 ? 48 8d 0d ? ? ? ? 48 89 08 48 89 c1 e8` | +0x10: the streaming configuration call; 8512 archive count on current build | found | 0x5093D5 |

References to the packfile array (rel32 moved to the new array). With the reference of the build (`R31a` or `R31b`) they are every reference the game's code has to the array (RVA 0x2C075B0 on the current build: 7260 entries of 8 bytes): 33 on 1.0.1158.16, 25 `lea` and the 8 `mov [rip+disp32], r64` of `P-E-R23..R30`; no other instruction reaches it or into it, and no stored pointer holds its address in the sections the dump holds. The relocation core checks the count with a scan for every `lea` and `mov` that reaches it. Archive indices are 16-bit, which caps the array at 65535 entries.

| Label | Pattern | Offset | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| P-E-R01 | `48 8d 15 ? ? ? ? 48 8b 3c ca 48 8b 2f` | 3 | found | 0x7EED5 |
| P-E-R02 | `48 8d 3d ? ? ? ? 48 8b 0c cf 48 85 c9` | 3 | found | 0xCE966C |
| P-E-R03 | `48 8d 3d ? ? ? ? 48 8b 0c 39 48 85 c9` | 3 | found | 0xCE9613 |
| P-E-R04 | `89 c0 48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 74` | 5 | found | 0xCEC43A |
| P-E-R05 | `89 c0 48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 0f 84` | 5 | found | 0xCE9CD3 |
| P-E-R06 | `48 8d 0d ? ? ? ? 4a 8b 0c c9 48 85 c9 0f 84 ? ? ? ? 48 8b 01` | 3 | found | 0xCEE169 |
| P-E-R07 | `48 8d 0d ? ? ? ? 48 8b 1c 08 48 8b 03` | 3 | found | 0xCF0D65 |
| P-E-R08 | `48 8d 0d ? ? ? ? 4a 8b 0c c1 48 85 c9` | 3 | found | 0x127A36E |
| P-E-R09 | `48 83 ec ? 48 89 ce 48 8d 05 ? ? ? ? 48 89 01 c6 81` | 39 (the rel32 of `lea rcx`, `48 8D 0D` at 36) | found | 0x10F781 |
| P-E-R10 | `48 c1 e8 ? 48 8d 0d ? ? ? ? 48 8b 0c c1 48 8b 01` | 7 | found | 0x1276047 |
| P-E-R11 | `48 8d 0d ? ? ? ? 48 83 3c c1 ? 40 0f 95 c6 eb` | 3 | found | 0x1276145 |
| P-E-R12 | `48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 ff 90 ? ? ? ? 48 8b 48` | 3 | found | 0xCF0EDB |
| P-E-R13 | `48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 4c 8d 44 24` | 3 | found | 0x12775CE |
| P-E-R14 | `48 8d 0d ? ? ? ? 4c 8b 24 08 49 8b 04 24 4c 8d 44 24` | 3 | found | 0x1277E25 |
| P-E-R15 | `48 8d 0d ? ? ? ? 48 8b 0c 08 48 85 c9 75` | 3 | found | 0xCEC3E0 |
| P-E-R16 | `48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 0f 84 ? ? ? ? 48 89 f5` | 3 | found | 0xCE3431 |
| P-E-R17 | `48 83 ec ? 89 d7 48 89 ce 48 8d 05 ? ? ? ? 48 89 01 c6 81 c1` | 41 (the rel32 of `lea rcx`, `48 8D 0D` at 38) | found | 0x113FB2 |
| P-E-R18 | `48 8d 0d ? ? ? ? 48 8b 3c 08 48 8b 07 48 89 f9` | 3 | found | 0x127AFB5 |
| P-E-R19 | `48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 ff 90 ? ? ? ? 48 8b 40` | 3 | found | 0xCF1384 |
| P-E-R20 | `48 8d 0d ? ? ? ? 48 8b 0c d1 48 85 c9 75` | 3 | found | 0x1275F75 |
| P-E-R21 | `48 8d 0d ? ? ? ? 4c 8b 24 c1 4d 85 e4 0f 84` | 3 | found | 0x1279164 |
| P-E-R22 | `48 8d 0d ? ? ? ? 48 8b 0c c1 48 8b 01 ff 90 ? ? ? ? 48 8b 38` | 3 | found | 0xCEC4F2 |
| P-E-R23 | `48 89 2d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 48 c7 44 08` | 3 | found | 0x127AAB5 |
| P-E-R24 | `48 89 35 ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 0f 1f 44 00 00` | 3 | found | 0x1275B50 |
| P-E-R25 | `48 89 3d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 66 90` | 3 | found | 0x1279C53 |
| P-E-R26 | `48 89 35 ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 66 66 66 2e 0f 1f 84 00 00 00 00 00` | 3 | found | 0x12799D9 |
| P-E-R27 | `48 89 2d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 0f 1f 84 00 00 00 00 00` | 3 | found | 0x127AD0D |
| P-E-R28 | `48 89 3d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 66 66 66 66 2e 0f 1f 84 00 00 00 00 00` | 3 | found | 0x1279B18 |
| P-E-R29 | `48 89 3d ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 0f 1f 40 00` | 3 | found | 0x127ABC1 |
| P-E-R30 | `48 89 35 ? ? ? ? b9 ? ? ? ? ba ? ? ? ? e8 ? ? ? ? b9 ? ? ? ? 48 c7 44 08` | 3 | found | 0x1276ED5 |
| P-E-R31a | `BF 08 00 00 00 BE 10 00 00 00 4C 8D 05` | 13 (builds 1.0.1158.13 and later). The two constants are TweaksV's: the original's `BF ? ? ? ? BE ? ? ? ? 4C 8D 05` also matched obfuscated code at 0x582A227 | found | 0x10F163 |
| P-E-R31b | `41 ? ? ? ? ? ? ? ? ? ? 41 bb ? ? ? ? ? 8d` | 20 (older builds) | not used on current build | - |
| P-E-R32 | `48 89 cf 31 f6 4c 8d 3d ? ? ? ? eb 10` | 8 (RVA 0x13EA794): the rel32 of `lea r15` (`4C 8D 3D` at 5), the load in the function 0x13EA760..0x13EA9A4, which walks a collection's entries, looks each one's packfile up as array[handle >> 16], tolerates a null entry and folds a value per entry into a hash it returns - left on the old array, it would read the game's own, empty array for the whole session | found | 0x13EA78C |
| P-E-R33 | `75 0c cc 48 8d 0d ? ? ? ? 48 8b 0c d1` | 6 (RVA 0x1275F89): the rel32 of `lea rcx` (`48 8D 0D` at 3), in `P-E-R20`'s function 0x1275F10..0x1275FF4, in the bytes after an `int3` trap - reached only when the index is at or past the N-4 variable or the entry is null, so never run; moved so that the count of references stays exact | found | 0x1275F83 |

## BoxStreamer

The four sites that hand the traversal list's capacity to the tree search (`mov dword ptr [rsp+20h], imm32`; every one MUST hold the same game size, 1000 on the current build). The list itself has no pattern of its own: two of the box streamer's four query functions are near-identical copies, so the second of the two `lea` instructions that load the list in each cannot be told apart by the bytes around it. Instead `B-E-L2`, `B-E-L3` and `B-E-L4` end in the `lea r13, [rip+disp32]` that loads the list (the three must agree on its address), and the game's code is then scanned for every `lea r64, [rip+disp32]` that loads it, which must find exactly 8 - on the current build at RVAs 0x92C4AB, 0x92C764, 0x92C7D3, 0x92CA12, 0x92DDCD, 0x92E013, 0x92E089 and 0x92E2D2, in the four query functions that start at RVAs 0x92C470, 0x92C7A0, 0x92DD70 and 0x92E040 (the list is at RVA 0x3ED6B40: 1000 entries of 8 bytes). `DIAGNOSTIC_MODE=1` logs every one of them. With `DIAGNOSTIC_MODE=1` the `call rel32` to the tree search that follows each site (the call column) is redirected through TweaksV's call stubs; `B-E-L1` and `B-E-L3` must lead to the same function (RVA 0x1EBFB20 on the current build), `B-E-L2` and `B-E-L4` to the same other one (0x1EC0140). A search stops 16 entries short of the capacity it is handed, so the game's own list of 1000 takes at most 984 entries from one search.

| Label | Pattern | Offset: capacity, lea rel32, call | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| B-E-L1 | `48 8B 44 24 40 48 8B 48 08 C7 44 24 20 ? ? ? ? 48 8D 54 24 60 4C 8D 44 24 70 4D 89 F1` | 13, -, 30 | found | 0x92C527 |
| B-E-L2 | `48 8B 81 08 01 00 00 48 8B 48 08 C7 44 24 20 ? ? ? ? 4C 8D 2D` | 15, 22, 34 | found | 0x92C7C0 |
| B-E-L3 | `0F 29 44 24 40 48 8B 48 08 C7 44 24 20 ? ? ? ? 4C 8D 2D` | 13, 20, 42 | found | 0x92DDBC |
| B-E-L4 | `48 8B 01 48 8B 48 08 C7 44 24 20 ? ? ? ? 4C 8D 2D` | 11, 18, 30 | found | 0x92E07A |

## Decals

The decal definitions array (260 entries of 80 bytes, at RVA 0x42EC8A0 on the current build) and the number of registered definitions, a dword 16 bytes before it (0x42EC890); the pair of constants 0x100 and 0x200 that `DecalPatch.asi` patches (my own finding, from the game's code in Ghidra) is not this build's decal definitions limit. The parser's "full" check `D-E-L1` gives the count's address (the disp32 of its `mov`) and N (its imm32 is N-1: 259, so N = 260, on the current build); the parser's `lea` `D-E-L2` gives the array's address, which MUST lie 16 bytes after the count, both inside the game. Every other site that carries the limit is found by its own pattern, MUST be the expected instruction and MUST hold N-1 or N: `D-E-L3` (FindById: three sites, two of them past the pattern's end, and a `lea` of the array), `D-E-L4` (PickVariant), `D-E-L5..L9` (the texture override; `L6` and `L9` also hold a `lea`), `D-E-L13..L16` (four decal-placing functions) and `D-E-L12`, the check seven more decal-placing functions share, which MUST match exactly 7 times. The decal manager's start-up loop, which zeroes the texture pointers of 26 entries per pass, gives its start offset and a `lea` of the array (`D-E-L10`) and the bytes one pass clears and its end (`D-E-L11`), which MUST be the start offset + N x 80. The array's loads have no pattern of their own: the game's code is scanned for every `lea r64, [rip+disp32]` that loads it, which MUST find exactly 31 - on the current build at RVAs 0xB195A8, 0xB3187E, 0xB31AD6, 0xB31FA7, 0xB324CA, 0xB326EF, 0xB32C71, 0xB32F2B, 0xB33423, 0xB338F6, 0xB33EF5, 0xB34892, 0xB34CF7, 0xB34F96, 0xCA0A1F, 0xCA11CE, 0xCA323B, 0xCA6DD5, 0xCA756C, 0xCA960E, 0xCAA2F7, 0xCAD859, 0xCADE17, 0xCB30C9, 0xCB386D, 0xCB3A4F, 0xCBEAE2, 0xCBF181, 0xCBF251, 0xCBF2D0 and 0xCD2B60. `DIAGNOSTIC_MODE=1` logs every one of them and every constant site.

An entry, as the parser fills it from the `decals.dat` columns (80 bytes): +0x00 int32 decal id (the first column); +0x08, +0x10 and +0x18 pointers to the diffuse, normal and specular textures (0 until loaded - the only pointers in an entry, and the ones the start-up loop zeroes); +0x20 float size; +0x24 specular multiplier, +0x28 specular falloff, +0x2C specular intensity, +0x30 fresnel, +0x34 steep, +0x38 parallax scale and +0x3C emissive value, all floats; +0x40 byte texture rows, +0x41 columns, +0x42 id A, +0x43 id B, +0x44 texture wrap (0 or 1; the game sets 4 later for "rotate to camera"), +0x45 anisotropic, +0x46 washable, +0x47 underwater create, +0x48 underwater fade; +0x49 to +0x4F unused. No entry holds a pointer to another entry or into the array, nothing else in the game is sized by the number of definitions, and the count 16 bytes before the array is addressed only by its own instructions, never through the array (nor the array through it), which is why the count stays where it is. Entries are read with scalar loads and cleared with unaligned stores, so a page-aligned buffer needs no more alignment. A definition's index is a uint16 in the game's decal instances. On 1.0.1158.16 vanilla registers 259 definitions.

| Label | Pattern | Offset: what | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| D-E-L1 | `8B 35 ? ? ? ? 48 81 FE ? ? ? ? 77 ? 48 89 F9 FF 15` | 2: the count's disp32; 9: N-1 (the parser's "full" check) | found | 0xCBEB8A |
| D-E-L2 | `4C 8D 2D ? ? ? ? F3 44 0F 10 0D ? ? ? ? F3 44 0F 10 15` | 3: the array's rel32 (the parser's lea) | found | 0xCBEAE2 |
| D-E-L3 | `56 85 C9 74 ? C7 02 04 01 00 00 41 C7 00 00 00 00 00 44 8B 1D` | 7, 0x53, 0x62: N (FindById); 0x21: a lea of the array | found | 0xCBF160 |
| D-E-L4 | `B8 04 01 00 00 81 F9 03 01 00 00 77 ? 41 89 D0 8B 05` | 1: N, 7: N-1 (PickVariant) | found | 0xCBF1D0 |
| D-E-L5 | `45 31 C9 41 BB 04 01 00 00 31 C0 45 31 D2 EB` | 5: N (the texture override, one definition) | found | 0xCBF232 |
| D-E-L6 | `41 BB 04 01 00 00 48 8D 3D ? ? ? ? 31 C0 45 31 D2` | 2: N (the texture override, its loop); 6: a lea of the array | found | 0xCBF24B |
| D-E-L7 | `41 81 FB 04 01 00 00 89 C3 41 0F 44 D9` | 3: N (the texture override) | found | 0xCBF260 |
| D-E-L8 | `81 FD 04 01 00 00 89 D8 41 0F 44 C1` | 2: N (the texture override) | found | 0xCBF283 |
| D-E-L9 | `41 81 FB 04 01 00 00 41 0F 44 C1 48 8D 3D` | 3: N (the texture override, its odd tail); 11: a lea of the array | found | 0xCBF2C5 |
| D-E-L10 | `B8 D8 07 00 00 48 8D 0D ? ? ? ? 0F 57 C0` | 1: the start-up loop's start offset (0x7D8); 5: a lea of the array | found | 0xCD2B5B |
| D-E-L11 | `48 05 20 08 00 00 48 3D 18 59 00 00 0F 85` | 2: the bytes one pass clears (0x820 = 26 x 80); 8: the loop's end, the start offset + N x 80 (0x5918) | found | 0xCD2D6B |
| D-E-L12 | `E8 ? ? ? ? 3D 03 01 00 00 0F 8F` | 6: N-1 (the check after the call to PickVariant in 7 decal-placing functions - matches 7 times) | found, 7 matches | 0xB31856, 0xB31F7D, 0xB326C5, 0xB32C49, 0xB32F03, 0xB333FB, 0xB34880 |
| D-E-L13 | `44 89 C3 41 81 F8 03 01 00 00 77` | 6: N-1 (the decal-placing function that takes the index as a parameter) | found | 0xB338A8 |
| D-E-L14 | `89 C6 81 FE 03 01 00 00 0F 87` | 4: N-1 | found | 0xB33ED1 |
| D-E-L15 | `E8 ? ? ? ? 3D 03 01 00 00 7F` | 6: N-1 (the one with a short jump) | found | 0xB34CD5 |
| D-E-L16 | `89 C3 31 C0 81 FB 03 01 00 00 0F 8F` | 6: N-1 | found | 0xB34F70 |

## RageOpenV

The RPF7 facts the OPEN archives rely on, which no pattern holds: the encryption field of an archive that is not encrypted holds the tag `OPEN` (0x4E45504F), and the game's FindEncryption (called at `R-E-E1`) answers yes, NG-encrypted, when `(encryption & 0x0FF00000) == 0x0FE00000`. In the entry table (entries of 16 bytes, their fields in "When Game Updates" item 5) an entry whose type at +4 is 0x7FFFFF00 is a folder and one whose type has bit 31 (0x80000000) set is a resource; every other entry is a binary file. For an unencrypted archive the game expects 0x0FEFFFFF in the encryption field (+12) of every binary entry whose name offset and encryption field are not zero, and in the packfile's current file offset (+0xBC from the `fiPackfile` object's start, item 5).

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| R-E-C1 | `ff 15 ? ? ? ? 48 85 c0 74 ? 48 89 c1 31 d2` | +16: the call that hides the console (only with `CONSOLE=1`) | found | 0x1F04 |
| R-E-K1 | `e8 ? ? ? ? 48 8d 0d ? ? ? ? 48 8d 15 ? ? ? ? 48 8d 35` | the RPF cache check: its callee gets `C3 90` | found | 0x4F73AE |
| R-E-F1 | `41 56 56 57 53 48 81 ec ? ? ? ? 4c 89 c7 66 c7 ? ? ? ? ? 4c 8d 74 24` | OpenBulk 1 (function start) | found | 0x10B110 |
| R-E-F2 | `66 c7 01 ? ? 29 d9 d1 e9 66 89 4c 24 ? 31 ff 4c 8d 44 ? ? 48 89 d9 31 d2 ff 15 ? ? ? ? 85 c0` | GetFileTime 1 = match - 0x10D | found | 0x10BA3D |
| R-E-F3 | `66 c7 01 ? ? 29 d9 d1 e9 66 89 4c 24 ? 31 ff 4c 8d 44 24 ? 48 89 d9 31 d2 ff 15 ? ? ? ? 8b 4c 24` | GetFileSize 1 = match - 0x10D | found | 0x10B8ED |
| R-E-F4 | `41 56 56 57 53 48 81 ec ? ? ? ? 66 c7 ? ? ? ? ? 4c 8d 74 24` | GetAttributes 1 (function start) | found | 0x10BC10 |
| R-E-F5 | `41 57 41 56 56 57 53 48 81 ec 60 02 00 00 48 89 d3 48 89 cf` | OpenBulk 2 (function start) | found | 0x107F60 |
| R-E-F6 | `66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 ? ? ? ? 85 c0 74 14 8b 44 24 64 8b 4c 24 68 48 c1 e0 20 48 09 c1 48 89 4c 24 38` | GetFileTime 2 = match - 0x15D | found | 0x10887D |
| R-E-F7 | `66 c7 01 00 00 29 d9 d1 e9 66 89 4c 24 70 4c 8d 44 24 48 48 89 d9 31 d2 ff 15 ? ? ? ? 85 c0 74 0a 48 8b 44 24 5c 48 89 44 24 38` | GetFileSize 2 = match - 0x15D | found | 0x108A6D |
| R-E-F8 | `56 57 53 48 81 ec 60 02 00 00 48 89 d7 48 89 ce c7 44 24 3c ff ff ff ff 48 89 d1 ba 3a 00 00 00` | GetAttributes 2 (function start) | found | 0x108C40 |
| R-E-E1 | `e8 ? ? ? ? 80 7c 24 2e ? 74 ? 48 8b 56` | FindEncryption call | found | 0x10FEF2 |
| R-E-E2 | `e8 ? ? ? ? 8b 46 ? 85 c0 74 ? b9` | DecryptHeader call | found | 0x10FF08 |
| R-E-E3 | `e8 ? ? ? ? 48 8b 46 ? 66 c7 00 00 ? eb` | first DecryptHeader2 call | found | 0x10FFAE |
| R-E-E4 | `e8 ? ? ? ? 48 8b 46 ? 8b 4c 24 ? 48 8d 14 08` | second DecryptHeader2 call (same callee as R-E-E3) | found | 0x11001C |
| R-E-E5 | `c6 86 ? ? ? ? ? 48 89 f1 4c 89 fa 45 89 f0 45 31 c9` | +19: ParseHeader call | found | 0x10F5B0 |
| R-E-M0 | `41 57 41 56 41 55 41 54 56 57 55 53 48 81 ec ? ? ? ? 89 d6 49 89 ce` | the game's GetDevice (called by the newmods device) - newmods is gone | removed | - |
| R-E-M1 | `e8 ? ? ? ? 48 8d 05 ? ? ? ? 66 c7 44 58 02 00 00 66 89 3d` | +26: the initial mount call (newmods) - newmods is gone | removed | - |
| R-E-M2 | `41 57 41 56 41 55 41 54 56 57 55 53 48 81 ec ? ? ? ? 45 89 cd 45 89 c6 49 89 d7 48 89 ce 8b 05` | the archive function (hooked for newmods) - newmods is gone | removed | - |
| R-E-D1 | `56 57 53 48 81 ec ? ? ? ? 48 83 79 ? ? 0f 84 ? ? ? ? 48 89 d7` | the game's fiDevice::Mount (called by the newmods device) - newmods is gone | removed | - |
| R-E-D2 | `56 57 53 48 81 ec ? ? ? ? 4c 89 cf 44 89 c3` | the game's fiDevice::SetPath (called by the newmods device) - newmods is gone | removed | - |
| R-E-D3 | `48 8d 05 ? ? ? ? 48 89 06 48 c7 46 ? ? ? ? ? 48 8d 15` | fiDeviceRelative's vtable - never used | removed | - |

## StraightToStoryMode

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| S-E1 | `0F 85 A9 00 00 00 48 8D 54` | splash screen: `jz` at match - 0x0D becomes `jmp` | found | 0x52DC67 |
| S-E2 | `00 E9 ? 05 00 00 E8 ? 07` | legal screen: the 6-byte `jz` (`0F 84`) at match - 0x0F is NOPed | found | 0x52F040 |
| S-E3 | `E8 ? ? ? ? 8B 0D ? ? ? ? 83 C1 ? 84 C0` | landing page: its callee returns 0 | found | 0x1166F |

## Script programs

A script program is one of the game's compiled scripts - `blip_controller` and the others its archives store, encrypted - as the game holds it in memory. The shared module `TweaksVScripts` hooks the place where the game makes one and reads its layout, for every part that works on a script program: the Blips part raises `blip_controller`'s threshold in one (SPECIFICATIONS.md, section 11.9), and `TEST_DUMP_SCRIPT` in test builds saves one (section 13.3). The facts, from the dump of 1.0.1158.16, verified against it on 2026-10-27:

- **The hook point:** in the program's resource constructor (RVA 0x91B9F0), the call of the natives' resolution, an `E8 rel32` at RVA 0x91BBAB to RVA 0x91BF20, with rcx = the program: `Y-E-H1` at RVA 0x91BBA8, the call at +3.
- **The function behind it** (RVA 0x91BF20) takes the program alone and returns a byte: 1 when every native resolved, 0 when one did not; the constructor ignores it. Before the call, the program's native table holds the natives' 64-bit hashes - the constructor has rotated them back already, in the loop `Y-E-N1` matches; after it, each entry holds its handler's address, and a hash the game does not know gets the handler at RVA 0x50CB60, `CC C3` (an `int3`, then a `ret`).
- **The thread and the timing:** the constructor runs on the game's resource placement thread whenever a script program is streamed in. The program enters the game's registry of programs only after the call, and every script thread looks its program up in the registry each time it runs (a thread whose program is not there faults rather than runs it), so no thread can run the program while the hook looks at it: the window of T-FREEZE (NOTES.md) does not arise.
- **The fields,** counted from the program's start: +0x10 the code page table (8-byte pointers, one per page), +0x1C the code length in bytes (dword), +0x2C the native count (dword), +0x40 the native table (count x 8 bytes), +0x58 the name's hash (dword), +0x60 the name (a pointer to a zero-terminated string), +0x68 the string page table, +0x70 the strings' length in bytes (dword); the object is at least 0x80 bytes. `Y-E-N1` pins +0x1C and +0x40, `Y-E-I1` +0x10, and `Y-E-H1` +0x70 with the page rounding: its last bytes begin the constructor's loop over the strings' pages (`mov r14d, 0x3FFF` at RVA 0x91BBB0, `mov eax, [r12+0x70]` at 0x91BBB6, `add eax, r14d; cmp eax, 0x4000` at 0x91BBBB; the loop at 0x91BD1B to 0x91BD37, `shr eax, 14` and the page table read at +0x68); the others are on the list of "When Game Updates", item 5.
- **The pages:** 0x4000 bytes each. A length gives (length + 0x3FFF) >> 14 pages, every one full but the last, which holds length & 0x3FFF bytes (0x4000 when that is 0); the strings are paged the same way.
- **The name's hash,** the game's own function (RVA 0x91B6A0): over the name's bytes - a leading double quote skipped, the name then ending at the next one; A to Z lowered and `\` taken as `/`; each byte sign-extended - h = (h + c) * 0x401 and h ^= h >> 6, all in 32 bits; then h *= 9, h ^= h >> 11, h *= 0x8001; a result below 2 gets 2 added. `blip_controller` gives 0x80CFE974.
- **The interpreter** (RVA 0x91DD30) fetches the byte at ip through the code page table - ip >> 14 the page, ip & 0x3FFF the offset in it - and dispatches through the table at .rdata 0x2610450 over 131 opcodes, 0x00 to 0x82; a byte above 0x82 is a fatal trap. Every instruction lies within one page, except SWITCH, whose entry table it reads from the page of ip + 2, so a page's end may lie between a SWITCH's count byte and its table. The lengths are the game's own table at .rdata 0x2610C50, the dispatch table + 0x800, one byte per opcode: 0x00..0x24 1; 0x25 2; 0x26 3; 0x27 4; 0x28..0x29 5; 0x2A..0x2B 1; 0x2C 4; 0x2D 0; 0x2E 3; 0x2F..0x33 1; 0x34..0x3E 2; 0x3F 1; 0x40..0x42 2; 0x43..0x5C 3; 0x5D..0x64 4; 0x65 0; 0x66..0x67 1; 0x68..0x6B 2; 0x6C..0x82 1 - 0 for the two of variable length, ENTER (0x2D: 5 bytes and its name's, whose length is the byte at +4) and SWITCH (0x65: 2 bytes and 6 for each entry, whose count is the byte at +1).
- **The opcodes a matcher needs:** NOP 0x00; NOT 0x06; the signed 32-bit compares IEQ 0x08, INE 0x09, IGT 0x0A, IGE 0x0B, ILT 0x0C and ILE 0x0D; PUSH_CONST_U8 0x25, PUSH_CONST_U8_U8 0x26, PUSH_CONST_U8_U8_U8 0x27, PUSH_CONST_U32 0x28 and PUSH_CONST_F 0x29; NATIVE 0x2C (+1 the argument count << 2 | the result count, +2 the native's index, big-endian); ENTER 0x2D and LEAVE 0x2E; PUSH_CONST_S16 0x43 (signed); J 0x55 and JZ 0x56 (a signed 16-bit offset from ip + 3), and the fused IEQ_JZ to ILE_JZ 0x57 to 0x5C, which jump when their comparison is false; CALL 0x5D (a 24-bit address in the code); PUSH_CONST_U24 0x64; SWITCH 0x65 (n entries of a 32-bit value and a 16-bit offset from the entry's end); CATCH 0x6D, THROW 0x6E and CALLINDIRECT 0x6F, whose targets are computed while the script runs; PUSH_CONST_M1 0x70 and PUSH_CONST_0 to PUSH_CONST_7 0x71 to 0x78.
- **The native table** in a script file holds each native's hash rotated right by (code length + index) mod 64; the constructor rotates them back before the call (the loop `Y-E-N1` matches), and the game's function writes each handler's address over its hash. A handler can serve more than one hash.
- **Every placement:** loading a save places `blip_controller` again - a new program object, its code and tables the same - so the hook sees each placement; at the switch's boot of 2026-10-27, the 48th program at the start and the 137th after Load Game, every placement on the one resource placement thread.

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| Y-E-H1 | `4C 89 E1 E8 ? ? ? ? 41 BE FF 3F 00 00 41 8B 44 24 70` | +3: the call to the function that resolves a script program's natives (RVA 0x91BF20), in the program's resource constructor (RVA 0x91B9F0), rcx = the program (`mov rcx, r12` at +0) - the call `TweaksVScripts` redirects; +18: 0x70, the offset of the strings' length (`mov eax, [r12+0x70]`), and +10: 0x3FFF, the page rounding (`mov r14d, 0x3FFF`) - the constructor's loop over the strings' pages after the call | found | 0x91BBA8 |
| Y-E-N1 | `41 8B 4C 24 1C 01 C1 49 8B 54 24 40 48 D3 04 C2` | +4: 0x1C, the offset of a program's code length (`mov ecx, [r12+0x1C]`); +11: 0x40, the offset of its native table (`mov rdx, [r12+0x40]`) - the constructor's loop that rotates the natives' hashes back | found | 0x91BAA0 |
| Y-E-I1 | `4D 8B 50 10 49 63 41 1C 4C 8D 7C C1 F8 49 63 41 18 4C 8D 0C C1` | +3: 0x10, the offset of a program's code page table (`mov r10, [r8+0x10]`) - the script interpreter's prologue | found | 0x91DD50 |

## Blips

The Blips part (`TweaksVBlips`, SPECIFICATIONS.md section 11.9; PLAN.md, the Blips section) has the game's own `blip_controller` script make its blips while a mod's blips fill the blip pool; its patterns are the rows at the end of this section (`L` for the pool's). The facts it rests on, from the dump of 1.0.1158.16 and from the program the switch saved at my boot of 2026-10-27, verified against both:

- **The blip pool:** 2,000 slots of 8 bytes at RVA 0x3DEBD70, its high-water mark at RVA 0x3DEFBF0; a blip's handle is (serial << 16) | index, and the pool's code bounds an index with 0x7CF.
- **The count native** (on this build its hash is 0xC802478C8862BDAD and its handler is at RVA 0x1D51F40, which starts with a jump into the game's protected code) counts the slots below the mark that hold a blip, except three: the slot whose index the dword at RVA 0x288FEF0 holds, and the slots of the handles at RVA 0x3DEB7F8 and 0x3DEB7F4, those two only while the byte at RVA 0x3DEB7F0 is set. With all 2,000 slots taken it answers 1,997 while the three name live blips, and 1,998, 1,999 or 2,000 when one or more of them names none.
- **WAIT:** on this build its hash is 0x4EDE34FBADD967A6 and its handler is at RVA 0x920EA0.
- **The guard** in `blip_controller` (on 1.0.1158.16 its code is 11,394 bytes in one page, with 55 natives and 1,469 bytes of strings), 21 bytes at 0x3FE of its code - the count native is its index 10, WAIT its index 47:

  | Offset | Bytes | Instruction |
  |---|---|---|
  | 0x3FE | `2C 01 00 0A` | NATIVE, 0 arguments, 1 result, index 10: the count |
  | 0x402 | `25 96` | PUSH_CONST_U8 150: the threshold |
  | 0x404 | `0C` | ILT |
  | 0x405 | `06` | NOT |
  | 0x406 | `56 0A 00` | JZ +10, to 0x413: the exit |
  | 0x409 | `43 D0 07` | PUSH_CONST_S16 2000 |
  | 0x40C | `2C 04 00 2F` | NATIVE, 1 argument, no result, index 47: WAIT |
  | 0x410 | `55 EB FF` | J -21, back to the count at 0x3FE |

  It holds the script back - WAIT with 2,000, then the count again - while count >= 150, and lets it on to make a blip as soon as count < 150. The count native is called nowhere else in the program, 150 is pushed nowhere else, no jump, call or SWITCH case lands in 0x402..0x408, and no target is computed (the program holds no CATCH, THROW or CALLINDIRECT). Told apart by their argument and result counts alone, the guard's NATIVE is the only one with 0 arguments and 1 result that a constant push and a comparison follow.
- **The threshold** is 150, the game's own. The program holds no 240, the value some setups show (PLAN.md, D4); where that comes from is open.
- **The room:** the 4 bytes 0x402..0x405 between the count's NATIVE and the JZ. PUSH_CONST_S16 and IGE fill them for any limit up to 32,767 (`43 lo hi 0B`), and NOP, PUSH_CONST_U8 and IGE for a limit up to 255 (`00 25 nn 0B`): IGE in the place of ILT and NOT keeps the polarity, and the JZ keeps its target.
- **The highest limit that still holds with the pool full** is 1,997, the count's lowest answer then: above it, the script can pass the guard with the pool full, where making a blip fails.
- **Not settled:** which of the game's two copies of the script it runs - update2.rpf's, resource version 12, or x64b.rpf's, 10. A lead: the script store is built with 12 at its +0x2C (RVA 0x13FA29D and 0x30E40). The part reads the program as the game placed it, so it does not depend on it.

The pool's two sites, for the part's measurement: looked for only with `DIAGNOSTIC_MODE=1`, each matching exactly once in the dump of 1.0.1158.16. The research's other two sites of the pool, the count's three slots it leaves out, have no pattern: the measurement counts the slots in use, not the count's answer.

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| L-E-P1 | `BA 04 00 00 00 31 C0 48 8D 1D ? ? ? ? 31 F6` | +10: the rel32 of `lea rbx, [rip+disp32]` (`48 8D 1D` at +7) in the pool's allocator - the blip pool, 2,000 slots of 8 bytes (RVA 0x3DEBD70) | found | 0x55D897 |
| L-E-P2 | `48 89 3B 39 35 ? ? ? ? 7F 09 8D 46 01 89 05` | +5: the disp32 of `cmp [rip+disp32], esi` (`39 35` at +3) in the pool's update of its high-water mark - the mark, a dword (RVA 0x3DEFBF0) | found | 0x55DA0E |
