# Plan: TweaksV 2.0.0

The order of work for TweaksV 2.0.0: the five merged plugins rewritten as TweaksV's own parts, newmods dropped, `TweaksV.ini` layout 2, the `upstream\` archives gone. [SPECIFICATIONS.md](SPECIFICATIONS.md) says what the result must be; this file says in which order it is built and where it is checked. "I" is the author, dachdecker369. Both files are removed again when 2.0.0 has shipped and their content has moved into NOTES.md, PATTERNS.md and README.md.

## Why

- TweaksV carries the five merged plugins line for line, each with its own names, namespace, logging shim and `// TweaksV:` markers, and each re-implementing what the others also do. The four array parts alone do the same job in four loops. One vocabulary and one core make the code smaller, readable and easier to change after a game update.
- The INI has grown to nine sections with a comment per key. Layout 2 is the file I want: short sections, the same shape for every part, one testing switch for all of them, the explanations in README.md.
- The newmods folder is experimental, off by default, the most code for the least value, and the one part with an open bug nobody can confirm. It goes.

## Ground rules

- The originals are GPL-3.0 and may be read while rewriting; no line of theirs is pasted. Every part is written from what it does (NOTES.md, the suites) and from PATTERNS.md, on TweaksV's own tables and services. The license, the credits and THIRD_PARTY_NOTICES.md stay as they are.
- The fake-game suites decide whether a rewritten part is the same part: they pass, updated only for names and wording, or the part is not done. `bash test/run_all.sh` is green at the end of every phase, not only at the end.
- The code standards of every TweaksV version: performant, modular, no tech debt, secure, never silent; three clean builds at /W4; CRLF for new source files.
- Nothing is pushed before I have booted the finished build and read its log. Each phase ends with a local commit and, where it changes what the game sees, a boot of mine.
- The new session asks me when SPECIFICATIONS.md leaves something open; it does not guess game facts, and it does not reopen the decisions below.

## Phases

### 0. Decisions - taken

- [x] Newmods is dropped, with its five patterns, its device, its keys and the open issue B-ROV-18 - entirely in phase 1: the suites must be green after every phase, and the patterns suite wants every pattern of the table used in the code, so the code cannot stay switched off until phase 4. Phase 4 then rewrites RageOpenV's three jobs only.
- [x] The INI is my mockup (SPECIFICATIONS.md, section 3.1) without its `LOG_DUMP` lines, plus `DRY_RUN` after `DIAGNOSTIC_MODE` and `VERSION` (was `INI_VERSION`) as the last line of `[TWEAKSV]`: 28 keys.
- [x] `DIAGNOSTIC_MODE` is the one testing switch, for every part: on, every part that is on dumps its sites and runs its measurement, and the scanner logs its lines; a part with `ENABLED=0` does nothing, so it logs nothing and its patterns are not looked for; off, a part logs one line and its problems. No part has a switch of its own: the `LOG_DUMP` keys of the first draft are dropped, and `LOG_SEARCHES`, `LOG_DECALDEFS`, `LOG_INFO`, `LOG_MODS`, `LOG_CALLS` and `LOG_FILE_OPENS` fold into `DIAGNOSTIC_MODE`. Every per-event output is capped, then counted, with a summary every minute (SPECIFICATIONS.md, section 3.3).
- [x] A 1.x `TweaksV.ini` is not migrated: a file without `VERSION` under `[TWEAKSV]`, whatever else it holds, is renamed to `TweaksV.old.ini` (an existing `TweaksV.old.ini` is replaced when it passes the own-file checks), a fresh default file is written, the start-up summary says so, and README tells people they can also delete the old file. A 1.x file TweaksV cannot rename is reported every boot and not read (section 3.4).
- [x] The names of SPECIFICATIONS.md, section 2: section, feature name, log tag and file per part; the "Enhanced" suffixes go; the pattern labels stay. Phase 1 changes the sections and keys; the feature names and log tags change with each part, in phases 2 to 4.
- [x] The suites' `base.ini` keeps the values it has (`HEAP_SIZE=1024`, `PACKFILEARRAY_SIZE=12000`): every suite asserts them, which proves the file is read.
- [x] What another mod has already changed, TweaksV does not change again: that job stays off for the session, as every part does with a moved array, a redirected call or a hooked function - the heap included (a heap another adjuster set is left as it is, logged, not a problem).
- [x] The release is 2.0.0, once, with everything in it. The INI comes first inside the work, as phase 1.

