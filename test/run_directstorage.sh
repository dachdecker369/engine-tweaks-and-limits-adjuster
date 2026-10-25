#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# DirectStorage (TweaksVDirectStorage) scenarios: the fake GTA5_Enhanced.exe loads the real
# TweaksV.asi. With "dstorage" it loads a stand-in dstorage.dll first, which opens its files for real and
# holds them open as DirectStorage does, and calls it afterwards, including the factory's OpenFile for 19
# paths: 4 archives and 7 other files in its folder, 7 paths outside it and one too long to check
# (fake_game_full.cpp lists them); with "dsshare" other programs open the files the stand-in holds, with
# "ownshare" the files the fake itself holds; with "fileopens" it opens files in its folder; with "wait" it
# stays 11 s longer, with "waitlong" 31 s. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
# the file system without file IDs is the test build's (TWEAKSV_TEST_FILE_INDEX), the only build that reads that switch
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata
rm -f DirectStorageFix.asi	# an old plugin another suite may have left

FACTORY='the game asked for the DirectStorage factory (call #1, interface {6924EA0C-C3CD-4826-B10A-F64F4ED927C1}) -> ok, from RVA 0x'
OTHER='{12345678-1234-5678-9ABC-DEF012345678}'
HOOKED='UNLOCK_FILES: NtCreateFile is hooked - a read-only open of a file in the game folder other than .rpf and .cache that shares reading gets write and delete sharing too, so those files stay free for other programs.'
# the 19 OpenFile results, every call reaching the stand-in: in the game folder 4 archives (x64a.rpf, a full path,
# rpf.cache, y.CACHE) and 7 other files (locktest.txt, TweaksV.asi, noextension, ds_dir/slash.txt, a \\?\ path,
# x.rpf.txt, other.manifest), all there and opened; outside it a save, a temp file, a \\?\UNC\ path, a sibling
# folder whose name starts with the game folder's, a path out of it through "..", an archive in %TEMP% and a
# \\?\unc\ path in lower case, none of them there (the stand-in opens no network path); a path too long to open
OPENED='open: rpf 0x0, RPF 0x0, cache 0x0, txt 0x0, asi 0x0, none 0x0, save 0x80070003, temp 0x80070002, unc 0x80070035, slash 0x0, prefixed 0x0, sibling 0x80070003, dotdot 0x80070002, rpftxt 0x0, CACHE 0x0, outrpf 0x80070002, unc2 0x80070035, long 0x80070003, manifest 0x0, reached 19 |'
# while the stand-in holds each file, the error of another program's open (0: it succeeded): a text file by a
# relative path - written while sharing reading, renamed, written while sharing nothing (32, the sharing
# violation: the limit) - and by its full path; an archive; a loose file in a subfolder (its bytes read through
# the stand-in's handle) and the same by a \\?\ path; a file in %TEMP%; a file reached through ".."
SHARED='share: txt 0x0 write 0 rename 0 write-exclusive 32, full 0x0 write 0, rpf 0x0 write 32, loose 0x0 bytes ok write 0, prefixed 0x0 write 0, temp 0x0 write 32, up 0x0 write 32 |'
LOCKED='share: txt 0x0 write 32 rename 32 write-exclusive 32, full 0x0 write 32, rpf 0x0 write 32, loose 0x0 bytes ok write 32, prefixed 0x0 write 32, temp 0x0 write 32, up 0x0 write 32 |'
# next to the fake's own opens: a read-only open sharing reading (written while sharing reading), an exclusive
# read-only open (opened again), a folder opened as one (renamed), a name relative to another folder's handle
# (written while sharing reading), a write open (written again)
OWN_SHARED='own: widened write 0, exclusive 32, folder rename 32, rooted write 32, write-open 32 |'
OWN_LOCKED='own: widened write 32, exclusive 32, folder rename 32, rooted write 32, write-open 32 |'

