#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# RageOpenV scenarios: a fake GTA5_Enhanced.exe with copies of the code RageOpenV patches
# (the hooked functions are real code, so the hooks are called end to end) loads the real
# TweaksV.asi and then asks for the time once, as the game does. Fake LOCALAPPDATA: the real one
# is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
mkdir -p appdata mods/test
echo hi > mods/test/file.txt

FILES_OK='file hooks 8/8, redirect 8/8, pass 8/8,'
OPEN_OK='open calls 5/5, behaviour ok'
NOTHING='file hooks 0/8 | open calls 0/5 |'

echo "== R1 defaults: everything hooked and working, the other plugins too"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "GetSystemTimeAsFileTime hooked, the one-time start-up ran" "has 'rov api hook: yes' && log_has 'RPF cache check patched: 48 89 -> C3 90.'"
check "the time hook switches itself off after the one-time start-up" "has 'rov api hook afterwards: removed'"
check "RPF cache check skipped (C3 90 at the callee)" "has 'rpf: C3 90 5C |'"
check "console call left alone (CONSOLE=0)" "has 'console: FF 15 55 66 77 08 |'"
check "mods folder: 8 functions hooked; a file in mods\\ is used, any other passes through" "has '$FILES_OK'"
check "mods folder read once (2 entries); other spellings and the folder found; '..', rooted, network, drive and too-long paths never redirected" "has 'spellings ok, folder ok, outside ok' && log_has 'The mods folder has 2 files and folders.'"
check "OPEN archives: 5 calls redirected; OPEN not decrypted, others decrypted; entries marked, and not again by a parse without its own FindEncryption" "has '$OPEN_OK'"
check "status lines; no line per hooked function without DIAGNOSTIC_MODE" "log_has 'Files in the mods folder are used (8 file functions hooked)' && log_has 'Unencrypted (OPEN) RPF archives can be loaded (5 calls redirected)' && log_lacks ' hooked at RVA'"
check "the other plugins still applied" "has 'heap: 1024 MB' && has 'splash: E9 86 01 00 00 90' && has 'modkit refs: new array (RW, 128 KB)' && has 'pack sizes: D3=12000 '"
check "every patched page back to its own protection (read+execute, as the game's code)" "has 'protect: all restored (RX)'"
check "the 6 redirected calls each lead to a call stub that is read+execute, never writable" "has 'stubs: 6, all RX'"
check "no problems; DIAGNOSTIC_MODE off, so no hot-path lines; nothing of newmods" "log_lacks 'PROBLEM' && log_lacks ' asked for ' && log_lacks ' served ' && log_lacks 'newmods'"

echo "== R1b an OPEN archive with an implausible entry count, parsed twice: left alone, logged each time, reported once after the start-up summary"
fresh_ini; out=$(game full_game.exe rovbadarchive)
check "left alone, the rest working; a line for each parse" "has '$OPEN_OK' && [ \$(count 'The OPEN archive test3.rpf has an implausible entry table (1048577 entries) - left as it is.') -eq 2 ]"
check "reported once, as a problem after the start-up summary" "[ \$(count 'PROBLEM: Unencrypted (OPEN) RPF archive test3.rpf could not be prepared: its entry table is implausible (1048577 entries) - its files may not load.') -eq 1 ] && log_has 'Problem after the start-up summary not shown (ERROR_POPUP=0): 1 problem'"

echo "== R1c an OPEN archive whose entry table cannot be read (a field moved by a game update would do that), parsed twice: left alone, logged each time, reported once"
fresh_ini; out=$(game full_game.exe rovbadtable)
check "left alone, nothing written through the pointer, the rest working; a line for each parse" "has '$OPEN_OK' && [ \$(count 'The OPEN archive test4.rpf has an entry table that cannot be read (4 entries at 0x[0-9A-F]* (outside the game)) - left as it is.') -eq 2 ]"
check "reported once, as a problem after the start-up summary" "[ \$(count 'PROBLEM: Unencrypted (OPEN) RPF archive test4.rpf could not be prepared: its entry table cannot be read (4 entries at 0x[0-9A-F]* (outside the game)) - its files may not load.') -eq 1 ] && log_has 'Problem after the start-up summary not shown (ERROR_POPUP=0): 1 problem'"

