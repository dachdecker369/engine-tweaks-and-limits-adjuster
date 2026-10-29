# Plan

The order of work for a change to TweaksV, phase by phase, each phase ending with the test suites green, the four builds (Release, Debug, the test build, the analysis build) clean, a local commit and my boot. It carried the decals part of 1.6.0, the whole rewrite of 2.0.0, the fixes of the 2.0.0 audit, the 2.1.1 patch for T-UNLOCK, the two versions from the NaturalVision Enhanced report, 2.2.0 and 2.2.1, built in parallel tracks, then 2.2.2, 2.2.3 and 2.3.0, the Blips part, with its research pass and its test-build switch (all in the git history). Under way since 2026-10-29: the first round of the full release - 2.4.0, the replace-rename of [ROADMAP.md](ROADMAP.md), beside the research for the four new parts on the roadmap, WeaponLimits, SirenSettings, PedProps and Resources. The parts' coding round follows, once the research is verified and I have chosen the parts to build.

## How the work runs

Three levels, as my rules for every project have them:

- **The overview session** (`_OVERVIEW (TweaksV)`) wrote this plan and starts the merge session at my go. It verifies every research pass against the dump before anything is built on it, checks the merged main from its diff, asks for my boot of 2.4.0, then tags and pushes. It is the only session I talk with.
- **The merge session** (`_MERGE (TweaksV)`) turns this plan into one brief per track and starts the eight sessions at my go in its session. It answers what this plan and its decisions answer, sends every decision of mine up to the overview session and brings my answer back down in my words. While the round runs it alone commits to main, and it keeps the Next steps below and the local notes there. It verifies the coding track from its branch, rebases it onto main, sets the version, rebuilds from clean, runs the suites and reports the build up.
- **Eight sessions**, each in a worktree and on a branch of its own from main, which the app makes, each reporting to the merge session alone: one coding session and seven research sessions. None merges, tags, pushes or sets the version.

The coding session commits on its branch only, each item its own commit. After every commit it writes its state into its paragraph of the Next steps and into the local notes of its worktree and reports to the merge session in two lines, without stopping; it ends its turn only when its track is done or a decision of mine blocks everything left.

The research sessions change nothing in the repository: the worktree is for reading TweaksV's own code beside the game's. Each works in my research tools' folder, `C:\_DEVELOPMENT\_Tools\TweaksV-research` (its README.txt the guide), on its own files only:

