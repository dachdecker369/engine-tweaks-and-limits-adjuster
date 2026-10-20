#!/usr/bin/env bash
# TweaksV as a whole, in the fake game: old plugins present, one pattern deliberately broken per
# plugin with the TEST build, another game exe, no C runtime DLLs, a renamed .asi, no code changes
# allowed, the user name kept out of the log, the log's limit, links, a read-only game folder, a held log, the code dump.
# Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata mods/test scripts plugins
echo hi > mods/test/file.txt

ALL_ON_HEAP='heap: 1024 MB'
GAME_HEAP='heap: 648 MB'
STORY_ON='splash: E9 86 01 00 00 90 | legal: 90 90 90 90 90 90 | landing: 31 C0 C3 24 08'
STORY_OFF='splash: 0F 84 85 01 00 00 | legal: 0F 84 07 05 00 00 | landing: 48 89 5C 24 08'
MODKIT_ON='modkit refs: new array (RW, 128 KB) | limits: 65535 65539'
MODKIT_OFF='modkit refs: game array | limits: 1023 1027'
PACK_ON='pack refs: 30 new 0 game 0 other, R31a new, R31b game (RW, 512 KB) | pack sizes: D3=12000'
PACK_OFF='pack refs: 0 new 30 game 0 other, R31a game, R31b game | pack sizes: D3=4500 D2=4500 D1=4500 I=4500 | hook: not set'
BOX_ON='box refs: 8 new 0 game 0 other (RW, 784 KB) | box caps: 10000 10000 10000 10000'
BOX_OFF='box refs: 0 new 8 game 0 other | box caps: 1000 1000 1000 1000'
DECAL_ON='decal refs: 31 new 0 game 0 other (RW, 1280 KB) | decal limits: 2047 2048 0x28198, 23 agree'
DECAL_OFF='decal refs: 0 new 31 game 0 other | decal limits: 259 260 0x5918, 23 agree'
ROV_ON='file hooks 8/8, redirect 8/8, pass 8/8, spellings ok, folder ok, outside ok | open calls 5/5, behaviour ok'
ROV_OFF='rov api hook: no'
summary_once() { [ "$(count 'Start-up summary not shown (ERROR_POPUP=0)')" -eq 1 ]; }

echo "== I1 each old plugin on its own: only its part is skipped"
for old in HeapAdjuster.asi ZHeapAdjuster.asi ModkitLimitAdjusterEnhanced.asi PackfileLimitAdjusterEnhanced.asi DecalPatch.asi RageOpenV.asi OpenRPF.asi OpenIV.asi StraightToStoryMode.asi DirectStorageFix.asi; do
	fresh_ini; touch "$old"; out=$(game full_game.exe); rm -f "$old"
	case "$old" in
		*HeapAdjuster.asi) ok="has '$GAME_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$BOX_ON' && has '$DECAL_ON' && has '$ROV_ON' && has '$STORY_ON'" ;;
		DecalPatch.asi) ok="has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$BOX_OFF' && has '$DECAL_OFF' && has '$ROV_ON' && has '$STORY_ON' && [ \$(count 'Old plugin found: .*DecalPatch.asi') -eq 2 ]" ;;
		Modkit*) ok="has '$ALL_ON_HEAP' && has '$MODKIT_OFF' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'" ;;
		Packfile*) ok="has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_OFF' && has '$ROV_ON' && has '$STORY_ON'" ;;
		RageOpenV.asi|OpenRPF.asi|OpenIV.asi) ok="has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_OFF' && has '$STORY_ON'" ;;
		StraightToStoryMode.asi) ok="has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_OFF'" ;;
		DirectStorageFix.asi) ok="has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON' && ! log_has '\[DirectStorage\]'" ;;
	esac
	check "$old: its part skipped, the rest applied, reported once in the summary" "$ok && log_has 'Old plugin found: .*$old' && summary_once"
done

echo "== I2 all old plugins at once, in the game folder, scripts\\ and plugins\\: everything skipped"
fresh_ini
touch HeapAdjuster.asi ModkitLimitAdjusterEnhanced.asi plugins/PackfileLimitAdjusterEnhanced.asi scripts/OpenRPF.asi StraightToStoryMode.asi scripts/DecalPatch.asi
out=$(game full_game.exe); echo "      $out"
rm -f HeapAdjuster.asi ModkitLimitAdjusterEnhanced.asi plugins/PackfileLimitAdjusterEnhanced.asi scripts/OpenRPF.asi StraightToStoryMode.asi scripts/DecalPatch.asi
check "nothing applied: the six old plugins switch every part off" "has '$GAME_HEAP' && has '$MODKIT_OFF' && has '$PACK_OFF' && has '$ROV_OFF' && has '$STORY_OFF' && has '$BOX_OFF' && has '$DECAL_OFF'"
check "all six reported (DecalPatch.asi for two parts), one summary with 7 problems, no crash" "[ \$(count 'PROBLEM: Old plugin found') -eq 7 ] && log_has 'Start-up summary not shown (ERROR_POPUP=0): 7 problem' && summary_once"

