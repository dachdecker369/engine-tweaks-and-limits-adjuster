# Patterns

Every byte pattern TweaksV searches for in `GTA5_Enhanced.exe`, and the game build each one was last verified on. TweaksV searches only the game's code (its executable sections). Each pattern is defined once, in the table in `source\TweaksVPatterns.h`, under the label it has here (`H-E1`, `P-E-R31a`, ...), and is written here exactly as there; `?` matches any byte. The tests check that this file and the table list the same patterns, and `TweaksV.log` names each pattern by its label. RVA is where each pattern matched on 1.0.1158.16, counted from the start of `GTA5_Enhanced.exe` in memory. What each part does with a match - the reference, limit, call or function it holds - is in [SPECIFICATIONS.md](SPECIFICATIONS.md).

## Status

| | |
|---|---|
| **Game Build** | 1.0.1158.16 |
| **Found** | 87 of the 87 patterns this build uses |
| **Untested** | None. The diagnostic start (`DIAGNOSTIC_MODE=1`, `CONSOLE=1`) also ran these: `R-E-C1` runs only with `CONSOLE=1`. `R-E-F1..F8` run only with a mods folder in the game directory (with `DIAGNOSTIC_MODE=1`, also without one). A part with `ENABLED=0` looks for none of its patterns. |
| **Matches** | All match exactly once, except `D-E-L12`, which matches exactly 7 times (the check seven decal-placing functions share). To check: boot once with `DIAGNOSTIC_MODE=1` and `CONSOLE=1`, every part on. Log lists each pattern address and number of matches, and the bytes around each match of a pattern that matches more than once. Every pattern must match exactly once (`D-E-L12`: exactly 7 times). |

The build uses 87 of the table's 90 patterns; six more rows below are kept as removed:
- Packfile picks one of three direct size patterns and one of two references by game build.
- `R-E-D3` belonged to code that was never used. Removed.
- `R-E-M0`, `R-E-M1`, `R-E-M2`, `R-E-D1` and `R-E-D2` belonged to the newmods folder, which 2.0.0 drops. Removed.

The DirectStorage part uses no patterns. It hooks `DStorageGetFactory` in `dstorage.dll` and, on the factory the game gets for the interface `{6924EA0C-C3CD-4826-B10A-F64F4ED927C1}`, its function [4], `OpenFile` (`DStorageCore.dll+0x3F9D0` with DirectStorage 1.2.2311.1405 on 1.0.1158.16).

### When Game Updates

1. Start the game with `DIAGNOSTIC_MODE=1` and `DRY_RUN=1` - and `CONSOLE=1`, with every part on, so every pattern is looked for. Then game binary isn't patched. `TweaksV.log` lists every pattern not found, by its label.
2. Check whether the project the pattern came from has changed it (`CREDITS.md` names them).
3. Update the pattern in `source\TweaksVPatterns.h` and in this file - the tests check that both agree - and confirm that it matches exactly once. `GTA5_Enhanced.exe` is encrypted on disk, so look for the code in a copy of the running game: a test build with `TEST_DUMP_CODE=60` saves one (README, Test Builds).
4. DirectStorage: if `UNLOCK_FILES=1` reports a problem (another interface, or an `OpenFile` outside DirectStorage's DLLs), look at the factory's functions with `DIAGNOSTIC_MODE=1`.
5. The game facts no pattern protects, to check in the dump even when every pattern still matches, because a change in them would go wrong silently: RageOpenV's `fiPackfile` fields (8 bytes further on than Legacy's: the entry table at +0x20, its count at +0x28, the current file offset at +0xB4; entries of 16 bytes with the name offset at +0, the type at +4 and the encryption at +12) and the fixed distances of its sites (GetFileTime and GetFileSize 0x10D before `R-E-F2` and `R-E-F3`, 0x15D before `R-E-F6` and `R-E-F7`; the ParseHeader call 19 bytes into `R-E-E5`; the console call 16 bytes into `R-E-C1`); the archive count at offset 24 of the streaming configuration that `P-E-H1`'s call reads, and that call's single argument; the tree-search calls 30, 34, 42 and 30 bytes into `B-E-L1..L4` and their fifth argument on the stack; the decal manager's loop (the start offset at +1 and the lea at +5 of `D-E-L10`, the pass and the end at +2 and +8 of `D-E-L11`); the heap adjuster's rule that a heap of 1024 MB or more is another adjuster's (a build whose own heap is that large would be left alone, with only a log line).

