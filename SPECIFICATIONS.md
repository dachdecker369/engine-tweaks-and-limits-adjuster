# Specifications: the decals limit part

The requirements for TweaksV's decals-limit part, written before the research so that the research knows what to deliver and the build knows what "done" means. The order of work is in [PLAN.md](PLAN.md). Everything about the game marked "to find" is unknown until the owner's findings file fills it in; nothing may be taken from the Legacy port or from FiveM.

## 1. What the part does

- The game registers decal definitions in a store of a fixed size, N entries (to find). When the store is full, further definitions are refused: the game crashes, or shows no decals for the content that brought them. The part moves the store to a larger one and raises N everywhere the game uses it, so that add-on weapons, vehicles and map data can register theirs.
- Everything else stays the game's: the definitions, their order, the code that uses them. The part changes addresses and a constant, nothing more.
- Like the other array parts: the new store is allocated at the maximum size up front, as read/write data near the game; it is moved before the count is raised; every reference and every use of N is found and checked before anything is written; all writes go in one all-or-nothing group; a missing pattern, an unexpected byte, a reference that does not point at the game's own store (another mod moved it), a count that disagrees between sites or an implausible count skips the part and reports it in the start-up summary; `DRY_RUN` writes nothing and logs what it would do; `DIAGNOSTIC_MODE` logs every site and value.

## 2. Settings and names (recommendations - decided in PLAN.md, phase 1)

| | Recommendation | Why |
|---|---|---|
| Section | `[DECALSTORE_SETTINGS]` with `ENABLED=1` | one section per part, like `[BOXSTREAMER_SETTINGS]` |
| Size key | `DECALSTORE_SIZE`: minimum the game's own N; default and maximum to find | like `TRAVERSAL_LIST_SIZE`, the capacity in entries |
| Measuring key | `LOG_DECALS` (testing, 0): logs every minute how many definitions are registered against the capacity, and whether the game's own store would have overflowed | the number that says whether a load needs the part, as `LOG_SEARCHES` does |
| Feature name | `DecalStore` - the log tag, the summary, the "Features:" line | one word, like `BoxStreamer` |
| Files | `source\TweaksVDecalStore.h`, `source\TweaksVDecalStore.cpp` | TweaksV's own parts are `TweaksV*` files |
| Pattern labels | `D-E-L1..` for the count sites, `D-E-R1..` for references the scan cannot find | `D` is free; `B-E-` is the box streamer's |
| Default size | the smallest value that holds the heaviest add-on load the owner tests with, with room to spare - decided from the findings and the index width, not before | the box streamer's 10000 was chosen the same way |
| Maximum size | the index width's limit or a memory bound of a few megabytes, whichever is smaller | the new store always has the maximum size |
| Conflict files | the closed-source plugin's `.asi` name(s), under this part and the box streamer part | both halves of it are replaced; the conflict check then names it and switches both off |
| Version | 1.6.0 | a new part |

## 3. Findings the research must deliver

A text file with every field below filled in, for game build 1.0.1158.16, from a `TEST_DUMP_CODE` copy of the running game (README, "Test Builds"). RVAs count from the start of `GTA5_Enhanced.exe` in memory. "None" is an answer; a field left open stops the build until it is answered.

### 3.1 Is it hard-coded?

- Where the game decides that the store is full: the instruction(s) and the constant they compare against. If the limit comes from `gameconfig.xml` or another data file instead, say so with the key - the part is then unnecessary.

### 3.2 The store

- RVA and section of the store; entry size in bytes; N; the alignment the game assumes (any SIMD loads of entries).
- One array, or several parallel ones (positions, names, flags)? Each one's RVA, entry size and count.
- Everything else sized by N: a name or hash lookup, a free list, a bitset of used entries. Each one's RVA and size.
- Does any entry hold a pointer to another entry or into the store (moving then changes those pointers)? Does any other global hold the store's address (a pointer in data rather than in code)?
- The global that holds the current number of definitions, if there is one.

### 3.3 Every reference to the store, and to everything in 3.2

For each: RVA, the instruction form (`lea r64, [rip+disp32]`, `mov`, `cmp`, `movaps`, ...), what it addresses (the base; the end, base + N x entry size; a field of entry 0 at base + k) and 12-20 bytes of context for a pattern. Say which references a scan for a target address would find (as `FindRipRelativeLeas` finds every `lea` of one address) and which would not. Count them: the build demands exactly that many.

### 3.4 Every use of N

For each: RVA, the instruction, the encoded value (N, N-1, N x entry size, N plus something), the operand width (imm8, imm16, imm32 - an imm8 or imm16 cannot take a larger value and needs a different plan) and 12-20 bytes of context. Say which are compares against the current count (the "full" check), which are loop bounds and which size an allocation or a clear.

### 3.5 The index width