- its findings file, `output-1158.16\<topic>_findings_for_the_clean_room_build.txt`, which outlives the session and carries its state at its head, rewritten after every item - every fact with where it was seen, and a guess marked as one;
- its work folder, `work-<topic>\`: `TWEAKSV_WORK=work-<topic>` before the Ghidra scripts, and Rebuild.java writing into it;
- its own scripts in `ghidra_scripts\`, its topic in front of their names.

It never writes README.txt, another pass's files, the game folder or the repository. It boots nothing - a fact that needs a boot of mine goes up as a question - and downloads nothing without my yes. After every item it reports to the merge session in two lines, which writes its state into its paragraph of the Next steps. When its pass is done, the overview session verifies the findings against the dump and keeps its record beside them, `output-1158.16\<topic>_verification.md`; the corrections come back down through the merge session, the session records them and empties its work folder. The verified facts go into PATTERNS.md with the plan of the coding round, for the parts I choose to build, and each part's research result into ROADMAP.md.

## The tracks

The finest split the work allows: items stay in one track only where they touch the same region of a file or one needs the other's result first, and the table names which. The research is split by limit, not by part: WeaponLimits, SirenSettings and Resources each change two limits that lie in different code, so each limit is a pass of its own; PedProps has one.

| Track | Session | Owns | Joined by |
|---|---|---|---|
| Replace-rename | `CODE - REPLACE (TweaksV)` | `source\TweaksVDirectStorage.cpp` and `.h`; `test\run_directstorage.sh`, the DirectStorage options and scenarios of `test\fake_game_full.cpp`, `test\fake_dstorage.cpp`, a second program for the suite in `test\` and its lines in `test\build.cmd`; README's DirectStorage section and its `UNLOCK_FILES` row; NOTES' DirectStorage bullet, T-REPLACE, the Tests bullet's count and a line for 2.4.0 under Versions; the specification's 6.3 lines of DirectStorage, 11.8, 12, 13.1, 14.1 and 14.2's directstorage row; ROADMAP's introduction and its 2.4.0 entry; PATTERNS' DirectStorage paragraph; its paragraph of the Next steps | the hook, its suite and its documents: the suite and the documents need the code's result first, its behaviour and its log lines |
| Weapon infos | `RESEARCH - WEAPON INFOS (TweaksV)` | `weaponinfos_findings_for_the_clean_room_build.txt`, `work-weaponinfos\`, `WeaponInfos*.java` | - |
| Weapon components | `RESEARCH - WEAPON COMPONENTS (TweaksV)` | `weaponcomponents_...`, `work-weaponcomponents\`, `WeaponComponents*.java` | - |
| Siren IDs | `RESEARCH - SIREN IDS (TweaksV)` | `sirenids_...`, `work-sirenids\`, `SirenIds*.java` | - |
| Siren count | `RESEARCH - SIREN COUNT (TweaksV)` | `sirencount_...`, `work-sirencount\`, `SirenCount*.java` | - |
| Ped props | `RESEARCH - PED PROPS (TweaksV)` | `pedprops_...`, `work-pedprops\`, `PedProps*.java` | - |
| Texture budget | `RESEARCH - TEXTURE BUDGET (TweaksV)` | `texturebudget_...`, `work-texturebudget\`, `TextureBudget*.java` | - |
| Streaming memory | `RESEARCH - STREAMING MEMORY (TweaksV)` | `streamingmemory_...`, `work-streamingmemory\`, `StreamingMemory*.java` | - |

The one shared file is PLAN.md: each track has its paragraph in the Next steps, the coding track writing its own on its branch and the merge session the research tracks' on main.

## Decisions

- **D1, the round** (2026-10-28 and 2026-10-29): 2.4.0's coding track beside the research for the four new parts; the research split by limit into seven passes, the overview session's finer split, which I accepted on 2026-10-29; the parts' coding round follows once the research is verified.
- **D2, the limit on reverse-engineering the game** (2026-10-28): lifted for these research passes alone, each for its topic; verifying their findings is the overview session's.
- **D3, the clean room,** as ROADMAP.md has each original:
  - The weapon infos pass may read the code of Weapon Limits Adjuster that the MIT license covers, its `CWeaponInfoBlob` half, but no file that also holds the function adapted from FiveM's `PatchWeaponLimits.cpp`. It tells the files apart from the repository's README and its file list - names and sizes - before it opens one, and reads none where it cannot tell. The weapon components pass reads none of that repository's code.
  - SirenSetting Limit Adjuster (no license), PedProp Limit Adjuster (all rights reserved, with FiveM's pull request 2421) and Resource Adjuster (no source) are idea only: their pages, READMEs, licenses, changelogs, a pull request's description and its file list may be read, none of their code, and none of FiveM's game components.
  - Every game fact is found anew in the Enhanced dump. CREDITS.md and THIRD_PARTY_NOTICES.md follow in the coding round, for what a part takes.
- **D4, the dump:** `GameCode-start.bin` of my boot of 2026-10-29 at 10:09 - the test build of 14a5ad5 with `TEST_DUMP_CODE=60`, every part applied, no problem - the game before TweaksV patched anything, 54,388,072 bytes, the one to analyse; `GameCode-later.bin`, 60 seconds later, for `Diff.java`. Both stay in `%LOCALAPPDATA%\TweaksV` until the parts' coding round is booted.
- **D5, the replace-rename,** as ROADMAP.md describes it:
  - under `UNLOCK_FILES=1`, with no key of its own: `TweaksV.ini` keeps layout 3 and its default file;
  - `NtSetInformationFile` hooked through `HookExport` at the start-up, beside `NtCreateFile`; a hook that cannot be made is a problem of the start-up summary, part of the job as `NtCreateFile`'s is; any other information class costs one comparison;
  - turned: `FileRenameInformation` with ReplaceIfExists, and `FileRenameInformationEx` with `FILE_RENAME_REPLACE_IF_EXISTS` but without `FILE_RENAME_POSIX_SEMANTICS`, both into `FileRenameInformationEx` with replace-if-exists and POSIX semantics, the caller's other flags kept, on a copy of the caller's buffer on the stack; a name too long for the copy is passed on as made, and counted;
  - only where the target lies in the game folder, told as `NtCreateFile`'s names are - a full name compared with the game folder, a name relative to a folder handle by that folder's identity - and a bare name, a rename inside the source's own folder, by the source's final path, one query per such rename; everything else is passed on as made;
  - where the file system refuses the class or the flag - a FAT volume, a network share - the call is made again as the caller made it and its result returned, and counted; the test build stands in for such a volume with an environment variable, as `TWEAKSV_TEST_FILE_INDEX` stands in for one without file IDs;
  - inside the hook no lock and no allocation, the caller's last error kept, TweaksV's own calls left alone, every field read within its length under the guard against a bad pointer;
  - the log and the `UNLOCK_FILES` summary count the renames turned, the fallbacks and the names passed on; with `DIAGNOSTIC_MODE` one line per rename turned, 20,000 at most, as for every kind of line per event; with `DRY_RUN` one "would hook" line and nothing hooked;
  - the directstorage suite, against the stand-in's held file: inside the fake game, the classic replace, the POSIX rename, `ReplaceFile` and the write in place each go through; the classic replace from the second program is refused with error 5, as Windows refuses it to every program outside the game; a rename onto a file outside the game folder, one without replace-if-exists and one that asks for POSIX semantics itself are passed on untouched; a bare name and a name relative to a folder handle are turned; the fallback; `DRY_RUN`, `DIAGNOSTIC_MODE` and the summary's count; a hook that cannot be made;
  - README, NOTES (T-REPLACE closed), the specification and ROADMAP follow; the version is the merge session's, 2.4.0.
- **D6, what each research pass delivers:**
  - whether its limit is in Enhanced 1.0.1158.16 at all, where, what it holds and whether it still binds, against what the game's own data uses of it;
  - every site a part would change, each with a candidate pattern unique in the dump, its offset and what the match holds - in the relocation core's terms where an array moves - and the complete set checked by a scan wherever the core checks one;
  - the game facts no pattern protects, for PATTERNS.md's "When Game Updates" item 5;
  - a measurement for `DIAGNOSTIC_MODE` that shows whether a mod load reaches the limit: a part goes in only when one does, and its measurement comes first;
  - what a mod load needs, from the mods' public pages, and the old plugin's file names for the conflict check;
  - the design, answered with recommendations: the section, the key, its range and default, the mechanism, the risks.
- **D7, the versions** (2026-10-28): 2.4.0 is tagged after my boot and pushed with main, for my own use; the next version I publish is the full one, with all of the upgrades.

## The items

**The replace-rename** (`CODE - REPLACE`), each item its own commit:

1. The hook in `TweaksVDirectStorage`, as D5 has it, its log lines in the wording of the specification's 6.3.
2. The suite: the second program, the fake game's scenarios, `run_directstorage.sh`'s checks and the count in NOTES.md.
3. The documents: README.md; NOTES.md with T-REPLACE closed and a line for 2.4.0 under Versions; the specification; ROADMAP.md without its 2.4.0 entry; PATTERNS.md's DirectStorage paragraph.

Then the four builds from scratch without a warning (`-t:Rebuild`, every log searched for `: warning `) and `bash test/run_all.sh` green in its worktree, and its final report.

**Each research pass,** item after item:

1. Prepared: the local notes copied into its worktree; its findings file with its State; its work folder with the PE rebuilt from `GameCode-start.bin` and the full analysis made (`TWEAKSV_WORK=work-<topic> cmd //c run_ghidra.cmd import`, about 25 minutes beside the other passes' - meanwhile the original's pages and the byte-level tools).
2. The original and its license: what was read and what was not, with the date, as D3 allows.
3. The limit in Enhanced: there or not, where, what it holds, whether it binds.
4. The sites, their candidate patterns and the complete sets.
5. The facts no pattern protects.
6. The measurement.
7. What a mod load needs, and the old plugin's file names.
8. The design answers.
9. The hand-over: the findings' last section whole - the material for PATTERNS.md and the answers - and a final report to the merge session under 300 words.
10. After the verification: its corrections recorded in the findings, its work folder emptied.

