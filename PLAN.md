# Plan: TweaksV 2.0.0

The order of work for TweaksV 2.0.0: the five merged plugins rewritten as TweaksV's own parts, newmods dropped, `TweaksV.ini` layout 2, the `upstream\` archives gone. [SPECIFICATIONS.md](SPECIFICATIONS.md) says what the result must be; this file says in which order it is built and where it is checked. "I" is the author, dachdecker369. Both files are removed again when 2.0.0 has shipped and their content has moved into NOTES.md, PATTERNS.md and README.md.

## Why

- TweaksV carries the five merged plugins line for line, each with its own names, namespace, logging shim and `// TweaksV:` markers, and each re-implementing what the others also do. The four array parts alone do the same job in four loops. One vocabulary and one core make the code smaller, readable and easier to change after a game update.
- The INI has grown to nine sections with a comment per key. Layout 2 is the file I want: short sections, the same shape for every part, one testing switch each, the explanations in README.md.
- The newmods folder is experimental, off by default, the most code for the least value, and the one part with an open bug nobody can confirm. It goes.

## Ground rules

- The originals are GPL-3.0 and may be read while rewriting; no line of theirs is pasted. Every part is written from what it does (NOTES.md, the suites) and from PATTERNS.md, on TweaksV's own tables and services. The license, the credits and THIRD_PARTY_NOTICES.md stay as they are.
- The fake-game suites decide whether a rewritten part is the same part: they pass, updated only for names and wording, or the part is not done. `bash test/run_all.sh` is green at the end of every phase, not only at the end.
- The code standards of every TweaksV version: performant, modular, no tech debt, secure, never silent; three clean builds at /W4; CRLF for new source files.
- Nothing is pushed before I have booted the finished build and read its log. Each phase ends with a local commit and, where it changes what the game sees, a boot of mine.
- The new session asks me when SPECIFICATIONS.md leaves something open; it does not guess game facts, and it does not reopen the decisions below.

## Phases

### 0. Decisions - taken

- [x] Newmods is dropped, with its five patterns, its device, its keys and the open issue B-ROV-18.
- [x] The INI is my mockup (SPECIFICATIONS.md, section 3.1), plus `DRY_RUN` after `DIAGNOSTIC_MODE` and `VERSION` (was `INI_VERSION`) as the last line of `[TWEAKSV]`.
- [x] `LOG_DUMP` is each part's one testing switch: its site dump plus its runtime measurement; `DIAGNOSTIC_MODE` is all of them plus the scanner's lines.
- [x] A 1.x `TweaksV.ini` is not migrated: it is renamed to `TweaksV.old.ini`, a fresh default file is written, the start-up summary says so, and README tells people they can also delete the old file.
- [x] The names of SPECIFICATIONS.md, section 2: section, feature name, log tag and file per part; the "Enhanced" suffixes go; the pattern labels stay.
- [x] The release is 2.0.0, once, with everything in it. The INI comes first inside the work, as phase 1.

### 1. INI layout 2

- [ ] The settings table in `TweaksVSettings.h` in the order and with the names, kinds, defaults and limits of section 3.2; the named references the code uses; `kIniLayoutVersion` 2; the header text of section 3.1; no comment per key.
- [ ] `TweaksVIni.cpp`: a file without `VERSION` is a 1.x file - renamed to `TweaksV.old.ini` through the own-file checks, the fresh default written, the summary line; the rest as today.
- [ ] The parts read the new keys (the old specific switches are gone: `LOG_SEARCHES`, `LOG_DECALDEFS`, `LOG_INFO`, `LOG_MODS`, `LOG_DEVICE`, `LOG_CALLS`, `LOG_FILE_OPENS` become `LOG_DUMP`; `NEWMODS` is read by nothing and newmods is switched off for good in this phase, the code goes in phase 4).
- [ ] Tests: `base.ini`, `expected_default.ini`, every `setkey` and `addkey`, the `ini_default` suite with the 1.x scenario and `test\layout1.ini`; the key count re-based.
- [ ] README: the "TweaksV.ini" section and the upgrade note.
- Checkpoint: suites green; I delete my `TweaksV.ini`, boot, and the fresh file equals the mockup byte for byte; a boot with my 1.6.0 file in place renames it and reports once.

### 2. The template: HeapAdjuster and StraightToStoryMode

- [ ] `TweaksVHeapAdjuster.cpp`, `.h` and `TweaksVStraightToStoryMode.cpp`, `.h` written on the pattern table, `TweaksVPatch` and the settings; the originals' files deleted; the feature table entries renamed; the log wording of section 5.4.
- [ ] The `story` suite and the integration checks on the new lines.
- Checkpoint: suites green; my boot with `DIAGNOSTIC_MODE=1` (the three screens skipped, the heap at 2000 MB, the lines in the new wording).

### 3. The relocation core and the four array parts

- [ ] `TweaksVRelocation.cpp`, `.h` as section 5.1 describes.
- [ ] `TweaksVModkitLimit` and `TweaksVPackfileLimit` written on it, with the packfile's streaming configuration hook kept in its own file; the originals' files deleted.
- [ ] `TweaksVBoxStreamer` and `TweaksVDecals` (renamed from `TweaksVDecalDefs`) moved onto the core where it fits; their counted calls, count global and loop stay theirs.
- [ ] The `modkit`, `packfile`, `boxstreamer` and `decals` (renamed) suites on the new lines.
- Checkpoint: suites green; my boot with `DIAGNOSTIC_MODE=1` and `DRY_RUN=1` (every pattern found once, nothing written), then a normal boot with my mods folder (every array moved, the archive count replaced, the searches and the decal count measured with `LOG_DUMP=1`).

### 4. RageOpenV

- [ ] `TweaksVRageOpenV.cpp`, `.h` (and a second `.cpp` only if one would pass about 40 KB): the mods list and the eight file hooks, the five OPEN archive redirects, the cache check, the console patch, the start-up at the game's first time request; `HookFunction` in `TweaksVPatch` from `memory.h`'s helpers; every original file of section 2's "Replaces" column deleted, the newmods code with them.
- [ ] The five newmods patterns removed from the table and marked removed in PATTERNS.md; the fake game's newmods sites and report field removed; the `rageopenv` suite without its newmods scenarios, the `integration` suite without the `R-E-M1` breaktest and the newmods part of I8.
- Checkpoint: suites green; my boot with my mods folder and OPEN archives: "file hooks 8/8" in the suite, the mods served and the archives loaded in game, `LOG_DUMP=1` once.

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
- A 1.x `TweaksV.ini` in a read-only game folder: the rename and the fresh file happen in `%LOCALAPPDATA%\TweaksV`, as the file itself does; the `ini_default` suite covers it.
- The newmods removal reaches a user who had `NEWMODS=1`: nothing reads it; the old file is renamed on the first start; README says newmods is gone.

## Next steps

1. A new session takes this file and SPECIFICATIONS.md, the code, the tests and the docs, and works through phases 1 to 5 in order, each phase ending with the suites green, the three builds clean, a local commit, and my boot where the phase changes what the game sees.
2. Phase 6: the 2.0.0 commit, my boots of SPECIFICATIONS.md section 8, then push and tag on my word, then Nexus, then the removal of these two files.
3. Open for the new session to ask me about: nothing that these two files decide; anything they leave unclear.