- How a definition is referred to after registration: a uint8, uint16 or uint32 index or handle, or a pointer. The narrowest width anywhere caps the new size.

### 3.6 Timing and threads

- When the first definition is registered: during the game's start-up after the DllMains have run (the store is then still empty when TweaksV moves it), or earlier.
- Which thread registers and which threads read, if known.

### 3.7 The measuring site

- The global of 3.2 with the current count, or - if there is none - the site where a registration is refused for lack of room and the function that registers (its calling convention and arguments), for a counting hook like `LOG_SEARCHES`.

### 3.8 Uniqueness

- Every candidate pattern checked against the whole dump: exactly one match. Where two sites are byte-identical (as the box streamer's second leas were), say so; the build then locates them by address instead.

## 4. The patch

1. Find every count site by pattern; read the value at each; all must agree and be plausible (0 < N <= maximum).
2. Take the store's address from a reference that a verified pattern reaches; cross-check it against every other such reference; it must lie inside the game.
3. Find every remaining reference by address scan where the findings allow it, by pattern otherwise; the number found must equal the number the findings list.
4. Clamp the setting to [N, maximum]; `DRY_RUN` stops here with one line that says what would change.
5. Allocate the new store, and every parallel array, with `AllocateStubMemory(size, PAGE_READWRITE)` at the maximum size; Windows zeroes it - if the game expects another initial state (a free list, a sentinel), reproduce it before the move.
6. One `WriteCode` group: every reference to its new address (the rel32 recomputed per site, the distance checked against int32), every use of N to the new value in the same encoding (N-1, N x entry size, ...) and, with the measuring switch on, the hook through `RedirectCalls`.
7. On success one log line; with the measuring switch, a worker thread logs a summary 10 seconds in and then every minute while the numbers change.

Checks that skip the part, each with a `ReportFailure`: a missing pattern; an unexpected instruction at a site; count sites that disagree; an implausible count; a store outside the game (moved already); a reference count other than expected; an allocation failure; a write failure (`WriteCode` has logged why).

## 5. Tests

- `test\fake_game_full.cpp`: copies of every site in `.fakesit` (CC-terminated, so no pattern matches across two), the store and its parallel arrays as data reached through a pointer (so that no `lea` of the harness is counted by a scan), fake registration code if the measuring switch hooks something, and options for: another mod moved the store, one reference points elsewhere in the game, an extra reference, one count that disagrees, implausible counts, a hooked call.
- `test\run_decalstore.sh`: the defaults applied (references, counts, the new store's size and protection); the setting's limits (below N, above the maximum, not a number); `DRY_RUN`; `DIAGNOSTIC_MODE` (every pattern once, every site logged); `ENABLED=0`; every failure option above (nothing written, the reason logged and reported); the plain fake game (no code present); the measuring switch on, off and with `DRY_RUN`; the closed-source plugin's file present (the part skipped and reported).
- `test\run_ini_default.sh` (the key count), `test\run_integration.sh` (the part's on/off strings in the old-plugin and broken-pattern loops), `test\run_all.sh` (the suite), NOTES.md (the check count).

## 6. Documentation

- README.md: a section like "Box streamer" (what, the setting, the idea's origin, the measuring switch); the "Other mods" line about the closed-source plugin changed to "detected and replaced"; the install list.
- NOTES.md: the feature bullet in the style of the box streamer's, with the checks it makes; the measurement once there is one.
- PATTERNS.md: a section with the labels, patterns, offsets, found and RVA; the status counts.
- CREDITS.md and UPSTREAM.md: the CitizenFX Collective and 0x-FADED entries extended to "the box streamer and decals limit fixes"; still nothing in THIRD_PARTY_NOTICES.md or `upstream\`.

## 7. Done

- Builds: Release, the test build and the analysis build at /W4 without a warning.
- Tests: `bash test/run_all.sh` green, the new suite included.
- In game, by the owner: a `DIAGNOSTIC_MODE=1` boot with every new pattern found once at the findings' RVAs and no `PROBLEM`; a boot with the measuring switch under a heavy add-on load that shows the count beyond the game's own N without a crash; a ten-minute session without a crash dump.
- Performance: nothing per frame beyond the measuring switch's counters; the store's memory a few megabytes at most.
- Security: every value read from the game is bounds-checked before use; the new store is read/write, never executable; a damaged or crafted `TweaksV.ini` value is clamped and reported.
- Never silent: every reason for skipping the part is in the start-up summary and the log.

## Next steps

1. The owner researches 1.0.1158.16 and fills section 3 into a findings file - every field; "none" is an answer, an open field is not.
2. With that file, the recommendations of section 2 are confirmed or changed - the default and maximum size last, from the index width and a heavy add-on load.
3. The part is then built to sections 4 to 6 and accepted by section 7, as TweaksV 1.6.0.
