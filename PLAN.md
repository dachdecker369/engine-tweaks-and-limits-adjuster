# Plan: the decal definitions part

What it takes to give TweaksV a decal definitions part of its own - the second half of what the Legacy "fwBoxStreamerVariable and decals limit" patch does - built the way the box streamer part was: from TweaksV's own findings about GTA V Enhanced, with none of the Legacy port's or FiveM's code read by whoever writes the code. [SPECIFICATIONS.md](SPECIFICATIONS.md) says what the part must do and what the research had to deliver; this file is the order of work and its checkpoints. Both files are removed again when the part has shipped and their content has moved into NOTES.md, PATTERNS.md and README.md.

## Why

The game keeps its decal definitions - the kinds of bullet holes, blood, scorch and tyre marks, liquids, graffiti and the like, read from the `decals.dat` files - in an array of 260 entries. Add-on and mod content brings its own files, and once 260 definitions are registered the rest are dropped silently: the decals that use them never appear. The closed-source Enhanced plugin that raises this limit together with the box streamer list (`DecalPatch.asi`) is crashing for people, and TweaksV's own box streamer part must not run next to it (README, "Other mods"). A part of TweaksV's own removes the need for that plugin, and lets its file name join the conflict check.

## Ground rules

- Clean room for the code. The research (phase 0) knew the published patches as the idea and as a guide to what to look for in the game; the code of this part is written from the findings file, this plan and the specification alone, by someone who has not read the Legacy port's or FiveM's code, and no line of it is read during the build. The idea is credited (CREDITS.md). Every fact about the Enhanced binary comes from the findings file; nothing is derived from the game during the build.
- The part is built like `TweaksVBoxStreamer.cpp`: patterns in the one table, the array moved at its maximum size before the limit is raised, every reference checked before anything is written, one all-or-nothing write, `DRY_RUN` and `DIAGNOSTIC_MODE`, a failure reported in the start-up summary, a fake-game suite.
- Nothing is pushed before the owner has booted the build and read its log.

## Phases

### 0. Research - done

Deliverable: a findings file that fills every field of the template in SPECIFICATIONS.md, section 3, from Ghidra on a `TEST_DUMP_CODE` dump of 1.0.1158.16. It exists (kept outside the repository, next to the box streamer findings) and says, in short:

- [x] The limit is hard-coded: 260 (0x104), one compare in the `decals.dat` parser; `gameconfig.xml` has nothing to do with it.
- [x] One static array of 260 x 80 bytes in zero-initialised `.data`, the count a dword 16 bytes before it; nothing else is sized by N; no entry holds a pointer into the array; no pointer to it in `.rdata`.
- [x] Exactly 31 references, all `lea r64, [rip+disp32]` of the array's first byte - the address scan finds every one, nothing else addresses the array.
- [x] 23 uses of N, all imm32: 13 x N-1, 9 x N (the "no such definition" index value), 1 x the end of an init loop that zeroes 26 entries per pass (0x7D8 + N x 0x50).
- [x] The index is a uint16 in decal instances and a uint32 in the helpers: N can go far beyond 260.
- [x] The count global is the measuring site; the parser's compare is where definitions are dropped; no hook is needed.
- [x] Registration happens during the game's start-up on the loading thread, long after the DllMains; the game's self-modifying code regions touch none of the sites.
- [x] Every anchor pattern matches once; one shared shape matches exactly the 7 "add decal" sites it should.

### 1. Decisions - taken (SPECIFICATIONS.md, section 2)

- [x] `[DECALDEFS_SETTINGS]`, `DECALDEFS_SIZE`, `LOG_DECALDEFS`; feature `DecalDefs`; files `TweaksVDecalDefs.h/.cpp`; labels `D-E-L1..`.
- [x] Minimum 260, default 2048, maximum 16384.
- [x] The measuring switch reads the count global; no hook.
- [x] The conflict file is `DecalPatch.asi`, under this part and the box streamer part.
- [x] A pointer variable in `.data` cannot be excluded from the dump (`.data` is never saved); accepted: everything in the code goes through the 31 leas, and the in-game test with more than 260 definitions would show a stale pointer at once.

