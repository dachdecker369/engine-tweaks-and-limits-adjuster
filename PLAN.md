# Plan: the decals limit part

What it takes to give TweaksV a decals-limit part of its own - the second half of what the Legacy "fwBoxStreamerVariable and decals limit" patch does - built the way the box streamer part was: from the owner's own analysis of GTA V Enhanced, with none of the Legacy port's or FiveM's code read. [SPECIFICATIONS.md](SPECIFICATIONS.md) says what the part must do and what the research must deliver; this file is the order of work and its checkpoints. Both files are removed again when the part has shipped and their content has moved into NOTES.md, PATTERNS.md and README.md.

## Why

The game keeps its decal definitions - the kinds of bullet holes, blood, scorch and tyre marks, liquids and the like that weapons, vehicles and map data register - in a store of a fixed size. Add-on weapons and vehicles bring their own, and a heavy add-on load fills the store: the game then crashes or drops decals. The closed-source Enhanced plugin that raises it together with the box streamer list is crashing for people, and TweaksV's own box streamer part must not run next to it (README, "Other mods"). A part of TweaksV's own removes the need for that plugin, and lets its file name join the conflict check.

## Ground rules

- Clean room. No line of the Legacy port, of FiveM's streaming component or of any other decals patch is read, before or during the work. Only the idea is used - a static store with a hard-coded count, moved to a larger one with every count check raised - and that idea is credited (CREDITS.md). Every fact about the Enhanced binary comes from the owner's analysis of a `TEST_DUMP_CODE` copy of the running game, written down in a findings file as for the box streamer.
- The part is built like `TweaksVBoxStreamer.cpp`: patterns in the one table, the store moved at its maximum size before the count is raised, every reference checked before anything is written, one all-or-nothing write, `DRY_RUN` and `DIAGNOSTIC_MODE`, a failure reported in the start-up summary, a fake-game suite.
- Nothing is pushed before the owner has booted the build and read its log.

## Phases

### 0. Research (the owner)

Deliverable: a findings file that fills every field of the template in SPECIFICATIONS.md, section 3, from Ghidra on a `TEST_DUMP_CODE` dump of 1.0.1158.16. In short:

- [ ] Confirm the limit is hard-coded in the game's code and not read from `gameconfig.xml` or another data file. If it is data-driven, the part is unnecessary: say so in README instead and stop.
- [ ] The store itself: address, section, entry size, the count N, alignment; whether it is one array or several parallel ones; what else is sized by N (a name or hash table, a free list, a bitset).
- [ ] Every instruction that references the store or anything sized by N: the base, the end (base + N x entry size), fields of the first entry; the instruction form of each (lea, mov, cmp, ...), with 12-20 bytes of context for a pattern, and which ones a plain scan can find by their target.
- [ ] Every use of N: compares against the current count, loop bounds, N-1, N x entry size in an allocation or a clear, any imm8 or imm16 form that cannot hold a larger value.
- [ ] How a definition is referenced afterwards - the width of its index or handle (uint8, uint16, uint32) - which caps the new size.
- [ ] Where the game gives up when the store is full (the site that answers "no room"), and the global that holds the current count, for the measuring switch.
- [ ] When the first definition is registered relative to TweaksV's start (it must be after the DllMains have run, as the game's own start-up is).
- [ ] A check of every candidate pattern for uniqueness in the dump.

### 1. Decisions (the owner, with recommendations in SPECIFICATIONS.md, section 2)

- [ ] Section and key names, the feature name and the pattern label prefix.
- [ ] Default and maximum of the new size, from the vanilla count, the index width and a heavy add-on load.
- [ ] The measuring switch: whether the current count can be read from a global, or a hook is needed.
- [ ] The exact file name(s) of the closed-source plugin for the conflict check.

### 2. Implementation

- [ ] `source\TweaksVDecalStore.h` and `.cpp` (names per decision 1), the entries in `TweaksVSettings.h`, `TweaksVPatterns.h` and `TweaksVFeatures.cpp`, the project and filters.
- [ ] A scan helper only if the findings need one beyond `MemScanner::FindRipRelativeLeas`.
- [ ] The conflict entries for the closed-source plugin, under the decals part and the box streamer part.
- [ ] The fake-game sites in `test\fake_game_full.cpp`, `test\run_decalstore.sh`, the ini and integration suites, the suite list in `test\run_all.sh`, the check count in NOTES.md.
- [ ] README (a section like "Box streamer", the "Other mods" lines, the install list), NOTES (the feature bullet, the measurement once there is one), PATTERNS.md (a section, the status counts), CREDITS.md and UPSTREAM.md (the idea's origin, extended from the box streamer entries).
- [ ] Version 1.5.0 -> 1.6.0 in `source\TweaksV.vcxproj`.

### 3. Verification

- [ ] Release, the test build and the analysis build at /W4 without a warning; `bash test/run_all.sh` green.
- [ ] A boot with `DIAGNOSTIC_MODE=1`: every new pattern found once at the RVAs of the findings, the store's address, every reference, every write; no `PROBLEM`.
- [ ] A boot with the measuring switch and a heavy add-on load: the count passes the game's own N without a crash or missing decals; with the part off, the same load shows the failure it is meant to cure (or the number says the load never reaches N - then the test load is too small, not the part wrong).
- [ ] A ten-minute session: no crash dump, no Windows error.

### 4. Release

- [ ] Commit locally, rebuild Release from that commit, the owner's boots, then push and the annotated tag `1.6.0`.
- [ ] Nexus: the BUILT section, IMPORTANT (the closed-source plugin is now detected and replaced), the install list (its file name), the SHA-256, the changelog, the credits.
- [ ] Remove PLAN.md and SPECIFICATIONS.md.

## Risks

- The index width caps the gain: a uint8 index allows 256 definitions at most, whatever the store's size. The findings decide this before any code.
- Entries may hold pointers into the store, or the game may keep the store's address in data rather than in code (a pointer, a table of stores). Then moving it needs more than code patches: the findings must say so, and the design changes before the build.
- More reference forms than leas (reads of a field of the first entry, compares against the end of the store) each need their own pattern or a decoding scan; near-identical functions may again defeat patterns, as with the box streamer.
- The closed-source plugin's exact file name is not known here; without it the conflict check cannot name it.

## Next steps

1. The owner fills the findings template (SPECIFICATIONS.md, section 3) from a `TEST_DUMP_CODE` dump of 1.0.1158.16 and hands over the file.
2. With the findings, the decisions of phase 1 are taken; the recommendations are in SPECIFICATIONS.md, section 2.
3. Then the part is built, tested and documented as in phases 2 and 3, as version 1.6.0, and booted by the owner before the push.
