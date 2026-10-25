#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# DirectStorage (TweaksVDirectStorage) scenarios: the fake GTA5_Enhanced.exe loads the real
# TweaksV.asi. With "dstorage" it loads a stand-in dstorage.dll first and calls it afterwards,
# including the factory's OpenFile for 19 paths: 4 archives and 7 other files in its folder, 7 paths
# outside it and one too long to check (fake_game_full.cpp lists them); with "fileopens" it opens files
# in its folder; with "wait" it stays 11 s longer, with "waitlong" 31 s. Fake LOCALAPPDATA: the real one
# is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
# the game that never asks for its factory runs the test build: with ERROR_POPUP=1 the problem gets a message
# box of its own after the start-up summary, and only a test build keeps that box off the screen
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata
rm -f DirectStorageFix.asi	# an old plugin another suite may have left

FACTORY='the game asked for the DirectStorage factory (call #1, interface {6924EA0C-C3CD-4826-B10A-F64F4ED927C1}) -> ok, from RVA 0x'
OTHER='{12345678-1234-5678-9ABC-DEF012345678}'
# the 19 OpenFile results: in the game folder 4 archives (x64a.rpf, a full path, rpf.cache, y.CACHE) and 6 other
# files (locktest.txt, TweaksV.asi, noextension, ds_dir/slash.txt, a \\?\ path, x.rpf.txt); outside it a save,
# a temp file, a \\?\UNC\ path, a sibling folder whose name starts with the game folder's, a path out of it
# through "..", an archive in %TEMP% and a \\?\unc\ path in lower case; a path too long to check; and in the
# game folder other.manifest, whose extension is too long for the table of refused types
UNLOCKED='open: rpf 0x0, RPF 0x0, cache 0x0, txt 0x80070002, asi 0x80070002, none 0x80070002, save 0x0, temp 0x0, unc 0x0, slash 0x80070002, prefixed 0x80070002, sibling 0x0, dotdot 0x0, rpftxt 0x80070002, CACHE 0x0, outrpf 0x0, unc2 0x0, long 0x0, manifest 0x80070002, reached 12 |'
ALL_OPEN='open: rpf 0x0, RPF 0x0, cache 0x0, txt 0x0, asi 0x0, none 0x0, save 0x0, temp 0x0, unc 0x0, slash 0x0, prefixed 0x0, sibling 0x0, dotdot 0x0, rpftxt 0x0, CACHE 0x0, outrpf 0x0, unc2 0x0, long 0x0, manifest 0x0, reached 19 |'

echo "== D1 defaults (UNLOCK_FILES=1, DIAGNOSTIC_MODE=0): in the game folder only .rpf and .cache files reach DirectStorage"
fresh_ini; out=$(game full_game.exe dstorage fileopens); echo "      $out"
check "header: DirectStorage on, its line holds UNLOCK_FILES=1 and nothing else" "log_has 'Features: .*, DirectStorage on' && log_line 'DirectStorage: UNLOCK_FILES=1\$'"
check "the factory hooked, then OpenFile" "log_has 'UNLOCK_FILES: when the game creates its DirectStorage factory, OpenFile is hooked so that in the game folder only .rpf and .cache files can be opened.' && log_has 'UNLOCK_FILES: from now on DirectStorage may open only the .rpf and .cache files in the game folder (OpenFile at dstorage.dll+0x[0-9A-F]*.* hooked); files outside it are not touched.'"
check "3 archives opened (also .RPF and a full path), 3 other files in the game folder refused as not found, never reaching DirectStorage" "has 'open: rpf 0x0, RPF 0x0, cache 0x0, txt 0x80070002, asi 0x80070002, none 0x80070002, '"
check "decided on the full path: '/' separators, a \\\\?\\ prefix and x.rpf.txt refused, .CACHE opened; a sibling folder whose name starts with the game folder's and a path out of it through '..' are outside" "has 'slash 0x80070002, prefixed 0x80070002, sibling 0x0, dotdot 0x0, rpftxt 0x80070002, CACHE 0x0, '"
check "a save, temp files (an archive too), network paths (\\\\?\\UNC\\ and \\\\?\\unc\\) outside the game folder and a path too long to check still reach DirectStorage" "has '$UNLOCKED'"
check "no call or open lines without DIAGNOSTIC_MODE, no problem" "log_lacks 'OpenFile #' && log_lacks 'the game asked for the DirectStorage factory' && log_lacks 'open #' && log_lacks 'PROBLEM'"
check "the game runs on" "has 'dstorage: configuration 0x0/0x0, factory 0x0 set' && has 'file opens: missing file error 2' && has 'protect: all restored'"

