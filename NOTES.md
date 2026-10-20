# Notes

- **`TweaksV.ini`:** created on the first start with its default settings. Missing keys are added back; existing values are never changed.
  - Every setting is defined once, in the table in `TweaksVSettings.h`: its section, key, default, limits and comment. The default file, the keys added back, the log header and every reader come from it, and each setting is read once at start-up.
  - A value TweaksV cannot use - not a number, not on or off, outside its limits - is shown in the start-up summary as well as the log, with the value used instead.
  - `INI_VERSION` under `[TWEAKSV]` says which layout the file has, so a later TweaksV can tell an older file apart; a file from a newer TweaksV is reported.
- **`TweaksV.log`:** one log with a header (versions, paths, switches).
  - Paths in the Windows user's profile folder are written as `%USERPROFILE%\...`, also where the folder has its short form (`C:\Users\NAME~1`, as `%TEMP%` often has): logs get posted in public, and that folder is usually named after the user.
  - It grows to 64 MB at most, room for hours of the logs for testing; a last line says when it stops.
- **Read-only game folders:** both files go to `%LOCALAPPDATA%\TweaksV` when the game folder is read-only, for example a Rockstar Games Launcher install under `C:\Program Files`.
  - A `TweaksV.ini` in the game folder always wins.
  - A read-only folder is normal, so it is only logged; it shows no message box. Any other reason, such as a `TweaksV.log` that another program holds open, is a problem in the start-up summary.
- **Links:** TweaksV writes only to its own files, never through a link to another file - which matters when the game runs as administrator:
  - A `TweaksV.log` in the game folder that is a link (a symbolic or a hard link) is left alone, and the log goes to `%LOCALAPPDATA%\TweaksV`;
  - A `TweaksV.ini` that is a link is neither read nor written: every setting keeps its built-in default;
  - `%LOCALAPPDATA%\TweaksV` must not be a link (a junction) itself; the game folder may be one, for an install moved to another drive;
  - Each of these is a problem in the start-up summary;
  - While TweaksV reads `TweaksV.ini` at start-up, the file cannot be renamed, deleted or replaced;
  - A file Windows keeps compressed (`compact`, CompactGUI) is not a link and is used as usual.

- **Conflicts:** old stand-alone plugins (HeapAdjuster.asi, OpenRPF.asi, ...) are detected. Their TweaksV part is switched off for the session and listed in a start-up summary.

- **Problems on screen:** every problem at start-up is listed in one message box, the start-up summary (`SHOW_ERROR_POPUPS=1`, the default). A part that stops working later, such as the DirectStorage hook a few seconds in, gets a message box of its own, at most three per session.