echo "== R2 CONSOLE=1: the call that hides the console is NOPed"
fresh_ini; setkey CONSOLE 1; out=$(game full_game.exe)
check "6 NOPs, logged with the bytes they replace" "has 'console: 90 90 90 90 90 90 |' && log_has 'Console hide call patched: FF 15 55 66 77 08 -> 90 90 90 90 90 90.'"

echo "== R2b CONSOLE=1 and another mod NOPed the call already: left as it is"
fresh_ini; setkey CONSOLE 1; out=$(game full_game.exe rovconsolenopped)
check "the NOPs as they were, logged as another mod's" "has 'console: 90 90 90 90 90 90 |' && log_has 'Console hide call already patched (another mod did it) - left as it is.' && log_lacks 'Console hide call patched' && log_lacks 'unexpected bytes'"

echo "== R3 each switch on its own"
fresh_ini; setkey MODS_FOLDER 0 RAGEOPENV; out=$(game full_game.exe)
check "MODS_FOLDER=0: no file hooks, the rest on, the switch logged" "has 'file hooks 0/8 |' && has '$OPEN_OK' && log_has 'MODS_FOLDER=0 - files in the mods folder are not used.'"
fresh_ini; setkey OPEN_RPF 0 RAGEOPENV; out=$(game full_game.exe)
check "OPEN_RPF=0: no encryption calls redirected, the rest on, the switch logged" "has 'open calls 0/5 |' && has '$FILES_OK' && log_has 'OPEN_RPF=0 - unencrypted (OPEN) RPF archives are not enabled.'"
fresh_ini; setkey SKIP_RPF_CACHE_CHECK 0 RAGEOPENV; out=$(game full_game.exe)
check "SKIP_RPF_CACHE_CHECK=0: the cache check is left on, the switch logged" "has 'rpf: 48 89 5C |' && log_has 'SKIP_RPF_CACHE_CHECK=0 - the RPF cache check is not skipped.' && has '$FILES_OK'"

echo "== R4 no mods folder: the game's file functions are not hooked at all, not a problem"
fresh_ini; mv mods mods_off; out=$(game full_game.exe); mv mods_off mods
check "no file hooks, the rest on" "has 'file hooks 0/8 |' && has '$OPEN_OK'"
check "logged, no problem" "log_has 'No mods folder in the current directory - the file functions are not hooked.' && log_lacks 'PROBLEM'"
fresh_ini; setkey DIAGNOSTIC_MODE 1; mv mods mods_off; out=$(game full_game.exe); mv mods_off mods
check "DIAGNOSTIC_MODE without a mods folder: the 8 patterns are still checked, nothing hooked" "has 'file hooks 0/8 |' && [ \$(count 'RageOpenV\] DIAGNOSTIC: \(OpenBulk\|GetFileTime\|GetFileSize\|GetAttributes\) [12] at') -eq 8 ] && log_has 'the patterns were checked (DIAGNOSTIC_MODE), the file functions are not hooked'"

echo "== R4c an empty mods folder: the file functions are not hooked, not a problem"
fresh_ini; mv mods mods_off; mkdir mods; out=$(game full_game.exe); rmdir mods; mv mods_off mods
check "no file hooks, the rest on; logged, no problem" "has 'file hooks 0/8 |' && has '$OPEN_OK' && log_has 'The mods folder is empty - the file functions are not hooked.' && log_lacks 'PROBLEM'"

