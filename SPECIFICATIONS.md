# Specifications: TweaksV 2.0.0

What TweaksV 2.0.0 must be: the five merged plugins rewritten as TweaksV's own parts on TweaksV's own machinery, the experimental newmods folder dropped, `TweaksV.ini` in its second layout, and the `upstream\` archives gone. The order of work is in [PLAN.md](PLAN.md). "I" in these two files is the author, dachdecker369; the session that does the work reads them, the code, the tests and the docs, and asks me when something is missing or unclear instead of guessing. Both files are removed again when 2.0.0 has shipped and their content has moved into README.md, NOTES.md and PATTERNS.md.

## 1. What changes, what stays

- Every part keeps the behaviour 1.6.0 has: the same patterns, the same checks before anything is written, the same patches and hooks, the same conflict handling, the same failure reporting. The exceptions are listed here and nowhere else: newmods is gone (section 4.6, in phase 1); each part's specific testing switches fold into `DIAGNOSTIC_MODE` (section 3.3); the log wording is unified (section 5.4); the names in the INI, the log and the code are those of section 2.
- The merged code is not kept line for line. Each part is written anew from what it does (NOTES.md, the test suites that pin it) and from the game facts in PATTERNS.md, on the tables and services TweaksV already has: the pattern table, the settings table, the feature table, `TweaksVPatch`, the scanner, the log, the summary. The originals are GPL-3.0 and may be read while doing it; no line of theirs is pasted. TweaksV stays GPL-3.0-only, the credits stay, THIRD_PARTY_NOTICES.md stays as it is.
- The fake-game test suites are the arbiter: a rewritten part passes the checks that pin its behaviour, updated only for the new names and wording, or it is not done.
- The box streamer, decals and DirectStorage parts are TweaksV's own already; they change names, settings and wording, and the two array parts move onto the shared relocation core (section 5.1).
- Version 2.0.0. `TweaksV.ini` layout 2.

## 2. Names

| Part | Section | Feature name and log tag | Source file | Replaces |
|---|---|---|---|---|
| Heap adjuster | `[HEAP_ADJUSTER]` | `HeapAdjuster` | `TweaksVHeapAdjuster.cpp`, `.h` | `HeapAdjusterEnhanced_Game.cpp`, `.h` |
| Modkit limit | `[MODKIT_LIMIT]` | `ModkitLimit` | `TweaksVModkitLimit.cpp`, `.h` | `ModkitLimitAdjusterEnhanced_Game.*`, `_Logging.*` |
| Packfile limit | `[PACKFILE_LIMIT]` | `PackfileLimit` | `TweaksVPackfileLimit.cpp`, `.h` | `PackfileLimitAdjusterEnhanced_Game.*`, `_Logging.*` |
| Box streamer | `[BOXSTREAMER]` | `BoxStreamer` | `TweaksVBoxStreamer.cpp`, `.h` (exists) | - |
| Decals | `[DECALS]` | `Decals` | `TweaksVDecals.cpp`, `.h` | `TweaksVDecalDefs.cpp`, `.h` (renamed) |
| RageOpenV | `[RAGEOPENV]` | `RageOpenV` | `TweaksVRageOpenV.cpp`, `.h`; a second `.cpp` only if one would pass about 40 KB | `main.cpp`, `main.h`, `config.h`, `log.cpp`, `log.h`, `memory.h`, `rpf.h`, `fiDevice.cpp`, `fiDevice.h`, `fiPackfile.h`, `CustomDevice.cpp`, `FileReadHooks.cpp`, `PackfileEncryptionHooks.cpp` |
| Straight to story mode | `[STRAIGHTTOSTORYMODE]` | `StraightToStoryMode` | `TweaksVStraightToStoryMode.cpp`, `.h` | `StraightToStoryMode_Game.*`, `_Logging.*` |
| DirectStorage | `[DIRECTSTORAGE]` | `DirectStorage` | `TweaksVDirectStorage.cpp`, `.h` (exists) | - |

Shared code, section 5: `TweaksVRelocation.cpp`, `.h` (new); `TweaksVPatch` absorbs `StubMemory.cpp`, `.h` and the hook helpers of `memory.h`; `MemScanner.cpp`, `.h` become `TweaksVScanner.cpp`, `.h`; `GameVersion.h` and `IsEnhanced.h` fold into `TweaksVGame.h`; `ClearLog.cpp`, `.h` go. The "Enhanced" suffixes go everywhere: file names, namespaces, log tags, the `Features:` line. The pattern labels keep their letters (`H-`, `M-`, `P-`, `B-`, `D-`, `R-`, `S-`) and their numbers, so PATTERNS.md and the diagnostics of 1.x stay comparable; the five newmods patterns (`R-E-M0`, `R-E-M1`, `R-E-M2`, `R-E-D1`, `R-E-D2`) are removed. The old plugins the conflict check looks for do not change: `HeapAdjuster.asi`, `ZHeapAdjuster.asi`, `ModkitLimitAdjusterEnhanced.asi`, `PackfileLimitAdjusterEnhanced.asi`, `DecalPatch.asi` (box streamer and decals), `RageOpenV.asi`, `OpenRPF.asi`, `OpenIV.asi`, `StraightToStoryMode.asi`, `DirectStorageFix.asi`.

## 3. TweaksV.ini, layout 2

### 3.1 The file

The fresh default file is exactly this, CRLF line endings, one blank line between sections, no comment anywhere but the header; `test\expected_default.ini` is this text and the `ini_default` suite compares byte for byte. Explanations of the keys live in README.md, not in the file.

```ini
; ===========================================================================
; TweaksV.asi Configuration File (GTA V Enhanced)
; 1 = on, 0 = off (true/false also work)
; TweaksV.ini in the game folder takes priority over %LOCALAPPDATA%\TweaksV\
; Delete this file to get a fresh one with default values on next game boot
; Missing settings are added back by TweaksV with the default value
; For more details on what each setting does, check the README.md
; ===========================================================================

