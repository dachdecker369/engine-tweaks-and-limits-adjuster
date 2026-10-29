# TweaksV

TweaksV is one ASI plugin for GTA V Enhanced, `TweaksV.asi`, with nine parts: a larger memory heap, room for more vehicle modkits and more RPF archives, a larger traversal list for the game's box streamer, more decal definitions, a mods folder with unencrypted archives, a start straight into Story Mode, the files DirectStorage keeps locked set free, and the game's own blips kept on the map next to a mod's. Five of the parts are written anew from plugins by [Chiheb-Bacha](https://github.com/Chiheb-Bacha), which TweaksV 1.x merged as they were; since 2.0.0 every part is TweaksV's own code, built on the patterns and the research of the originals. Each part has its own section in `TweaksV.ini` and can be switched off there.

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

Three switches under `[RAGEOPENV]`. With `MODS_FOLDER=1`, a file that exists under the `mods` folder in the game's current directory, normally the game folder, is served instead of the game's own, by the same path (`mods\update\update.rpf` for `update\update.rpf`); the folder is read once at start-up, so a file added while the game runs is seen at the next start. A file in the mods folder that cannot be opened - another program holds it, or it was deleted while the game runs - is never replaced by the game's own unnoticed: the game gets the failure, as for any file it cannot open, the log names each such file and a message box the first. With `OPEN_RPF=1`, RPF archives that are not encrypted - the kind OpenIV writes, marked OPEN - load. With `SKIP_RPF_CACHE_CHECK=1`, the game's check of its RPF cache is skipped, which modified archives need. Replaces RageOpenV; OpenIV's `OpenIV.asi` and `OpenRPF.asi` do the same job and must not be installed next to TweaksV. The experimental newmods folder of the original is not carried over. With NaturalVision Enhanced's `onigiri.asi` installed, TweaksV switches this part off (see "Other mods").

With `DIAGNOSTIC_MODE=1` the part logs every file the game asks for, every file served from the mods folder and every archive header read, 20,000 lines of each kind at most, with a summary 10 seconds in and then every minute while the numbers change.

### StraightToStoryMode

`SKIP_SPLASH_SCREEN`, `SKIP_LEGAL_SCREEN` and `SKIP_LANDING_PAGE` under `[STRAIGHTTOSTORYMODE]` (all on) skip the splash screen, the legal screen and the landing page, so the game goes straight into Story Mode. Replaces StraightToStoryMode. With NaturalVision Enhanced's `onigiri.asi` installed, TweaksV switches this part off (see "Other mods").

### DirectStorage

With DirectStorage on, GTA V Enhanced opens the files in its folder at start-up, not just its archives - every file in every subfolder, once its RPF cache check is skipped, which TweaksV's RageOpenV part and every other mods loader do - and keeps them open, letting other programs only read them: while the game runs, mods can't write their own files there and scripts can't be reloaded.

`UNLOCK_FILES=1` under `[DIRECTSTORAGE]` (on by default) lets the game open those files as before, but shared: a file in the game folder that is opened only for reading - by the game, by DirectStorage or by a mods loader such as NaturalVision Enhanced's - can be written, renamed and deleted by other programs while it is open. The `.rpf` archives and `.cache` files stay locked as before, nothing is refused, so the game and its loaders get every file they ask for, and files outside the game folder, such as your saves, are not touched. A mod or a script inside the game can also replace such a file by renaming a new file over it, the usual safe way to save settings: Windows refuses that kind of rename over a file somebody holds open, whatever its sharing, so TweaksV makes it a rename with POSIX semantics, which goes through - the game goes on reading the file it opened until it opens it again; on a drive that does not take POSIX semantics, a FAT drive or a network share, the rename is made as the mod asked. Two limits remain, both Windows' rules for a file somebody holds open: a program that opens a file for writing while letting no one else even read it gets "file in use" while the game holds that file open - one that lets others read while it writes, as most editors and scripts do, and renaming a file are not affected; and a program outside the game that replaces a file by renaming a new file over it gets "access denied" while the game holds that file - such a program has to rename with POSIX semantics, use `ReplaceFile` or write the file in place. TweaksV's own files are written at the start-up, before the game opens its folder, so they are not affected. Ten seconds after the first file, the log counts the opens it shared, their files by type, the opens it left as they were and the renames it gave POSIX semantics.

