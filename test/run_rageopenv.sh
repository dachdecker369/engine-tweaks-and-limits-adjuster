#!/usr/bin/env bash
# RageOpenV scenarios: a fake GTA5_Enhanced.exe with copies of the code RageOpenV patches
# (the hooked functions are real code, so the hooks are called end to end) loads the real
# TweaksV.asi and then asks for the time once, as the game does. Fake LOCALAPPDATA: the real one
# is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
mkdir -p appdata mods/test newmods
echo hi > mods/test/file.txt

FILES_OK='file hooks 8/8, redirect 8/8, pass 8/8'
OPEN_OK='open calls 5/5, behaviour ok'
MOUNTS='mounts: mods:/ platform:/ platformcrc:/ common:/ commoncrc:/ dlcpacks:/'
PATHS='newmods\ mods:/platform/ mods:/platform/ mods:/common/ mods:/common/ mods:/dlcpacks/, archive: dlcpacks:/test/'
NEWMODS_OK='newmods hooks 2/2, original mount 1'
NOTHING='file hooks 0/8 | open calls 0/5 | newmods hooks 0/2'

echo "== R1 defaults: everything hooked and working, the other plugins too"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "GetSystemTimeAsFileTime hooked, the one-time start-up ran" "has 'rov api hook: yes' && log_has 'The RPF cache check is skipped'"
check "the time hook switches itself off after the one-time start-up" "has 'rov api hook afterwards: removed'"
check "RPF cache check skipped (C3 90 at the callee)" "has 'rpf: C3 90 5C'"
check "console call left alone (CONSOLE=0)" "has 'console: FF 15'"
check "mods folder: 8 functions hooked; a file in mods\\ is used, any other passes through" "has '$FILES_OK'"
check "mods folder read once (2 entries); other spellings and the folder found, '..' and too-long paths never redirected" "has 'spellings ok, folder ok, outside ok' && log_has 'The mods folder has 2 files and folders.'"
check "OPEN archives: 5 calls redirected; OPEN not decrypted, others decrypted; entries marked" "has '$OPEN_OK'"
check "newmods: both hooks; the game's mount first, then mods:/ and the five devices" "has '$NEWMODS_OK' && has '$MOUNTS' && has '$PATHS'"
check "status lines" "log_has 'Files in the mods folder are used (8 file functions hooked)' && log_has 'Unencrypted (OPEN) RPF archives can be loaded (5 calls redirected)' && log_has 'The newmods folder is mounted'"
check "the other plugins still applied" "has 'heap: 1024 MB' && has 'splash: E9 86 01 00 00 90' && has 'modkit refs: new array (RW, 128 KB)' && has 'pack sizes: D3=12000'"
check "every patched page back to its own protection (read+execute, as the game's code)" "has 'protect: all restored (RX)'"
check "the 7 redirected calls each lead to a call stub that is read+execute, never writable" "has 'stubs: 7, all RX'"
check "no problems; LOG_* off, so no hot-path lines" "! log_has 'PROBLEM' && ! log_has '\[mods\]' && ! log_has '\[info\]' && ! log_has '\[device\]'"

echo "== R2 CONSOLE=1: the call that hides the console is NOPed"
fresh_ini; setkey CONSOLE 1; out=$(game full_game.exe)
check "6 NOPs" "has 'console: 90 90'"

echo "== R3 each switch on its own"
fresh_ini; setkey MODS_FOLDER 0 RAGEOPENV_SETTINGS; out=$(game full_game.exe)
check "MODS_FOLDER=0: no file hooks, the rest on" "has 'file hooks 0/8' && has '$OPEN_OK' && has '$NEWMODS_OK' && log_has 'MODS_FOLDER=0'"
fresh_ini; setkey OPEN_RPF 0 RAGEOPENV_SETTINGS; out=$(game full_game.exe)
check "OPEN_RPF=0: no encryption calls redirected, the rest on" "has 'open calls 0/5' && has '$FILES_OK' && has '$NEWMODS_OK' && log_has 'OPEN_RPF=0'"
fresh_ini; setkey NEWMODS 0 RAGEOPENV_SETTINGS; out=$(game full_game.exe)
check "NEWMODS=0: no newmods hooks, the rest on" "has 'newmods hooks 0/2' && has '$FILES_OK' && has '$OPEN_OK' && log_has 'NEWMODS=0'"
fresh_ini; setkey SKIP_RPF_CACHE_CHECK 0 RAGEOPENV_SETTINGS; out=$(game full_game.exe)
check "SKIP_RPF_CACHE_CHECK=0: the cache check is left on" "has 'rpf: 48 89 5C' && log_has 'SKIP_RPF_CACHE_CHECK=0' && has '$FILES_OK'"

