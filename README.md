# TweaksV

TweaksV is one ASI plugin for GTA V Enhanced, `TweaksV.asi`, with eight parts: a larger memory heap, room for more vehicle modkits and more RPF archives, a larger traversal list for the game's box streamer, more decal definitions, a mods folder with unencrypted archives, a start straight into Story Mode, and the files DirectStorage keeps locked set free. Five of the parts are written anew from plugins by [Chiheb-Bacha](https://github.com/Chiheb-Bacha), which TweaksV 1.x merged as they were; since 2.0.0 every part is TweaksV's own code, built on the patterns and the research of the originals. Each part has its own section in `TweaksV.ini` and can be switched off there.

## Parts

### HeapAdjuster

`HEAP_SIZE` under `[HEAP_ADJUSTER]` (default 2000 MB, 1 to 2047) sets the size of the game's memory heap, one number in its start-up code; the game's own is 646 MB. A value below the game's own is raised to it and reported. A heap of 1024 MB or more is not the game's own: another heap adjuster set it, and TweaksV leaves it as it is and says so in the log. Replaces HeapAdjusterEnhanced.

### ModkitLimit

`MODKITARRAY_SIZE` under `[MODKIT_LIMIT]` (default 65536, 1 to 65536; a value below the game's own 1024 is raised to it and reported) moves the game's array of vehicle modkit IDs, 1024 entries, to a larger one and raises the eight limits the game's code carries for it, so more add-on vehicles with modkits of their own fit. Replaces ModkitLimitAdjusterEnhanced.

### PackfileLimit

`PACKFILEARRAY_SIZE` under `[PACKFILE_LIMIT]` (default 13000, 1 to 65535; a value below the game's own 7260 is raised to it and reported) moves the game's array of RPF archives, 7260 entries, to a larger one, raises both limits the game's code carries for it, and replaces the archive count of `gameconfig.xml` with the same number when the game reads its configuration, so more add-on archives load. The count is replaced exactly, also when it was higher: the game's own configuration holds 8512 on 1.0.1158.16, so a `PACKFILEARRAY_SIZE` below 8512 lowers it. Replaces PackfileLimitAdjusterEnhanced.

### BoxStreamer

The game's box streamer finds the map boxes to stream for the area around the player and keeps what one search returns in a list of 1000 entries. With heavy map mods a search returns more: the boxes the list has no room for are never streamed, and their LODs are missing.

`TRAVERSAL_LIST_SIZE` under `[BOXSTREAMER]` (default 10000, 1000 to 100000) moves the list to a larger one. TweaksV's own part; the idea comes from the CitizenFX Collective's fix for FiveM and 0x-FADED's port of it to GTA V Legacy, and none of their code was read or used.

With `DIAGNOSTIC_MODE=1` the part counts the searches and logs, 10 seconds in and then every minute while the numbers change, how many map boxes the largest search returned and how many searches returned more than the game's own list could hold - so you can see whether your map load needs the larger list at all. Measure with a larger list: with `TRAVERSAL_LIST_SIZE=1000`, the game's own size, no search can return more than the list holds, and the log says so.

### Decals

The game reads its decal definitions - the kinds of bullet holes, blood, scorch and tyre marks, liquids, graffiti and so on - from the `decals.dat` files into an array of 260 entries. Once 260 are registered, every further definition is dropped silently; the files are read in order, so it is the definitions of the last files - add-on and mod content - that go missing, and the decals that use them never appear. This is a limit on the kinds of decal, not on the decals on screen. Vanilla 1.0.1158.16 already registers 259 of the 260, so a mod that adds two definitions is one too many for the game's own array.

`DECALDEFS_SIZE` under `[DECALS]` (default 2048, 260 to 16384) moves the array to a larger one and raises the limit everywhere the game uses it. TweaksV's own part, written from TweaksV's own findings about the game; the idea comes from the CitizenFX Collective's fix for FiveM and 0x-FADED's port of it to GTA V Legacy. The published patches were known as the idea and as a guide to what to look for; no line of their code was read for the build or went into TweaksV.

With `DIAGNOSTIC_MODE=1` the part logs how many decal definitions are registered - 10 seconds in and then every minute while the number changes - and whether the game's own array of 260 would have dropped some, so you can see whether your load needs the larger array at all. With `DRY_RUN=1` as well, it measures the game as it is, without the mods folder, which a dry run does not serve.

### RageOpenV

Three switches under `[RAGEOPENV]`. With `MODS_FOLDER=1`, a file that exists under the `mods` folder in the game's current directory, normally the game folder, is served instead of the game's own, by the same path (`mods\update\update.rpf` for `update\update.rpf`); the folder is read once at start-up, so a file added while the game runs is seen at the next start. A file in the mods folder that cannot be opened - another program holds it, or it was deleted while the game runs - is never replaced by the game's own unnoticed: the game gets the failure, as for any file it cannot open, the log names each such file and a message box the first. With `OPEN_RPF=1`, RPF archives that are not encrypted - the kind OpenIV writes, marked OPEN - load. With `SKIP_RPF_CACHE_CHECK=1`, the game's check of its RPF cache is skipped, which modified archives need. Replaces RageOpenV; OpenIV's `OpenIV.asi` and `OpenRPF.asi` do the same job and must not be installed next to TweaksV. The experimental newmods folder of the original is not carried over.

With `DIAGNOSTIC_MODE=1` the part logs every file the game asks for, every file served from the mods folder and every archive header read, 20,000 lines of each kind at most, with a summary 10 seconds in and then every minute while the numbers change.

### StraightToStoryMode

`SKIP_SPLASH_SCREEN`, `SKIP_LEGAL_SCREEN` and `SKIP_LANDING_PAGE` under `[STRAIGHTTOSTORYMODE]` (all on) skip the splash screen, the legal screen and the landing page, so the game goes straight into Story Mode. Replaces StraightToStoryMode.

### DirectStorage

With DirectStorage on, GTA V Enhanced opens the files in its folder at start-up, not just its archives, and keeps them open, letting other programs only read them: while the game runs, mods can't write their own files there and scripts can't be reloaded.

`UNLOCK_FILES=1` under `[DIRECTSTORAGE]` (on by default) lets the game open those files as before, but shared: a file in the game folder that is opened only for reading - by the game, by DirectStorage or by a mods loader such as NaturalVision Enhanced's - can be written, renamed and deleted by other programs while it is open. The `.rpf` archives and `.cache` files stay locked as before, nothing is refused, so the game and its loaders get every file they ask for, and files outside the game folder, such as your saves, are not touched. One limit: a program that opens a file for writing while letting no one else even read it gets "file in use" while the game holds that file open; one that lets others read while it writes, as most editors and scripts do, and renaming a file are not affected. Ten seconds after the first file, the log counts the opens it shared, their files by type, and the opens it left as they were.

With `DIAGNOSTIC_MODE=1` the part logs the game's calls into DirectStorage, including every file it asks DirectStorage to open - a file outside the game folder by its name only, without its folder - and every file and folder in the game folder that is opened, with its access, the sharing asked for and the sharing given, its result and caller (see "TweaksV.ini" below).

TweaksV's own part; the idea comes from alloc8or's [DirectStorageFix](https://github.com/alloc8or/DirectStorageFix), and none of its code is used. With `DirectStorageFix.asi` installed, TweaksV switches its own DirectStorage part off.

## TweaksV.ini

TweaksV writes `TweaksV.ini` in the game folder, next to `GTA5_Enhanced.exe`, on the first start - or in `%LOCALAPPDATA%\TweaksV` if the game folder cannot be written to - with every setting at its default, and adds back any setting that goes missing. `1` is on, `0` is off (`true` and `false` work too). The file carries a short header and no comment per key; this is what each key does. The settings are read once, at start-up: TweaksV's parts patch the game before it runs, so a live panel would change nothing - there is none, by my choice; a changed file takes effect at the next start.

| Section | Key | Default | What it does |
|---|---|---|---|
| `[TWEAKSV]` | `ENABLED` | 1 | the master switch: 0 means TweaksV changes nothing |
| | `LOG` | 1 | writes `TweaksV.log`, in the game folder or in `%LOCALAPPDATA%\TweaksV` when the game folder cannot take it |
| | `DIAGNOSTIC_MODE` | 0 | testing: every part that is on logs every pattern, site and write, and runs its measurement - see "Diagnostics" below |
| | `DRY_RUN` | 0 | testing: everything is looked for and checked; nothing in the game is written or hooked (only RageOpenV's one-shot hook on `GetSystemTimeAsFileTime`, which runs its checks and switches itself off); RageOpenV's hooks are off too, so the mods folder is not served - measuring with that part off alone means its own `ENABLED=0`, not `DRY_RUN` |
| | `CONSOLE` | 0 | a console window that mirrors the log |
| | `ERROR_POPUP` | 1 | a message box when something could not be applied, and one for a part that stops working later (at most three per session) |
| | `CONFLICT_CHECK` | 1 | looks for the old plugins TweaksV replaces, in the game folder and its `scripts` and `plugins` subfolders, and switches the matching part off while they are there |
| | `VERSION` | 2 | the layout of this file; TweaksV keeps it - leave the line in |
| `[HEAP_ADJUSTER]` | `ENABLED` | 1 | the part on or off - every part's section has this key |
| | `HEAP_SIZE` | 2000 | the game's heap in MB, 1 to 2047; a value below the game's own is raised to it and reported |
| `[MODKIT_LIMIT]` | `MODKITARRAY_SIZE` | 65536 | the number of vehicle modkit IDs, 1 to 65536; a value below the game's own 1024 is raised to it and reported |
| `[PACKFILE_LIMIT]` | `PACKFILEARRAY_SIZE` | 13000 | the number of RPF archives, 1 to 65535; a value below the game's own 7260 is raised to it and reported; it also replaces the archive count of `gameconfig.xml`, exactly |
| `[BOXSTREAMER]` | `TRAVERSAL_LIST_SIZE` | 10000 | map boxes one search of the box streamer can return, 1000 to 100000 (the game's own: 1000) |
| `[DECALS]` | `DECALDEFS_SIZE` | 2048 | decal definitions the game can hold, 260 to 16384 (the game's own: 260) |
| `[RAGEOPENV]` | `MODS_FOLDER` | 1 | replacement files from the `mods` folder in the game's current directory, normally the game folder |
| | `OPEN_RPF` | 1 | unencrypted ("OPEN") RPF archives can be loaded |
| | `SKIP_RPF_CACHE_CHECK` | 1 | skips the game's RPF cache check (needed for modified archives; 0 only for testing) |
| `[STRAIGHTTOSTORYMODE]` | `SKIP_SPLASH_SCREEN` | 1 | skips the splash screen |
| | `SKIP_LEGAL_SCREEN` | 1 | skips the legal screen |
| | `SKIP_LANDING_PAGE` | 1 | goes straight into Story Mode |
| `[DIRECTSTORAGE]` | `UNLOCK_FILES` | 1 | the files in the game folder other than `.rpf` and `.cache` stay free while the game holds them open: other programs can write, rename and delete them - not a program that writes while letting no one else read |

**Coming from TweaksV 1.x:** the 1.x file has another layout and is not migrated. On the first start, TweaksV renames it to `TweaksV.old.ini`, writes a fresh `TweaksV.ini` with the defaults and says so in the start-up message - set your values again, the old file still holds them. You can also delete the old file before the first start. The same happens to any `TweaksV.ini` without `VERSION` under `[TWEAKSV]`, so leave that line in. What changed: `[HEAP_SETTINGS]` is `[HEAP_ADJUSTER]`, `[MODKIT_SETTINGS]` is `[MODKIT_LIMIT]`, `[PACKFILE_SETTINGS]` is `[PACKFILE_LIMIT]` and `[DECALDEFS_SETTINGS]` is `[DECALS]`; the other four sections lost their `_SETTINGS` ending (`[BOXSTREAMER]`, `[RAGEOPENV]`, `[STRAIGHTTOSTORYMODE]`, `[DIRECTSTORAGE]`), and `[TWEAKSV]` kept its name; `LOG_TO_FILE`, `SHOW_ERROR_POPUPS`, `CHECK_FOR_CONFLICTS` and `INI_VERSION` are `LOG`, `ERROR_POPUP`, `CONFLICT_CHECK` and `VERSION`; the testing switches `LOG_SEARCHES`, `LOG_DECALDEFS`, `LOG_INFO`, `LOG_MODS`, `LOG_CALLS` and `LOG_FILE_OPENS` are all `DIAGNOSTIC_MODE` now; `NEWMODS` and `LOG_DEVICE` are gone with the newmods folder.

**Diagnostics:** `DIAGNOSTIC_MODE=1` makes every part that is on log every pattern it looks for (address, number of matches, bytes), every site it finds and every write, and run its measurement: the box streamer counts its searches, the decals part reads how many definitions are registered, RageOpenV logs every file request, every file served from `mods` and every archive header read, DirectStorage logs the game's calls into it and every file opened in the game folder. A part with `ENABLED=0` does nothing, so it logs nothing. Two ways to use it: with `DRY_RUN=1` nothing in the game is written or hooked (only RageOpenV's one-shot hook on `GetSystemTimeAsFileTime`, which runs its checks and switches itself off) - the quiet check after a game update, which shows the patterns, the sites and the decal count of the unpatched game; alone, it is the full measurement of the running game with the mods folder served, and it hooks a little more than a normal boot (the box streamer's four search calls, DirectStorage's configuration calls, its factory and `OpenFile`, and - with `UNLOCK_FILES=0` - the file opens). Every kind of line that comes per event stops after 20,000 - also DirectStorage's `OpenFile` lines - and the events are then only counted, with a summary every minute where the part has one.

## Design

- **GTA V Enhanced only:** TweaksV does nothing in Legacy.
- **Generated files:** `TweaksV.ini` and `TweaksV.log` are created on the first boot. Every part can be switched off or on.
  - They go in the game folder, or in `%LOCALAPPDATA%\TweaksV` if the game folder is read-only.
  - The files in the game folder have priority.
- **Own code:** every part is TweaksV's own, in the `TweaksV*` files and `dllmain.cpp`; the parts carry none of the original plugins' code - what remains of them is the console opener and the scanner's pattern parser, named in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). What TweaksV knows about the game - the byte patterns and what each site holds - is in [PATTERNS.md](PATTERNS.md), and that is the one place to work when the game changes.
- **Tables:** each setting is one entry in `TweaksVSettings.h`, each byte pattern one in `TweaksVPatterns.h`, each part one in `TweaksVFeatures.cpp`. A new part is an entry in these tables plus its own file.
- **Shared code:** patches to the game's code go through `TweaksVPatch` - one writer for bytes, one for redirected calls, the hooks on a function's start through MinHook - which writes all or nothing and gives every page its own protection back; `TweaksVScanner` finds the patterns in the running game's code; the four parts that move one of the game's arrays describe it in a table and share the relocation core, `TweaksVRelocation`, which finds and checks every site before anything is written and then writes all of it in one go.
- **The whole design:** [SPECIFICATIONS.md](SPECIFICATIONS.md) describes every module, format and log line as built, [NOTES.md](NOTES.md) what each part does and measures, [PATTERNS.md](PATTERNS.md) every game fact - with this file, the whole truth about the code: enough to fork it or build on it without reading the source first.

## Building

You need Visual Studio 2026 with the "Desktop development with C++" workload.
Everything else, including MinHook, is in this repo.

Open `source\TweaksV.slnx` in Visual Studio, choose **Release** or **Debug**, and build.
Or use x64 Native Tools Command Prompt for VS:

```
git clone https://github.com/dachdecker369/engine-tweaks-and-limits-adjuster
cd engine-tweaks-and-limits-adjuster
msbuild source\TweaksV.slnx -p:Configuration=Release -p:Platform=x64
```

- **Output:** `x64\Release\TweaksV.asi` and `TweaksV.pdb`. Debug builds go to `x64\Debug` instead.
- **Intermediates:** `object\Release` and `object\Debug`.
- **No Redistributables:** C runtime is statically linked so players don't need it.
- **Commits:** When the folder is a git repo, `TweaksV.log` lists the commit the build came from; otherwise it lists "unknown".

Developer checks - not for release:

- **Tests:** `bash test/run_all.sh` in Git Bash, in the repository folder. It builds the Release and test builds of TweaksV and small fake-game programs, runs every test suite against them and deletes what it made in `object\` (`--keep` leaves it, `--no-build` skips building TweaksV). Building Release replaces `x64\Release\TweaksV.asi`.

Add these switches to the `msbuild` line above:

- **Code Analysis:** `-p:TweaksVAnalyze=true` runs the MSVC code analysis (`/analyze`). Builds into `object\Analyze`.
- **Test Builds:** `-p:TweaksVTestBuild=true` gives `object\Test\Release\TweaksV.asi`. Add `TEST_BREAK_PATTERN=<label>` under `[TWEAKSV]` in `TweaksV.ini`, with the label of a pattern in [PATTERNS.md](PATTERNS.md), such as `S-E2`. That pattern then counts as missing, and you can watch its part be skipped cleanly. `TEST_LOG_LIMIT_KB=<KB>` lowers the 64 MB limit of `TweaksV.log`, so you can watch it being reached. `TEST_DUMP_CODE=<seconds>` (1 to 3600) saves the game's code as it is in memory - decrypted, unlike `GTA5_Enhanced.exe` on disk - for looking for patterns: `GameCode-start.bin` before TweaksV patches anything, `GameCode-later.bin` that many seconds later, about 50 MB each, in `%LOCALAPPDATA%\TweaksV`. They hold the game's code, constants and function table, never its data; the format is in `source\TweaksVDump.h`. Delete them when you are done. The log header will list `TEST BUILD`.

## Testing

1. Use GTA V Enhanced in Story Mode with BattlEye disabled. Never go online with mods.
2. Remove the old plugins TweaksV replaces from the game folder and its `scripts` and `plugins` folders: `HeapAdjuster.asi` (or `ZHeapAdjuster.asi`, the name its author suggests), `ModkitLimitAdjusterEnhanced.asi`, `PackfileLimitAdjusterEnhanced.asi`, `RageOpenV.asi`, `OpenRPF.asi`, `OpenIV.asi`, `StraightToStoryMode.asi`, alloc8or's `DirectStorageFix.asi` and `DecalPatch.asi` - the ten files the conflict check knows. While one of them is there, TweaksV switches its matching part off and says so in the start-up message.
3. `TweaksV.asi` goes in the game folder next to `GTA5_Enhanced.exe`.
4. Add the ASI loader file `xinput1_4.dll` from the [Script Hook V](http://www.dev-c.com/gtav/scripthookv/) download. It is the only file needed; Script Hook V itself is not.
5. Coming from TweaksV 1.x: your old `TweaksV.ini` is renamed to `TweaksV.old.ini` on the first start and a fresh one is written - see "TweaksV.ini" above. You can also delete the old file first.

## Other mods

- **NaturalVision Enhanced** sets the heap size as well. TweaksV leaves a heap another mod has already set alone and says so in the log, so with NVE installed its heap size wins, whatever the load order.
- **gameconfig.xml:** a gameconfig that raises the archive count has no effect while the Packfile part is on - TweaksV replaces that count with `PACKFILEARRAY_SIZE` when the game reads its configuration, exactly, also when the gameconfig's count is higher. Everything else in a gameconfig works as usual.
- **Zolika Patch:** the ModkitLimit Extender's author warned that its modkit patch may not be compatible with Zolika Patch's; the same goes for TweaksV's Modkit part.
- **DecalPatch.asi**, a closed-source plugin that raises the box streamer list and the decal definitions limit for GTA V Enhanced, is detected as an old plugin: TweaksV switches its decal definitions part and its box streamer part off while it is installed, so nothing is patched twice. Delete it - TweaksV replaces both halves.
- **Mods that hook the game's file functions:** TweaksV's RageOpenV part hooks on top of another mod's hook on GetFileTime or GetFileSize and chains through it. A hook on OpenBulk or GetAttributes changes the bytes TweaksV finds those two functions by: the mods folder is then not used, and the start-up message names the pattern that was not found.
- **Script Hook V, ScriptHookVDotNet, OpenIV:** neither needed nor touched. Only the ASI loader file from the Script Hook V download is used, and OpenIV's `OpenIV.asi` and `OpenRPF.asi` must not be installed next to TweaksV (their job is TweaksV's RageOpenV part).

## Reporting a problem

Attach `TweaksV.log` - its first lines name the game build and the files in use - and `TweaksV.ini`; say which store the game comes from (Rockstar, Steam, Epic); list the other `.asi` and `.dll` files in the game folder; and describe what happened against what you expected. A crash dump helps if you have one. Security problems go the private way described below.

## Security

Please report a security problem privately, not in a public issue: [SECURITY.md](SECURITY.md) says how.

## Credits

Five of TweaksV's parts are rewritten from the five plugins by Chiheb-Bacha and build on the research of everyone listed in [CREDITS.md](CREDITS.md), which also credits the ideas behind TweaksV's own three parts: BoxStreamer, Decals and DirectStorage. All credit for the original research and code goes to them.

Third-party notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Engine Tweaks and Limits Adjuster is GPL-3.0-only.

## License

Copyright (C) dachdecker369. Engine Tweaks and Limits Adjuster is free software under the GNU General Public License, version 3 only; the text is in [LICENSE](LICENSE).