echo "== R4b crafted mods folders: deeper than the limit, read before the rest; a name outside the Basic Multilingual Plane"
# zdeep sorts after test, so it is read first: the reading must go on after it
fresh_ini; deep=mods/zdeep; for i in $(seq 1 33); do deep="$deep/d"; done; mkdir -p "$deep"
out=$(game full_game.exe)
check "too deep: that folder skipped and reported, the rest of the folder still read and used" "log_has 'PROBLEM: Only part of the mods folder is used this session: 1 folder deeper than 32 levels was skipped.' && has '$FILES_OK'"
rm -rf mods/zdeep
# An emoji is a character outside the Basic Multilingual Plane, which no ANSI code page holds but UTF-8: the
# game asks for its files in UTF-8 on every Windows, so the name is listed and served whatever the code page.
fresh_ini; touch "mods/test/😀.txt"; out=$(game full_game.exe rovask 'test/😀.txt')
check "a name outside the Basic Multilingual Plane: listed in UTF-8, nothing left out" "log_has 'The mods folder has 3 files and folders.' && log_lacks 'left out of the list'"
check "its file served by its UTF-8 path from every file function" "has 'ask 1 served 8/8 passed 0/8'"
check "the lookup still works with the name there" "has 'spellings ok, folder ok, outside ok' && has '$FILES_OK'"
rm -f "mods/test/😀.txt"

echo "== R4d a folder in the mods folder that cannot be listed: counted and reported, the rest used; the mods folder itself: not called empty"
fresh_ini; mkdir -p mods/locked; touch mods/locked/inner.txt; deny_listing mods/locked; out=$(game full_game.exe); allow_writes mods/locked
check "the folder logged and reported with its Windows error, the rest of the folder used" "log_has 'The folder mods.locked could not be read (Windows error 5, access denied) - its files are not used.' && log_has 'PROBLEM: Only part of the mods folder is used this session: 1 folder could not be read (Windows error 5, access denied).' && has '$FILES_OK'"
rm -rf mods/locked
fresh_ini; deny_listing mods; out=$(game full_game.exe); allow_writes mods
check "the mods folder cannot be listed: reported as such, not as empty; nothing hooked, the rest on" "log_has 'PROBLEM: Files in the mods folder are not used this session: the mods folder could not be read (Windows error 5, access denied).' && log_lacks 'is empty' && has 'file hooks 0/8 |' && has '$OPEN_OK'"

echo "== R4e names with letters outside ASCII: a file and folders served by their UTF-8 path, also with those letters in the other case; a request whose bytes are not UTF-8 passed on untouched"
# The game asks in UTF-8 on every Windows; asked in code page 1252, the name's bytes are not UTF-8 - the
# spelling TweaksV's list had on a Windows whose ANSI code page is 1252, before it was read in UTF-8.
fresh_ini; touch "mods/test/über.txt"; mkdir -p "mods/Ελληνικά/Модели"; touch "mods/Ελληνικά/Модели/фара.txt"
out=$(game full_game.exe rovask 'test/über.txt' rovask 'TEST/ÜBER.TXT' rovask 'Ελληνικά/Модели' rovask 'ελληνικά\МОДЕЛИ\ФАРА.txt' rov1252 'test/über.txt')
check "listed in UTF-8: 6 files and folders, nothing left out" "log_has 'The mods folder has 6 files and folders.' && log_lacks 'left out of the list'"
check "the file served by its UTF-8 path from every file function, also with its letter in the other case" "has 'ask 1 served 8/8 passed 0/8' && has 'ask 2 served 8/8 passed 0/8'"
check "the folders and the file in them served by their UTF-8 path, also with their letters in the other case and backslashes" "has 'ask 3 served 8/8 passed 0/8' && has 'ask 4 served 8/8 passed 0/8'"
check "the file's name in code page 1252, bytes that are not UTF-8: never redirected, passed on untouched" "has 'ask 5 served 0/8 passed 8/8'"
check "the other spellings, the folder and the paths never redirected as before" "has 'spellings ok, folder ok, outside ok' && has '$FILES_OK'"
rm -rf "mods/test/über.txt" "mods/Ελληνικά"