## HeapAdjuster

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| H-E1 | `BA ? ? ? ? 45 31 C0 41 B1 01 E8 ? ? ? ? 48 8D 0D` | +1: imm32 of `mov edx` - heap size in bytes | found | 0x13F7 |

## ModkitLimit

References to modkit array (rel32 of a `lea` - moved to new array):

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
| M-E-L2 | `48 83 c5 ? 48 83 c0 ? 48 83 c7` | 15, 1, 3 | found | 0x137863F |
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

References to the packfile array (rel32 moved to the new array):

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
| P-E-R09 | `48 83 ec ? 48 89 ce 48 8d 05 ? ? ? ? 48 89 01 c6 81` | 39 | found | 0x10F781 |
| P-E-R10 | `48 c1 e8 ? 48 8d 0d ? ? ? ? 48 8b 0c c1 48 8b 01` | 7 | found | 0x1276047 |
| P-E-R11 | `48 8d 0d ? ? ? ? 48 83 3c c1 ? 40 0f 95 c6 eb` | 3 | found | 0x1276145 |
| P-E-R12 | `48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 ff 90 ? ? ? ? 48 8b 48` | 3 | found | 0xCF0EDB |
| P-E-R13 | `48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 4c 8d 44 24` | 3 | found | 0x12775CE |
| P-E-R14 | `48 8d 0d ? ? ? ? 4c 8b 24 08 49 8b 04 24 4c 8d 44 24` | 3 | found | 0x1277E25 |
| P-E-R15 | `48 8d 0d ? ? ? ? 48 8b 0c 08 48 85 c9 75` | 3 | found | 0xCEC3E0 |
| P-E-R16 | `48 8d 0d ? ? ? ? 48 8b 0c c1 48 85 c9 0f 84 ? ? ? ? 48 89 f5` | 3 | found | 0xCE3431 |
| P-E-R17 | `48 83 ec ? 89 d7 48 89 ce 48 8d 05 ? ? ? ? 48 89 01 c6 81 c1` | 41 | found | 0x113FB2 |
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

## BoxStreamer

The four sites that hand the traversal list's capacity to the tree search (`mov dword ptr [rsp+20h], imm32`; every one MUST hold the same game size, 1000 on the current build). The list itself has no pattern of its own: two of the box streamer's four query functions are near-identical copies, so the second of the two `lea` instructions that load the list in each cannot be told apart by the bytes around it. Instead `B-E-L2`, `B-E-L3` and `B-E-L4` end in the `lea r13, [rip+disp32]` that loads the list (the three must agree on its address), and the game's code is then scanned for every `lea r64, [rip+disp32]` that loads it, which must find exactly 8 - on the current build at RVAs 0x92C4AB, 0x92C764, 0x92C7D3, 0x92CA12, 0x92DDCD, 0x92E013, 0x92E089 and 0x92E2D2 (the list is at RVA 0x3ED6B40: 1000 entries of 8 bytes). `DIAGNOSTIC_MODE=1` logs every one of them. With `DIAGNOSTIC_MODE=1` the `call rel32` to the tree search that follows each site (the call column) is redirected through TweaksV's call stubs; `B-E-L1` and `B-E-L3` must lead to the same function (RVA 0x1EBFB20 on the current build), `B-E-L2` and `B-E-L4` to the same other one (0x1EC0140).