With `DIAGNOSTIC_MODE=1` the part logs the game's calls into DirectStorage, including every file it asks DirectStorage to open - a file outside the game folder by its name only, without its folder - and every file and folder in the game folder that is opened, with its access, the sharing asked for and the sharing given, its result and caller, and every rename it gives POSIX semantics, with its result and caller (see "TweaksV.ini" below).

TweaksV's own part; the idea comes from alloc8or's [DirectStorageFix](https://github.com/alloc8or/DirectStorageFix), and none of its code is used. With `DirectStorageFix.asi` installed, TweaksV switches its own DirectStorage part off.

### Blips

The game's own blips - the collectibles, the shops, the activities and the rest of the map's markers - are made by one of its scripts, `blip_controller`, which makes them only while fewer than 150 blips are on the map. The blips a mod adds count too, so a mod that puts many of them on the map pushes the game's own off it: the script waits for room that never comes. Collectibles on Map and Radar is the case this part was made for.

`ACTIVE_BLIP_LIMIT` under `[BLIPS]` (default 1900, 150 to 1997) raises that threshold in the script each time the game loads it - at the start and again at every save load - so the game's blips stay on the map next to a mod's. 1997 is the most that still holds when the game's blip pool of 2000 is full; 1900 leaves room for the blips of missions and other scripts. TweaksV changes the script only where it finds the check exactly as it expects: otherwise it leaves the script as it is and says why in a message box, and a threshold another mod has already changed is left as it is and only logged.

With `DIAGNOSTIC_MODE=1` the part logs the two natives the script's check calls, and how many of the blip pool's 2000 slots are in use, 10 seconds in and then every minute while the numbers change.

TweaksV's own part; the design comes from rootBrz's [Blip Limit Fix](https://github.com/rootBrz/gtav-blip-limit-fix), and none of its code was read or used. With `BlipLimitFix.asi` installed, TweaksV switches its own Blips part off.

### Measured first: WeaponLimits, SirenSettings, PedProps and Resources

Four parts to come ([ROADMAP.md](ROADMAP.md)) lift seven more limits of the game: the number of `weapons.meta` files and of weapon components, the siren setting IDs and the sirens per vehicle, the props per anchor of a ped, and the video memory budget for streamed textures and the memory the game streams the world into. A part goes in only once a mod load reaches its limit, so these limits are measured first: with `DIAGNOSTIC_MODE=1`, TweaksV finds the places in the game's code each limit lies in, checks them as the part will, and logs how close the game and your mods come to the limit - 10 seconds in and then every minute while the numbers change. The measurements change nothing in the game - but the siren count's, which has the game's call that draws a vehicle's sirens pass through TweaksV, to look at each vehicle model's siren bones once - and they have no section in `TweaksV.ini` yet; without `DIAGNOSTIC_MODE`, nothing of them runs. What each one's lines tell you:

<!-- [weapon infos] -->
**Weapon infos** (WeaponLimits): the game reads each `weapons.meta` file - add-on weapons bring one each as a rule, packed on their own - into one entry of an array with room for 160 on 1.0.1158.16, and the game never checks that room: a file past it is written into memory the array does not own, and the game crashes while loading or later. The log says, 10 seconds in and then every minute while it changes, how many `weapons.meta` files the game has loaded against that room - `N weapons.meta files loaded (the game's own array holds 160, this one 160)`, the first number the room the game's code gives, the second the room the array really got - so the room left below 160 is how many more weapon files your game takes. At 160 the line adds that the array is full and the next file would be written past its end; past it, that the game's memory is already damaged.

<!-- [weapon components] -->
**Weapon components**: the game keeps the weapon component definitions of all `weaponcomponents.meta` files - each scope, clip, suppressor, flashlight, grip and tint the game's weapons and add-on weapons can take - in one list of 470, and more than that ends the game while it loads. `[WeaponLimits] DIAGNOSTIC: 412 weapon component definitions from 23 weaponcomponents.meta files (the game's own list holds 470).` says how many your game has, from how many files, and so how much room is left for weapon packs; ` - it is full` at its end means that one more definition crashes the game, and a line that says the list's count reads a number outside 0 to 470 means that the list has overflowed already. A pack's definitions count once the game has mounted the pack, on the way into story mode.

<!-- [siren ids] -->
**Siren IDs** (SirenSettings): every siren setting of `carcols.meta` - the lights and pattern of an emergency vehicle's sirens - has an ID that the vehicles name it by, and the game keeps both in one byte: it has 256 IDs and lets vehicles use 255 settings, and it drops or mixes up the settings of packs whose IDs clash, without a word - an ID above 255 in a pack's file is cut to its last byte (300 becomes 44), so packs that use large IDs clash with each other and with the game's own. With `DIAGNOSTIC_MODE=1` the log says, 10 seconds in and then whenever the numbers change, how many siren settings the game and your packs loaded, how many of the 256 IDs are in use, how many settings lie past position 255, which no vehicle can reach, and how many IDs two or more settings were given: `[SirenSettings] DIAGNOSTIC: siren settings loaded: 24 (vehicles can use 255 at most); siren setting IDs in use: 24 of 256; ...`. IDs in use close to 256, or any setting past position 255, mean your packs need the SirenSettings part. A first summary that says `no siren settings loaded yet` only means the game had not read its vehicle files 10 seconds in; the lines before the summary - the places in the game's code it found and checked - are for me after a game update.

<!-- [siren count] -->

**Siren count** (SirenSettings): the game lights 20 sirens of a vehicle, `siren1` to `siren20`. An emergency vehicle made for 32 sirens, as some packs ship them, loads all the same, and its lights past the 20th stay dark without an error; and a siren whose bone lies past the model's 128th bone stays dark too, because the game cannot name it. With `DIAGNOSTIC_MODE=1` TweaksV looks once at each vehicle model whose sirens the game draws, and 10 seconds after the first logs how many such models it has seen, the most sirens the game names on one of them (20 at most), how many carry siren bones past `siren20` that the game never lights and how many lose siren bones past bone 127 - with a line for each model concerned, named by the hash of its name, saying how many of its siren and glass bones are lost. The game draws sirens only while they are on: drive an emergency vehicle with its sirens on, or let the police chase you. This is the one measurement that changes something in the game: the game's call that draws a vehicle's sirens passes through TweaksV, which has the game draw them unchanged first; with `DRY_RUN=1` the call stays as it is and nothing is counted.

<!-- [ped props] -->

<!-- [texture budget] -->

<!-- [streaming memory] -->

## TweaksV.ini

TweaksV writes `TweaksV.ini` in the game folder, next to `GTA5_Enhanced.exe`, on the first start - or in `%LOCALAPPDATA%\TweaksV` if the game folder cannot be written to - with every setting at its default, and adds back any setting that goes missing. `1` is on, `0` is off (`true` and `false` work too). The file carries a short header and, after each key's value, a short comment from `//` to the end of the line; this is what each key does in full. A comment after a value - after `//` or `;` - is never part of it, so you can write your own there. A `TweaksV.ini` from an earlier 2.x version is kept as it is, without the comments: delete it to get the commented one, then set your values again. One from before the Blips part gets the two keys of `[BLIPS]` added and its `VERSION` raised to 3; its values stay as they are. The settings are read once, at start-up: TweaksV's parts patch the game before it runs, so a live panel would change nothing - there is none, by my choice; a changed file takes effect at the next start.

| Section | Key | Default | What it does |
|---|---|---|---|
| `[TWEAKSV]` | `ENABLED` | 1 | the master switch: 0 means TweaksV changes nothing |
| | `LOG` | 1 | writes `TweaksV.log`, in the game folder or in `%LOCALAPPDATA%\TweaksV` when the game folder cannot take it |
| | `DIAGNOSTIC_MODE` | 0 | testing: every part that is on logs every pattern, site and write, and runs its measurement - see "Diagnostics" below |
| | `DRY_RUN` | 0 | testing: everything is looked for and checked - but `blip_controller`'s guard, which the Blips part sees only through its hook; nothing in the game is written or hooked (only RageOpenV's one-shot hook on `GetSystemTimeAsFileTime`, which runs its checks and switches itself off); RageOpenV's hooks are off too, so the mods folder is not served - measuring with that part off alone means its own `ENABLED=0`, not `DRY_RUN` |
| | `CONSOLE` | 0 | a console window that mirrors the log |
| | `ERROR_POPUP` | 1 | a message box when something could not be applied, and one for a part that stops working later (at most three per session) |
| | `CONFLICT_CHECK` | 1 | looks for the old plugins TweaksV replaces and for NaturalVision Enhanced's loader, in the game folder and its `scripts` and `plugins` subfolders, and switches the matching part off while they are there (see "Other mods") |
| | `VERSION` | 3 | the layout of this file; TweaksV keeps it, and raises it when it adds the keys of a newer layout - leave the line in |
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
| `[DIRECTSTORAGE]` | `UNLOCK_FILES` | 1 | the files in the game folder other than `.rpf` and `.cache` stay free while the game holds them open: other programs can write, rename and delete them, and mods inside the game can also replace them by renaming a new file over them - not a program that writes while letting no one else read, and not a program outside the game that replaces a file that way |
| `[BLIPS]` | `ACTIVE_BLIP_LIMIT` | 1900 | the game's own blips are made while fewer blips than this are on the map, 150 to 1997 (the game's own: 150) |