echo "== R4f letters compared as Windows' file systems compare them, by their upper case: the Kelvin sign and k told apart, a letter outside the Basic Multilingual Plane keeping its case, a name whose upper case is longer in UTF-8"
kelvin=$(printf '\xe2\x84\xaa')			# the Kelvin sign, U+212A: its lower case is k, its upper case itself
deseret=$(printf '\xf0\x90\x90\xa8')		# DESERET SMALL LETTER LONG I, U+10428
deseretCap=$(printf '\xf0\x90\x90\x80')	# its capital, U+10400
long=$(printf 'ɐ%.0s' $(seq 1 124))		# 124 letters of 2 bytes, their capitals of 3: test/ and these are 253 bytes, upcased 377
fresh_ini; touch "mods/test/$kelvin.txt" "mods/test/$deseret.txt" "mods/test/$long"
out=$(game full_game.exe rovask "test/$kelvin.txt" rovask 'test/k.txt' rovask "test/$deseret.txt" rovask "test/$deseretCap.txt" rovask "TEST/$long")
check "the Kelvin sign's file served; k, which shares its lower case but not its upper case, passed on, as NTFS tells the two apart" "has 'ask 1 served 8/8 passed 0/8' && has 'ask 2 served 0/8 passed 8/8'"
check "a Deseret letter's file served; its capital passed on, as NTFS keeps the case of a letter outside the Basic Multilingual Plane" "has 'ask 3 served 8/8 passed 0/8' && has 'ask 4 served 0/8 passed 8/8'"
check "a name of 253 bytes whose upper case is 377 listed and served" "log_has 'The mods folder has 5 files and folders.' && has 'ask 5 served 8/8 passed 0/8'"
rm -f "mods/test/$kelvin.txt" "mods/test/$deseret.txt" "mods/test/$long"

echo "== R4g a name that is not valid UTF-16 (an unpaired surrogate, made by the fake before TweaksV starts): left out and counted, the rest used"
fresh_ini; out=$(game full_game.exe rovbadname)
if has 'bad name NOT made'; then
	skip "this file system refuses a name with an unpaired surrogate, so none can be left out"
else
	check "left out and counted with its reason, the rest listed and used; the name gone again with the fake" "has 'bad name made |' && log_has 'The mods folder has 2 files and folders.' && log_has 'Names in the mods folder left out of the list: 1 whose name is not valid Unicode (an unpaired surrogate) - the game cannot ask for such a name.' && has '$FILES_OK' && [ \"\$(ls -A mods/test 2>/dev/null)\" = file.txt ]"
fi

echo "== R4h DIAGNOSTIC_MODE=1 and paths outside ASCII: the lines name them in UTF-8, a request that is not UTF-8 with the replacement character the game reads"
fresh_ini; setkey DIAGNOSTIC_MODE 1; touch "mods/test/über.txt"; out=$(game full_game.exe rovask 'test/über.txt' rov1252 'test/über.txt')
check "the request and the file served named in UTF-8" "log_has 'DIAGNOSTIC: OpenBulk 1 asked for test/über.txt' && log_has 'DIAGNOSTIC: OpenBulk 1 served mods/test/über.txt' && has 'ask 1 served 8/8 passed 0/8'"
check "the request in code page 1252 named with a replacement character, as the game reads it, and passed on" "log_has 'DIAGNOSTIC: OpenBulk 1 asked for test/�ber.txt' && has 'ask 2 served 0/8 passed 8/8'"
rm -f "mods/test/über.txt"

echo "== R5 DRY_RUN=1: the time hook is installed so the checks run at their moment; nothing else written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe); echo "      $out"
check "time hook yes; nothing written" "has 'rov api hook: yes' && has 'rpf: 48 89 5C |' && has '$NOTHING' && has 'stubs: 0,'"
check "the three 'would' lines" "log_has 'DRY_RUN: would patch the RPF cache check: 48 89 -> C3 90' && log_has 'DRY_RUN: would hook the 8 file functions' && log_has 'DRY_RUN: would redirect the 5 RPF encryption calls'"