echo "== D1b UNLOCK_FILES=0 and DIAGNOSTIC_MODE=0: nothing hooked"
fresh_ini; setkey UNLOCK_FILES 0 DIRECTSTORAGE; out=$(game full_game.exe dstorage fileopens)
check "nothing to do, said once" "[ \$(count 'UNLOCK_FILES=0 and DIAGNOSTIC_MODE=0 - nothing to do.') -eq 1 ]"
check "every file reaches DirectStorage, the game runs on" "has '$ALL_OPEN' && has 'file opens: missing file error 2' && has 'protect: all restored'"

echo "== D2 the section is missing from an older TweaksV.ini: its two keys are added, with their defaults"
fresh_ini; sed -b -i '/^\[DIRECTSTORAGE\]\r$/,$d' TweaksV.ini; out=$(game full_game.exe dstorage)
check "the keys added" "log_has 'added the missing key \[DIRECTSTORAGE\] ENABLED=1' && log_has 'added the missing key \[DIRECTSTORAGE\] UNLOCK_FILES=1' && [ \$(count 'added the missing key') -eq 2 ]"
check "and in the file" "grep -q '^\[DIRECTSTORAGE\]' TweaksV.ini && grep -q '^UNLOCK_FILES=1' TweaksV.ini"
check "and used: unlocked" "has '$UNLOCKED'"

echo "== D3 UNLOCK_FILES=1 and DIAGNOSTIC_MODE=1: the game's calls into DirectStorage, OpenFile included, the factory's functions and the configuration values"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage); echo "      $out"
check "versions and load state" "log_has 'dstorage.dll unknown version, dstoragecore.dll not found; dstorage.dll is loaded.'"
check "3 of 3 functions hooked" "log_has 'DIAGNOSTIC: the game.s DirectStorage calls are logged (3 of 3 functions hooked).'"
check "both configuration calls, from the game" "log_has 'the game set the DirectStorage configuration (DStorageSetConfiguration) -> ok, from RVA 0x' && log_has 'the game set the DirectStorage configuration (DStorageSetConfiguration1) -> ok, from RVA 0x'"
check "the factory call with its interface, from the game" "log_has '$FACTORY'"
check "the archives ok, the first one with its caller" "log_has 'OpenFile #1: x64a.rpf -> ok, from RVA 0x' && log_has 'OpenFile #2: update.update.RPF -> ok' && log_has 'OpenFile #3: rpf.cache -> ok' && log_has 'OpenFile #15: y.CACHE -> ok'"
check "the other files in the game folder refused, named by their full path below it" "log_has 'OpenFile #4: locktest.txt -> refused (in the game folder, not .rpf or .cache)' && log_has 'OpenFile #5: TweaksV.asi -> refused' && log_has 'OpenFile #6: noextension -> refused' && log_has 'OpenFile #10: ds_dir.slash.txt -> refused' && log_has 'OpenFile #11: prefixed.txt -> refused' && log_has 'OpenFile #14: x.rpf.txt -> refused' && log_has 'OpenFile #19: other.manifest -> refused'"
check "paths outside the game folder ok, each logged as a category and its file name only" "log_has 'OpenFile #7: (outside the game folder) SGTA50000 -> ok' && log_has 'OpenFile #8: (temp) cloud_tweaksv.dat -> ok' && log_has 'OpenFile #12: (outside the game folder) sibling.txt -> ok' && log_has 'OpenFile #13: (outside the game folder) outside.txt -> ok' && log_has 'OpenFile #16: (temp) outside.rpf -> ok'"
check "a path in the \\\\?\\UNC\\ form, in either case, is the network path it names, outside the game folder: opened, not refused" "log_has 'OpenFile #9: (network) cloud.dat -> ok' && log_has 'OpenFile #17: (network) cloud2.dat -> ok'"
check "a path too long to check: passed on, logged as such; no folder outside the game folder in the log" "log_has 'OpenFile #18: (unreadable path) -> ok' && log_lacks 'tweaksv_profile' && log_lacks 'tweaksv_share' && log_lacks 'TestTemp'"
check "unlocked as without DIAGNOSTIC_MODE" "has '$UNLOCKED'"
check "8 functions, all in dstorage.dll" "log_has 'DIAGNOSTIC: the factory.s first 8 functions: \[0\] dstorage.dll+0x[0-9A-F]* .*\[7\] dstorage.dll+0x'"
check "7 and 8 configuration values" "log_line 'DIAGNOSTIC: DStorageSetConfiguration values: 00000004 FFFFFFFF 00000000 00000001 00000000 00000000 00000001\$' && log_has 'DIAGNOSTIC: DStorageSetConfiguration1 values: 00000004 FFFFFFFF 00000000 00000001 00000000 00000000 00000001 00000001'"

