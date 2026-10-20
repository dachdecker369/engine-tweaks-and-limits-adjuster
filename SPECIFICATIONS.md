# Specifications: the decal definitions part

The requirements for TweaksV's decal definitions part - the second half of what the Legacy "fwBoxStreamerVariable and decals limit" patch does. The order of work is in [PLAN.md](PLAN.md). The research is done: a findings file for 1.0.1158.16 (kept outside the repository, next to the box streamer findings) fills every field of section 3 and carries the site tables the build needs. The build reads this file, PLAN.md and the findings file, and nothing of the Legacy port or of FiveM.

## 1. What the part does

- The game keeps its decal definitions - the kinds of bullet holes, blood, scorch and tyre marks, liquids, graffiti and the like, read from the `decals.dat` files - in a static array of 260 entries (1.0.1158.16). Once 260 are registered, every further definition is dropped silently: the files are read in order, so the definitions of the last files - add-on and mod content - go missing, and the decals that use them never appear. The part moves the array to a larger one and raises the limit everywhere the game uses it. It is a limit on definitions (kinds of decal), not on decals on screen.
- Everything else stays the game's: the definitions, their order, the code that uses them. The part changes addresses and a constant, nothing more.
- Like the other array parts: the new array is allocated at the maximum size up front, as read/write data near the game; it is moved before the count is raised; every reference and every use of N is found and checked before anything is written; all writes go in one all-or-nothing group; a missing pattern, an unexpected byte, a reference that does not point at the game's own array (another mod moved it), a constant that disagrees between sites or an implausible one skips the part and reports it in the start-up summary; `DRY_RUN` writes nothing and logs what it would do; `DIAGNOSTIC_MODE` logs every site and value.

## 2. Settings and names (decided)