echo "== R6 DIAGNOSTIC_MODE=1: every pattern found once, every site logged, the hot-path lines on"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "the 14 RageOpenV patterns each found exactly once (all but the console's), none of newmods" "[ \$(grep -c -E 'Pattern R-E-[A-Z][0-9] found: .* 1 match,' TweaksV.log) -eq 14 ] && log_lacks 'Pattern R-E-C1' && log_lacks 'Pattern R-E-[MD]'"
check "the 8 file functions logged; the 4 computed starts checked (no unwind data, CC before)" "[ \$(count 'RageOpenV\] DIAGNOSTIC: \(OpenBulk\|GetFileTime\|GetFileSize\|GetAttributes\) [12] at RVA') -eq 8 ] && [ \$(count 'no unwind data, byte before it CC') -eq 4 ] && [ \$(count 'RageOpenV\] DIAGNOSTIC: \(OpenBulk\|GetFileTime\|GetFileSize\|GetAttributes\) [12] hooked at RVA') -eq 8 ]"
check "the 5 encryption calls logged" "[ \$(count 'RageOpenV\] DIAGNOSTIC: the .* call at RVA') -eq 5 ]"
check "still all hooked" "has '$FILES_OK' && has '$OPEN_OK'"
check "a file request and a file served are logged, and what is logged is announced" "log_has 'DIAGNOSTIC: OpenBulk 1 asked for test/file.txt' && log_has 'DIAGNOSTIC: OpenBulk 1 served mods/test/file.txt' && log_has 'DIAGNOSTIC: every file request, every file served from the mods folder and every archive header read are logged'"

echo "== R6b DIAGNOSTIC_MODE=1 with 20,021 file requests: the lines stop at the cap, the requests are still counted"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe rovflood)
check "20000 request lines, then the cap line once; the served lines unaffected" "[ \$(count ' asked for ') -eq 20000 ] && [ \$(count '20000 lines of file requests written - from now on they are only counted') -eq 1 ] && [ \$(count ' served mods/') -eq 11 ]"
check "still all hooked and working" "has '$FILES_OK' && has '$OPEN_OK'"

echo "== R7 [RAGEOPENV] ENABLED=0: no hook at all"
fresh_ini; setkey ENABLED 0 RAGEOPENV; out=$(game full_game.exe)
check "time hook not installed, nothing written" "has 'rov api hook: no' && has 'rpf: 48 89 5C |' && has '$NOTHING'"
check "header: RageOpenV off, the other parts on (the heap); no RageOpenV lines" "log_has 'Features: .*, RageOpenV off' && has 'heap: 1024 MB' && log_lacks '\[RageOpenV\]'"

echo "== R8 an old OpenRPF.asi in the folder: RageOpenV is skipped"
fresh_ini; touch OpenRPF.asi; out=$(game full_game.exe); rm -f OpenRPF.asi
check "time hook not installed, nothing written" "has 'rov api hook: no' && has '$NOTHING'"
check "old plugin reported" "log_has 'Old plugin found: .*OpenRPF.asi'"

echo "== R9 another mod hooked GetFileTime 1 (a jmp at its start): warned, hooked on top, chains through"
fresh_ini; out=$(game full_game.exe rovjump); echo "      $out"
check "all 8 hooked and working through the other mod's jump" "has '$FILES_OK' && has '$OPEN_OK'"
check "warning logged, not a problem" "log_has 'GetFileTime 1 at RVA .* already starts with a jump - another mod may have hooked it' && log_lacks 'PROBLEM'"
check "every page back to its own protection" "has 'protect: all restored'"

echo "== R10 the computed start of GetFileTime 2 is not a function start: skipped"
fresh_ini; out=$(game full_game.exe rovnotstart)
check "no file hooks; the others on" "has 'file hooks 0/8 |' && has '$OPEN_OK'"
check "reason logged with the byte found" "log_has 'GetFileTime 2 at RVA .* is not a function start (no unwind data, byte before it 48)'"

echo "== R11 GetFileSize 2's pattern is missing: the file group is skipped"
fresh_ini; out=$(game full_game.exe rovmissing)
check "no file hooks; the others on" "has 'file hooks 0/8 |' && has '$OPEN_OK'"
check "reason logged" "log_has 'Files in the mods folder are not used this session: R-E-F7 was not found (GetFileSize 2).'"