### 2. Implementation

- [x] `source\TweaksVDecalDefs.h` and `.cpp`, the entries in `TweaksVSettings.h`, `TweaksVPatterns.h` (16 patterns, `D-E-L1..L16`) and `TweaksVFeatures.cpp`, the project and filters.
- [x] No new scan helper for the references: `MemScanner::FindRipRelativeLeas` covers the 31. One small addition, `MemScanner::FindAll`, gives every match of a pattern, for the "add decal" check that must match exactly 7 times (`Find` is now its first-match case); `Rel32Target` moved from the box streamer part into `TweaksVPatch` for both parts.
- [x] The conflict entries for `DecalPatch.asi`, under this part and the box streamer part.
- [x] The fake-game sites in `test\fake_game_full.cpp`, `test\run_decaldefs.sh` (20 scenarios), the ini and integration suites, the suite list in `test\run_all.sh`, the check count in NOTES.md.
- [x] README (a section like "Box streamer", the "Other mods" lines, the install list), NOTES (the feature bullet; the measurement waits for the owner's boot), PATTERNS.md (a section, the status counts), CREDITS.md and UPSTREAM.md (the idea's origin, extended from the box streamer entries).
- [x] Version 1.5.0 -> 1.6.0 in `source\TweaksV.vcxproj`.

### 3. Verification

- [x] Release, the test build and the analysis build at /W4 without a warning; `bash test/run_all.sh` green (418 checks).
- [ ] A boot with `DIAGNOSTIC_MODE=1`: every new pattern found once at the RVAs of the findings, the array's address, 31 references, 23 constants, every write; no `PROBLEM`.
- [ ] A boot with `LOG_DECALDEFS=1` and a decal-heavy load: the count passes 260 without a crash or missing decals; with the part off, the same load shows the failure it is meant to cure (or the number says the load never reaches 260 - then the test load is too small, not the part wrong).
- [ ] A ten-minute session: no crash dump, no Windows error.

### 4. Release

- [ ] Commit locally, rebuild Release from that commit, the owner's boots, then push and the annotated tag `1.6.0`.
- [ ] Nexus: the BUILT section, IMPORTANT (`DecalPatch.asi` is now detected and replaced), the install list (its file name), the SHA-256, the changelog, the credits.
- [ ] Remove PLAN.md and SPECIFICATIONS.md.

## Risks

- A pointer variable in `.data` holding the array's address (see phase 1, last point): accepted, caught by the first in-game test beyond 260 definitions.
- A game update moves the sites: the findings file's anchors (section 3.8) are the way to find them again from a new dump; the RVAs themselves are not to be trusted after an update.
- The init loop's end must stay a multiple of 26 entries from its start; the rounding rule in SPECIFICATIONS.md, section 4, keeps it so for any setting value.

## Next steps

1. Done here: the part is built to SPECIFICATIONS.md and passes phase 3's own checks (three clean builds, 418 checks green); it is committed locally as TweaksV 1.6.0, with `x64\Release\TweaksV.asi` rebuilt from that commit.
2. The owner boots it. First with `DIAGNOSTIC_MODE=1`: every `D-E-L` pattern found once (`D-E-L12` 7 times) at the RVAs in PATTERNS.md, 31 "the array is loaded at" lines, 23 "limit at ... holds" lines, 54 writes, the line "Decal definitions array moved and its limit raised from 260 to 2048 entries.", no `PROBLEM`. Then with `LOG_DECALDEFS=1` and the decal-heavy mods: the count passes 260 without a crash or missing decals (with `DRY_RUN=1` and the same load, the game's own array shows as full at 260). Then a ten-minute session without a crash dump. Then push `main` and the tag `1.6.0`.
3. Phase 4: the measurement into NOTES.md, Nexus, and the removal of this file and SPECIFICATIONS.md.