**Coming from TweaksV 1.x:** the 1.x file has another layout and is not migrated. On the first start, TweaksV renames it to `TweaksV.old.ini`, writes a fresh `TweaksV.ini` with the defaults and says so in the start-up message - set your values again, the old file still holds them. You can also delete the old file before the first start. The same happens to any `TweaksV.ini` without `VERSION` under `[TWEAKSV]`, so leave that line in. What changed: `[HEAP_SETTINGS]` is `[HEAP_ADJUSTER]`, `[MODKIT_SETTINGS]` is `[MODKIT_LIMIT]`, `[PACKFILE_SETTINGS]` is `[PACKFILE_LIMIT]` and `[DECALDEFS_SETTINGS]` is `[DECALS]`; the other four sections lost their `_SETTINGS` ending (`[BOXSTREAMER]`, `[RAGEOPENV]`, `[STRAIGHTTOSTORYMODE]`, `[DIRECTSTORAGE]`), and `[TWEAKSV]` kept its name; `LOG_TO_FILE`, `SHOW_ERROR_POPUPS`, `CHECK_FOR_CONFLICTS` and `INI_VERSION` are `LOG`, `ERROR_POPUP`, `CONFLICT_CHECK` and `VERSION`; the testing switches `LOG_SEARCHES`, `LOG_DECALDEFS`, `LOG_INFO`, `LOG_MODS`, `LOG_CALLS` and `LOG_FILE_OPENS` are all `DIAGNOSTIC_MODE` now; `NEWMODS` and `LOG_DEVICE` are gone with the newmods folder.