### 1. INI layout 2

- [x] The settings table in `TweaksVSettings.h` in the order and with the names, kinds, defaults and limits of section 3.2; the named references the code uses; `kIniLayoutVersion` 2; the header text of section 3.1; no comment per key. The section names and keys only: the feature names and log tags change with each part (phases 2 to 4).
- [x] `TweaksVIni.cpp`: a file without `VERSION` is a 1.x file - renamed to `TweaksV.old.ini` through the own-file checks (an existing `TweaksV.old.ini` replaced when it passes them), the fresh default written, the summary line, the log header's `TweaksV.ini` line; a rename that fails reported in its own words, the defaults used; the rest as today.
- [x] The parts read `DIAGNOSTIC_MODE` where they read `LOG_SEARCHES`, `LOG_DECALDEFS`, `LOG_INFO`, `LOG_MODS`, `LOG_CALLS` and `LOG_FILE_OPENS`; the merged parts' site dumps stay on `DIAGNOSTIC_MODE` as they are; DirectStorage gets the whole of section 3.3 now, since no later phase touches it. The wording that names a key follows the key: the message box footer and the "summary not shown" line say `ERROR_POPUP`, the log path line says `LOG=0`.
- [x] Newmods removed entirely: `CustomDevice.cpp` and `fiDevice.cpp` deleted, the device code of `fiDevice.h` with them (the vtable stays for `fiPackfile.h` until phase 4), the `device` log kind, the five patterns out of the table and marked removed in PATTERNS.md with its counts re-based, the fake game's newmods sites and its "newmods hooks" field, the `rageopenv` newmods scenarios, the `R-E-M1` breaktest and the newmods part of I8, the log header's newmods line, the open issue B-ROV-18.
- [x] Tests: `base.ini` (its values kept), `expected_default.ini`, every `setkey` and `addkey`, the measuring scenarios as `DIAGNOSTIC_MODE` scenarios, the `ini_default` suite with the 1.x scenarios of section 6 and `test\layout1.ini`; the key count re-based.
- [x] README: the "TweaksV.ini" section, the upgrade note and the two diagnostic recipes; every old section or key the existing sections name updated and the Testing step about settings carrying over gone, so README never contradicts the file. PATTERNS.md: the five rows removed, the counts re-based, the after-update step says every part must be on.
- Checkpoint: suites green; I delete my `TweaksV.ini`, boot, and the fresh file equals section 3.1 byte for byte; a boot with my 1.6.0 file in place renames it and reports once.

### 2. The template: HeapAdjuster and StraightToStoryMode

- [x] `TweaksVHeapAdjuster.cpp`, `.h` and `TweaksVStraightToStoryMode.cpp`, `.h` written on the pattern table, `TweaksVPatch` and the settings; the originals' files deleted; the feature table entries renamed; the log wording of section 5.4.
- [x] The `story` suite and the integration checks on the new lines.
- Checkpoint: suites green; my boot with `DIAGNOSTIC_MODE=1` (the three screens skipped, the heap at 2000 MB, the lines in the new wording).

### 3. The relocation core and the four array parts

- [ ] `TweaksVRelocation.cpp`, `.h` as section 5.1 describes.
- [ ] `TweaksVModkitLimit` and `TweaksVPackfileLimit` written on it, with the packfile's streaming configuration hook kept in its own file; the originals' files deleted.
- [ ] `TweaksVBoxStreamer` and `TweaksVDecals` (renamed from `TweaksVDecalDefs`) moved onto the core where it fits; their counted calls, count global and loop stay theirs.
- [ ] The `modkit`, `packfile`, `boxstreamer` and `decals` (renamed) suites on the new lines.
- Checkpoint: suites green; my boot with `DIAGNOSTIC_MODE=1` and `DRY_RUN=1` (every pattern found once, nothing written), then a normal boot with my mods folder (every array moved, the archive count replaced, the searches and the decal count measured with `DIAGNOSTIC_MODE=1`).