The topics:

- **Weapon infos:** the `CWeaponInfoBlob` limit - the number of `weapons.meta` files the game loads, 512 in the original's INI: where Enhanced keeps the blobs, how many fit and every site that carries the number (the relocation core's shape if it is a static array sized in the code); whether Enhanced's limit is higher than Legacy's; what the game does past it; how many `weapons.meta` files add-on weapon packs bring. The old plugin's files: `WeaponLimitsAdjuster.asi` and `CWeaponInfoBlob Adjuster.asi`.
- **Weapon components:** the `CWeaponComponentInfo` limit - the weapon components of all `weaponcomponents.meta` files together, 1024 in the original's INI, which since its 2.1 also replaces the pool size `gameconfig.xml` gives: the array or pool, where its size comes from (the PackfileLimit part's shape if a call reads it from the configuration), every site; whether Enhanced's limit is higher; what a weapon pack needs.
- **Siren IDs:** whether Enhanced keeps the siren setting ID of `carcols.meta` at 8 bits (255; 0 and 255 reserved in the original); where the ID is stored and compared - the vehicle model's variation data, the array of siren settings - and every reference a 16-bit ID would change; the number of unique siren settings; the base game's IDs that clash above 255. The old plugin's file name, probably `SirenSetting_Limit_Adjuster.asi`.
- **Siren count:** whether Enhanced keeps 20 sirens per vehicle (`siren1` to `siren20`, `siren_glass` likewise); the siren settings' layout and every site that carries the 20; the 128 bones per vehicle, which stay; what an emergency-vehicle pack reaches first.
- **Ped props:** whether Enhanced clamps a ped's prop indices to 8 bits - 255 props per anchor - and where: the functions FiveM's pull request 2421 names by their job, found anew; whether story mode reaches 255 props per anchor at all, and what clothing packs for the MP Freemode peds add. The old plugin's file name.
- **Texture budget:** whether Enhanced, on DirectX 12, still keeps a budget for textures and assets in video memory - the Legacy game's a little over 1.5 GiB at the highest settings; where it is set and read; whether it is fixed or taken from the adapter's memory; whether raising it would do anything against texture loss; a measurement of the budget and its use.
- **Streaming memory:** whether Enhanced still keeps the memory sizes for streaming the map and props that the Legacy game had; where they are set - fixed, from `gameconfig.xml` or from the system's memory; what a heavy map load such as mine uses; how they relate to the heap HeapAdjuster sets and to `gameconfig.xml`'s pools; a measurement.

