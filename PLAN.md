# Plan

The order of work for a change to TweaksV, phase by phase, each phase ending with the test suites green, the four builds (Release, Debug, the test build, the analysis build) clean, a local commit and my boot. It carried the decals part of 1.6.0, the whole rewrite of 2.0.0, the fixes of the 2.0.0 audit, the 2.1.1 patch for T-UNLOCK and the 2.2.0 redesign of the DirectStorage part (all in the git history). One version remains planned: 2.2.1, the RageOpenV and StraightToStoryMode parts switching themselves off next to NaturalVision Enhanced's loader, from the same player's report as 2.2.0; [ROADMAP.md](ROADMAP.md) holds its design and the findings behind it.

## How the work runs

Each version is a track: one coding session in a worktree of its own (the app makes it under `.claude\worktrees\` with a `claude/...` branch), with a prompt from the overview session, kept in `.claude\prompts\` and never committed. A track owns its files and regions, so two tracks never touch the same region of a file; where a line is shared all the same - the check count in NOTES.md and in SPECIFICATIONS.md 14.1, the adjacent rows of 14.2, the RageOpenV row of 4.3 - the overview session resolves the merge. A track commits on its branch only, each item its own commit; it never merges, tags, pushes or sets the version; after every commit it writes its paragraph into the State block below and a two-line report into the chat, and it ends its turn only when the track is done or a decision of mine blocks everything left. The overview session rebases the branches onto main in order, makes the changelog lines and the check count one, sets the version, runs the suites from a clean rebuild, hands me the boot, then tags and pushes main and the tag together. Tracks 1 (2.1.1) and 2 (2.2.0) are released; track 3 ran in parallel with track 1 and is rebased onto main now.

## 2.2.1 - two parts switch themselves off next to NVE's loader (track 3)

Track 3 ran in parallel with track 1 and is released after 2.2.0. One phase:

1. `TweaksVFeatures.h`, `TweaksVFeatures.cpp`, `TweaksVConflicts.h`, `TweaksVConflicts.cpp`: a second kind of plugin per part, one that does the part's job and stays installed, with a name and a short description for the wording; `onigiri.asi` ("NaturalVision Enhanced's loader") for RageOpenV and StraightToStoryMode, not for DirectStorage (2.2.0 refuses nothing). Found in one of the loader folders while the part is on, it switches the part off for the session like an old plugin and is reported as an item of the start-up summary: `<path> is installed and does this job too (NaturalVision Enhanced's loader): TweaksV's RageOpenV part is switched off while it is there, so nothing is patched twice. To stop this message, set ENABLED=0 under [RAGEOPENV] in TweaksV.ini.` A part already off by its INI reports nothing; a log line says the file is there and the part is off already, so the log always shows what the check found. The old plugins keep their wording and behaviour; the "none found" line covers both kinds.
2. `test\run_integration.sh`: `onigiri.asi` alone in each of the three loader folders - the two parts off with the exact wording, the other six applied, DirectStorage on; together with the old plugins all at once; with the two sections off by their INI, no item. The check count moves.
3. The documents: README.md's "Other mods" (the NVE paragraph: the heap, and the loader with what TweaksV does next to it, where the mods then go, the landing page not skipped unless onigiri's INI does it, the INI switches that stop the message) and "Testing" step 2 (the eleventh file, which stays); NOTES.md's "Conflicts" bullet (the two kinds, and the facts behind this one: onigiri loads before TweaksV but patches at the game's first request for the time, after TweaksV's changes, and faults at its offsets 0x1ea45 with RageOpenV's sites changed and 0x1f2be with the story sites changed, version 1.0.1.0 - what a report to NVE's author needs); SPECIFICATIONS.md 4.3 (the table), 7 (the second wording), 14.2's integration row.
4. Found by the track, fixed with it: the log header's `Features:` line was written before the conflict check ran, so a part the check switched off said `on` there, against SPECIFICATIONS.md 6.2, since 2.0.0 - the check now runs before the header, its lines kept and written after it (6.1), pinned in the integration suite; SPECIFICATIONS.md 3 and 6.2 as built.

Then the release: version 2.2.1, a clean rebuild, the suites, my boot with NVE installed (the start-up summary names onigiri.asi and the two parts, the game runs, onigiri's own skips work) - then the tag.

## State

**State:** 2.2.0 is released: the tag on 88b5c54, the commit I booted twice, with NVE and without, pushed with main on 2026-10-25; this documents-only commit stands above it unpushed. 2.2.1 remains, its track done and waiting for its rebase. The suites stand at 496 checks; the open issues are T-HARDEN, T-LAYOUT, T-CODEPAGE, T-DIAG, T-SUMMARY and T-FREEZE.

**Track 1 (2.1.1, T-UNLOCK):** released. Its worktree `.claude\worktrees\tweaksv-2-1-1-t-unlock-bc7c30` and its branch stay until I remove them.

**Track 2 (2.2.0, the DirectStorage redesign):** released. My boots confirmed it: next to NVE's loader the game ran with the loose files shared, and with my mods folder the 280 files the game locks when the cache check is skipped were shared and one of them renamed while playing (NOTES.md's DirectStorage bullet has the figures). Its worktree `.claude\worktrees\directstorage-file-handling-8fa623` and its branch stay until I remove them.

**Track 3 (2.2.1, onigiri.asi):** done, on `claude/tweaksv-onigiri-parts-switch-61130f` from d54259e, nine commits (f1a0fd4 to 3eb6c82), verified: the second kind of plugin and the conflict check as settled, the check before the log header, the line for a part already off in `TweaksV.ini`, 25 new checks (510 in its worktree, 485 and its 25), the documents; the four builds clean there. Next: rebased onto main by the overview session, the shared lines merged, then the 2.2.1 version commit, the rebuild, the suites, my boot with NVE and the tag.

**Decisions waiting on me:** none. D2 is built as recommended, the start-up summary item with the INI switch that stops it, and stands unless I say otherwise.

**Boots:** 2.2.1 with NVE.