| | Decision | Why |
|---|---|---|
| Section | `[DECALDEFS_SETTINGS]` with `ENABLED=1` | one section per part, like `[BOXSTREAMER_SETTINGS]` |
| Size key | `DECALDEFS_SIZE`: minimum 260 (the game's own array), default 2048, maximum 16384 | the capacity in definitions, like `TRAVERSAL_LIST_SIZE`; 2048 holds every known decal mod many times over; 16384 x 80 bytes is 1.3 MB and stays far below the uint16 index limit |
| Measuring key | `LOG_DECALDEFS` (testing, 0): logs how many definitions are registered 10 seconds in and then every minute while the number changes, against the capacity and the game's own 260 | the number that says whether a load needs the part, as `LOG_SEARCHES` does |
| Feature name | `DecalDefs` - the log tag, the summary, the "Features:" line | one word, like `BoxStreamer` |
| Files | `source\TweaksVDecalDefs.h`, `source\TweaksVDecalDefs.cpp` | TweaksV's own parts are `TweaksV*` files |
| Pattern labels | `D-E-L1..` for the sites found by pattern (anchors and constants); references found by the address scan carry no label | `D` is free; `B-E-` is the box streamer's |
| Conflict file | `DecalPatch.asi` (the closed-source Enhanced plugin; it also ships `DecalPatch.ini` and `.log`), under this part and the box streamer part | both halves of it are replaced; the conflict check then names it and switches both off |
| Version | 1.6.0 | a new part |

Setting descriptions for the INI, in the style of the others: `DECALDEFS_SIZE` - "Decal definitions (the kinds of bullet holes, blood, scorch marks and so on from decals.dat) the game can hold - its own array holds 260, and add-on content silently loses the rest"; `LOG_DECALDEFS` - "TESTING ONLY: logs how many decal definitions are registered, and whether the game's own array of 260 would have dropped some (changes nothing)".

## 3. Findings the research must deliver

A text file with every field below filled in, for the game build in question, from a `TEST_DUMP_CODE` copy of the running game (README, "Test Builds"). RVAs count from the start of `GTA5_Enhanced.exe` in memory. "None" is an answer; a field left open stops the build until it is answered. The file for 1.0.1158.16 exists and answers every field; this section stays as the template for the next game build.

### 3.1 Is it hard-coded?

- Where the game decides that the array is full: the instruction(s) and the constant they compare against. If the limit comes from `gameconfig.xml` or another data file instead, say so with the key - the part is then unnecessary. (1.0.1158.16: hard-coded, 260, one compare in the `decals.dat` parser.)

### 3.2 The store

- RVA and section of the array; entry size in bytes; N; the alignment the game assumes (any SIMD loads of entries).
- One array, or several parallel ones (positions, names, flags)? Each one's RVA, entry size and count.
- Everything else sized by N: a name or hash lookup, a free list, a bitset of used entries. Each one's RVA and size.
- Does any entry hold a pointer to another entry or into the array (moving then changes those pointers)? Does any other global hold the array's address (a pointer in data rather than in code)?
- The global that holds the current number of definitions, if there is one.

### 3.3 Every reference to the store, and to everything in 3.2

For each: RVA, the instruction form (`lea r64, [rip+disp32]`, `mov`, `cmp`, `movaps`, ...), what it addresses (the base; the end, base + N x entry size; a field of entry 0 at base + k) and 12-20 bytes of context for a pattern. Say which references a scan for a target address would find (as `FindRipRelativeLeas` finds every `lea` of one address) and which would not. Count them: the build demands exactly that many.

### 3.4 Every use of N

For each: RVA, the instruction, the encoded value (N, N-1, N x entry size, N plus something), the operand width (imm8, imm16, imm32 - an imm8 or imm16 cannot take a larger value and needs a different plan) and 12-20 bytes of context. Say which are compares against the current count (the "full" check), which are loop bounds and which size an allocation or a clear.

### 3.5 The index width

- How a definition is referred to after registration: a uint8, uint16 or uint32 index or handle, or a pointer. The narrowest width anywhere caps the new size.

### 3.6 Timing and threads

- When the first definition is registered: during the game's start-up after the DllMains have run (the array is then still empty when TweaksV moves it), or earlier.
- Which thread registers and which threads read, if known.

### 3.7 The measuring site

- The global of 3.2 with the current count, or - if there is none - the site where a registration is refused for lack of room and the function that registers (its calling convention and arguments), for a counting hook like `LOG_SEARCHES`.

### 3.8 Uniqueness

- Every candidate pattern checked against the whole dump: exactly one match. Where two sites are byte-identical (as the box streamer's second leas were), say so; the build then locates them by address instead.

## 4. The patch

What the findings for 1.0.1158.16 lead to; the numbers (31, 23, 13, 9, the offsets) are theirs and are checked at run time, never assumed.

1. Find the two anchors in the parser by pattern: the "full" check gives the count's address (from its `mov`'s disp32) and the first constant site (N-1); the parser's `lea` gives the array's address. The array must lie at count + 0x10, both inside the game; the imm32 at the first site must be N-1 = 259. N comes from that site (260), not from the code.
2. Find every other constant site by its pattern (section 3.8 of the findings lists one unique anchor per site, and one anchor that matches exactly 7 times for 7 of the "add decal" sites - a count check of 7); read each imm32; it must be N-1, N or, for the init loop end, 0x7D8 + N x 0x50. Any other value skips the part.
3. `MemScanner::FindRipRelativeLeas(array, 32)` must return exactly 31 leas, every one `48 8D` or `4C 8D`; each pointing at the array's first byte. Any other count skips the part.
4. Clamp the setting to [260, 16384]; `DRY_RUN` stops here with one line that says what would change ("would move the decal definitions array (31 references) and raise its limit from 260 to 2048 (23 sites)").
5. `AllocateStubMemory(80 x 16384, PAGE_READWRITE)` - Windows zeroes it, and zero is the array's initial state (the game's own array is zero-initialised data; its init routine only zeroes the three texture pointers of every entry).
6. One `WriteCode` group: the 31 rel32 rewrites (recomputed per site, the distance checked against int32), 13 imm32 = newN - 1, 9 imm32 = newN, 1 imm32 = 0x7D8 + 0x50 x (newN - newN mod 26) for the init loop end (the loop clears 26 entries per pass; the rounding leaves at most 25 entries uncleared, which the fresh buffer holds as zeros anyway). The count global stays where it is: every access to it is its own rip-relative instruction, none goes through the array, and none of the array's 31 leas goes through the count.
7. On success one log line ("Decal definitions array moved and its limit raised from 260 to 2048 entries."); with `LOG_DECALDEFS`, a worker thread reads the count 10 seconds in and then every minute while it changes and logs "LOG_DECALDEFS: N decal definitions registered (the game's own array holds 260, this one 2048)" plus, when N > 260, how many the game's own array would have dropped. No hook is needed.