echo "== D1 defaults (UNLOCK_FILES=1, DIAGNOSTIC_MODE=0): the files the stand-in holds in the game folder stay free, archives and files outside it do not"
fresh_ini; out=$(game full_game.exe dstorage dsshare); echo "      $out"
check "header: DirectStorage on, its line holds UNLOCK_FILES=1 and nothing else" "log_has 'Features: .*, DirectStorage on' && log_line 'DirectStorage: UNLOCK_FILES=1\$'"
check "NtCreateFile hooked at the start-up, said once" "[ \$(count '$HOOKED') -eq 1 ]"
check "a text file the stand-in holds by a relative path: written while sharing reading and renamed, both succeed" "has 'share: txt 0x0 write 0 rename 0 '"
check "the limit: written while sharing nothing, the sharing violation" "has 'write-exclusive 32, '"
check "the same file by its full path: shared" "has 'full 0x0 write 0, '"
check "an archive the stand-in holds: a write open fails as before" "has 'rpf 0x0 write 32, '"
check "a loose file in a subfolder, as a mods loader serves it: opened, its bytes read through the stand-in's handle, shared" "has 'loose 0x0 bytes ok write 0, '"
check "the same loose file by a \\\\?\\ path: shared" "has 'prefixed 0x0 write 0, '"
check "a file in %TEMP% and one reached through '..', both outside the game folder: not shared" "has 'temp 0x0 write 32, up 0x0 write 32 |'"
check "nothing refused: all 19 OpenFile calls reach the stand-in, every file in the game folder opened" "has '$OPENED'"
check "no call, OpenFile or open lines without DIAGNOSTIC_MODE, no problem" "log_lacks 'OpenFile' && log_lacks 'the game asked for the DirectStorage factory' && log_lacks 'open #' && log_lacks 'PROBLEM'"
check "the game runs on" "has 'dstorage: configuration 0x0/0x0, factory 0x0 set' && has 'protect: all restored'"

echo "== D2 dstorage.dll not loaded: NtCreateFile is hooked all the same; of the fake's own opens in the game folder only the read-only one that shares reading is widened"
fresh_ini; out=$(game full_game.exe ownshare); echo "      $out"
check "versions and load state, then the hook, nothing said about unlocking nothing" "log_has 'dstorage.dll unknown version, dstoragecore.dll not found; dstorage.dll is not loaded.' && log_has '$HOOKED' && log_lacks 'nothing to unlock' && log_lacks 'nothing to log'"
check "a read-only open that shares reading: written while it is held, sharing reading - freed whoever opens the file" "has 'own: widened write 0, '"
check "an exclusive read-only open stays exclusive: a second open fails" "has 'exclusive 32, '"
check "a folder opened as one (FILE_DIRECTORY_FILE) is left as it is: it cannot be renamed while held" "has 'folder rename 32, '"
check "a name relative to another folder's handle is left as it is: a write open fails" "has 'rooted write 32, '"
check "a write open is left as it is: a second write open fails" "has 'write-open 32 |'"
check "no problem, the game runs on" "log_lacks 'PROBLEM' && has 'protect: all restored'"

echo "== D3 the summary 10 seconds after the first open counted: the opens widened, their files by type, the rest by reason, adding up (RageOpenV off, so only the fake's opens count)"
fresh_ini; setkey ENABLED 0 RAGEOPENV; out=$(game full_game.exe ownshare dstypes wait)
check "19 opens of 19 files widened, .txt 2 and 14 more types listed, 3 as others (2 types past the table, 1 too long for it); 0 archives, 8 write opens, 1 exclusive, 3 folders, 1 relative to another folder" "log_has 'UNLOCK_FILES: in its first 10 seconds, 19 opens of 19 files in the game folder other than .rpf and .cache were given write and delete sharing, so those files stay free for other programs; by type: .txt 2, .e01 1, .e02 1, .e03 1, .e04 1, .e05 1, .e06 1, .e07 1, .e08 1, .e09 1, .e10 1, .e11 1, .e12 1, .e13 1, .e14 1, others 3; left as they were: 0 opens of archives, 8 write opens, 1 exclusive opens, 3 directories, 1 whose path could not be examined.'"
check "one summary in the first minute, no line per open, no problem" "[ \$(count 'UNLOCK_FILES: ') -eq 2 ] && log_lacks 'open #' && log_lacks 'PROBLEM'"

