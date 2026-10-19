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
  - A second search, built the same way, finds every `lea` that loads one address - for a static list the game loads in places no pattern can tell apart (the box streamer's).

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

- **Tests:** `test\` holds fake-game test suites (363 checks): small programs that pretend to be the game, with copies of the code TweaksV patches, load the real `TweaksV.asi` and report what it changed. One more suite checks the pattern table against `PATTERNS.md`. `bash test/run_all.sh` builds and runs them (see README).

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
