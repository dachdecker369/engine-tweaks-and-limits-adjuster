# Plan

The order of work for a change to TweaksV, phase by phase, each phase ending with the test suites green, the four builds (Release, Debug, the test build, the analysis build) clean, a local commit and my boot. It carried the decals part of 1.6.0, the whole rewrite of 2.0.0, the fixes of the 2.0.0 audit, the 2.1.1 patch for T-UNLOCK, and the two versions from the NaturalVision Enhanced report, 2.2.0 and 2.2.1, built in parallel tracks - one coding session in a worktree of its own per track, each owning its files, rebased onto main in order and released one after the other - and 2.2.2, a comment after each key's value in the default `TweaksV.ini`, made in one go on 2026-10-27 and not published by itself (all in the git history). Under way since 2026-10-27: the Blips part, the first of the five new parts on the roadmap ([ROADMAP.md](ROADMAP.md)), its research pass first and a test-build switch that saves a script program from memory before the part itself (D6); 2.3.0 stays on the roadmap, not urgent, and so do the other four parts.

## How the work runs

The research comes first and the code after it, each session with a prompt from the overview session, kept in `.claude\prompts\` and never committed:

- **The research session** answers the questions of the research brief, `.claude\prompts\blips-research-brief.md`, with the prompt `.claude\prompts\blips-research-session.md`; I lifted my limit on analysing the game for this pass alone (2026-10-27). It runs in the repository folder without a worktree, because it changes nothing in the repository: it builds the test build for my dump boot, analyses the dump with my research tools and Ghidra, reads the script program the switch of D6 saves from memory, and writes every verified fact, with where it was seen, into a findings file in my research tools' folder, `output-1158.16\blips_findings_for_the_clean_room_build.txt`, which outlives the session and carries its state at its head. It writes no file into the game folder: I boot.
- **The overview session** verifies the findings against the dump, writes the game facts into PATTERNS.md - as prose only, since the patterns suite reads every row of a pattern table and a row comes with its entry in `TweaksVPatterns.h` - writes the coding prompts, and then the dumps go.
- **The coding sessions** build the switch of D6 and then the part, one track each and one after the other, each in a worktree of its own (the app makes it under `.claude\worktrees\` with a `claude/...` branch). It commits on its branch only, each item its own commit; it never merges, tags, pushes or sets the version; after every commit it writes its state into the State block below and a two-line report into the chat. The overview session verifies the branch, runs the suites from a clean rebuild and hands me the boot; for the part it also sets the version, then tags and pushes main and the tag together - the switch's commits wait on main for that release.

## Blips - the game's own scripts keep making their blips next to a mod's

2.4.0 on the roadmap; the version takes the next free number when it ships. The original, its license and the shape I intend are in ROADMAP.md, corrected on 2026-10-27: Blip Limit Fix takes its locators of the program registry, its layout of a script program and its opcode table from scrDbg, which is idea only, and mixes them into every source file that matters.

Decided on 2026-10-27:

- **D1, the clean room:** nobody who works on the part reads Blip Limit Fix's sources or tests, nor anything of scrDbg. The plugin's design comes from the research brief, as prose taken from its sources through a filter that held back every pattern, offset, layout value and opcode value; its README's own findings on the blip pool may be read. CREDITS.md credits it for the design and the pool findings, and scrDbg only if the part takes its idea, the registry; no line is carried over, so THIRD_PARTY_NOTICES.md gets nothing.
- **D2, the mechanism:** the threshold is patched in `blip_controller`'s bytecode, as the original does. The facts for the other way - the count native answering `blip_controller` alone a count that reaches the threshold only at the configured limit - are collected in passing, so that the choice can be revisited without a second pass.
- **D3, the registry:** none, if the hook point hands over the whole program.
- **D4, a threshold other than the game's own** (240 on some setups): left as it is and logged, as the HeapAdjuster part leaves a heap another adjuster set, unless the research shows it is the game's own on some build.
- **D5, the pool:** measured under `DIAGNOSTIC_MODE`, not moved in this version; its sites are recorded in the research, for a later one.
- **D6, the script's source:** the script file exported from the game's archive; a test-build switch that saves the program from memory only if that file cannot be read - a coding track of its own, before the part.
- **D7, the research:** by the research session, with my limit lifted for this pass alone.

Planned on 2026-10-27, once the research pass had found the exported scripts encrypted - the archives store them that way, and the keys of the game's cipher lie in its `.data`, which the dump never holds - and I had said yes to D6's switch:

- **The switch,** `TEST_DUMP_SCRIPT=<name>` under `[TWEAKSV]`, test builds only: each time the game places the script program of that name, its code, its native table before and after the game resolves it, and its strings go to `%LOCALAPPDATA%\TweaksV\Script-<name>-<n>.bin`, never into the game folder, in a layout the research session's reader is written against. It stays as test tooling: after a game update, the check in the script can only be read this way.
- **Its hook is the part's:** the call of the natives' resolution in the program's resource constructor, redirected. It runs on the game's resource placement thread, before the program enters the game's registry of programs, so no script thread can run the program then. A shared module, `TweaksVScripts`, owns that hook and checks the program's layout; the switch is its first user, the part's patch will be its second.
- **Its patterns** get a section of PATTERNS.md of their own, "Script programs": `Y-E-H1` (the hook), `Y-E-N1` and `Y-E-I1` (the layout). `L` stays for the part's own, the blip pool's.
- **No release of its own:** its commits stay on main, not pushed, until the Blips part's release carries them.

The phases:

1. **The research pass, section 2** (the research session): the test build, built alone; my dump boot with `TEST_DUMP_CODE=60` and my export of `blip_controller.ysc`; in the dump's code the interpreter, a script program's fields, the load path and the hook point, the native tables, the blip pool and the active script thread - done on 2026-10-27. Section 3 stopped there: the exports are encrypted (D6).
2. **The switch** (one coding track): the shared module and `TEST_DUMP_SCRIPT`, its scenarios in the integration suite, the documents; then the overview session's checks of the branch, and my boot with the test build and `TEST_DUMP_SCRIPT=blip_controller`, a save loaded once more to see whether a save load places the program again.
3. **The research pass, section 3** (the research session): its reader, written meanwhile, on the saved program - the check, its uniqueness, the natives, the threshold and the room for the rewrite; then the findings file's hand-over: the material for PATTERNS.md and the answers to the design questions.
4. **The findings** (the overview session): section 2 verified against the dump, section 3 against the saved program; the game facts into PATTERNS.md as prose; the dumps and the saved programs deleted and my research tools' `work\` folder emptied; the part's coding prompt.
5. **The part** (one coding track): `TweaksVBlips.cpp` and `.h` on the shared code and `TweaksVScripts`; `[BLIPS]` with `ENABLED` and `ACTIVE_BLIP_LIMIT`, which takes `TweaksV.ini` to 30 keys and layout 3, so T-LAYOUT's carry-forward comes with it; the feature with its old plugin `BlipLimitFix.asi`; the patterns with their rows in PATTERNS.md - the patterns suite reads a label as one capital letter and `-E`: `L` for the part's, while `B` is the box streamer's and `Y` the script programs'; the measurement; the suite, with the fake game's script program; the documents.
6. **The release:** the version, a clean rebuild, the suites, my boots - with Collectibles on Map and Radar, where the vanilla blips must stay, and with `DIAGNOSTIC_MODE=1`, the part's lines at the researched RVAs - then the tag, which carries the switch too.

## State

**State:** 2.2.2 is tagged on 2ba8c8c, the commit I booted on 2026-10-27, and pushed with main; it is not published by itself - the next release carries it, and 2.2.1 stays the last one published. The Blips part (2.4.0 on the roadmap) is under way. Its research pass did section 2 on 2026-10-27, its findings in my research tools' `output-1158.16\blips_findings_for_the_clean_room_build.txt`, and stopped at section 3: the scripts I exported are encrypted as the archives store them, and I said yes to D6's switch the same day. Next is that switch, `TEST_DUMP_SCRIPT`, one coding track (phase 2, its prompt `.claude\prompts\blips-d6-switch-session.md`); meanwhile the overview session verifies section 2 against the dump, and the research session writes its reader for the saved program. The suites stand at 525 checks; the open issues are T-HARDEN, T-LAYOUT, T-CODEPAGE, T-DIAG, T-SUMMARY, T-FREEZE and T-REPLACE - T-LAYOUT goes with the Blips part, T-REPLACE with 2.3.0, not urgent.

**The switch's track:** not started; it branches from main with this plan.

**The research session:** section 2 done; it writes its reader for the saved program, then waits for my boot with the switch.

**The verification:** section 2 against the dump, by the overview session - under way.

**Decisions waiting on me:** whether the research pass also settles T-CODEPAGE's open fact - the encoding of the paths the game's file functions get - while the dump is there (the overview session recommends yes).

**Boots:** the switch's boot - the test build with `TEST_DUMP_SCRIPT=blip_controller`, a save loaded once more - then the Blips part's boots.