echo "== I3 TEST build, one pattern broken per plugin: only that part is skipped, one summary"
breaktest() {	# description, the pattern's label, expected-output check
	fresh_ini; addkey TEST_BREAK_PATTERN "$2"; out=$(ASI=TweaksV_test.asi game full_game.exe)
	check "$1" "$3 && log_has 'TEST BUILD - not for release' && log_has 'TEST BUILD: pattern [A-Z].* is treated as not found' && summary_once && has 'protect: all restored'"
}
breaktest "H-E1 (heap): heap unchanged, the rest applied" "H-E1" \
	"has '$GAME_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'"
breaktest "M-E-R3 (a modkit reference): modkit untouched, the rest applied" "M-E-R3" \
	"has '$ALL_ON_HEAP' && has '$MODKIT_OFF' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'"
breaktest "M-E-L6 (a modkit limit): modkit untouched, the rest applied" "M-E-L6" \
	"has '$ALL_ON_HEAP' && has '$MODKIT_OFF' && has '$PACK_ON' && has '$ROV_ON'"
breaktest "P-E-R12 (a packfile reference): packfile untouched, the rest applied" "P-E-R12" \
	"has '$PACK_OFF' && has '$MODKIT_ON' && has '$ROV_ON' && has '$STORY_ON'"
breaktest "P-E-H1 (the streaming configuration call): packfile untouched, the rest applied" "P-E-H1" \
	"has '$PACK_OFF' && has '$MODKIT_ON' && has '$ROV_ON'"
breaktest "B-E-L2 (a box streamer capacity): list untouched, the rest applied" "B-E-L2" \
	"has '$BOX_OFF' && has '$DECAL_ON' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'"
breaktest "D-E-L3 (a decal definitions site): array untouched, the rest applied" "D-E-L3" \
	"has '$DECAL_OFF' && has '$BOX_ON' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'"
breaktest "D-E-L12 (the shared 'add decal' check, needed 7 times): array untouched, the rest applied" "D-E-L12" \
	"has '$DECAL_OFF' && has '$BOX_ON' && has '$ROV_ON' && log_has 'The decal definitions limit is not raised this session: D-E-L12 was not found.'"
breaktest "R-E-F5 (a file function): no file hooks, OPEN on" "R-E-F5" \
	"has 'file hooks 0/8 | open calls 5/5, behaviour ok' && has '$PACK_ON'"
breaktest "R-E-E3 (an encryption call): no OPEN calls, files on" "R-E-E3" \
	"has 'file hooks 8/8, redirect 8/8, pass 8/8, spellings ok, folder ok, outside ok | open calls 0/5'"
breaktest "S-E2 (the legal screen): only that screen is left, the others skipped" "S-E2" \
	"has 'splash: E9 86 01 00 00 90 | legal: 0F 84 07 05 00 00 | landing: 31 C0 C3 24 08' && has '$ROV_ON'"
breaktest "a label in small letters works too" "h-e1" \
	"has '$GAME_HEAP' && has '$MODKIT_ON'"
fresh_ini; addkey TEST_BREAK_PATTERN "H-E1"; out=$(game full_game.exe)
check "the release build ignores TEST_BREAK_PATTERN" "has '$ALL_ON_HEAP' && ! log_has 'TEST BUILD' && ! log_has 'PROBLEM'"

echo "== I4 loaded by another exe (GTA5.exe): TweaksV does nothing and creates nothing"
rm -f TweaksV.ini TweaksV.log; rm -rf appdata/TweaksV
cp full_game.exe GTA5.exe; LOCALAPPDATA="$AW" timeout 60 ./GTA5.exe TweaksV.asi > harness.out 2>&1; out=$(tr -d '\r' < harness.out); rm -f GTA5.exe
check "nothing applied" "has '$GAME_HEAP' && has '$MODKIT_OFF' && has '$PACK_OFF' && has '$BOX_OFF' && has '$DECAL_OFF' && has '$ROV_OFF' && has '$STORY_OFF'"
check "no TweaksV.ini or TweaksV.log anywhere" "[ ! -e TweaksV.ini ] && [ ! -e TweaksV.log ] && [ ! -e appdata/TweaksV ]"