echo "== D3b 100,001 opens of files that are not there, each by another name: every call counted, the files told apart up to 8192"
fresh_ini; out=$(game full_game.exe dsopenflood wait)
check "the opens of more than 8192 files, .txt 8192 by type" "log_has 'UNLOCK_FILES: in its first 10 seconds, [0-9]* opens of more than 8192 files in the game folder other than .rpf and .cache were given write and delete sharing, so those files stay free for other programs; by type: .txt 8192; left as they were: '"
check "all 100,001 in it, with RageOpenV's two looks at the game folder; no line per open, no problem, the game runs on" "log_has 'in its first 10 seconds, 100001 opens of more than 8192 files .* 0 write opens, 0 exclusive opens, 2 directories, 0 whose path' && log_lacks 'open #' && log_lacks 'PROBLEM' && has 'protect: all restored'"

echo "== D4 everything on, with DIAGNOSTIC_MODE=1: the calls into DirectStorage, OpenFile passed on, the open lines with the sharing given"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage dsshare ownshare)
check "versions and load state" "log_has 'dstorage.dll unknown version, dstoragecore.dll not found; dstorage.dll is loaded.'"
check "3 of 3 functions hooked, NtCreateFile for both jobs" "log_has 'DIAGNOSTIC: the game.s DirectStorage calls are logged (3 of 3 functions hooked).' && log_has '$HOOKED' && log_has 'DIAGNOSTIC: every open of a file or folder in .*TestRun is logged, 2 lines per file, caller and sharing'"
check "both configuration calls, from the game, with 7 and 8 values" "log_has 'the game set the DirectStorage configuration (DStorageSetConfiguration) -> ok, from RVA 0x' && log_has 'the game set the DirectStorage configuration (DStorageSetConfiguration1) -> ok, from RVA 0x' && log_line 'DIAGNOSTIC: DStorageSetConfiguration values: 00000004 FFFFFFFF 00000000 00000001 00000000 00000000 00000001\$' && log_has 'DIAGNOSTIC: DStorageSetConfiguration1 values: 00000004 FFFFFFFF 00000000 00000001 00000000 00000000 00000001 00000001'"
check "the factory call with its interface, its 8 functions, all in dstorage.dll, and OpenFile hooked for the log" "log_has '$FACTORY' && log_has 'DIAGNOSTIC: the factory.s first 8 functions: \[0\] dstorage.dll+0x[0-9A-F]* .*\[7\] dstorage.dll+0x' && log_line 'DIAGNOSTIC: the game.s OpenFile calls are logged (OpenFile at dstorage.dll+0x[0-9A-F]* (function +0x[0-9A-F]*) hooked).\$'"
check "the files in the game folder named by their path below it, every result passed on, the first with its caller" "log_has 'OpenFile #1: x64a.rpf -> ok, from RVA 0x' && log_has 'OpenFile #2: ds_update.update.RPF -> ok' && log_has 'OpenFile #4: locktest.txt -> ok' && log_has 'OpenFile #6: noextension -> ok' && log_has 'OpenFile #10: ds_dir.slash.txt -> ok' && log_has 'OpenFile #11: prefixed.txt -> ok' && log_has 'OpenFile #14: x.rpf.txt -> ok' && log_has 'OpenFile #19: other.manifest -> ok'"
check "paths outside the game folder logged as a category and a file name only, with what the stand-in answered" "log_has 'OpenFile #7: (outside the game folder) SGTA50000 -> 0x80070003' && log_has 'OpenFile #8: (temp) cloud_tweaksv.dat -> 0x80070002' && log_has 'OpenFile #12: (outside the game folder) sibling.txt -> 0x80070003' && log_has 'OpenFile #13: (outside the game folder) outside.txt -> 0x80070002' && log_has 'OpenFile #16: (temp) outside.rpf -> 0x80070002'"
check "a path in the \\\\?\\UNC\\ form, in either case, is the network path it names; a path too long to check is (unreadable path); no folder outside the game folder in the log" "log_has 'OpenFile #9: (network) cloud.dat -> 0x80070035' && log_has 'OpenFile #17: (network) cloud2.dat -> 0x80070035' && log_has 'OpenFile #18: (unreadable path) -> ' && log_lacks 'tweaksv_profile' && log_lacks 'tweaksv_share' && log_lacks 'TestTemp'"
check "the stand-in's open of a text file: its open line says it was widened" "log_has 'open #[0-9]*: ds_lock.txt | ok | access 0x80100080, share R-- widened to RWD, open, options 0x00000060 | from dstorage.dll+0x'"
check "the stand-in's open of an archive, the fake's exclusive open and its open relative to another folder: their lines say nothing of it" "log_has 'open #[0-9]*: ds_lock.rpf | ok | access 0x80100080, share R--, open, options 0x00000060 | from dstorage.dll+0x' && log_has 'open #[0-9]*: ds_excl.txt | ok | access 0x80100080, share ---, open, ' && log_has 'open #[0-9]*: ds_own_dir.inner.txt | ok | access 0x80100000, share R--, open, '"
check "freed as without DIAGNOSTIC_MODE, every call passed on, no problem" "has '$SHARED' && has '$OWN_SHARED' && has '$OPENED' && log_lacks 'PROBLEM'"