### 4. RageOpenV

- [ ] `TweaksVRageOpenV.cpp`, `.h` (and a second `.cpp` only if one would pass about 40 KB): the mods list and the eight file hooks, the five OPEN archive redirects, the cache check, the console patch, the start-up at the game's first time request; `HookFunction` in `TweaksVPatch` from `memory.h`'s helpers; every original file of section 2's "Replaces" column deleted; RageOpenV's per-event lines capped (section 3.3).
- Checkpoint: suites green; my boot with my mods folder and OPEN archives: "file hooks 8/8" in the suite, the mods served and the archives loaded in game, `DIAGNOSTIC_MODE=1` once.

### 5. Shared code and docs

- [ ] `TweaksVPatch` absorbs `StubMemory` (`AllocateNearGame`); `MemScanner` becomes `TweaksVScanner`; `GameVersion.h` and `IsEnhanced.h` become `TweaksVGame.h`; `ClearLog` goes; the project and the filters list only `TweaksV*`, `dllmain.cpp`, `version.rc` and MinHook, in two filters.
- [ ] `upstream\` deleted (`git rm`); UPSTREAM.md rewritten; CREDITS.md and `version.rc` reworded; README.md, NOTES.md and PATTERNS.md as section 7; the check count in NOTES.md.
- [ ] Version 2.0.0 in `source\TweaksV.vcxproj`.
- Checkpoint: the whole of section 8 - the three builds from scratch, the suites, my boots.

### 6. Release

- [ ] Commit locally in the style of the "TweaksV 1.x.y" commits, Release rebuilt from that commit, my boots of section 8, then push `main` and the annotated tag `2.0.0`.
- [ ] Nexus: the description rewritten around the parts, the upgrade note (delete or let TweaksV rename the old `TweaksV.ini`), the SHA-256, the changelog, the credits.
- [ ] This file and SPECIFICATIONS.md removed.

## Risks

- Behaviour drift in RageOpenV, where the subtle facts live (the function starts found at a distance from a match, the per-thread encryption tag, the start-up at the first time request, a function another mod hooked). The `rageopenv` and `integration` suites pin them site by site; what they cannot reach - real archives, real files - my boots do, with my mods folder and OPEN archives.
- A part that proves awkward on the relocation core: the core is not forced (section 5.1); the part keeps its own loop and the core is used for the rest.
- Users posting logs with the new wording while the old wording is in the Nexus description: the description is rewritten in phase 6, and the header still names the version.
- A 1.x `TweaksV.ini` in a read-only game folder: the rename and the fresh file happen in `%LOCALAPPDATA%\TweaksV`, as the file itself does; one that sits inside the read-only folder is reported every boot and not read; the `ini_default` suite covers both.
- A layout-2 file whose `VERSION` line was deleted counts as a 1.x file: it is kept as `TweaksV.old.ini` and named on screen, so nothing is lost; README says `VERSION` stays in the file.
- A diagnostic boot of a long session: every per-event output is capped (section 3.3), so the minute summaries survive and the 64 MB log is never filled by them.
- The newmods removal reaches a user who had `NEWMODS=1`: nothing reads it; the old file is renamed on the first start; README says newmods is gone.

## Next steps

1. The work session has read this file, SPECIFICATIONS.md, the code, the tests and the docs, and recorded what they left open under the decisions above; it works through phases 1 to 5 in order, each phase ending with the suites green, the three builds clean, a local commit, and my boot where the phase changes what the game sees. It starts each phase on my go. Phases 1 and 2 are built, committed and booted (phase 2's checkpoint boot is next), the suites green with 423 checks; then phase 3.
2. Phase 6: the 2.0.0 commit, my boots of SPECIFICATIONS.md section 8, then push and tag on my word, then Nexus, then the removal of these two files.
3. Open: nothing; the session asks me again whenever something new comes up.