echo "== D3b UNLOCK_FILES=0 and DIAGNOSTIC_MODE=1: OpenFile only logged"
fresh_ini; setkey UNLOCK_FILES 0 DIRECTSTORAGE; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage)
check "OpenFile hooked for the log only" "log_has 'DIAGNOSTIC: the game.s OpenFile calls are logged (OpenFile at dstorage.dll+0x[0-9A-F]*.* hooked); UNLOCK_FILES=0, so every file is opened.'"
check "every file opened, and logged as ok" "has '$ALL_OPEN' && log_has 'OpenFile #4: locktest.txt -> ok'"
check "no UNLOCK_FILES lines" "log_lacks 'UNLOCK_FILES:'"

echo "== D4 dstorage.dll not loaded: said so, no problem"
fresh_ini; setkey UNLOCK_FILES 0 DIRECTSTORAGE; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "DIAGNOSTIC_MODE: not loaded, nothing to log" "log_has 'dstorage.dll is not loaded.' && log_has 'dstorage.dll is not loaded, so the game does not use DirectStorage here: there is nothing to log.'"
fresh_ini; out=$(game full_game.exe)
check "UNLOCK_FILES: not loaded, nothing to unlock" "log_has 'dstorage.dll is not loaded, so the game does not use DirectStorage here: there is nothing to unlock.'"
check "no problem, game runs on" "log_lacks 'PROBLEM' && has 'protect: all restored'"

echo "== D5 DIAGNOSTIC_MODE=1: the opens inside the game folder"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe fileopens); echo "      $out"
check "hooked, the game folder named" "log_has 'DIAGNOSTIC: every open of a file or folder in .*TestRun is logged, 2 lines per file, caller and sharing'"
check "a created file: share R--, overwrite-if, from the game's code" "log_has 'open #[0-9]*: ds_test.txt | ok | access 0x[0-9A-F]*, share R--, overwrite-if, options 0x[0-9A-F]* | from RVA 0x[0-9A-F]* <- RVA 0x'"
check "6 opens the same way: 2 lines" "[ \$(count ': ds_test.txt | .*share R--') -eq 2 ] && log_has 'open #[0-9]*: ds_test.txt | ok | access 0x[0-9A-F]*, share R--, open, '"
check "a missing file: not found, and the game still gets ERROR_FILE_NOT_FOUND" "log_has ': ds_missing.txt | 0xC0000034 not found | ' && has 'file opens: missing file error 2'"
check "the game folder itself and a file in a subfolder" "log_has 'open #[0-9]*: \. | ok | access 0x[0-9A-F]*, share RWD, open, ' && log_has ': ds_sub.inner.rpf | ok | access 0x[0-9A-F]*, share RW-, overwrite-if'"
check "a file outside the game folder: not logged" "log_lacks 'tweaksv_ds_outside'"
check "the game runs on" "has 'protect: all restored' && log_lacks 'PROBLEM'"