**Diagnostics:** `DIAGNOSTIC_MODE=1` makes every part that is on log every pattern it looks for (address, number of matches, bytes), every site it finds and every write, and run its measurement: the box streamer counts its searches, the decals part reads how many definitions are registered, RageOpenV logs every file request, every file served from `mods` and every archive header read, DirectStorage logs the game's calls into it, every file opened in the game folder and every rename it gives POSIX semantics, the Blips part the natives its script's check calls and how full the game's blip pool is, and the four parts measured first how close the game and your mods come to their limits (see "Measured first" above). A part with `ENABLED=0` does nothing, so it logs nothing. Two ways to use it: with `DRY_RUN=1` nothing in the game is written or hooked (only RageOpenV's one-shot hook on `GetSystemTimeAsFileTime`, which runs its checks and switches itself off) - the quiet check after a game update, which shows the patterns, the sites and the decal count of the unpatched game - `blip_controller`'s guard only a boot without `DRY_RUN` shows; alone, it is the full measurement of the running game with the mods folder served, and it hooks a little more than a normal boot (the box streamer's four search calls, DirectStorage's configuration calls, its factory and `OpenFile`, and - with `UNLOCK_FILES=0` - the file opens). Every kind of line that comes per event stops after 20,000 - also DirectStorage's `OpenFile` lines - and the events are then only counted, with a summary every minute where the part has one.

## Design

