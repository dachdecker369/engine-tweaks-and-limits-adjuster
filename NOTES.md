# Notes

- **`TweaksV.ini`:** created on the first start with its default settings. Missing keys are added back; existing values are never changed.
- **`TweaksV.log`:** one log with a header (versions, paths, switches).
- **Read-only game folders:** both files go to `%LOCALAPPDATA%\TweaksV` when the game folder is read-only, for example a Rockstar Games Launcher install under `C:\Program Files`.
  - A `TweaksV.ini` in the game folder always wins.
  - A read-only folder is normal, so it is only logged; it shows no message box.

- **Conflicts:** old stand-alone plugins (HeapAdjuster.asi, OpenRPF.asi, ...) are detected. Their TweaksV part is switched off for the session and listed in a start-up summary.

- **MemScanner fixes:**
  - A pattern miss is logged and returns null instead of exiting the game;
  - Malformed patterns no longer throw;
  - DIAGNOSTIC_MODE logs the address, match count and original bytes, and for a pattern that matches more than once, the bytes around each match;
  - A new search: only the game's executable sections are searched, and only memory that can be read, so a page the game protects is skipped instead of crashing it. It compares a pattern's two rarest bytes at 16 positions at a time and checks the whole pattern only where both match - about ten times faster than the original search, which went through the whole image almost byte by byte.

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

- **RageOpenV:** the mods folder, unencrypted (OPEN) RPF archives, the RPF cache check skip and the experimental newmods folder each have a switch in `[RAGEOPENV_SETTINGS]`, read from `TweaksV.ini` (no more `RageOpenV.ini`). Fixes:
  - Every hook group finds and checks all of its addresses first, and installs all of its hooks or none; a missing pattern no longer crashes the game;
  - Function starts worked out from a pattern are checked, and calls that another mod has already redirected are left alone;
  - Both DecryptHeader2 calls must lead to the same function;
  - The archive encryption tag is kept per thread, so archives opened at the same time no longer mix up;
  - Page protection is restored after every write, and the trampolines come from checked memory near the game;
  - The console patch is only applied when the console is on (`CONSOLE=1`);
  - Newmods: files are closed properly, folder listing works, file sizes are right, reading at the end of a file no longer hangs, and nothing is mounted without a newmods folder (DIAGNOSTIC_MODE still checks its patterns);
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

- **Tests:** `test\` holds fake-game test suites (260 checks): small programs that pretend to be the game, with copies of the code TweaksV patches, load the real `TweaksV.asi` and report what it changed. `bash test/run_all.sh` builds and runs them (see README).

## Open Issues

Known bugs and open issues in TweaksV.

### B-ROV-18: newmods may seek from the end of a file in the wrong direction

- **Where:** `fiDevice.cpp`, `rage::fiDeviceLocal::Seek64`. Only RageOpenV's experimental newmods folder is affected (`NEWMODS=1`; it is off by default).
- **What:** for `SEEK_END`, the original RageOpenV code subtracts the distance, while Win32's `FILE_END` adds it, so the original's sign may be wrong.
- **Status:** unconfirmed. The original behaviour is kept until it is known how the game calls this. If the game ever seeks from the end of a file with a non-zero distance, `TweaksV.log` says so once, in a line that names B-ROV-18. Nothing needs to be done unless that line appears.
- **Tested:** one boot on 1.0.1158.16 with `NEWMODS=1`, `LOG_DEVICE=1` and an empty `newmods` folder. The newmods devices were mounted without a problem, but the game opened no file through them, so this code never ran. Confirming the sign needs a boot in which the game loads a file from the `newmods` folder.
- **Workaround:** `NEWMODS=0` (the default) under `[RAGEOPENV_SETTINGS]` in `TweaksV.ini` turns newmods off.

### T-DEBT: shared code still written more than once

- **What:**
  - Each setting and its default is written in several places (the key list, the default INI text, the readers, the log header).
  - There are five copies of the code that makes game code writable and restores it, and two of the code that redirects a call.
  - The patterns are text in nine files.
  - About 240 comments in the source, and some in the tests, still cite the sections and bug IDs of the original merge plan, which is not part of the repository.
  - Every test suite has its own copy of the test helpers.
  - The scanner keeps the original's name, `FindPatternBmh`.
- **When it goes:** in the refactoring that follows the faster scanner (one settings table, one patch module, one pattern table, the comments), before the next release.