echo "== R12 the two DecryptHeader2 calls lead to different functions: nothing redirected"
fresh_ini; out=$(game full_game.exe rovdecrypt2)
check "no encryption call redirected; the others on" "has 'open calls 0/5 |' && has '$FILES_OK'"
check "reason logged and reported" "log_has 'the two DecryptHeader2 calls lead to different functions' && log_has 'PROBLEM: Unencrypted (OPEN) RPF archives cannot be loaded'"

echo "== R13 another mod already redirected the DecryptHeader call: nothing redirected"
fresh_ini; out=$(game full_game.exe rovredirected)
check "no encryption call redirected" "has 'open calls 0/5 |' && has '$FILES_OK'"
check "reason logged" "log_has 'the DecryptHeader call at RVA .* is not a call into the game (another mod may have hooked it already)'"

echo "== R14 the RPF cache check is already skipped by another mod: left alone, not a problem"
fresh_ini; out=$(game full_game.exe rovcachepatched)
check "bytes as they were (C3 89 5C)" "has 'rpf: C3 89 5C |'"
check "logged, no problem" "log_has 'The RPF cache check is already skipped (another mod did it)' && log_lacks 'PROBLEM'"

echo "== R14b another mod hooked the RPF cache check's function (a jmp at its start): left alone, not a problem"
fresh_ini; out=$(game full_game.exe rovcachehooked)
check "bytes as they were (E9 00 00), the rest on" "has 'rpf: E9 00 00 |' && has '$FILES_OK' && has '$OPEN_OK'"
check "logged, no problem" "log_has 'The RPF cache check.s function is already hooked by another mod - left as it is.' && log_lacks 'PROBLEM'"

echo "== R15 none of the game code present (plain fake game): each part reported, no crash"
fresh_ini; out=$(game plain_game.exe)
check "harness survived" "grep -q 'loaded' harness.out"
check "the three parts reported" "log_has 'PROBLEM: The RPF cache check is not skipped' && log_has 'PROBLEM: Files in the mods folder are not used' && log_has 'PROBLEM: Unencrypted (OPEN) RPF archives cannot be loaded'"
check "one start-up summary for both moments (shown after the one-time start-up)" "[ \$(count 'Start-up summary not shown (ERROR_POPUP=0)') -eq 1 ]"

echo "== R16 FreeLibrary while hooked, eight times over: TweaksV.asi stays loaded (pinned), its hooks called afterwards still work"
# one FreeLibrary would not tell: TweaksV's own threads hold references of their own for a while (the start-up check, 30 s)
fresh_ini; out=$(game full_game.exe unload); echo "      $out"
check "still loaded after 8 x FreeLibrary; the file functions and the OPEN calls work through the hooks afterwards; the pin not reported" "has 'unload: still loaded after 8 x FreeLibrary |' && has '$FILES_OK' && has '$OPEN_OK' && log_lacks 'could not be pinned'"

echo "== R17 the mods file cannot be opened: OpenBulk gives the game that failure, never its own file, and says so"
# 7 calls of OpenBulk cannot open their mods file: the redirect test and the refusal test call both devices once
# each, the spellings test OpenBulk 1 three times (two spellings of the file and the folder)
fresh_ini; out=$(game full_game.exe rovopenfail); echo "      $out"
check "the failure and the game's last error returned, the mods path the only one asked for; every hook redirects" "has 'refusal ok' && has '$FILES_OK' && has 'spellings ok, folder ok, outside ok' && has '$OPEN_OK'"
check "each one logged without DIAGNOSTIC_MODE, by the hook that could not open it" "[ \$(count 'RageOpenV\] OpenBulk 1 could not open mods/') -eq 5 ] && [ \$(count 'RageOpenV\] OpenBulk 2 could not open mods/') -eq 2 ] && log_has 'OpenBulk 1 could not open mods/test/file.txt - the game gets that failure, not its own file.'"
check "the first one reported, once, as a problem after the start-up summary" "[ \$(count 'PROBLEM: A file in the mods folder could not be opened - the game gets that failure, not its own file: mods/test/file.txt.') -eq 1 ] && log_has 'Problem after the start-up summary not shown (ERROR_POPUP=0): 1 problem'"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe rovopenfail)
check "DIAGNOSTIC_MODE: the same lines, none of them as served; the other functions' files served" "[ \$(count 'RageOpenV\] OpenBulk [12] could not open mods/') -eq 7 ] && log_lacks 'OpenBulk [12] served' && log_has 'DIAGNOSTIC: GetFileTime 1 served mods/test/file.txt'"
fresh_ini; out=$(game full_game.exe rovopenfail rovflood)
check "20,007 that cannot be opened: 20000 lines, then the cap line once; reported once" "[ \$(count 'could not open mods/') -eq 20000 ] && [ \$(count '20000 lines of mods files that could not be opened written - from now on they are only counted.') -eq 1 ] && [ \$(count 'PROBLEM: A file in the mods folder could not be opened') -eq 1 ] && has '$FILES_OK'"