Checks that skip the part, each with a `ReportFailure`: a missing pattern; an unexpected instruction or value at a site; the array not at count + 0x10 or outside the game (moved already); a lea count other than 31; an allocation failure; a write failure (`WriteCode` has logged why).

## 5. Tests

- `test\fake_game_full.cpp`: copies of every site in `.fakesit` (CC-terminated, so no pattern matches across two): the parser's two anchors, the 21 other constant sites with their anchors, the 31 leas (their rel32s pointing at a fake array of 260 x 80 bytes that the harness reaches only through a `volatile` pointer, so that no lea of the harness itself is counted by the scan), the count global 16 bytes before it; and options for: another mod moved the array, one lea pointing elsewhere in the game, a 32nd lea, a constant that disagrees, an implausible constant, the count global holding a chosen value (for the measuring switch).
- `test\run_decaldefs.sh`: the defaults applied (31 references, 23 constants, the new array's size and protection); the setting's limits (below 260, above 16384, not a number); `DRY_RUN`; `DIAGNOSTIC_MODE` (every pattern once, every site logged); `ENABLED=0`; every failure option above (nothing written, the reason logged and reported); the plain fake game (no code present); the measuring switch on, off and with `DRY_RUN`; `DecalPatch.asi` present (this part and the box streamer part skipped and reported).
- `test\run_ini_default.sh` (the key count), `test\run_integration.sh` (the part's on/off strings in the old-plugin and broken-pattern loops), `test\run_all.sh` (the suite), NOTES.md (the check count).

## 6. Documentation

- README.md: a section like "Box streamer" (what a decal definition is, the setting, the measuring switch); the "Other mods" line about `DecalPatch.asi` changed to "detected: switches the decal definitions part and the box streamer part off"; the install list. The clean-room statement for this part: written from TweaksV's own findings about the game; the published patches were known as the idea and as a guide to what to look for; no line of their code was read for the build or went into TweaksV.
- NOTES.md: the feature bullet in the style of the box streamer's, with the checks it makes; the measurement once there is one.
- PATTERNS.md: a section with the labels, patterns, offsets, found and RVA; the status counts.
- CREDITS.md and UPSTREAM.md: the CitizenFX Collective and 0x-FADED entries extended to "the box streamer and decal definitions fixes"; still nothing in THIRD_PARTY_NOTICES.md or `upstream\`.

## 7. Done

- Builds: Release, the test build and the analysis build at /W4 without a warning.
- Tests: `bash test/run_all.sh` green, the new suite included.
- In game, by the owner: a `DIAGNOSTIC_MODE=1` boot with every new pattern found once at the findings' RVAs, 31 leas, 23 constants and no `PROBLEM`; a boot with `LOG_DECALDEFS=1` under a decal-heavy load that shows the count beyond 260 without a crash or missing decals; a ten-minute session without a crash dump.
- Performance: nothing per frame beyond the measuring thread's one read per minute; the array's memory 1.3 MB.
- Security: every value read from the game is bounds-checked before use; the new array is read/write, never executable; a damaged or crafted `TweaksV.ini` value is clamped and reported.
- Never silent: every reason for skipping the part is in the start-up summary and the log.

## Next steps

1. The part is built to sections 4 to 6 from this file, PLAN.md and the findings file, and accepted by section 7, as TweaksV 1.6.0 - committed locally, with the Release build rebuilt from that commit.
2. The owner boots it: `DIAGNOSTIC_MODE=1` first, then `LOG_DECALDEFS=1` with a decal-heavy load; then push and the tag.
3. Nexus and the removal of PLAN.md and this file follow (PLAN.md, phase 4).