echo "== I5 the build: no Visual C++ runtime needed (/MT), only system DLLs imported, the exploit mitigations on"
deps=$(cmd //c "$(cygpath -w "$T/deps.cmd")" "$(cygpath -w TweaksV.asi)" 2>/dev/null | tr -d '\r' | grep -i '\.dll' | tr -d ' ' | sort | tr '\n' ' ')
echo "      imports: $deps"
check "KERNEL32, USER32 and VERSION only" "[ \"$deps\" = 'KERNEL32.dll USER32.dll VERSION.dll ' ]"
heads=$(cmd //c "$(cygpath -w "$T/headers.cmd")" "$(cygpath -w TweaksV.asi)" 2>/dev/null | tr -d '\r')
check "ASLR with high entropy, DEP, Control Flow Guard and CET" "grep -q 'High Entropy Virtual Addresses' <<< \"\$heads\" && grep -q 'Dynamic base' <<< \"\$heads\" && grep -q 'NX compatible' <<< \"\$heads\" && grep -q 'Control Flow Guard' <<< \"\$heads\" && grep -q 'CET compatible' <<< \"\$heads\""

echo "== I6 renamed to zTweaksV.asi: the same TweaksV.ini and TweaksV.log, everything works"
fresh_ini; cp TweaksV.asi zTweaksV.asi; out=$(ASI=zTweaksV.asi game full_game.exe); rm -f zTweaksV.asi
check "everything applied" "has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$BOX_ON' && has '$DECAL_ON' && has '$ROV_ON' && has '$STORY_ON'"
check "TweaksV.log names zTweaksV.asi, no problems" "log_has 'TweaksV.asi: .*zTweaksV.asi' && ! log_has 'PROBLEM'"

echo "== I7 no code may be changed (Windows' dynamic code policy): every write fails whole, each part reported"
fresh_ini; out=$(game full_game.exe nodyncode); echo "      $out"
check "harness survived to its report, the policy was set" "has 'protect: ' && ! has 'dynamic code policy not set'"
check "nothing written: game code, protection and call targets as they were" "has '$GAME_HEAP' && has '$STORY_OFF' && has '$MODKIT_OFF' && has '$PACK_OFF' && has '$BOX_OFF' && has '$DECAL_OFF' && has 'stubs: 0, all RX' && has 'protect: all restored (RX)'"
check "every part reported" "log_has 'PROBLEM: The heap size is not changed this session: the game code could not be made writable' && [ \$(count 'is not skipped this session: the game code could not be made writable') -eq 3 ] && log_has 'PROBLEM: .*modkit' && log_has 'PROBLEM: The packfile limit is not changed' && log_has 'PROBLEM: The box streamer.s traversal list is not enlarged' && log_has 'PROBLEM: The decal definitions limit is not raised' && log_has 'PROBLEM: RageOpenV could not start'"
check "one start-up summary" "summary_once"

echo "== I9 the Windows user name stays out of the log: the profile folder is written as %USERPROFILE%, in both its forms"
bw=$(cygpath -w "$B")	# the game folder, here the profile folder too
fresh_ini; out=$(USERPROFILE="$bw" game full_game.exe)
check "the paths in it start with %USERPROFILE%, the folder's own name is nowhere in the log" "log_has 'TweaksV.asi: %USERPROFILE%.TweaksV.asi' && ! grep -qiF -- \"\$bw\" TweaksV.log"
# A profile folder with a long name has a short one too (C:\Users\NAME~1), which %TEMP% often has. Here
# %LOCALAPPDATA% has it, and the log header names the TweaksV.ini there that the game folder's wins over.
pl="$B/Some Long User Name"; mkdir -p "$pl/AppData/TweaksV"; cp TweaksV.ini "$pl/AppData/TweaksV/"
pw=$(cygpath -w "$pl"); ps=$(cygpath -d "$pl")
cp full_game.exe GTA5_Enhanced.exe; rm -f TweaksV.log
USERPROFILE="$pw" LOCALAPPDATA="$ps\\AppData" timeout 60 ./GTA5_Enhanced.exe TweaksV.asi > harness.out 2>&1
check "its short form as well: the log names %USERPROFILE%\\AppData\\TweaksV, neither form is in it" "[ \"\$ps\" != \"\$pw\" ] && log_has 'wins over the unused copy in %USERPROFILE%.AppData.TweaksV)' && ! grep -qiF -- \"\$ps\" TweaksV.log && ! grep -qiF -- \"\$pw\" TweaksV.log"
rm -rf "$pl"

echo "== I10 TweaksV.log stops growing at its limit (16 KB here, set with TEST_LOG_LIMIT_KB in a test build)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; addkey TEST_LOG_LIMIT_KB 16; out=$(ASI=TweaksV_test.asi game full_game.exe)
check "at most 16 KB, ending with the line that says so" "[ \$(stat -c %s TweaksV.log) -le 16384 ] && tail -n 1 TweaksV.log | grep -q 'TweaksV.log has reached its limit of 16 KB - nothing more is written to it this session.'"
check "the game is patched all the same" "has '$ALL_ON_HEAP' && has '$STORY_ON' && has '$ROV_ON'"

# Where TweaksV's own files belong: a link, a compressed file, a read-only folder, a file in use.
# link_run [OPTION...] runs the fake game without game(), which would delete TweaksV.log.
link_run() { cp full_game.exe GTA5_Enhanced.exe; rm -f harness.out; LOCALAPPDATA="$AW" timeout 60 ./GTA5_Enhanced.exe TweaksV.asi "$@" > harness.out 2>&1; out=$(tr -d '\r' < harness.out); }

echo "== I11 TweaksV.log in the game folder is a hard link to another file: that file is left alone"
fresh_ini; rm -f TweaksV.log; rm -rf appdata/TweaksV; echo "not the log" > victim.txt; ln victim.txt TweaksV.log
link_run
check "the other file is unchanged" "[ \"\$(cat victim.txt)\" = 'not the log' ]"
check "the log goes to %LOCALAPPDATA%\\TweaksV and says why; the game is patched" "grep -q 'PROBLEM: The TweaksV.log in the game folder is not used: it is a hard link' appdata/TweaksV/TweaksV.log && has '$ALL_ON_HEAP'"
rm -f TweaksV.log victim.txt; rm -rf appdata/TweaksV

echo "== I12 TweaksV.ini in the game folder is a hard link to another file: neither read nor written"
fresh_ini; setkey SKIP_SPLASH_SCREEN 0 STRAIGHTTOSTORYMODE; sed -b -i '/^HEAP_SIZE=/d' TweaksV.ini
mv TweaksV.ini victim.ini; cp victim.ini victim.before; ln victim.ini TweaksV.ini
out=$(ASI=TweaksV_test.asi game full_game.exe)
check "the other file is unchanged - its missing key was not added" "cmp -s victim.ini victim.before"
check "reported; every setting at its built-in default, not the file's (the splash screen is skipped)" "log_has 'PROBLEM: TweaksV.ini (.*) is not used: it is a hard link' && has 'splash: E9 86 01 00 00 90' && has 'heap: 2000 MB'"
check "the log header says so" "log_has 'TweaksV.ini: .*(not used: it is a hard link'"
rm -f TweaksV.ini victim.ini victim.before

echo "== I13 and %LOCALAPPDATA%\\TweaksV is a junction to another folder: nothing is written through it"
fresh_ini; rm -f TweaksV.log; rm -rf appdata/TweaksV elsewhere; mkdir -p elsewhere; echo "not the log" > victim.txt; ln victim.txt TweaksV.log
cmd //c "$(cygpath -w "$T/junction.cmd")" "$(cygpath -w "$B/appdata/TweaksV")" "$(cygpath -w "$B/elsewhere")"
link_run
check "the junction is there; nothing was left in its target, the other file unchanged" "[ -d appdata/TweaksV ] && [ -z \"\$(ls -A elsewhere)\" ] && [ \"\$(cat victim.txt)\" = 'not the log' ]"
check "the game is patched all the same" "has '$ALL_ON_HEAP' && has '$STORY_ON'"
cmd //c "$(cygpath -w "$T/junction.cmd")" remove "$(cygpath -w "$B/appdata/TweaksV")"
check "the junction is removed again - the link only, its target is still there" "[ ! -e appdata/TweaksV ] && [ -d elsewhere ]"
rm -f TweaksV.log victim.txt; rm -rf elsewhere

# Windows keeps a compressed file as a reparse point, as it does a link, but shows it as a plain file.
echo "== I14 TweaksV.ini and TweaksV.log compressed by Windows (compact /exe, as CompactGUI does): both used as usual"
fresh_ini; rm -rf appdata/TweaksV
for i in $(seq 100); do printf '; a comment line, so that Windows has something to compress\r\n'; done >> TweaksV.ini
for i in $(seq 400); do printf '12:00:00.000 [TweaksV] a line of an older log\r\n'; done > TweaksV.log
MSYS_NO_PATHCONV=1 compact /c /exe:xpress4k "$(cygpath -w TweaksV.ini)" "$(cygpath -w TweaksV.log)" > /dev/null
compressed=$(MSYS_NO_PATHCONV=1 compact "$(cygpath -w TweaksV.ini)" "$(cygpath -w TweaksV.log)" | tr -d '\r')
link_run
check "both files were compressed" "[[ \"\$compressed\" == *'2 are compressed and 0 are not compressed'* ]]"
check "TweaksV.ini is read (its HEAP_SIZE, not the default), TweaksV.log rewritten in the game folder, no problem" "has '$ALL_ON_HEAP' && log_has '\[TweaksV\] TweaksV [0-9]' && ! log_has 'an older log' && ! log_has 'PROBLEM' && [ ! -e appdata/TweaksV ]"
rm -f TweaksV.log

# Read-only as under C:\Program Files: a deny entry for this user on the folder alone (not on the files
# in it) lets nothing be added to it. The test build, as TweaksV.ini starts out with ERROR_POPUP=1.
echo "== I15 a read-only game folder (as under C:\\Program Files): both files in %LOCALAPPDATA%\\TweaksV, only logged"
rm -rf ro appdata/TweaksV; mkdir -p ro; cp full_game.exe ro/GTA5_Enhanced.exe; cp TweaksV_test.asi ro/TweaksV.asi
sid=$(MSYS_NO_PATHCONV=1 "$(cygpath -S)/whoami.exe" /user /fo csv /nh | tr -d '\r' | sed -n 's/.*,"\(S-[0-9-]*\)"$/\1/p')
MSYS_NO_PATHCONV=1 icacls "$(cygpath -w ro)" /deny "*$sid:(WD,AD)" > /dev/null
ro_run() { rm -f harness.out; (cd ro && LOCALAPPDATA="$AW" timeout 60 ./GTA5_Enhanced.exe TweaksV.asi > ../harness.out 2>&1); out=$(tr -d '\r' < harness.out); }
fb=appdata/TweaksV
check "the folder is read-only (a check of the test itself)" "[ -n '$sid' ] && ! touch ro/probe.txt 2>/dev/null"
ro_run
check "TweaksV.ini created there with the defaults, TweaksV.log written there: logged, no problem" "[ -f $fb/TweaksV.ini ] && grep -q 'The game folder is read-only (Windows error 5, access denied), so TweaksV.ini was created with the default settings in ' $fb/TweaksV.log && grep -q 'The game folder is read-only (Windows error 5, access denied), so TweaksV.log is written to ' $fb/TweaksV.log && ! grep -q 'PROBLEM' $fb/TweaksV.log"
check "nothing in the game folder; the game is patched with the defaults" "[ ! -e ro/TweaksV.ini ] && [ ! -e ro/TweaksV.log ] && has 'heap: 2000 MB' && has 'splash: E9 86 01 00 00 90'"
sed -b -i 's/^HEAP_SIZE=.*\r$/HEAP_SIZE=1500\r/; s/^ERROR_POPUP=.*\r$/ERROR_POPUP=0\r/' $fb/TweaksV.ini
ro_run
check "the next start reads that TweaksV.ini (HEAP_SIZE=1500) and says where it is" "has 'heap: 1500 MB' && grep -q 'TweaksV.ini: .*TweaksV.ini (the game folder has no TweaksV.ini)' $fb/TweaksV.log && ! grep -q 'PROBLEM' $fb/TweaksV.log"
MSYS_NO_PATHCONV=1 icacls "$(cygpath -w ro)" /remove:d "*$sid" > /dev/null
check "the folder is writable again (the test's clean-up)" "touch ro/probe.txt"
rm -rf ro appdata/TweaksV

echo "== I16 another program holds TweaksV.log in the game folder open: the log goes to %LOCALAPPDATA%\\TweaksV, a problem"
fresh_ini; rm -rf appdata/TweaksV; echo "an older log" > TweaksV.log
link_run lockedlog
check "the log was held, and is left as it was" "has 'log held' && [ \"\$(cat TweaksV.log)\" = 'an older log' ]"
check "the log is written to %LOCALAPPDATA%\\TweaksV, and the start-up summary says why; the game is patched" "grep -q 'PROBLEM: The TweaksV.log in the game folder is not used: Windows error 32, file in use. TweaksV.log is written to ' appdata/TweaksV/TweaksV.log && grep -q 'Start-up summary not shown (ERROR_POPUP=0): 1 problem' appdata/TweaksV/TweaksV.log && has '$ALL_ON_HEAP'"
rm -f TweaksV.log; rm -rf appdata/TweaksV

# Test builds only: the game's code as it is in memory, for finding patterns. The fake game's copies of
# the game code lie in .fakesit, its own data in .data - the part that must never be saved.
echo "== I17 TEST_DUMP_CODE in a test build: the game's code saved in %LOCALAPPDATA%\\TweaksV at start and later, never its data"
d=appdata/TweaksV
fresh_ini; setkey DRY_RUN 1; addkey TEST_DUMP_CODE 1; rm -rf $d
out=$(ASI=TweaksV_test.asi game full_game.exe wait)
check "both copies there, format 1, the second taken 1 s later; nothing in the game folder; the log header lists the switch" "[ \"\$(head -c 8 $d/GameCode-start.bin)\" = TWKVDUMP ] && [ \"\$(head -c 8 $d/GameCode-later.bin)\" = TWKVDUMP ] && [ \"\$(od -An -tu4 -j8 -N4 $d/GameCode-start.bin | tr -d ' ')\" = 1 ] && [ \"\$(od -An -tu4 -j32 -N4 $d/GameCode-later.bin | tr -d ' ')\" = 1 ] && [ ! -e GameCode-start.bin ] && log_has 'TEST BUILD - not for release. .* TEST_DUMP_CODE=1'"
check "the log names the sections saved, the code among them, and leaves out .data" "log_has 'TEST BUILD: GameCode-start.bin saved, the game as it was 0 s after TweaksV started, before anything was patched: .* - sections HEADERS .text .*fakesit' && log_has 'TEST BUILD: GameCode-later.bin saved, the game as it was 1 s after TweaksV started: ' && log_has 'not saved (the game writes to them):.* .data[ .]'"
check "the copy holds the game's code as TweaksV found it (the heap site), the game is unpatched (DRY_RUN), no problem" "od -An -tx1 -v $d/GameCode-start.bin | tr -d ' \n' | grep -q 'ba000080284531c041b101e811223344488d0d' && has '$GAME_HEAP' && ! log_has 'PROBLEM'"
fresh_ini; addkey TEST_DUMP_CODE 99999; rm -rf $d; out=$(ASI=TweaksV_test.asi game full_game.exe)
check "TEST_DUMP_CODE=99999 is a problem, and nothing is saved" "log_has 'PROBLEM: TweaksV.ini: \[TWEAKSV\] TEST_DUMP_CODE=99999 is not a number of seconds from 1 to 3600 - the game.s code is not saved.' && [ ! -e $d/GameCode-start.bin ]"
fresh_ini; addkey TEST_DUMP_CODE abc; rm -rf $d; out=$(ASI=TweaksV_test.asi game full_game.exe)
check "TEST_DUMP_CODE=abc as well" "log_has 'PROBLEM: TweaksV.ini: \[TWEAKSV\] TEST_DUMP_CODE=abc is not a number of seconds' && [ ! -e $d/GameCode-start.bin ]"
mkdir -p $d; echo "not a dump" > victim.txt; ln victim.txt $d/GameCode-start.bin
fresh_ini; setkey DRY_RUN 1; addkey TEST_DUMP_CODE 1; out=$(ASI=TweaksV_test.asi game full_game.exe wait)
check "GameCode-start.bin a hard link to another file: that file left alone, a problem; the later copy saved" "[ \"\$(cat victim.txt)\" = 'not a dump' ] && log_has 'PROBLEM: TEST_DUMP_CODE: GameCode-start.bin was not saved: it is a hard link' && [ -f $d/GameCode-later.bin ]"
rm -f victim.txt
fresh_ini; addkey TEST_DUMP_CODE 1; rm -rf $d; out=$(game full_game.exe)
check "the release build ignores TEST_DUMP_CODE: nothing saved, no problem" "[ ! -e $d/GameCode-start.bin ] && ! log_has 'TEST BUILD' && ! log_has 'PROBLEM' && has '$ALL_ON_HEAP'"
rm -rf $d

echo
echo "INTEGRATION SCENARIOS: $pass passed, $fail failed"