[TWEAKSV]
ENABLED=1
LOG=1
DIAGNOSTIC_MODE=0
DRY_RUN=0
CONSOLE=0
ERROR_POPUP=1
CONFLICT_CHECK=1
VERSION=2

[HEAP_ADJUSTER]
ENABLED=1
HEAP_SIZE=2000

[MODKIT_LIMIT]
ENABLED=1
MODKITARRAY_SIZE=65536

[PACKFILE_LIMIT]
ENABLED=1
PACKFILEARRAY_SIZE=13000

[BOXSTREAMER]
ENABLED=1
TRAVERSAL_LIST_SIZE=10000

[DECALS]
ENABLED=1
DECALDEFS_SIZE=2048

[RAGEOPENV]
ENABLED=1
MODS_FOLDER=1
OPEN_RPF=1
SKIP_RPF_CACHE_CHECK=1

[STRAIGHTTOSTORYMODE]
ENABLED=1
SKIP_SPLASH_SCREEN=1
SKIP_LEGAL_SCREEN=1
SKIP_LANDING_PAGE=1

[DIRECTSTORAGE]
ENABLED=1
UNLOCK_FILES=1
```

### 3.2 The keys

Every key is one entry of the settings table in `TweaksVSettings.h`, in this order; kinds, defaults and limits as given. "Was" is the 1.x key it replaces, for README's upgrade note and for the mapping in my head - nothing in the code reads the old names (section 3.4). Gone without a replacement: `NEWMODS` and `LOG_DEVICE` (newmods goes, PLAN.md phase 1) and every per-part testing switch, which `DIAGNOSTIC_MODE` covers (section 3.3).

| Section | Key | Kind, default, limits | Does | Was |
|---|---|---|---|---|
| `[TWEAKSV]` | `ENABLED` | switch, 1 | the master switch: 0 means TweaksV changes nothing (the file is still kept) | same |
| | `LOG` | switch, 1 | writes `TweaksV.log` (in the game folder, or in `%LOCALAPPDATA%\TweaksV` when the game folder cannot take it) | `LOG_TO_FILE` |
| | `DIAGNOSTIC_MODE` | switch, 0 | the one testing switch (section 3.3): every part that is on dumps the sites it finds, the values they hold and its writes, and runs its measurement where it has one; the scanner logs its line per pattern (address, number of matches, bytes; the bytes around each match of a pattern that matches more than once) | same, plus `LOG_SEARCHES`, `LOG_DECALDEFS`, `LOG_INFO`, `LOG_MODS`, `LOG_CALLS`, `LOG_FILE_OPENS` |
| | `DRY_RUN` | switch, 0 | everything is looked for and checked, nothing is written or hooked; the "would" lines say what would change. `DIAGNOSTIC_MODE=1` with `DRY_RUN=1` is the check after a game update | same |
| | `CONSOLE` | switch, 0 | a console window that mirrors the log | same |
| | `ERROR_POPUP` | switch, 1 | the start-up summary as a message box when there is something to report, and a box for a part that stops working later (at most three per session) | `SHOW_ERROR_POPUPS` |
| | `CONFLICT_CHECK` | switch, 1 | the old plugins of section 2 are looked for in the game folder and its `scripts` and `plugins` subfolders; a match switches the part off for the session and is reported | `CHECK_FOR_CONFLICTS` |
| | `VERSION` | number, 2, 2..1000000 | the layout of this file, kept by TweaksV; the last line of the section; a file with a higher number is reported as from a newer TweaksV | `INI_VERSION` |
| `[HEAP_ADJUSTER]` | `ENABLED` | switch, 1 | the part on or off | `[HEAP_SETTINGS] ENABLED` |
| | `HEAP_SIZE` | number, 2000, 1..2047 | the heap size in MB; a value below the game's own is raised to the game's own | same key |
| `[MODKIT_LIMIT]` | `ENABLED` | switch, 1 | | `[MODKIT_SETTINGS] ENABLED` |
| | `MODKITARRAY_SIZE` | number, 65536, 1..65536 | the number of modkit IDs; clamped to the game's own size..65536 | same key |
| `[PACKFILE_LIMIT]` | `ENABLED` | switch, 1 | | `[PACKFILE_SETTINGS] ENABLED` |
| | `PACKFILEARRAY_SIZE` | number, 13000, 1..65535 | the number of RPF archives; clamped to the game's own size..65535; replaces the archive count of `gameconfig.xml` when the game reads its configuration | same key |
| `[BOXSTREAMER]` | `ENABLED` | switch, 1 | | `[BOXSTREAMER_SETTINGS] ENABLED` |
| | `TRAVERSAL_LIST_SIZE` | number, 10000, 1000..100000 | map boxes one search of the box streamer can return | same key |
| `[DECALS]` | `ENABLED` | switch, 1 | | `[DECALDEFS_SETTINGS] ENABLED` |
| | `DECALDEFS_SIZE` | number, 2048, 260..16384 | decal definitions the game can hold | same key |
| `[RAGEOPENV]` | `ENABLED` | switch, 1 | | `[RAGEOPENV_SETTINGS] ENABLED` |
| | `MODS_FOLDER` | switch, 1 | replacement files from the `mods` folder in the game folder | same key |
| | `OPEN_RPF` | switch, 1 | unencrypted ("OPEN") RPF archives can be loaded | same key |
| | `SKIP_RPF_CACHE_CHECK` | switch, 1 | the game's RPF cache check is skipped (needed for modified archives; 0 only for testing) | same key |
| `[STRAIGHTTOSTORYMODE]` | `ENABLED` | switch, 1 | | `[STRAIGHTTOSTORYMODE_SETTINGS] ENABLED` |
| | `SKIP_SPLASH_SCREEN` | switch, 1 | | same key |
| | `SKIP_LEGAL_SCREEN` | switch, 1 | | same key |
| | `SKIP_LANDING_PAGE` | switch, 1 | straight into story mode | same key |
| `[DIRECTSTORAGE]` | `ENABLED` | switch, 1 | | `[DIRECTSTORAGE_SETTINGS] ENABLED` |
| | `UNLOCK_FILES` | switch, 1 | DirectStorage may open only the `.rpf` and `.cache` files in the game folder, so its other files stay free while the game runs | same key |

The three test-build-only keys stay as they are: `TEST_BREAK_PATTERN`, `TEST_LOG_LIMIT_KB` and `TEST_DUMP_CODE` under `[TWEAKSV]`, read by test builds only, never written to the file. The section order is the order the parts start in (the feature table), as today.

### 3.3 DIAGNOSTIC_MODE

One testing switch for every part, `DIAGNOSTIC_MODE` under `[TWEAKSV]`; no part has a switch of its own. On, every part that is on (`ENABLED=1`) logs everything it can: every site it finds, the value it holds and every write (what `DIAGNOSTIC_MODE` logs for the part in 1.x), plus the part's runtime measurement where it has one (what the part's own switch logged in 1.x), and the scanner logs its line per pattern. A part with `ENABLED=0` does not run, so it logs nothing and its patterns are not looked for: the check after a game update needs every part on. Off, a part logs what it does in one line, and its problems.

Two recipes, both in README: `DIAGNOSTIC_MODE=1` with `DRY_RUN=1` is the quiet check after a game update - nothing is hooked, so the log holds the patterns, the sites and the decal count of the unpatched game, nothing else. `DIAGNOSTIC_MODE=1` alone is the full measurement of the running game with the mods folder served: the searches are counted, every file request and every file open is logged. A diagnostic boot hooks a little more than a normal one - the four tree-search calls and the file opens - and README says so.

Every output that comes per event - a line per file request, per file served, per file open - is capped at 20,000 lines of its kind; after that the events are only counted, and a summary follows every minute, so a diagnostic boot can never fill `TweaksV.log` and lose its own summaries. DirectStorage's file opens work that way in 1.6.0; RageOpenV's lines get the same cap in phase 4.

| Part | The site dump | The runtime measurement |
|---|---|---|
| HeapAdjuster | the pattern, the vanilla size, the write | none |
| ModkitLimit | the 6 references, the 8 limits with their decoded game size, the new array, the writes | none |
| PackfileLimit | the references, the direct and indirect sizes, the streaming configuration call, the new array, the writes; the archive count the game's configuration held when the hook replaced it | none |
| BoxStreamer | the 4 capacities, the list, the 8 loads, the writes | the searches: the four tree-search calls go through TweaksV's call stubs to a counting wrapper (as `LOG_SEARCHES` did - with `DIAGNOSTIC_MODE` on, the calls are redirected in the same all-or-nothing write as the references and capacities); 10 seconds in and then every minute while the numbers change, how many searches ran, the most entries one returned and how many returned more than the game's own list could hold |
| Decals | the 16 patterns' sites, the array and its count, the 31 loads, the writes | the count of registered definitions 10 seconds in and then every minute while it changes, against the limit and the game's own 260, with how many the game's own array would have dropped; nothing is hooked |
| RageOpenV | the hooked functions and redirected calls with their addresses, the mods list's size | status messages and every file request (`LOG_INFO`), every file served from `mods\` and every OPEN archive (`LOG_MODS`) - capped, then counted, with a summary every minute (phase 4) |
| StraightToStoryMode | the three sites, their bytes before and after | none |
| DirectStorage | the factory's functions and the configuration values | the game's calls into `dstorage.dll` and every `OpenFile` (`LOG_CALLS`); every open of a file or folder in the game folder with access, sharing, result and caller - capped, then counted - plus a summary every minute (`LOG_FILE_OPENS`) |

### 3.4 An existing file

- A file with `VERSION` under `[TWEAKSV]` is a layout-2 file: missing keys are added back with their defaults, existing values are never changed, a `VERSION` above 2 is reported as a file from a newer TweaksV - all as today.
- A file without `VERSION` under `[TWEAKSV]` is a 1.x file, whatever else it holds - a 1.6.0 file, an empty or damaged one, a hand-written one, a layout-2 file whose `VERSION` line was deleted: one rule, no guessing, and nothing is lost, because the old file is kept and named on screen. Nothing of it is read. TweaksV renames it to `TweaksV.old.ini` next to it through the same checks as its own files: a link is never touched; an existing `TweaksV.old.ini` is replaced when it passes those checks and otherwise refused; a rename that fails is reported in its own words - the file has the layout of 1.x and could not be renamed, delete or rename it yourself - and TweaksV runs on its built-in defaults, every boot, until the file is gone. Then it writes the fresh default file and says so in the start-up summary, which is a PROBLEM line in the log and a message box with `ERROR_POPUP=1`; the log header's `TweaksV.ini` line says it too: "TweaksV.ini had the layout of TweaksV 1.x. It was renamed to TweaksV.old.ini and a fresh TweaksV.ini written with the default settings - set your values again." No value migrates; the old names are not known to the code. README's upgrade note says the same, that the old file can simply be deleted, and that `VERSION=2` stays in the file.
- A read-only game folder keeps its 1.x behaviour: the file goes to `%LOCALAPPDATA%\TweaksV`, and a 1.x file there is handled the same way. A 1.x file that sits in a game folder TweaksV cannot write to cannot be renamed, and a fresh file elsewhere would never win over it: that is the reported case above, every boot, with the built-in defaults.

## 4. The parts

Each part: what it does (unchanged from 1.6.0 unless said), what the rewrite must keep, its settings, what pins it. The game facts - patterns, offsets, encodings - are PATTERNS.md's and are not repeated here.

### 4.1 HeapAdjuster

- Finds `H-E1` and raises the game's heap size (the imm32 of a `mov edx` at +1, in bytes) to `HEAP_SIZE` MB. The instruction is checked before the write; the vanilla size is read from it and logged; `HEAP_SIZE` is clamped to the game's own size..2047 with no integer overflow; a heap size that is not the vanilla one - 1024 MB or more - was set by another heap adjuster and is left alone, logged, not a problem: what another mod has already changed, TweaksV does not change again (NaturalVision Enhanced sets it too, so its size wins whatever the load order; README's "Other mods" says so, and the rename advice goes).
- One `WriteCode`, `DRY_RUN`, `DIAGNOSTIC_MODE`, the failure line in the summary. Old plugins: `HeapAdjuster.asi`, `ZHeapAdjuster.asi`.
- Pinned by the `ini_default`, `story` (the heap line of every scenario) and `integration` suites: the fake site's "heap: 2000 MB".

### 4.2 ModkitLimit

- Moves the game's modkit array (entries of 2 bytes, 1024 on the current build) to one of 65536 entries, always allocated in full as read/write data near the game, and raises the eight limits that encode its size (`M-E-L1..L8`: `imm32 = size x mul + add`, every one must decode to the same game size) to `MODKITARRAY_SIZE`, clamped to the game's own size..65536. The six references (`M-E-R1..R6`, the rel32 of a `lea`) must all point at the game's own array, so it is never moved twice.
- Written on the relocation core (section 5.1): everything found and checked first, one write, every page its own protection back. Old plugin: `ModkitLimitAdjusterEnhanced.asi`. README's "Other mods" keeps the Zolika Patch warning.
- Pinned by the `modkit` suite (32 checks: the defaults, the limits of the setting, an array another mod moved, a reference that points elsewhere, a limit that disagrees, `DRY_RUN`, `DIAGNOSTIC_MODE`, every page restored) and the `integration` suite.

### 4.3 PackfileLimit

- Moves the game's packfile array (entries of 8 bytes, 7260 on the current build) to one of 65535 entries and raises its two size limits - the direct one (`P-E-D3`, `D2` or `D1` by game build) and the indirect one, a variable the game compares against (`P-E-I1`) - to `PACKFILEARRAY_SIZE`, clamped to the game's own size..65535. The references are `P-E-R01..R30` plus `R31a` or `R31b` by build; all must point at the game's own array. The array moves before the limits rise.
- The streaming configuration call (`P-E-H1`) is redirected through a call stub to a function that replaces the archive count the game read from `gameconfig.xml` with `PACKFILEARRAY_SIZE` and then calls the game's own function; the vanilla count and the replaced one are logged when it runs. A call that another mod already redirected is left alone and reported. The redirection is written together with the references and limits, all or nothing.
- Old plugin: `PackfileLimitAdjusterEnhanced.asi`. README's "Other mods" keeps the gameconfig note.
- Pinned by the `packfile` suite (51 checks: the defaults, the per-build patterns, the array moved, the hook called the way the game would, a hooked call, an implausible size, the second copy of `R31a`, the obfuscated code the old pattern matched, `DRY_RUN`, `DIAGNOSTIC_MODE`) and the `integration` suite.

### 4.4 BoxStreamer and 4.5 Decals

- As 1.6.0 (NOTES.md has every check), moved onto the relocation core where it fits: the references by capacity site plus address scan with an exact count, the limits by pattern, the all-or-nothing write. The box streamer's counted tree searches and the decals' count global, start-up loop and multi-site anchors stay in their own files as their specifics.
- `LOG_SEARCHES` and `LOG_DECALDEFS` fold into `DIAGNOSTIC_MODE` (phase 1); the `DecalDefs` name becomes `Decals` in the file, the tag, the `Features:` line and the pattern comments; the labels `B-E-L1..L4` and `D-E-L1..L16` stay.
- Old plugin for both: `DecalPatch.asi`.
- Pinned by the `boxstreamer` (52 checks) and `decaldefs` suites (51 checks; the suite is renamed `decals`).

### 4.6 RageOpenV

- Three jobs, each its own switch, each hooked all or nothing: the mods folder (`MODS_FOLDER`), unencrypted archives (`OPEN_RPF`), the cache check (`SKIP_RPF_CACHE_CHECK`); plus the console patch when `CONSOLE=1`. Everything about newmods goes in phase 1, before the rewrite: the custom device, the two mount hooks, the five patterns, the `NEWMODS` and `LOG_DEVICE` keys, the `newmods` folder line of the log header, the fake game's newmods sites and the suites' newmods scenarios, and the open issue B-ROV-18 with it. `LOG_INFO` and `LOG_MODS` fold into `DIAGNOSTIC_MODE` (phase 1); their per-request lines are capped (section 3.3, phase 4).
- The mods folder: the eight file functions (`R-E-F1..F8`: OpenBulk, GetFileTime, GetFileSize and GetAttributes, two of each; two are found by pattern at their start, the others as a fixed distance before a match, and every start is checked with the game's unwind data before it is hooked) are hooked with MinHook so that a path that exists under `mods\` is served from there. The folder is read once at start-up into a list (at most 200,000 files and folders, 32 levels deep; more is reported in the summary) and every call looks its path up in that list: no file system call, memory allocation or lock per call; both spellings of a path (slashes, case) match; a path with `..` is never redirected; a file that does not exist under `mods\` reaches the game's own function untouched. Without a `mods` folder nothing is hooked (`DIAGNOSTIC_MODE` still checks the patterns). A function another mod already hooked (a jump at its start) is warned about, hooked on top, and chains through.
- OPEN archives: five calls (`R-E-E1..E5`: FindEncryption, DecryptHeader, DecryptHeader2 twice - both must lead to the same function - and ParseHeader) are redirected through call stubs so that an archive whose header says OPEN is read without decryption; the encryption tag is kept per thread, so archives opened at the same time never mix; an encrypted archive's entry table is never walked. A call another mod already redirected is left alone.
- The cache check (`R-E-K1`): the checked function's start gets `C3 90`; a check another mod already skipped is left alone and is not a problem.
- The start-up order stays: the part installs its hooks not in DllMain but when the game first asks for the time (a hook on `GetSystemTimeAsFileTime` that runs the part's start-up once, then shows the start-up summary and switches itself off), because the game's file system is not up in DllMain. The fake game's `RovFirstTimeRequest()` is that call.
- Old plugins: `RageOpenV.asi`, `OpenRPF.asi`, `OpenIV.asi`. README's "Other mods" keeps the line that OpenIV's `OpenIV.asi` must not be installed next to TweaksV.
- Pinned by the `rageopenv` suite (57 checks today; minus the newmods scenarios) and the `integration` suite: the harness's "file hooks 8/8, redirect 8/8, pass 8/8, spellings ok, folder ok, outside ok | open calls 5/5, behaviour ok" line - its "newmods hooks" field goes.

### 4.7 StraightToStoryMode

- Three patches, one switch each: the splash screen (`S-E1`: the `jz` at match - 0x0D becomes a `jmp` computed from the game's own instruction), the legal screen (`S-E2`: the 6-byte `jz` at match - 0x0F is NOPed) and the landing page (`S-E3`: its callee's start becomes `return 0`). The bytes must be the game's own before a write (the splash and legal sites must hold a `jz`); an already-patched screen is skipped quietly; a landing-page function another mod hooked is left alone and reported; a missing pattern skips only that screen.
- Old plugin: `StraightToStoryMode.asi`.
- Pinned by the `story` suite (33 checks) and the `integration` suite: the harness's "splash: ... | legal: ... | landing: ..." bytes.

### 4.8 DirectStorage

- As 1.6.0: `UNLOCK_FILES` hooks the factory's `OpenFile` while the game creates the factory, so that in the game folder only `.rpf` and `.cache` files reach DirectStorage; files outside the game folder are never refused; the summary 10 seconds in; `LOG_CALLS` and `LOG_FILE_OPENS` fold into `DIAGNOSTIC_MODE`, together with the factory's functions and configuration values it logs there already - in phase 1, since no later phase touches this part. Old plugin: `DirectStorageFix.asi`.
- Pinned by the `directstorage` suite (49 checks).

## 5. Shared code

### 5.1 The relocation core, `TweaksVRelocation`

The four array parts do the same thing with different tables; the core does it once. A part gives it, as `constexpr` tables:

- the entry size and the maximum number of entries (the new array is always allocated in full, read/write, near the game, zeroed);
- how the references are found: patterns with the offset of the rel32 in the match, and/or an address scan for every `lea` of the array with an exact expected count (the box streamer's 8, the decals' 31); which found reference gives the array's address first;
- the limit sites: patterns with the offset of the imm32 and its encoding - `N x mul + add` (the modkit's eight), `N` (the box streamer's four), `N-1` or `N` or a loop end (the decals'), a variable the game reads instead of an immediate (the packfile's indirect size);
- its names for the log: the array ("the modkit array", "the box streamer's traversal list") and the failure line.

The core finds everything, verifies it (every reference points at one array inside the game; every limit decodes to the same game size; the expected counts; a plausible size), clamps the setting to [the game's own size, the maximum], logs the `DRY_RUN` line, allocates, and writes once: the rel32 of every reference recomputed with the int32 distance check, every limit re-encoded. Success and failure lines and `DIAGNOSTIC` lines have one format for all four parts. What does not fit - the packfile's hook, the box streamer's counted calls, the decals' count global and loop - stays in the part's file and uses the core for the rest. The core is not forced on a part where it would be longer than the part's own loop.

### 5.2 TweaksVPatch

As today (`WriteCode`, `RedirectCalls`, `CallTarget`, `Rel32Target`, `StartsWithJump`, `IsFunctionStart`), plus `AllocateNearGame` (from `StubMemory`, the same search below the game's image) and the function-start hooks from `memory.h`: one `HookFunction` on MinHook with the checks the parts need (a function start by unwind data, a jump already there), used by RageOpenV's eight file functions and DirectStorage's `OpenFile`. MinHook stays in `vendor\` as it is; T-HARDEN stays an open issue.

### 5.3 TweaksVScanner

`MemScanner` renamed, nothing else: the search is TweaksV's own, the pattern parser keeps its ScriptHookVDotNet notice in the file and in THIRD_PARTY_NOTICES.md. `Find`, `FindAll`, `FindRipRelativeLeas` as today.

### 5.4 The log

- The header as today, with the new names: "Features: HeapAdjuster on, ModkitLimit on, PackfileLimit on, BoxStreamer on, Decals on, RageOpenV on, StraightToStoryMode on, DirectStorage on", then one line per part with its keys ("HeapAdjuster: HEAP_SIZE=2000"). No `newmods` line.
- One wording for the same thing in every part: a part that moved an array says "<the array> moved and its limit raised from A to B entries."; a patch says "<what> patched: <old bytes> -> <new bytes>."; a hook says "<function> hooked at RVA 0x..."; a missing pattern "Pattern <label> not found" (the scanner's line) and the part's own "<the part's job> is skipped this session: <label> was not found."; every failure ends in the start-up summary with one line that names what is not done this session and points at the log. The originals' lines ("Patch successful!", "Heap Size Adjusted to:", "Vanilla Modkit List Size is:") go, and the suites' checks move to the new lines.
- `TweaksV.log` is still cut at 64 MB, still writes `%USERPROFILE%` for the profile folder, still goes to `%LOCALAPPDATA%\TweaksV` when the game folder cannot take it.

### 5.5 What goes

`upstream\` with its five archives; `ClearLog.*`, `GameVersion.h`, `IsEnhanced.h`, `StubMemory.*`, `MemScanner.*` (renamed), every file in the "Replaces" column of section 2. The project and the filters list only the `TweaksV*` files, `dllmain.cpp`, `version.rc` and MinHook; the filters are "TweaksV" and "MinHook". The `// TweaksV:` marker convention ends with the merged code.