- **GTA V Enhanced only:** TweaksV does nothing in Legacy.
- **Generated files:** `TweaksV.ini` and `TweaksV.log` are created on the first boot. Every part can be switched off or on.
  - They go in the game folder, or in `%LOCALAPPDATA%\TweaksV` if the game folder is read-only.
  - The files in the game folder have priority.
- **Own code:** every part is TweaksV's own, in the `TweaksV*` files and `dllmain.cpp`; the parts carry none of the original plugins' code - what remains of them is the console opener and the scanner's pattern parser, named in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). What TweaksV knows about the game - the byte patterns and what each site holds - is in [PATTERNS.md](PATTERNS.md), and that is the one place to work when the game changes.
- **Tables:** each setting is one entry in `TweaksVSettings.h`, each byte pattern one in `TweaksVPatterns.h`, each part one in `TweaksVFeatures.cpp`. A new part is an entry in these tables plus its own file.
- **Shared code:** patches to the game's code go through `TweaksVPatch` - one writer for bytes, one for redirected calls, the hooks on a function's start through MinHook - which writes all or nothing and gives every page its own protection back; `TweaksVScanner` finds the patterns in the running game's code; the four parts that move one of the game's arrays describe it in a table and share the relocation core, `TweaksVRelocation`, which finds and checks every site before anything is written and then writes all of it in one go; `TweaksVScripts` holds the one hook where the game places its script programs, for every part that works on one - the Blips part, and the test builds' `TEST_DUMP_SCRIPT`.
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
- **Test Builds:** `-p:TweaksVTestBuild=true` gives `object\Test\Release\TweaksV.asi`. Add `TEST_BREAK_PATTERN=<label>` under `[TWEAKSV]` in `TweaksV.ini`, with the label of a pattern in [PATTERNS.md](PATTERNS.md), such as `S-E2`. That pattern then counts as missing, and you can watch its part be skipped cleanly. `TEST_LOG_LIMIT_KB=<KB>` lowers the 64 MB limit of `TweaksV.log`, so you can watch it being reached. `TEST_DUMP_CODE=<seconds>` (1 to 3600) saves the game's code as it is in memory - decrypted, unlike `GTA5_Enhanced.exe` on disk - for looking for patterns: `GameCode-start.bin` before TweaksV patches anything, `GameCode-later.bin` that many seconds later, about 50 MB each, in `%LOCALAPPDATA%\TweaksV`. They hold the game's code, constants and function table, never its data; the format is in `source\TweaksVDump.h`. Delete them when you are done. `TEST_DUMP_SCRIPT=<name>` saves one of the game's script programs - `blip_controller`, say: 1 to 63 letters, digits and `_` - as the game places it in memory, for reading a script's code, which the game's archives store encrypted: each time the game places it, at most 16 times a session, as `Script-<name>-<n>.bin` in `%LOCALAPPDATA%\TweaksV`, with its code, its native table before and after the game resolves it, and its strings; the format is in `source\TweaksVDump.h` too. The log header will list `TEST BUILD`.

## Testing