echo "== D6 everything on, with dstorage.dll and the file opens"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage fileopens)
check "calls, OpenFile and opens all logged" "log_has '$FACTORY' && log_has 'OpenFile #4: locktest.txt -> refused' && log_has ': ds_test.txt | ok | '"
check "unlocked, the game runs on" "has '$UNLOCKED' && has 'file opens: missing file error 2' && has 'protect: all restored'"

echo "== D7 DIAGNOSTIC_MODE=1 and DRY_RUN=1: nothing hooked, nothing refused, nothing logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe dstorage fileopens)
check "three 'would' lines" "log_has 'DRY_RUN: would let DirectStorage open only .rpf and .cache files (UNLOCK_FILES).' && log_has 'DRY_RUN: would hook dstorage.dll to log the game.s DirectStorage calls.' && log_has 'DRY_RUN: would hook NtCreateFile to log the opens in the game folder.'"
check "every file opened, nothing logged" "has '$ALL_OPEN' && log_lacks 'OpenFile #' && log_lacks 'the game asked for the DirectStorage factory' && log_lacks 'open #'"

echo "== D8 [DIRECTSTORAGE] ENABLED=0 with DIAGNOSTIC_MODE=1: nothing at all - a part that is off dumps nothing"
fresh_ini; setkey ENABLED 0 DIRECTSTORAGE; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage fileopens)
check "header: DirectStorage off; no DirectStorage lines" "log_has 'Features: .*, DirectStorage off' && log_lacks '\[DirectStorage\]'"
check "every file opened" "has '$ALL_OPEN'"

echo "== D9 alloc8or's DirectStorageFix.asi in the game folder: an old plugin, this part is off"
fresh_ini; touch DirectStorageFix.asi; out=$(game full_game.exe dstorage); rm -f DirectStorageFix.asi
check "reported as an old plugin" "log_has 'PROBLEM: Old plugin found: .*DirectStorageFix.asi. TweaksV.s DirectStorage part is switched off for this session'"
check "nothing hooked, every file opened" "log_lacks '\[DirectStorage\]' && has '$ALL_OPEN'"

echo "== D9b the same with CONFLICT_CHECK=0: noted, unlocked anyway"
fresh_ini; setkey CONFLICT_CHECK 0; touch DirectStorageFix.asi; out=$(game full_game.exe dstorage); rm -f DirectStorageFix.asi
check "noted" "log_has 'Note: .*DirectStorageFix.asi is installed as well - it changes DirectStorage.s file opens, too.'"
check "unlocked" "has '$UNLOCKED'"

echo "== D10 the summary 10 seconds after the first OpenFile: exclusive counts that add up, the refused files by type"
fresh_ini; out=$(game full_game.exe dstorage wait)
check "UNLOCK_FILES=1: 19 asked, 4 archives opened, 7 refused (.txt 4, .asi 1, (none) 1, a type too long for the table among others), 7 outside opened (the archive outside counted once), 1 unreadable passed on" "log_has 'UNLOCK_FILES: in its first 10 seconds the game asked DirectStorage for 19 files: 4 .rpf and .cache files were opened, 7 other files in the game folder refused, so they stay free for other programs, 7 outside the game folder opened as usual, 1 with an unreadable path passed on; refused by type: .txt 4, .asi 1, (none) 1, others 1.'"
fresh_ini; setkey UNLOCK_FILES 0 DIRECTSTORAGE; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage wait)
check "UNLOCK_FILES=0: 19 asked, all opened" "log_has 'OpenFile: in its first 10 seconds the game asked DirectStorage for 19 files: 4 .rpf and .cache files, 7 other files in the game folder and 7 outside it, 1 with an unreadable path passed on (UNLOCK_FILES=0 - all of them opened).'"

echo "== D11 the game first asks for an interface TweaksV does not know, then for the known one: 10 seconds later a line, no problem"
fresh_ini; out=$(game full_game.exe dstorage dsotherinterface wait)
check "the other interface logged once the check found OpenFile hooked; no problem" "[ \$(count 'The game also asked for another DirectStorage interface, $OTHER, whose functions TweaksV does not know - left as it is.') -eq 1 ] && log_lacks 'PROBLEM'"
check "the factory TweaksV knows is unlocked" "has '$UNLOCKED'"