## 6. Tests

- The fake games (`fake_game.cpp`, `fake_game_full.cpp`, `fake_game_rov.cpp`, `fake_dstorage.cpp`) simulate the game, not TweaksV, and change only where the game-side simulation does: the newmods sites, the "newmods hooks" field and the `RovForEachDataSite` entries for them go from `fake_game_rov.cpp` (phase 1). Every other site, option and report string stays.
- `test\base.ini` and `test\expected_default.ini` move to layout 2 (`expected_default.ini` is section 3.1 byte for byte; `base.ini` keeps the values it has, `HEAP_SIZE=1024` and `PACKFILEARRAY_SIZE=12000`, which every suite asserts and which prove the file is read). Every `setkey KEY VALUE SECTION` and `addkey` in the suites uses the new names; every check of a log line uses the new wording; the scenarios of the measuring switches (`LOG_SEARCHES`, `LOG_DECALDEFS`, `LOG_MODS`, `LOG_CALLS`, `LOG_FILE_OPENS`) become `DIAGNOSTIC_MODE` scenarios, which makes them fewer; the `ini_default` suite's count of keys added back and the `all` line of `run_all.sh` are re-based and written into NOTES.md.
- `ini_default` gets the 1.x file scenarios of section 3.4: a 1.x `TweaksV.ini` (the 1.6.0 default text, kept as `test\layout1.ini`, with one value that is not the default, so that it can be proved that nothing of it was read) is renamed to `TweaksV.old.ini`, a fresh file equal to `expected_default.ini` is written, the summary line appears once, the game runs on the defaults; the same in `%LOCALAPPDATA%\TweaksV` with a read-only game folder; a 1.x file inside the read-only game folder, which is reported in its own words and not read; a second 1.x file with `TweaksV.old.ini` already there, which is replaced; an empty file and a damaged one, which count as 1.x files.
- `rageopenv` loses its newmods scenarios (the mount call, the device paths, `NEWMODS=1` with and without the folder) and `integration` the `R-E-M1` breaktest and the newmods part of I8, both in phase 1; `decaldefs` is renamed `decals` (phase 3); `patterns` checks the table minus the five removed patterns against PATTERNS.md, where their rows say "removed" (phase 1).
- `bash test/run_all.sh` must be green at every phase of PLAN.md, not only at the end.