echo "== R4 no newmods folder: nothing to mount, no hooks for it, not a problem"
fresh_ini; rmdir newmods; out=$(game full_game.exe); mkdir -p newmods
check "no newmods hooks, the rest on" "has 'newmods hooks 0/2' && has '$FILES_OK' && has '$OPEN_OK'"
check "logged, no problem" "log_has 'No newmods folder in the current directory' && ! log_has 'PROBLEM'"

echo "== R4b no mods folder: the game's file functions are not hooked at all, not a problem"
fresh_ini; mv mods mods_off; out=$(game full_game.exe); mv mods_off mods
check "no file hooks, the rest on" "has 'file hooks 0/8' && has '$OPEN_OK' && has '$NEWMODS_OK'"
check "logged, no problem" "log_has 'No mods folder in the current directory - the file functions are not hooked.' && ! log_has 'PROBLEM'"
fresh_ini; setkey DIAGNOSTIC_MODE 1; mv mods mods_off; out=$(game full_game.exe); mv mods_off mods
check "DIAGNOSTIC_MODE without a mods folder: the 8 patterns are still checked, nothing hooked" "has 'file hooks 0/8' && [ \$(count 'RageOpenV\] DIAGNOSTIC: \(OpenBulk\|GetFileTime\|GetFileSize\|GetAttributes\) [12] at') -eq 8 ] && log_has 'the patterns were checked (DIAGNOSTIC_MODE), the file functions are not hooked'"

echo "== R4c crafted mods folders: deeper than the limit, a name outside ASCII"
fresh_ini; deep=mods/deep; for i in $(seq 1 33); do deep="$deep/d"; done; mkdir -p "$deep"; touch "mods/test/ファイル.txt"
out=$(game full_game.exe)
check "too deep: reported, the rest of the folder still used" "log_has 'PROBLEM: Only part of the mods folder is used' && has '$FILES_OK'"
check "a name outside ASCII does not break the lookup" "has 'spellings ok, folder ok, outside ok'"
rm -rf mods/deep "mods/test/ファイル.txt"

echo "== R5 DRY_RUN=1: the time hook is installed so the checks run at their moment; nothing else written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe); echo "      $out"
check "time hook yes; nothing written" "has 'rov api hook: yes' && has 'rpf: 48 89 5C' && has '$NOTHING' && has 'stubs: 0,'"
check "the four 'would' lines" "log_has 'DRY_RUN: would skip the RPF cache check' && log_has 'DRY_RUN: would hook the 8 file functions' && log_has 'DRY_RUN: would redirect the 5 RPF encryption calls' && log_has 'DRY_RUN: would hook the initial mount call'"