- **MemScanner fixes:**
  - A pattern miss is logged and returns null instead of exiting the game;
  - Malformed patterns no longer throw;
  - DIAGNOSTIC_MODE logs the address, match count and original bytes, and for a pattern that matches more than once, the bytes around each match;
  - A new search: only the game's executable sections are searched, and only memory that can be read, so a page the game protects is skipped instead of crashing it. It compares a pattern's two rarest bytes at 16 positions at a time and checks the whole pattern only where both match - about ten times faster than the original search, which went through the whole image almost byte by byte;
  - A second search, built the same way, finds every `lea` that loads one address - for a static list the game loads in places no pattern can tell apart (the box streamer's);
  - A pattern can be asked for every match instead of the first, for a site the game repeats - the check seven decal-placing functions share (the decal definitions part).

- **One patch module:** every patch to the game's code goes through `TweaksVPatch`, one writer for bytes and one for redirected calls (a hook on a function's start - RageOpenV's file and archive functions, DirectStorage's - is MinHook's, see T-HARDEN):
  - A group of writes is all or nothing: every page is made writable first, so a page that cannot be changed leaves the game as it was;
  - Every page gets its own protection back, and the processor sees the new code;
  - A redirected call goes through a 16-byte jump near the game, which is written once and then only read and executed - never writable and executable at once (the originals' trampolines were both);
  - DRY_RUN and DIAGNOSTIC_MODE work the same for every part.

- **One pattern table:** every byte pattern TweaksV looks for is defined once, in `TweaksVPatterns.h`, under the label `PATTERNS.md` gives it:
  - A label the table does not have, or a malformed pattern, stops the build;
  - The tests check that `PATTERNS.md` lists exactly the table's patterns, and that the code uses each one once;
  - `TweaksV.log` names each pattern by its label, and `TEST_BREAK_PATTERN` (test builds only) takes a label.
  - `GTA5_Enhanced.exe` is encrypted on disk, so patterns are looked for in the running game: test builds save a copy of its code with `TEST_DUMP_CODE` - its code, constants and function table, never its data.

- **One feature table:** `TweaksVFeatures.cpp` lists each feature once: its section of `TweaksV.ini`, the old stand-alone plugins that do its job and the function that starts it. DllMain starts the features from it and the conflict check takes their old plugins from it, so a new feature is one entry there, plus its settings.

- **HeapAdjusterEnhanced merged:** `[HEAP_SETTINGS] HEAP_SIZE` (default 2000 MB) is read from `TweaksV.ini`, and its original code is kept in its own namespace. Fixes:
  - A missing pattern no longer crashes the game;
  - The target instruction is checked before writing;
  - `HEAP_SIZE` is clamped to the game's own size..2047 MB, with no integer overflow;
  - It supports DRY_RUN and DIAGNOSTIC_MODE;
  - It warns if another heap adjuster already ran.

- **StraightToStoryMode merged:** one `TweaksV.ini` switch per screen (`SKIP_SPLASH_SCREEN`, `SKIP_LEGAL_SCREEN`, `SKIP_LANDING_PAGE`). Fixes:
  - A missing pattern skips only that screen instead of quitting the game;
  - The bytes are checked before writing (the splash and legal screens must still hold the game's own `jz`), and already-patched screens are skipped quietly;
  - The splash jump is computed from the game's own instruction;
  - A landing-page function hooked by another mod is left alone;
  - It supports DRY_RUN and DIAGNOSTIC_MODE.

- **ModkitLimitAdjusterEnhanced merged:** `[MODKIT_SETTINGS] MODKITARRAY_SIZE` (default 65536) is read from `TweaksV.ini`. Fixes:
  - Every reference and limit is found and checked before anything is written;
  - All references must point to the game's own array, so it is not moved twice;
  - All limits must agree on the game's own size;
  - The new array always has the full 65536 entries, as read/write data;
  - The limits are clamped to the game's own size..65536;
  - Each page of game code gets its original protection back, even when two sites share a page;
  - A failure no longer quits the game;
  - It supports DRY_RUN and DIAGNOSTIC_MODE.

- **PackfileLimitAdjusterEnhanced merged:** `[PACKFILE_SETTINGS] PACKFILEARRAY_SIZE` (default 13000) is read from `TweaksV.ini`. Fixes:
  - The array is moved before the limits are raised, so a failure at any step leaves the game safe;
  - Every reference, both size limits and the hooked call are found and checked before anything is written;
  - All references must point to the game's own array, so it is not moved twice;
  - The pattern for the reference that builds 1.0.1158.13 and later use now includes its two constants, so it no longer also matches obfuscated game code;
  - A call that another mod has already hooked is left alone;
  - The hook is written with the right page protection, and every page gets its original protection back;
  - The new array always has the full 65535 entries, as read/write data;
  - The limits are clamped to the game's own size..65535;
  - A failure no longer quits the game;
  - The original's link error (a variable defined in a header) is gone;
  - It supports DRY_RUN and DIAGNOSTIC_MODE.

- **BoxStreamer (TweaksV's own part):** `[BOXSTREAMER_SETTINGS] TRAVERSAL_LIST_SIZE` (default 10000) is read from `TweaksV.ini`. The game's box streamer keeps the map boxes one search returns in a static list of 1000 entries; with heavy map mods it fills up, and the boxes it has no room for are never streamed, so their LODs are missing. TweaksV moves the list to a larger one and raises the capacity the game hands its tree searches:
  - The idea is the CitizenFX Collective's fix for FiveM, as 0x-FADED ported it to GTA V Legacy. TweaksV's code is its own, written from the game's code alone: none of theirs was read;
  - The four capacity sites are found by pattern and must agree on the game's own size. The list's address comes from the `lea` after three of them, and the game's code is scanned for every `lea` that loads it: exactly 8 must be found (two of the game's four query functions are near-identical copies, which no pattern tells apart), so a list another mod has moved, or a game that has changed, is left alone;
  - The new list always has the full 100000 entries, as read/write data; the capacity is clamped to the game's own size..100000;
  - The 8 references and the 4 capacities are written together, all or nothing;
  - `LOG_SEARCHES=1` (testing) sends the four calls to the game's tree searches through TweaksV's call stubs to a wrapper that only counts (atomics, nothing else on the streaming thread) and logs, 10 seconds in and then every minute while the numbers change, how many searches ran, the most entries one returned and how many returned more than the game's own list could hold - the number that says whether a map load needs the larger list. A call another mod has hooked, or a pair of calls that does not lead to the same tree search, skips the part; the calls are redirected in the same all-or-nothing write as the references and capacities;
  - Measured on 1.0.1158.16 with Forests of San Andreas: Revised and GTA V Remastered: Enhanced loaded (`LOG_SEARCHES=1`): 180353 searches in three minutes, the largest returned 1123 entries, and 7209 returned more than the game's own list of 1000 could hold - each of those would have been cut off without the larger list;
  - A failure is reported in the start-up summary and leaves the game as it was;
  - It supports DRY_RUN and DIAGNOSTIC_MODE.

- **DecalDefs (TweaksV's own part):** `[DECALDEFS_SETTINGS] DECALDEFS_SIZE` (default 2048) is read from `TweaksV.ini`. The game keeps its decal definitions - the kinds of decal it reads from the `decals.dat` files - in a static array of 260 entries, with the number registered so far in a dword 16 bytes before it; once 260 are registered the parser drops every further definition silently, and add-on content, which comes last, loses its decals. TweaksV moves the array to a larger one and raises the limit at every site that carries it:
  - The idea is the CitizenFX Collective's fix for FiveM, as 0x-FADED ported it to GTA V Legacy. TweaksV's code is its own, written from TweaksV's own findings about the game's code; the published patches were known as the idea and as a guide to what to look for, and no line of their code was read for the build;
  - The parser's "full" check gives the count's address and N (260), its `lea` the array's address, which must lie 16 bytes after the count, both inside the game. Every other site that carries the limit - 21 instructions, one pattern matching the 7 identical checks of the decal-placing functions - is found by pattern, must be the expected instruction and must hold N-1 or N; the decal manager's start-up loop must clear a whole number of entries per pass and end at its start offset + N x 80. The game's code is then scanned for every `lea` that loads the array: exactly 31 must be found, each with a 48 or 4C prefix, so an array another mod has moved, or a game that has changed, is left alone;
  - The new array always has the full 16384 entries (1.3 MB), as read/write data, zeroed - the array's initial state; the limit is clamped to the game's own size..16384 (a definition's index is a uint16 in the game's decal instances);
  - The 31 references and the 23 constants are written together, all or nothing; the loop end becomes a whole number of passes below the new size (the entries it leaves are zero anyway); the count stays where it is, since nothing reaches it through the array or the array through it;
  - `LOG_DECALDEFS=1` (testing) reads the count 10 seconds in and then every minute while it changes, and logs it against the limit and the game's own 260, with how many the game's own array would have dropped - nothing is hooked; with `DRY_RUN=1` it measures the unpatched game;
  - `DecalPatch.asi` counts as an old plugin for this part and the box streamer part: with it installed, both are switched off;
  - A failure is reported in the start-up summary and leaves the game as it was;
  - It supports DRY_RUN and DIAGNOSTIC_MODE.

- **RageOpenV:** the mods folder, unencrypted (OPEN) RPF archives, the RPF cache check skip and the experimental newmods folder each have a switch in `[RAGEOPENV_SETTINGS]`, read from `TweaksV.ini` (no more `RageOpenV.ini`). Fixes:
  - Every hook group finds and checks all of its addresses first, and installs all of its hooks or none; a missing pattern no longer crashes the game;
  - Function starts worked out from a pattern are checked, and calls that another mod has already redirected are left alone;
  - Both DecryptHeader2 calls must lead to the same function;
  - The archive encryption tag is kept per thread, so archives opened at the same time no longer mix up;
  - Page protection is restored after every write, and the redirected calls go through stubs that are never writable and executable at once;
  - The console patch is only applied when the console is on (`CONSOLE=1`);
  - Newmods: files are closed properly, folder listing works, file sizes are right, reading at the end of a file no longer hangs, and nothing is mounted without a newmods folder (DIAGNOSTIC_MODE still checks its patterns);
  - Newmods: a path that leads out of the `newmods` folder (a rooted one, or one with `..`) matches no file; paths are built at their real length instead of in fixed 256-character buffers, where a long one stopped the game; a name is copied without reading past its end;
  - Its log switches (`LOG_INFO`, `LOG_MODS`, `LOG_DEVICE`) work at runtime, and cost nothing when off;
  - The mods folder is read once at start-up, and the game's file calls look their paths up in that list: no file system call, memory allocation or lock per call. Without a mods folder, the game's file functions are not hooked at all (DIAGNOSTIC_MODE still checks their patterns). A path with `..` is never redirected, and the list holds at most 200,000 files and folders, 32 levels deep (more is reported in the start-up summary);
  - The `GetSystemTimeAsFileTime` hook switches itself off once its one-time start-up has run;
  - The OPEN-archive hook no longer walks the entry table of encrypted archives, which it never changes;
  - It supports DRY_RUN and DIAGNOSTIC_MODE.

- **DirectStorage (TweaksV's own part):** `[DIRECTSTORAGE_SETTINGS]` in `TweaksV.ini` (`ENABLED`, `UNLOCK_FILES`, `LOG_CALLS`, `LOG_FILE_OPENS`):
  - `UNLOCK_FILES=1` (on by default) lets DirectStorage open only the `.rpf` and `.cache` files in the game folder. The game asks it to open every file in its folder at start-up and keeps them locked; the folder's other files are now refused as "not found", so they can be written, renamed and deleted while the game runs;
  - Files outside the game folder are never refused: the game reads its saves and `pc_settings.bin` (`Documents\Rockstar Games\GTAV Enhanced\Profiles`) and its cloud files in `%TEMP%` through DirectStorage, too;
  - It hooks the factory's `OpenFile` while the game creates the factory, before its first file, so it needs no game patterns. A summary 10 seconds later counts the files opened and refused;
  - `LOG_CALLS` (testing) logs the game's calls into `dstorage.dll` and every `OpenFile`; with DIAGNOSTIC_MODE also the factory's functions and the configuration values;
  - `LOG_FILE_OPENS` (testing) logs every open of a file or folder inside the game folder, with the access, sharing, result and caller, plus a summary every minute;
  - alloc8or's `DirectStorageFix.asi` counts as an old plugin: with it installed, this part is switched off;
  - It supports DRY_RUN and DIAGNOSTIC_MODE;
  - Confirmed on 1.0.1158.16: of the 454 files the game asked DirectStorage for at start-up, the 180 `.rpf` and `.cache` files were opened and the other 274 refused (among them `TweaksV.asi`, `ScriptHookV.dll`, `xinput1_4.dll` and the text and INI files). The save loaded, the game ran normally, and a text file in the game folder could be renamed while playing.

- **Hook:** `TweaksV.asi` stays loaded once enabled until the game exits.

- **Build:** all of TweaksV's code, its own and the merged plugins', builds at the strictest warning level (`/W4`) without a warning, and the code analysis finds nothing. MinHook, compiled unchanged, builds at level 3 (`/W3`) without a warning; at `/W4` its own code warns, and that code is not TweaksV's to change.
  - `TweaksV.asi` carries Windows' exploit mitigations: ASLR with high entropy, DEP, Control Flow Guard, and it is marked compatible with CET, the processor's shadow stack. The tests check all four. Control Flow Guard and CET only take effect in a game that turns them on itself, which GTA V Enhanced 1.0.1158.16 does not; TweaksV carries them so that it never leaves a gap in a game that does, nor is refused by one.

- **Releasing:** a release zip carries `TweaksV.asi`, `TweaksV.pdb` (it reads crash dumps), `LICENSE` and `THIRD_PARTY_NOTICES.md`; the release notes name the tag the build came from; the mod page carries the credits and adds no restrictions of its own. Check the zip on VirusTotal before uploading: a plain build with its version resource and no packer stays clean.
  - Provenance: RageOpenV's README asks that its binaries not be redistributed - a request, not a license term (the GPL-3.0 allows redistribution and forbids further restrictions), so the credits stay prominent; martonp96's original ClosedIV repository is no longer public, and RageOpenV's README is the source of that credit.

- **Tests:** `test\` holds fake-game test suites (418 checks): small programs that pretend to be the game, with copies of the code TweaksV patches, load the real `TweaksV.asi` and report what it changed. One more suite checks the pattern table against `PATTERNS.md`. `bash test/run_all.sh` builds and runs them (see README).

## Open Issues

Known bugs and open issues in TweaksV.

### B-ROV-18: newmods may seek from the end of a file in the wrong direction

- **Where:** `fiDevice.cpp`, `rage::fiDeviceLocal::Seek64`. Only RageOpenV's experimental newmods folder is affected (`NEWMODS=1`; it is off by default).
- **What:** for `SEEK_END`, the original RageOpenV code subtracts the distance, while Win32's `FILE_END` adds it, so the original's sign may be wrong.
- **Status:** unconfirmed. The original behaviour is kept until it is known how the game calls this. If the game ever seeks from the end of a file with a non-zero distance, `TweaksV.log` says so once, in a line that names B-ROV-18. Nothing needs to be done unless that line appears.
- **Tested:** one boot on 1.0.1158.16 with `NEWMODS=1`, `LOG_DEVICE=1` and an empty `newmods` folder. The newmods devices were mounted without a problem, but the game opened no file through them, so this code never ran. Confirming the sign needs a boot in which the game loads a file from the `newmods` folder.
- **Workaround:** `NEWMODS=0` (the default) under `[RAGEOPENV_SETTINGS]` in `TweaksV.ini` turns newmods off.

### T-HARDEN: MinHook's trampolines are writable and executable

- **What:** MinHook, the hook library, keeps the trampolines of TweaksV's hooks in memory that is writable and executable at once. That only helps an attack that can already write to the game's memory. The rest of the code review's hardening is done: links, the user name and the size limit of `TweaksV.log`, newmods' paths, problems after the start-up summary, the exploit mitigations.
- **Status:** accepted. It is MinHook's own design, and `vendor\MinHook` stays unchanged; TweaksV's own stubs are never writable and executable at once.
- **When it goes:** when MinHook keeps its trampolines read and execute only, or TweaksV moves to a hook library that does.

## Bugs Found in the Originals

What the merge found in the five original projects, with the file and line in the originals (the archives in `upstream\`) and what TweaksV does instead - the list to work from if they are ever reported upstream. The IDs are the ones the open issues use (B-ROV-18). Severity: crash = could crash or close the game, corrupt = could silently corrupt memory or behaviour, robust = fragile or unsafe practice, hygiene = cleanliness; "latent" = only reached in some situations, such as a pattern miss after a game update. Two entries concerned the Legacy code, which TweaksV does not carry.

### HeapAdjusterEnhanced (`Heap Adjuster\Game.cpp`)

| ID | Severity | Where and what | TweaksV |
|---|---|---|---|
| B-HEAP-01 | crash, latent | `initializeGame`, lines 38 and 45: the pattern's success flag is never checked; after a miss `loc` is 0x1 and line 45 writes through it | a miss is reported and the part skipped |
| B-HEAP-02 | corrupt | line 51: `heapSize * 1024 * 1024` overflows `int` from `HEAP_SIZE=2048` up | 64-bit math; clamped to the game's own size..2047 |
| B-HEAP-03 | robust | line 11: the INI value is written as it is, 0 and negative included | clamped and logged |
| B-HEAP-04 | robust | nothing checks that the target is the imm32 of `mov edx, imm32` | the byte before it must be `BA` |
| B-HEAP-05 | robust | line 11: the INI is read by a global initialiser during CRT start-up, through a relative path | read in `initializeGame` from the settings table |
| B-HEAP-06 | hygiene | a C++/CLI project for native code; the .sln points at a missing path; `app.rc` and `app.ico` are not built; an orphan .filters file; a Win32 post-build copy | not carried over: one native project |

### ModkitLimitAdjusterEnhanced (`Game.cpp`, `MemScanner.cpp`)

| ID | Severity | Where and what | TweaksV |
|---|---|---|---|
| B-MOD-01 | corrupt, latent | `RelocateRelativeModkitArray`, line 104: `assert(curTarget == oldAddress)` is the only consistency check, and Release builds leave it out | a real check, and the array must lie inside the game |
| B-MOD-02 | crash, latent | `initializeGame`, line 29: `AllocateStubMemory`'s result is not checked | checked; the part is skipped |
| B-MOD-03 | robust | line 234: the data array is allocated read/write/execute | read/write |
| B-MOD-04 | corrupt | line 18: `MODKITARRAY_SIZE` is not clamped - a value below the game's own shrinks the array | the array always has the full size; the limits are clamped to the game's own size..65536 |
| B-MOD-05 | crash, latent | `FatalErrorExit`, lines 242-252, and `MemScanner.cpp` lines 140-143: a miss shows a message box and calls `exit(1)` inside DllMain, after earlier patterns were already written | two passes: nothing is written before everything is found; a failure is reported and the part skipped |
| B-MOD-06 | robust | `MemScanner.cpp` line 58 relies on windows.h's `max()` macro; line 90: `std::stoi` throws on a malformed token, inside DllMain; the scanner calls the plugin's `FatalErrorExit` | `std::max`; a malformed pattern is caught; a miss returns null |

### PackfileLimitAdjusterEnhanced (`Game.cpp`, `Hooking.h`, `Logging.h`)

| ID | Severity | Where and what | TweaksV |
|---|---|---|---|
| B-PACK-01 | crash, latent | `Hooking.h` lines 50-54, `HookManager::SetCall`: writes `E8` and the rel32 into game code without `VirtualProtect` | `TweaksV::RedirectCalls`: protection changed and restored, the cache flushed, `E8` verified first |
| B-PACK-02 | robust | `EnhancedPatch`, line 120: the second `VirtualProtect` passes `PAGE_EXECUTE_READWRITE` instead of the old protection, leaving the page writable and executable | every page gets its own protection back |
| B-PACK-03 | corrupt, latent | `RelocateRelativePackFileArray`, line 178: an assert-only consistency check, as B-MOD-01 | a real check |
| B-PACK-04 | crash, latent | `initializeGame`, lines 51-53: the allocation is not checked before the `memset` | checked |
| B-PACK-05 | corrupt, latent | lines 55-64 and 325-335: the limits are raised before the array is moved, so a failed move leaves raised limits on the small array; `FatalErrorExit` exits inside DllMain | the array is moved first, always at full size; two passes; a failure is reported, never fatal |
| B-PACK-06 | corrupt, latent | line 182: every reference is pointed at the new array's start without checking that it pointed at the old array's start | every reference must point at the same array, inside the game |
| B-PACK-07 | robust | line 40: no clamp; the code's default 12000 differs from the released INI's 13000 | clamped; the default is 13000 |
| B-PACK-08 | build break | `Logging.h` line 5 defines a non-inline global in a header every .cpp includes: linking fails with LNK2005 "LOGFILE already defined" and LNK1169 (MSVC 14.51), so the published project does not build; `Game.h` declares static functions in a header | the global is gone; the static functions are declared in `Game.cpp` |

### StraightToStoryMode (`Game.cpp`)

| ID | Severity | Where and what | TweaksV |
|---|---|---|---|
| B-STORY-01 | crash, latent | `initializeGame`, lines 36-37 and 81: no null checks (the author's TODO on line 14); a miss ends in `exit(1)` or pointer arithmetic on null | a miss skips only that screen |
| B-STORY-02 | corrupt, latent | line 44: the splash jump is hard-coded as `E9 86 01 00 00`; an update that moves the jump's target but keeps the pattern lands it in the wrong place | `0F 84` verified, the `jmp` computed from the game's own `jz` |
| B-STORY-03 | robust | no site verifies the original bytes | verified; bytes already patched are skipped quietly |
| B-STORY-04 | robust | no configuration | `SKIP_SPLASH_SCREEN`, `SKIP_LEGAL_SCREEN`, `SKIP_LANDING_PAGE` |
| B-STORY-05 | - | concerned the Legacy code, which TweaksV does not carry | - |

### RageOpenV (`src\`)

| ID | Severity | Where and what | TweaksV |
|---|---|---|---|
| B-ROV-01 | crash, latent | the init lambdas in `hooks\*.cpp` and `gtav\rage\fiDevice.cpp` lines 6 and 16: scan results are used unchecked - `.add(n).set_call()`, `.rip()` and `.hook()` on address 0 | every group finds and checks all of its addresses first; a miss skips the group |
| B-ROV-02 | robust | `utils\memory.h` line 325, `scoped_unlock`: `VirtualProtect` with a null old-protection pointer fails, so the protection is never restored | restored after every write |
| B-ROV-03 | - | concerned the Legacy console patch, which TweaksV does not carry | - |
| B-ROV-04 | hygiene | `main.cpp` lines 11-16: the console patch is applied with the console off | only with `CONSOLE=1` |
| B-ROV-05 | corrupt, latent | `hooks\PackfileEncryptionHooks.cpp` lines 73-77: the second DecryptHeader2 call is redirected without checking that it calls the same function as the first | both must lead to the same function |
| B-ROV-06 | corrupt | `PackfileEncryptionHooks.cpp` line 5: `currentEncryption` is one global, written by one hook and read by the later ones; archives opened on different threads mix up their tags | `thread_local`; `ParseHeaderHook` checks the entry table and the file count |
| B-ROV-07 | crash | `utils\config.h` with `inicpp.h`: inicpp throws on a line without `=` (a `;` comment), on section errors and on a missing or non-boolean key - reached from DllMain and from every log write | the settings come from `TweaksV.ini` through the Win32 INI functions |
| B-ROV-08 | crash, latent | `utils\memory.h` line 65, `memory::init`: the trampoline area is requested at the fixed address exe+0x20000000 without a failure check | memory near the game from `AllocateStubMemory`, checked |
| B-ROV-09 | robust | `memory.h` lines 153-159, `memory::hook`: `MH_EnableHook(MH_ALL_HOOKS)` after every hook, freezing all threads each time | hooks are queued, one apply per group |
| B-ROV-10 | robust | `main.cpp` lines 3-10: the one-time guard is a plain `bool` | an atomic, claimed with `exchange` before the start-up runs; the hook switches itself off afterwards |
| B-ROV-11 | robust | `utils\log.cpp` lines 16, 33 and 35: a 256-byte line buffer truncates paths; `printf` on every line even without a console; INI lookups and a file open per line | lines go to `TweaksV.log`; the switches are read once |
| B-ROV-12 | robust | `fiDevice.cpp`: a "device" log line is formatted on every device call even when device logging is off | the switch is checked first |
| B-ROV-13 | corrupt | `fiDevice.cpp` lines 51 and 92-98, `Open` and `Close`: `Open` allocates an `ifstream` that `Close` never deletes - memory and a file handle leak per file | deleted in `Close` |
| B-ROV-14 | corrupt | `fiDevice.cpp` line 46: the `handleNames` map is used from several game threads without a lock, and `operator[]` in log calls inserts entries | a shared mutex; log calls insert nothing |
| B-ROV-15 | robust | `fiDevice.cpp` line 156, `FindFileBegin`: `wprintf_s(fileName, 256, ...)` prints to stdout with the name as the format string instead of formatting into it, so the search always fails | formatted into a buffer of the real length |
| B-ROV-16 | corrupt | `fiDevice.cpp` line 402, `ConvertFoundData`: `*(uint64*)&nFileSizeHigh` swaps the halves of the file size | `(high << 32) \| low` |
| B-ROV-17 | hang | `fiDevice.cpp` lines 232-240, `SafeRead`: `bytesRead` is unsigned, so `bytesRead < 0` is never true and the loop never ends at the end of a file | stops when 0 bytes are read |
| B-ROV-18 | robust | `fiDevice.cpp` line 86, `Seek64`: `SEEK_END` subtracts the distance; Win32 adds it | the open issue B-ROV-18 above |
| B-ROV-19 | corrupt | `fiDevice.cpp` lines 56-70, `ReadFile` and `ReadBulk`: after a short read the stream's failbit is set and every later `seekg` and `read` silently fails | the stream is cleared before seeking |
| B-ROV-20 | hygiene | dead code: `memory::HookIAT`, `memory::ret`, `memory::make_jmp_ret`, `memory::call`, `rage::fiDeviceRelative`, the "rpf" log key; lazy scans inside game calls | removed; `Mount` and `SetPath` are resolved at start-up |
| B-ROV-21 | robust | paths relative to the current directory (`CustomDevice.cpp` line 36, `fiDevice.h` line 138, `FileReadHooks.cpp` line 22); `ToFullPath` ignores each device's `SetPath`; DllMain writes `RageOpenV.ini` and uses `std::filesystem` calls that can throw | absolute paths, logged at start-up; `TweaksV.ini`; non-throwing calls |