echo "== R18 R-E-K1 matches twice (a second copy of the cache check's call site): not used, the cache check left on, the rest on"
fresh_ini; out=$(game full_game.exe tworovk1)
check "the cache check left on; the file and OPEN hooks on" "has 'rpf: 48 89 5C |' && has '$FILES_OK' && has '$OPEN_OK'"
check "the scanner says why; the problem names the pattern" "log_has 'Scanner\] Pattern R-E-K1 matches 2 or more times - not used (the site must be unique).' && log_has 'PROBLEM: The RPF cache check is not skipped this session: R-E-K1 was not found.'"

echo "== R19 the game never asks for the time: 30 seconds in, RageOpenV says so, shows the start-up summary and switches its hook off"
fresh_ini; out=$(game full_game.exe rovnotime); echo "      $out"
check "reported, the start-up summary shown, the time hook off again" "log_has 'PROBLEM: RageOpenV.s start-up never ran: the game did not ask for the time through GetSystemTimeAsFileTime - the mods folder, OPEN archives and the cache check skip are off this session.' && log_has 'Start-up summary not shown (ERROR_POPUP=0): 1 problem' && has 'rov api hook: yes' && has 'rov api hook afterwards: removed'"
check "nothing of RageOpenV done; the parts that start in DllMain on" "has '$NOTHING' && has 'rpf: 48 89 5C |' && has 'heap: 1024 MB'"

echo "== R20 the game runs in another folder than the game folder: said and reported; a junction to the game folder is the game folder"
fresh_ini; mkdir -p notgame; out=$(game full_game.exe rovcwd notgame); rmdir notgame
check "another folder: logged, and the mods folder of the game folder reported as not used; nothing hooked" "has 'cwd set |' && log_has 'The current directory is .*notgame, not the game folder - the mods folder is looked for there.' && log_has 'PROBLEM: Files in the mods folder are not used this session: the game runs in .*notgame, not in the game folder.' && has 'file hooks 0/8 |'"
if ! cmd //c "$(cygpath -w "$T/junction.cmd")" "$(cygpath -w "$B/samegame")" "$(cygpath -w "$B")"; then
	check "the junction could be made (a check of the test itself) - the scenario stops here" "false"
else
	fresh_ini; out=$(game full_game.exe rovcwd samegame)
	cmd //c "$(cygpath -w "$T/junction.cmd")" remove "$(cygpath -w "$B/samegame")"
	check "a junction to the game folder as the current directory: the same folder - nothing said, the mods folder used" "has 'cwd set |' && log_lacks 'not the game folder' && log_lacks 'PROBLEM' && has '$FILES_OK'"
fi

echo "== R21 DIAGNOSTIC_MODE=1 and a request under a profile folder whose name is not ASCII: written as %USERPROFILE%"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(USERPROFILE='C:\Users\Jürgen' game full_game.exe rovprofile)
check "the request, in UTF-8 as the game asks, logged under %USERPROFILE% and not redirected; the name nowhere in the log" "has 'profile path asked' && log_has 'OpenBulk 1 asked for %USERPROFILE%.Documents.tweaksv.txt' && log_lacks 'rgen'"

echo
echo "RAGEOPENV SCENARIOS: $pass passed, $fail failed"