echo "== D5 UNLOCK_FILES=0 and DIAGNOSTIC_MODE=1: NtCreateFile hooked for the log only, nothing widened"
fresh_ini; setkey UNLOCK_FILES 0 DIRECTSTORAGE; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage dsshare ownshare)
check "no UNLOCK_FILES lines, no open line widened" "log_lacks 'UNLOCK_FILES:' && log_has 'open #[0-9]*: ds_lock.txt | ok | access 0x80100080, share R--, open, ' && log_lacks 'widened'"
check "the files stay locked; OpenFile logged, every call reaching the stand-in" "has '$LOCKED' && has '$OWN_LOCKED' && has '$OPENED' && log_has 'OpenFile #4: locktest.txt -> ok'"

echo "== D6 UNLOCK_FILES=0 and DIAGNOSTIC_MODE=0: nothing hooked"
fresh_ini; setkey UNLOCK_FILES 0 DIRECTSTORAGE; out=$(game full_game.exe dstorage dsshare ownshare)
check "nothing to do, said once" "[ \$(count 'UNLOCK_FILES=0 and DIAGNOSTIC_MODE=0 - nothing to do.') -eq 1 ] && log_lacks 'NtCreateFile'"
check "the files stay locked, every call reaches the stand-in, the game runs on" "has '$LOCKED' && has '$OWN_LOCKED' && has '$OPENED' && has 'protect: all restored'"

echo "== D7 the section is missing from an older TweaksV.ini: its two keys are added, with their defaults"
fresh_ini; sed -b -i '/^\[DIRECTSTORAGE\]\r$/,$d' TweaksV.ini; out=$(game full_game.exe ownshare)
check "the keys added" "log_has 'added the missing key \[DIRECTSTORAGE\] ENABLED=1' && log_has 'added the missing key \[DIRECTSTORAGE\] UNLOCK_FILES=1' && [ \$(count 'added the missing key') -eq 2 ]"
check "and in the file" "grep -q '^\[DIRECTSTORAGE\]' TweaksV.ini && grep -q '^UNLOCK_FILES=1' TweaksV.ini"
check "and used: freed" "has '$OWN_SHARED'"

echo "== D8 DIAGNOSTIC_MODE=1 with UNLOCK_FILES=0: the opens inside the game folder"
fresh_ini; setkey UNLOCK_FILES 0 DIRECTSTORAGE; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe fileopens); echo "      $out"
check "hooked, the game folder named" "log_has 'DIAGNOSTIC: every open of a file or folder in .*TestRun is logged, 2 lines per file, caller and sharing'"
check "a created file: share R--, overwrite-if, from the game's code" "log_has 'open #[0-9]*: ds_test.txt | ok | access 0x[0-9A-F]*, share R--, overwrite-if, options 0x[0-9A-F]* | from RVA 0x[0-9A-F]* <- RVA 0x'"
check "6 opens the same way: 2 lines" "[ \$(count ': ds_test.txt | .*share R--') -eq 2 ] && log_has 'open #[0-9]*: ds_test.txt | ok | access 0x[0-9A-F]*, share R--, open, '"
check "a missing file: not found, and the game still gets ERROR_FILE_NOT_FOUND" "log_has ': ds_missing.txt | 0xC0000034 not found | ' && has 'file opens: missing file error 2'"
check "the game folder itself and a file in a subfolder" "log_has 'open #[0-9]*: \. | ok | access 0x[0-9A-F]*, share RWD, open, ' && log_has ': ds_sub.inner.rpf | ok | access 0x[0-9A-F]*, share RW-, overwrite-if'"
check "a file outside the game folder: not logged" "log_lacks 'tweaksv_ds_outside'"
check "the game runs on" "has 'protect: all restored' && log_lacks 'PROBLEM'"