1. Use GTA V Enhanced in Story Mode with BattlEye disabled. Never go online with mods.
2. Remove the old plugins TweaksV replaces from the game folder and its `scripts` and `plugins` folders: `HeapAdjuster.asi` (or `ZHeapAdjuster.asi`, the name its author suggests), `ModkitLimitAdjusterEnhanced.asi`, `PackfileLimitAdjusterEnhanced.asi`, `RageOpenV.asi`, `OpenRPF.asi`, `OpenIV.asi`, `StraightToStoryMode.asi`, alloc8or's `DirectStorageFix.asi`, `DecalPatch.asi` and rootBrz's `BlipLimitFix.asi` - the eleven old plugins the conflict check knows. While one of them is there, TweaksV switches its matching part off and says so in the start-up message. The twelfth file it knows stays: NaturalVision Enhanced's loader, `onigiri.asi`, which NVE needs; while it is there, TweaksV's RageOpenV and StraightToStoryMode parts are off (see "Other mods").
3. `TweaksV.asi` goes in the game folder next to `GTA5_Enhanced.exe`.
4. Add the ASI loader file `xinput1_4.dll` from the [Script Hook V](http://www.dev-c.com/gtav/scripthookv/) download. It is the only file needed; Script Hook V itself is not.
5. Coming from TweaksV 1.x: your old `TweaksV.ini` is renamed to `TweaksV.old.ini` on the first start and a fresh one is written - see "TweaksV.ini" above. You can also delete the old file first.

## Other mods

- **NaturalVision Enhanced** sets the heap size as well. TweaksV leaves a heap another mod has already set alone and says so in the log, so with NVE installed its heap size wins, whatever the load order. NVE also brings its own loader, `onigiri.asi`, next to `GTA5_Enhanced.exe`: a mods loader that serves the files and archives in its `onigiri` folder, and that skips the start-up screens its INI, `onigiri.ini`, names. It patches the same places in the game as TweaksV's RageOpenV and StraightToStoryMode parts, after TweaksV, and crashes the game when TweaksV has changed them first. So while `onigiri.asi` is installed, TweaksV switches those two parts off and says so in the start-up message; the other seven parts work as usual (with `CONFLICT_CHECK=0` TweaksV does not look for it, and the game crashes while those two parts are on). Your mods then go where onigiri serves them, in its `onigiri` folder, not in TweaksV's `mods` folder, and which start-up screens are skipped is onigiri's business, set in its `onigiri.ini` under `[PATCHES]`: with NVE's defaults the game went straight into Story Mode on my boots. To stop the message, set `ENABLED=0` under `[RAGEOPENV]` and under `[STRAIGHTTOSTORYMODE]` in `TweaksV.ini`.
- **gameconfig.xml:** a gameconfig that raises the archive count has no effect while the Packfile part is on - TweaksV replaces that count with `PACKFILEARRAY_SIZE` when the game reads its configuration, exactly, also when the gameconfig's count is higher. Everything else in a gameconfig works as usual.
- **Zolika Patch:** the ModkitLimit Extender's author warned that its modkit patch may not be compatible with Zolika Patch's; the same goes for TweaksV's Modkit part.
- **DecalPatch.asi**, a closed-source plugin that raises the box streamer list and the decal definitions limit for GTA V Enhanced, is detected as an old plugin: TweaksV switches its decal definitions part and its box streamer part off while it is installed, so nothing is patched twice. Delete it - TweaksV replaces both halves.
- **Mods that hook the game's file functions:** TweaksV's RageOpenV part hooks on top of another mod's hook on GetFileTime or GetFileSize and chains through it. A hook on OpenBulk or GetAttributes changes the bytes TweaksV finds those two functions by: the mods folder is then not used, and the start-up message names the pattern that was not found.
- **Collectibles on Map and Radar** puts so many markers on the map that the blips in use pass the threshold of the game's own script: 243 of them on my boot of 2.3.0, against its 150. With the Blips part on, the game's own blips stay next to the mod's markers, and Script Hook V, which the mod needs, runs next to TweaksV without a conflict (my boots of 2026-10-28).
- **Script Hook V, ScriptHookVDotNet, OpenIV:** neither needed nor touched. Only the ASI loader file from the Script Hook V download is used, and OpenIV's `OpenIV.asi` and `OpenRPF.asi` must not be installed next to TweaksV (their job is TweaksV's RageOpenV part).

## Reporting a problem

Attach `TweaksV.log` - its first lines name the game build and the files in use - and `TweaksV.ini`; say which store the game comes from (Rockstar, Steam, Epic); list the other `.asi` and `.dll` files in the game folder; and describe what happened against what you expected. A crash dump helps if you have one. Security problems go the private way described below.

## Security

Please report a security problem privately, not in a public issue: [SECURITY.md](SECURITY.md) says how.

## Credits

Five of TweaksV's parts are rewritten from the five plugins by Chiheb-Bacha and build on the research of everyone listed in [CREDITS.md](CREDITS.md), which also credits the ideas behind TweaksV's own four parts: BoxStreamer, Decals, DirectStorage and Blips. All credit for the original research and code goes to them.

Third-party notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Engine Tweaks and Limits Adjuster is GPL-3.0-only.

## License

Copyright (C) dachdecker369. Engine Tweaks and Limits Adjuster is free software under the GNU General Public License, version 3 only; the text is in [LICENSE](LICENSE).