echo "== D11b the game asks for another interface after the known one: a line at once, no problem"
fresh_ini; out=$(game full_game.exe dstorage dsotherlater)
check "logged once, no problem; unlocked" "[ \$(count 'The game also asked for another DirectStorage interface, $OTHER, whose functions TweaksV does not know - left as it is.') -eq 1 ] && log_lacks 'PROBLEM' && has '$UNLOCKED'"

echo "== D11c the game asks only for an interface TweaksV does not know: 10 seconds later a problem after the start-up summary"
fresh_ini; out=$(game full_game.exe dstorage dsotheronly wait)
check "reported, and given a message box of its own after the start-up summary (here ERROR_POPUP=0)" "has 'other interface only 0x0 |' && log_has 'PROBLEM: The files in the game folder are not unlocked this session: the game asked only for another DirectStorage interface, $OTHER, whose functions TweaksV does not know.' && log_has 'Problem after the start-up summary not shown (ERROR_POPUP=0): 1 problem(s)' && log_lacks 'also asked'"

echo "== D12 the game never asks for its DirectStorage factory: 30 seconds in, a problem after the start-up summary, with a box of its own (the test build, ERROR_POPUP=1)"
fresh_ini; setkey ERROR_POPUP 1; out=$(ASI=TweaksV_test.asi game full_game.exe dstorage dsnofactory waitlong)
check "reported once: nothing is unlocked, TweaksV may have been loaded too late" "has 'dstorage: not called |' && [ \$(count 'PROBLEM') -eq 1 ] && log_has 'PROBLEM: The files in the game folder are not unlocked this session: the game has not asked for its DirectStorage factory since TweaksV started - TweaksV may have been loaded too late.'"
check "after the start-up summary, so in a message box of its own (which the test build keeps off the screen)" "log_has 'Showing a problem after the start-up summary (1 problem(s)).' && log_has 'TEST BUILD: message box not shown (TWEAKSV_TEST_NO_BOXES=1): TweaksV: a part stopped working after the start-up:'"

echo "== D13 20,001 refusals after the summary, 31 seconds in: the lines stop at the cap; the factory was asked for, so its check reports nothing"
fresh_ini; out=$(game full_game.exe dstorage dsflood waitlong)
check "the summary, then 20000 lines of late refusals and the cap line once" "has 'flood: 20001 refused |' && log_has 'UNLOCK_FILES: in its first 10 seconds' && [ \$(count 'UNLOCK_FILES: refused locktest.txt (in the game folder, not .rpf or .cache).') -eq 20000 ] && [ \$(count 'UNLOCK_FILES: 20000 lines of files refused after the summary written - from now on they are only counted.') -eq 1 ]"
check "the factory check says nothing when the factory was asked for" "log_lacks 'has not asked for its DirectStorage factory' && log_lacks 'PROBLEM'"

echo "== D14 DIAGNOSTIC_MODE=1 with 20,020 OpenFile calls: the lines stop at the cap, the calls still refused"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage dsflood)
check "20000 OpenFile lines, then the cap line once" "has 'flood: 20001 refused |' && [ \$(count 'OpenFile #') -eq 20000 ] && [ \$(count 'DIAGNOSTIC: 20000 lines of OpenFile calls written - from now on they are only counted.') -eq 1 ]"

echo "== D15 DIAGNOSTIC_MODE=1 with 100,001 opens of files that are not there: the open lines and the summary's tables stop at their limits"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dsopenflood)
check "20000 open lines, then the cap line once; a table full at 100000 entries, said once" "[ \$(count 'DirectStorage\] open #') -eq 20000 ] && log_has 'open #[0-9]*: dsflood_000000.txt | 0xC0000034 not found' && [ \$(count 'DIAGNOSTIC: 20000 open lines written - from now on the opens are only counted (see the summaries).') -eq 1 ] && [ \$(count 'DIAGNOSTIC: a table of the open summary is full (100000 entries) - the opens it cannot take are only counted (see the summaries).') -eq 1 ]"
check "the game runs on" "has 'protect: all restored' && log_lacks 'PROBLEM'"

echo
echo "DIRECTSTORAGE SCENARIOS: $pass passed, $fail failed"