echo "== R6 DIAGNOSTIC_MODE=1: every pattern found once, every site logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "the 19 RageOpenV patterns each found exactly once (all but the console's)" "[ \$(grep -c -E 'Pattern R-E-[A-Z][0-9] found: .* 1 match,' TweaksV.log) -eq 19 ] && ! log_has 'Pattern R-E-C1'"
check "the 8 file functions logged; the 4 computed starts checked (no unwind data, CC before)" "[ \$(count 'RageOpenV\] DIAGNOSTIC: \(OpenBulk\|GetFileTime\|GetFileSize\|GetAttributes\)') -eq 8 ] && [ \$(count 'no unwind data, byte before it CC') -eq 4 ]"
check "the 5 encryption calls and the newmods sites logged" "[ \$(count 'RageOpenV\] DIAGNOSTIC: the .* call at RVA') -eq 5 ] && log_has 'DIAGNOSTIC: initial mount call at RVA'"
check "still all hooked" "has '$FILES_OK' && has '$OPEN_OK' && has '$NEWMODS_OK'"

echo "== R7 [RAGEOPENV_SETTINGS] ENABLED=0: no hook at all"
fresh_ini; setkey ENABLED 0 RAGEOPENV_SETTINGS; out=$(game full_game.exe)
check "time hook not installed, nothing written" "has 'rov api hook: no' && has 'rpf: 48 89 5C' && has '$NOTHING'"
check "no RageOpenV lines" "! log_has '\[RageOpenV\]'"

echo "== R8 an old OpenRPF.asi in the folder: RageOpenV is skipped"
fresh_ini; touch OpenRPF.asi; out=$(game full_game.exe); rm -f OpenRPF.asi
check "time hook not installed, nothing written" "has 'rov api hook: no' && has '$NOTHING'"
check "old plugin reported" "log_has 'Old plugin found: .*OpenRPF.asi'"

echo "== R9 another mod hooked GetFileTime 1 (a jmp at its start): warned, hooked on top, chains through"
fresh_ini; out=$(game full_game.exe rovjump); echo "      $out"
check "all 8 hooked and working through the other mod's jump" "has '$FILES_OK' && has '$OPEN_OK' && has '$NEWMODS_OK'"
check "warning logged, not a problem" "log_has 'GetFileTime 1 at RVA .* already starts with a jump - another mod may have hooked it' && ! log_has 'PROBLEM'"
check "every page back to its own protection" "has 'protect: all restored'"

echo "== R10 the computed start of GetFileTime 2 is not a function start: skipped"
fresh_ini; out=$(game full_game.exe rovnotstart)
check "no file hooks; the others on" "has 'file hooks 0/8' && has '$OPEN_OK' && has '$NEWMODS_OK'"
check "reason logged with the byte found" "log_has 'GetFileTime 2 at RVA .* is not a function start (no unwind data, byte before it 48)'"

echo "== R11 GetFileSize 2's pattern is missing: the file group is skipped"
fresh_ini; out=$(game full_game.exe rovmissing)
check "no file hooks; the others on" "has 'file hooks 0/8' && has '$OPEN_OK'"
check "reason logged" "log_has 'GetFileSize 2 was not found in the game code'"

echo "== R12 the two DecryptHeader2 calls lead to different functions: nothing redirected"
fresh_ini; out=$(game full_game.exe rovdecrypt2)
check "no encryption call redirected; the others on" "has 'open calls 0/5' && has '$FILES_OK' && has '$NEWMODS_OK'"
check "reason logged and reported" "log_has 'the two DecryptHeader2 calls lead to different functions' && log_has 'PROBLEM: Unencrypted (OPEN) RPF archives cannot be loaded'"

echo "== R13 another mod already redirected the DecryptHeader call: nothing redirected"
fresh_ini; out=$(game full_game.exe rovredirected)
check "no encryption call redirected" "has 'open calls 0/5' && has '$FILES_OK'"
check "reason logged" "log_has 'the DecryptHeader call at RVA .* leads outside the game'"

echo "== R14 another mod already redirected the initial mount call: newmods skipped"
fresh_ini; out=$(game full_game.exe rovmountredirected)
check "no newmods hooks (the archive hook was not left behind)" "has 'newmods hooks 0/2' && has '$FILES_OK' && has '$OPEN_OK'"
check "reason logged and reported" "log_has 'the initial mount call at RVA .* leads outside the game' && log_has 'PROBLEM: The newmods folder is not mounted'"

echo "== R15 the RPF cache check is already skipped by another mod: left alone, not a problem"
fresh_ini; out=$(game full_game.exe rovcachepatched)
check "bytes as they were (C3 89 5C)" "has 'rpf: C3 89 5C'"
check "logged, no problem" "log_has 'The RPF cache check is already skipped (another mod did it)' && ! log_has 'PROBLEM'"

echo "== R16 none of the game code present (plain fake game): each part reported, no crash"
fresh_ini; out=$(game plain_game.exe)
check "harness survived" "grep -q 'loaded' harness.out"
check "the four parts reported" "log_has 'PROBLEM: The RPF cache check is not skipped' && log_has 'PROBLEM: Files in the mods folder are not used' && log_has 'PROBLEM: Unencrypted (OPEN) RPF archives cannot be loaded' && log_has 'PROBLEM: The newmods folder is not mounted'"
check "one start-up summary for both moments (shown after the one-time start-up)" "[ \$(count 'Start-up summary not shown (SHOW_ERROR_POPUPS=0)') -eq 1 ]"

echo "== R17 LOG_MODS=1 / LOG_DEVICE=1: RageOpenV's own lines appear"
fresh_ini; setkey LOG_MODS 1 RAGEOPENV_SETTINGS; setkey LOG_DEVICE 1 RAGEOPENV_SETTINGS; out=$(game full_game.exe)
check "a redirected file is logged" "log_has '\[mods\]\[RageOpenV::OpenBulkImpl\] Redirecting to /mods/test/file.txt'"
check "the device mounts are logged" "log_has '\[device\].*fiDevice::Mount\] platform:/' && log_has '\[device\]\[RageOpenV::InitialMountHook\] Mods path:'"

echo "== R18 FreeLibrary while hooked: TweaksV.asi stays loaded (pinned), no crash afterwards"
fresh_ini; cp plain_game.exe GTA5_Enhanced.exe; LOCALAPPDATA="$AW" timeout 60 ./GTA5_Enhanced.exe TweaksV.asi unload > harness.out 2>&1
check "loaded and 'unloaded' without a crash" "grep -q 'loaded and unloaded' harness.out"

echo
echo "RAGEOPENV SCENARIOS: $pass passed, $fail failed"