## Next steps

**State:** the first round of the full release, under way since 2026-10-29, as planned above: the merge session, started by the overview session at my go, wrote the eight briefs, which the overview session checked, and started the eight sessions at my go in its session at 10:53, each in a worktree and on a branch of its own from main at 252b090. Before it: 2.3.0, tagged on 1bc8532 and pushed, not published - the next version I publish is the full one, with all of the upgrades, and until then 2.2.1 stays the last one published, 2.2.2, 2.2.3 and 2.3.0 tags for my own use. My dump boot of 2026-10-29, 10:09 to 10:11, with the test build of 14a5ad5 and LuminanceV out: line 1 `TweaksV 2.3.0 (git 14a5ad5)`, every part applied, no PROBLEM line, no crash; both dumps saved, 54,388,072 bytes each, none unreadable. The suites stand at 609 checks on main; the open issues are T-HARDEN, T-DIAG, T-SUMMARY, T-FREEZE and T-REPLACE, which goes with 2.4.0. Above the pushed main, documents-only commits wait for 2.4.0's push.

**The replace-rename:** under way since 10:53, in a worktree and on a branch of its own; its paragraph there carries its state until the merge session rebases it onto main.

**Weapon infos:** under way since 10:53: items 1 and 2 done - the PE rebuilt from the dump (55,809,024 bytes) and the full analysis running since about 10:55; the original's license unchanged (FiveM's terms for its components function, MIT for the rest); at master both halves share one file, `dllmain.cpp`, which was not read; the one file of the original read is `CWeaponInfoBlob Adjuster/dllmain.cpp` at revision 698f729 (2021-08-14, the last before the components function; 1,582 bytes; that revision's LICENSE plain MIT of 2018, no FiveM notice), told apart by the history, the names and the sizes before it was opened - a liberty of method, which I accepted on 2026-10-29 as recommended, the overview session having checked it against the commit history and both LICENSE texts: it is D3's CWeaponInfoBlob half under MIT, without the FiveM function, and no later revision of the file is read; the coding round credits its idea in CREDITS.md, and THIRD_PARTY_NOTICES.md only if code is adapted; its idea, one capacity as an imm32 handed to the call that allocates the blob array, the game's own value + 25; the old plugin's files confirmed, `WeaponLimitsAdjuster.asi` and `CWeaponInfoBlob Adjuster.asi`, each with its INI; next item 3, the limit in the dump.

**Weapon components:** under way since 10:53: items 1 and 2 done - the PE rebuilt from the dump and the full analysis running since 10:56; read on 2026-10-29: the original's page, README, LICENSE and the repository's metadata only (MIT, 2021, alexguirre, for its own code; the CitizenFX terms for its components function, adapted from FiveM's `PatchWeaponLimits.cpp`), no source file, not even the file list, nothing of FiveM's; item 3 under way: the limit found in the dump, being written up.

**Siren IDs:** under way since 10:53: items 1 and 2 done - the PE rebuilt from the dump and the full analysis running since 10:54; the original's pages, README, license (none: idea only), changelog, file list by name and the forum's prose recorded, with what was not read; beside ROADMAP: LCPDFR carries a 2.0.4 (build 3788) and a 2.0.5 (FiveM, 3889); the 2021 technique in its author's relayed words, byte references (8-bit) replaced with word references (16-bit); a forum hint that IDs above 255 are taken modulo 255, to be checked in the dump; the plugin's file name on no official page, so my yes of 2026-10-29 to one download of the official 2.0.3 zip (21 KB, from its gta5-mods.com page) only to list its file names - into a new, empty folder of its own in its work folder, the names listed without extracting, the .asi never opened, run or disassembled, the zip deleted afterwards; the conflict check takes the names the zip holds, both known spellings until then; item 3 under way, the limit in the dump.

**Siren count:** under way since 10:53: items 1 and 2 done - the PE rebuilt from the dump (55,809,024 bytes) and the full analysis running since about 10:56; the original has no license (no file, its README asks for links, not redistribution), and its page, README, file list by name, commit messages, LCPDFR's page (2.0.5, Legacy only) and the CitizenFX thread were read, no code; its file name one of two, settled by the siren IDs pass's listing of the official zip, which I allowed on 2026-10-29; item 3 under way: Enhanced keeps `siren1` to `siren20` and `siren_glass1` to `siren_glass20`, and the parser's tables in .rdata give a siren setting of 0x5E8 bytes with 20 lights of 0x48 bytes at +0x3C - being checked in the code.

**Ped props:** under way since 10:53: item 1 done - the PE rebuilt from the dump (55,809,024 bytes, PE timestamp 0x6AA45F10), the full analysis running since 10:54; next item 2, the original's pages and license as D3 allows, beside the byte-level tools on the dump.

**Texture budget:** under way since 10:53: item 1 done - the PE rebuilt from the dump (55,809,024 bytes), the full analysis running since 10:54, slower than 25 minutes beside six others; next item 2, the original's pages as D3 allows, beside the byte-level tools on the dump.

**Streaming memory:** under way since 10:53: items 1 and 2 done - the PE rebuilt from the dump and the full analysis running since 10:54; the original's page read on 2026-10-29: Resource Adjuster 1.0 (zombieguy, 2022-12-21), no license and no source, idea only, its files `GTAV.ResourceAdjuster.asi` and `ResourceAdjuster.ini`; the Enhanced port's page shows its file withdrawn, without a description; item 3 under way: Enhanced keeps two streaming budgets - an allocator at RVA 0x42F9E10, set by 0xCE5E90 from two sizes in MB of the game's configuration beside the archive count PackfileLimit raises, each capped to a resource heap's size - next the heaps' sizes and whether the budgets bind.

**The verification:** none yet.

**Decisions waiting on me:** none - the two points of 2026-10-29, the weapon infos pass's liberty and the SirenSettings original's file name, settled that day as recommended.

**Boots:** 2.4.0's, once the merge session reports its build - the Profiles folder (`Documents\Rockstar Games\GTAV Enhanced\Profiles`) backed up first, since the hook sees every rename the game's process makes, its saves' too; the Release build; its log's line 1 `TweaksV 2.4.0 (git <hash>)` without `-dirty`, every part applied, no PROBLEM line, and the line that says `NtSetInformationFile` is hooked. The research needs no boot.