echo "== D9 the two summaries 10 seconds in, with DIAGNOSTIC_MODE=1 (RageOpenV off): UNLOCK_FILES decides on the full path, OpenFile counts every call"
fresh_ini; setkey ENABLED 0 RAGEOPENV; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage wait)
check "UNLOCK_FILES: the stand-in's 7 opens of other files widened - '/' and a \\\\?\\ prefix made full, x.rpf.txt a .txt - 8 of archives, .CACHE and a full path among them, with the fake's own writes; the long path never reached NtCreateFile" "log_has 'UNLOCK_FILES: in its first 10 seconds, 7 opens of 7 files in the game folder other than .rpf and .cache were given write and delete sharing, so those files stay free for other programs; by type: .txt 4, .asi 1, (none) 1, others 1; left as they were: 8 opens of archives, 6 write opens, 0 exclusive opens, 2 directories, 0 whose path could not be examined.'"
check "OpenFile: 19 asked, 4 archives, 7 other files in the game folder, 7 outside (a sibling folder and a path out through '..' among them), 1 unreadable; all passed on" "log_has 'OpenFile: in its first 10 seconds the game asked DirectStorage for 19 files: 4 .rpf and .cache files, 7 other files in the game folder and 7 outside it, 1 with an unreadable path - all of them passed on to DirectStorage.'"

echo "== D10 the game first asks for an interface TweaksV does not know, then for the known one: 10 seconds later a line, no problem"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage dsotherinterface wait)
check "the other interface logged once the check found OpenFile hooked; no problem" "[ \$(count 'The game also asked for another DirectStorage interface, $OTHER, whose functions TweaksV does not know - left as it is.') -eq 1 ] && log_lacks 'PROBLEM'"
check "the factory TweaksV knows has its OpenFile logged" "log_has 'OpenFile #1: x64a.rpf -> ok' && has '$OPENED'"

echo "== D10b the game asks for another interface after the known one: a line at once, no problem"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage dsotherlater)
check "logged once, no problem" "[ \$(count 'The game also asked for another DirectStorage interface, $OTHER, whose functions TweaksV does not know - left as it is.') -eq 1 ] && log_lacks 'PROBLEM' && has '$OPENED'"

echo "== D10c the game asks only for an interface TweaksV does not know: 10 seconds later a diagnostic line, no problem"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage dsotheronly wait)
check "a log line, not a problem: nothing is freed through the factory" "has 'other interface only 0x0 |' && log_has 'DIAGNOSTIC: OpenFile is not logged: the game asked only for another DirectStorage interface, $OTHER, whose functions TweaksV does not know.' && log_lacks 'PROBLEM' && log_lacks 'also asked'"

echo "== D11 the game never asks for its DirectStorage factory: 30 seconds in, a diagnostic line, no problem, no box"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage dsnofactory waitlong)
check "said once as a diagnostic line, nothing reported, so no box" "has 'dstorage: not called |' && [ \$(count 'DIAGNOSTIC: the game has not asked for its DirectStorage factory since TweaksV started.') -eq 1 ] && log_lacks 'PROBLEM' && log_lacks 'after the start-up summary'"
fresh_ini; out=$(game full_game.exe dstorage dsnofactory ownshare waitlong)
check "UNLOCK_FILES alone: nothing waits for the factory - no line, no problem, the files freed all the same" "log_lacks 'factory' && log_lacks 'PROBLEM' && has '$OWN_SHARED'"

echo "== D12 DIAGNOSTIC_MODE=1 and DRY_RUN=1: nothing hooked, nothing widened, nothing logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe dstorage ownshare fileopens)
check "three 'would' lines" "log_has 'DRY_RUN: would hook NtCreateFile so that the game folder.s other files are opened with write and delete sharing (UNLOCK_FILES).' && log_has 'DRY_RUN: would hook dstorage.dll to log the game.s DirectStorage calls.' && log_has 'DRY_RUN: would hook NtCreateFile to log the opens in the game folder.'"
check "the files stay locked, every call reaches the stand-in, nothing logged" "has '$OWN_LOCKED' && has '$OPENED' && log_lacks 'OpenFile #' && log_lacks 'the game asked for the DirectStorage factory' && log_lacks 'open #' && log_lacks '$HOOKED'"

