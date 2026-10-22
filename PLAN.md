# Plan

The order of work for a change to TweaksV, phase by phase, each phase ending with the test suites green, the three builds clean, a local commit and my boot. It carried the decals part of 1.6.0 and then the whole rewrite of 2.0.0 (both in the git history). Now it carries the fixes of the 2.0.0 audit, which found no critical or high issue and a ranked list of medium and low ones; the numbers in the phases are the audit's.

## The rules of this change

- Only the findings below change, as their phase says: no refactor beyond what a finding asks, no renamed files, no version bump, nothing in `vendor\`, and no new key in `TweaksV.ini` (a new key is a new layout number); a test-build key only where a phase names one.
- A finding that turns out wrong, or needs a game fact I have not given, is skipped and recorded under "Next steps". The game facts are PATTERNS.md's and those of my dump of 1.0.1158.16; nothing is guessed.
- Every changed behaviour gets a scenario in its suite, and the four documents change in the same commit as the code.
- Each phase ends with `bash test/run_all.sh` green, Release, the test build and the analysis build rebuilt from scratch without a warning, and one local commit. A phase that cannot be made green within its scope is reverted and recorded here. I boot once, after the last phase.

## My decisions

1. The INI layout rule stays: a new key in the file means a new layout number, and an older `VERSION` is not carried forward. Documented only (phase 6).
2. The packfile hook keeps writing exactly the new size over the configured archive count, also when that lowers it. Documented only (phase 5).
3. The game facts this change needs come from my dump of 1.0.1158.16 and go into PATTERNS.md in the phase that uses them.
4. The box streamer keeps refusing its job under `DIAGNOSTIC_MODE` when a tree-search call cannot be redirected; the documents name it as the one exception to "the switches work the same for every part" (phase 5).

## Phase 0: the plan

This file and the state in my local notes. Commit: `PLAN: the phases of the 2.0.0 audit fixes`.

## Phase 1: the harness (findings 10, 11, 36-39)

Only `test\`, SPECIFICATIONS 14 and the check count in NOTES.md change.

- `common.sh`: one runner, `run_fake`, used by `game` and in place of the copies of the run line in `run_ini_default.sh`, `run_integration.sh` and `run_rageopenv.sh`, keeps the fake's exit status and whether it printed its report; `check` fails every check of a run that timed out or printed no report (`TIMEOUT`, `NO REPORT`). `deny_writes DIR` and `allow_writes DIR` for the two icacls blocks, and an optional file for `setkey`.
- R16 can fail without the pin: `full_game.exe unload` frees `TweaksV.asi` after the start-up, calls a hooked file function and an OPEN call, and reports whether the module is still loaded.
- The checks that passed too easily: positive checks in S4 and R7; `log_lacks`, which fails without a `TweaksV.log`, for every negated log check; the parts' full switch lines; all six console bytes in R2; `breaktest` checks the label it broke and the part's reason; report strings that end in a number end in ` |`; P13 to P16 check the file version; the 255-character limit of an INI value made observable; the dump's section table read for `.data`; the junction scenario stops when no junction was made; the 8.3 check skipped as `SKIP`.
- New scenarios: `[HEAP_ADJUSTER] ENABLED=0` and the heap's DIAGNOSTIC line (story), an empty mods folder (rageopenv); R4b's name outside the Basic Multilingual Plane, which only UTF-8 holds.
- `run_all.sh` prints why a suite stopped, compares the `all` count with NOTES.md's and fails on a build warning; the fakes build with `/WX`; the `fileopens` option keeps its temp file under `object\TestRun`; `base.ini` gets its SPDX line.

## Phase 2: the scanner, the relocation core and the patterns (findings 2, 3, 23, 32)

- A pattern that matches twice is refused by `Scanner::Find` (`Pattern <label> matches 2 or more times - not used (the site must be unique).`); the parts keep their `<label> was not found.` Scenarios with a second S-E3 and a second R-E-K1.
- The arrays moved by their anchors are cross-checked by a scan: `Scanner::FindRipRelativeReferences` finds every `lea r64, [rip+disp32]` and `mov [rip+disp32], r64` that leads to the array, and the modkit (6) and packfile (33) parts refuse their job unless the scan finds exactly the anchors' references. The box streamer and decals keep their lea scan.
- The two packfile references of my dump: `P-E-R32` (the live load in the function that hashes a collection's entries) and `P-E-R33` (in the dead bytes after a trap); every 31 becomes 33.
- Opcode checks for M-E-L2 (`cmp rdi, imm32`, `48 81 FF`), P-E-R09 and P-E-R17 (`lea rcx`, `48 8D 0D`); P-E-R31b stays as it is.
- Three stale comments (`TweaksVPatch.h`, `TweaksV.vcxproj`, `TweaksVPatterns.h`).

## Phase 3: RageOpenV (findings 4-7, 16-19, 21, 22)

- The mods folder: the depth limit skips only the deep folder; folders that cannot be read are counted and reported, an unreadable root is no longer called empty; a game that runs in another folder than the game folder is reported.
- OpenBulk's fall-back is logged after the fact and reported once as a late problem; the three path functions stay as they are.
- OPEN archives: an archive that cannot be prepared is reported once and its line capped; `ParseHeaderHook` consumes the encryption tag (insurance: my dump shows every successful ParseHeader passes FindEncryption).
- The cache check's function already hooked by another mod is left alone; "hooked on top and chains through" is documented as holding for the four computed starts only.
- A fallback thread reports a start-up that never ran because the game never asked for the time.
- The small ones: a console call already NOPed, the left-out line's two reasons, the last error kept, the game's path converted for the log, rooted and drive paths never redirected, the mods list never destroyed. The double-byte code pages are left: they cannot be tested here.

## Phase 4: DirectStorage (findings 8, 9, 26-30)

- A second interface is no longer a false problem; one asked for alone is reported 10 seconds later if OpenFile is still not hooked.
- The containment test runs on the normalised path, without heap memory.
- Mutually exclusive counts; the late refusal line and the `OpenFile #N` lines stop after 20,000; paths outside the game folder are logged as a category and a file name; the summary lists the refused files by extension; the diagnostic tables take no new keys after 100,000.
- A summary thread that cannot start is logged (the UNLOCK_FILES summary's is reported); the OpenFile hook's flag is set after the attempt; a one-shot line when the game never asks for its factory.
- Debt: one diagnostic flag, one last-error guard, one body for the two configuration hooks.

## Phase 5: the parts (findings 13, 14, 21, 24, 26, 27, 31)

- A heap of 2048 MB or more is another adjuster's too; the second clamp, to the game's own size, is reported.
- Documented: the packfile hook replaces the configured count exactly (decision 2), the box streamer's refusal under `DIAGNOSTIC_MODE` (decision 4).
- The box streamer's summary says when the list has the game's own size; `kSearchHeadroom` goes into PATTERNS.md.
- A measurement thread that cannot start is logged, not reported; StraightToStoryMode logs its sites with `DIAGNOSTIC_MODE`.
- Debt: one measurement helper (`TweaksVMeasure`), `FileExists` and `Capitalised` in one place, the loader folders defined once, `version.lib` in the project only.

## Phase 6: the documents (findings 1, 10, 11, 33, 35 and the rest of 32)

- `TweaksVSettings.h` says the layout rule I kept; the run-time pattern parser no longer takes `??`.
- T-LAYOUT as an open issue in NOTES.md; the 1.x section renames named one by one; CREDITS.md's newmods history; the DIAGNOSTIC prefix and the other wording in SPECIFICATIONS, README, SECURITY, NOTES, PATTERNS and THIRD_PARTY_NOTICES.

## Phase 7: close

This file back to its heading, its first paragraph and "Next steps" with what was skipped; my local notes with the state. Then my boot (`DIAGNOSTIC_MODE=1`, because patterns changed, with my map mods), the version bump and the release.

## Next steps

- Phase 0 is committed; next is phase 1, the harness.
- Skipped so far: nothing.
- Waiting on me: nothing until the last phase is committed.
