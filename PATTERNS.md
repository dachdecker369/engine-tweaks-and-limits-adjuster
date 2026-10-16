# Patterns

Every byte pattern TweaksV searches for in `GTA5_Enhanced.exe`, and the game build each one was last verified on. The labels are the plan's (Appendix A). Patterns are written exactly as in the source, where `?` matches any byte. RVA is where each pattern matched on 1.0.1158.16, counted from the start of `GTA5_Enhanced.exe` in memory.

## Status

| | |
|---|---|
| **Game Build** | 1.0.1158.16 |
| **Found** | 72 of the 72 patterns this build uses |
| **Untested** | None. The diagnostic start (`DIAGNOSTIC_MODE=1`, `CONSOLE=1`) also ran these: `R-E-C1` runs only with `CONSOLE=1`. `R-E-M0..M2` and `R-E-D1..D2` run only with a newmods folder in the game directory (or in DIAGNOSTIC_MODE). |
| **Matches** | All match exactly once. To check: boot once with `DIAGNOSTIC_MODE=1` and `CONSOLE=1` enabled. Log lists each pattern address and number of matches, and the bytes around each match of a pattern that matches more than once. Every pattern MUST match exactly once (plan section 5.9 - R7). |

Build uses 72 of 76 patterns:
- Packfile picks one of three direct size patterns and one of two references by game build.
- `R-E-D3` belonged to code that was never used. Removed.

### When Game Updates

1. Start the game with `DIAGNOSTIC_MODE=1` and `DRY_RUN=1`. Then game binary isn't patched. `TweaksV.log` lists every pattern not found.
2. Check whether the original plugin has been updated (`UPSTREAM.md`).
3. Update the pattern, confirm that it matches exactly once, then update this file.

## HeapAdjusterEnhanced

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| H-E1 | `BA ? ? ? ? 45 31 C0 41 B1 01 E8 ? ? ? ? 48 8D 0D` | +1: imm32 of `mov edx` - heap size in bytes | found | 0x13F7 |

## ModkitLimitAdjusterEnhanced

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

## PackfileLimitAdjusterEnhanced

Sizes and hooked call (`EnhancedPatch`):

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| P-E-D3 | `48 83 C6 ? 48 81 F9` | +7: direct size (builds 1.0.1158.13 and later); 7260 on current build | found | 0x10F1BF |
| P-E-D2 | `49 83 c3 ? 48 81 f9 ? ? ? ? 75 ? eb` | +7: direct size (builds 1.0.1013.17 to 1158.12) | not used on current build | - |
| P-E-D1 | `48 81 f9 ? ? ? ? 74 ? 48 83 3c ca 00` | +3: direct size (older builds) | not used on current build | - |
| P-E-I1 | `3b 05 ? ? ? ? 0f 8d ? ? ? ? 89 c0` | +2: rel32 to the indirect size variable | found | 0x1279156 |
| P-E-H1 | `89 48 ? 48 8d 0d ? ? ? ? 48 89 08 48 89 c1 e8` | +0x10: the streaming configuration call; 8512 archive count on current build | found | 0x5093D5 |

References to the packfile array (`AdjustPackFileArrayLimit`, rel32 moved to the new array):

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
| P-E-R31a | `BF 08 00 00 00 BE 10 00 00 00 4C 8D 05` | 13 (builds 1.0.1158.13 and later). The two constants are TweaksV's: upstream's `BF ? ? ? ? BE ? ? ? ? 4C 8D 05` also matched obfuscated code at 0x582A227 | found | 0x10F163 |
| P-E-R31b | `41 ? ? ? ? ? ? ? ? ? ? 41 bb ? ? ? ? ? 8d` | 20 (older builds) | not used on current build | - |

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
| R-E-M0 | `41 57 41 56 41 55 41 54 56 57 55 53 48 81 ec ? ? ? ? 89 d6 49 89 ce` | the game's GetDevice (called) | found | 0x10CAF0 |
| R-E-M1 | `e8 ? ? ? ? 48 8d 05 ? ? ? ? 66 c7 44 58 02 00 00 66 89 3d` | +26: the initial mount call | found | 0x4F8987 |
| R-E-M2 | `41 57 41 56 41 55 41 54 56 57 55 53 48 81 ec ? ? ? ? 45 89 cd 45 89 c6 49 89 d7 48 89 ce 8b 05` | the archive function (hooked) | found | 0x10F210 |
| R-E-D1 | `56 57 53 48 81 ec ? ? ? ? 48 83 79 ? ? 0f 84 ? ? ? ? 48 89 d7` | the game's fiDevice::Mount (called) | found | 0x109D80 |
| R-E-D2 | `56 57 53 48 81 ec ? ? ? ? 4c 89 cf 44 89 c3` | the game's fiDevice::SetPath (called) | found | 0x109B20 |
| R-E-D3 | `48 8d 05 ? ? ? ? 48 89 06 48 c7 46 ? ? ? ? ? 48 8d 15` | fiDeviceRelative's vtable - never used | removed | - |

## StraightToStoryMode

| Label | Pattern | Use | 1.0.1158.16 | RVA |
|---|---|---|---|---|
| S-E1 | `0F 85 A9 00 00 00 48 8D 54` | splash screen: `jz` at match - 0x0D becomes `jmp` | found | 0x52DC67 |
| S-E2 | `00 E9 ? 05 00 00 E8 ? 07` | legal screen: the 6-byte `jz` (`0F 84`) at match - 0x0F is NOPed | found | 0x52F040 |
| S-E3 | `E8 ? ? ? ? 8B 0D ? ? ? ? 83 C1 ? 84 C0` | landing page: its callee returns 0 | found | 0x1166F |