echo "== D13 [DIRECTSTORAGE] ENABLED=0 with DIAGNOSTIC_MODE=1: nothing at all - a part that is off dumps nothing"
fresh_ini; setkey ENABLED 0 DIRECTSTORAGE; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage ownshare fileopens)
check "header: DirectStorage off; no DirectStorage lines" "log_has 'Features: .*, DirectStorage off' && log_lacks '\[DirectStorage\]'"
check "the files stay locked" "has '$OWN_LOCKED' && has '$OPENED'"

echo "== D14 alloc8or's DirectStorageFix.asi in the game folder: an old plugin, this part is off"
fresh_ini; touch DirectStorageFix.asi; out=$(game full_game.exe ownshare); rm -f DirectStorageFix.asi
check "reported as an old plugin" "log_has 'PROBLEM: Old plugin found: .*DirectStorageFix.asi. TweaksV.s DirectStorage part is switched off for this session'"
check "nothing hooked, the files stay locked" "log_lacks '\[DirectStorage\]' && has '$OWN_LOCKED'"

echo "== D14b the same with CONFLICT_CHECK=0: noted, freed anyway"
fresh_ini; setkey CONFLICT_CHECK 0; touch DirectStorageFix.asi; out=$(game full_game.exe ownshare); rm -f DirectStorageFix.asi
check "noted" "log_has 'Note: .*DirectStorageFix.asi is installed as well - it changes DirectStorage.s file opens, too.'"
check "freed" "has '$OWN_SHARED'"

echo "== D15 NtCreateFile cannot be hooked (Windows' dynamic code policy): a problem of the start-up summary"
fresh_ini; out=$(game full_game.exe ownshare nodyncode)
check "reported once, with the reason in the log, before the start-up summary" "[ \$(count 'PROBLEM: The files in the game folder are not unlocked this session: NtCreateFile could not be hooked (details in TweaksV.log).') -eq 1 ] && log_has 'NtCreateFile could not be hooked (MinHook: ' && log_has 'Start-up summary not shown (ERROR_POPUP=0): ' && log_lacks 'after the start-up summary'"
check "nothing said as hooked, the files stay locked, the game runs on" "log_lacks '$HOOKED' && has '$OWN_LOCKED' && has 'protect: all restored'"

echo "== D16 DIAGNOSTIC_MODE=1 with 20,020 OpenFile calls: the lines stop at the cap, every call passed on"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dstorage dsflood)
check "20000 OpenFile lines, then the cap line once" "has 'flood: 20001 calls |' && [ \$(count 'OpenFile #') -eq 20000 ] && [ \$(count 'DIAGNOSTIC: 20000 lines of OpenFile calls written - from now on they are only counted.') -eq 1 ]"

echo "== D17 DIAGNOSTIC_MODE=1 with 100,001 opens of files that are not there: the open lines and the summary's tables stop at their limits"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe dsopenflood)
check "20000 open lines, then the cap line once; a table full at 100000 entries, said once" "[ \$(count 'DirectStorage\] open #') -eq 20000 ] && log_has 'open #[0-9]*: dsflood_000000.txt | 0xC0000034 not found' && [ \$(count 'DIAGNOSTIC: 20000 open lines written - from now on the opens are only counted (see the summaries).') -eq 1 ] && [ \$(count 'DIAGNOSTIC: a table of the open summary is full (100000 entries) - the opens it cannot take are only counted (see the summaries).') -eq 1 ]"
check "the game runs on" "has 'protect: all restored' && log_lacks 'PROBLEM'"

echo "== D18 a game folder on a file system without file IDs (the test build with TWEAKSV_TEST_FILE_INDEX=1): a relative open told by the file number and the volume's serial number"
fresh_ini; out=$(TWEAKSV_TEST_FILE_INDEX=1 ASI=TweaksV_test.asi game full_game.exe dstorage dsshare ownshare)
check "the identity read that way, said once" "[ \$(count 'TEST BUILD: the game folder.s identity is read without FileIdInformation (TWEAKSV_TEST_FILE_INDEX=1).') -eq 1 ]"
check "the files the stand-in holds by a relative path freed as by a full one, the fake's own too; a relative open of another folder left as it is; no problem" "has '$SHARED' && has '$OWN_SHARED' && log_lacks 'PROBLEM'"

echo
echo "DIRECTSTORAGE SCENARIOS: $pass passed, $fail failed"