## 7. Documentation

- README.md: "Plugins Merged" becomes "Parts", one section per part in the order of section 2, each with what it does, its keys and the old plugin it replaces; a "TweaksV.ini" section that explains every key of section 3.2 (the file has no comments), the upgrade from 1.x (the old file is renamed, or can be deleted) and the two diagnostic recipes of section 3.3; "Design" names the tables and the relocation core; "Testing" loses the "settings carry over by name" step; "Other mods" as today; the credits paragraph says the parts are rewritten from the five projects. In phase 1 already: the "TweaksV.ini" section, the upgrade note, the recipes, and every old section or key the existing sections name updated, so README never contradicts the file.
- NOTES.md: the feature bullets rewritten per part, each with the checks it makes, as the box streamer's and decals' bullets are today; the shared code bullets (patch module, pattern table, feature table, relocation core, the log); "Open Issues": B-ROV-18 removed with newmods, T-HARDEN stays; "Bugs Found in the Originals" stays as history, with one line that 2.0.0 carries none of the originals' code.
- PATTERNS.md: the labels and bytes unchanged; the five newmods rows marked removed, the status counts re-based and the after-update step told that every part must be on (phase 1); the "Use" column where it names an original's function gets the part's own words.
- CREDITS.md: "TweaksV's parts are rewritten from the projects below; the research and the original code are theirs" - the five projects, the works they build on, the ideas behind TweaksV's own parts, the libraries, unchanged otherwise. UPSTREAM.md: the five projects with the commit or release each part was derived from (the versions the archives held), no archive table, the note that the code is rewritten. THIRD_PARTY_NOTICES.md: unchanged. `version.rc`: the comment says "Based on plugins by Chiheb-Bacha and others - see CREDITS.md". SECURITY.md: unchanged.
- The global rule that `upstream\` holds the originals as archives is mine to change in my own notes; it no longer applies to this project.

## 8. Done

- Builds: Release, the test build and the analysis build at /W4 without a warning, rebuilt from scratch.
- Tests: `bash test/run_all.sh` green with the re-based count.
- In game, by me, with my mods folder in place: a boot with `DIAGNOSTIC_MODE=1` and `DRY_RUN=1` - every pattern found once, nothing written or hooked, no PROBLEM; a normal boot - every part applied, the mods folder served, OPEN archives loaded, the game runs as it did on 1.6.0; one boot with `DIAGNOSTIC_MODE=1` and without `DRY_RUN`, the full measurement; a boot with my `TweaksV.ini` deleted - the fresh file equals section 3.1 byte for byte; a boot with a 1.x `TweaksV.ini` in place - renamed, replaced, reported once; ten minutes of play without a crash dump or an Application-log error.
- Performance: nothing per frame beyond what 1.6.0 has (the mods folder lookup without allocation or lock; the measuring threads' one read per minute).
- Security: every value read from the game or the INI bounds-checked as today; the new arrays read/write, never executable; the call stubs read+execute only; the mods list's limits; no code of the originals.
- Never silent: every part that is skipped says so in the start-up summary; the 1.x file is reported when replaced.

## Next steps

1. The work session has read this file, PLAN.md, the code, the tests and the docs; what they left open is decided and written in here and under PLAN.md's decisions; it asks me again whenever something new comes up, and starts each phase on my go.
2. PLAN.md's phases in order, each ending with the suites green, the three builds clean and my boot; nothing is pushed before my boots of the finished build.
3. The release as 2.0.0: commit, my boots of section 8, push and tag, Nexus with the upgrade note, then this file and PLAN.md are removed.