| Label | Pattern | Offset: capacity, lea rel32, call | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| B-E-L1 | `48 8B 44 24 40 48 8B 48 08 C7 44 24 20 ? ? ? ? 48 8D 54 24 60 4C 8D 44 24 70 4D 89 F1` | 13, -, 30 | found | 0x92C527 |
| B-E-L2 | `48 8B 81 08 01 00 00 48 8B 48 08 C7 44 24 20 ? ? ? ? 4C 8D 2D` | 15, 22, 34 | found | 0x92C7C0 |
| B-E-L3 | `0F 29 44 24 40 48 8B 48 08 C7 44 24 20 ? ? ? ? 4C 8D 2D` | 13, 20, 42 | found | 0x92DDBC |
| B-E-L4 | `48 8B 01 48 8B 48 08 C7 44 24 20 ? ? ? ? 4C 8D 2D` | 11, 18, 30 | found | 0x92E07A |

## Decals

The decal definitions array (260 entries of 80 bytes, at RVA 0x42EC8A0 on the current build) and the number of registered definitions, a dword 16 bytes before it (0x42EC890). The parser's "full" check `D-E-L1` gives the count's address (the disp32 of its `mov`) and N (its imm32 is N-1: 259, so N = 260, on the current build); the parser's `lea` `D-E-L2` gives the array's address, which MUST lie 16 bytes after the count, both inside the game. Every other site that carries the limit is found by its own pattern, MUST be the expected instruction and MUST hold N-1 or N: `D-E-L3` (FindById: three sites, two of them past the pattern's end, and a `lea` of the array), `D-E-L4` (PickVariant), `D-E-L5..L9` (the texture override; `L6` and `L9` also hold a `lea`), `D-E-L13..L16` (four decal-placing functions) and `D-E-L12`, the check seven more decal-placing functions share, which MUST match exactly 7 times. The decal manager's start-up loop, which zeroes the texture pointers of 26 entries per pass, gives its start offset and a `lea` of the array (`D-E-L10`) and the bytes one pass clears and its end (`D-E-L11`), which MUST be the start offset + N x 80. The array's loads have no pattern of their own: the game's code is scanned for every `lea r64, [rip+disp32]` that loads it, which MUST find exactly 31 - on the current build at RVAs 0xB195A8, 0xB3187E, 0xB31AD6, 0xB31FA7, 0xB324CA, 0xB326EF, 0xB32C71, 0xB32F2B, 0xB33423, 0xB338F6, 0xB33EF5, 0xB34892, 0xB34CF7, 0xB34F96, 0xCA0A1F, 0xCA11CE, 0xCA323B, 0xCA6DD5, 0xCA756C, 0xCA960E, 0xCAA2F7, 0xCAD859, 0xCADE17, 0xCB30C9, 0xCB386D, 0xCB3A4F, 0xCBEAE2, 0xCBF181, 0xCBF251, 0xCBF2D0 and 0xCD2B60. `DIAGNOSTIC_MODE=1` logs every one of them and every constant site.

An entry, as the parser fills it from the `decals.dat` columns (80 bytes): +0x00 int32 decal id (the first column); +0x08, +0x10 and +0x18 pointers to the diffuse, normal and specular textures (0 until loaded - the only pointers in an entry, and the ones the start-up loop zeroes); +0x20 float size; +0x24 specular multiplier, +0x28 specular falloff, +0x2C specular intensity, +0x30 fresnel, +0x34 steep, +0x38 parallax scale and +0x3C emissive value, all floats; +0x40 byte texture rows, +0x41 columns, +0x42 id A, +0x43 id B, +0x44 texture wrap (0 or 1; the game sets 4 later for "rotate to camera"), +0x45 anisotropic, +0x46 washable, +0x47 underwater create, +0x48 underwater fade; +0x49 to +0x4F unused. No entry holds a pointer to another entry or into the array, nothing else in the game is sized by the number of definitions, and the count 16 bytes before the array is addressed only by its own instructions, never through the array (nor the array through it), which is why the count stays where it is. Entries are read with scalar loads and cleared with unaligned stores, so a page-aligned buffer needs no more alignment. On 1.0.1158.16 vanilla registers 259 definitions.

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
