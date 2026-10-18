#!/usr/bin/env bash
# Phase E (integration and hardening) scenarios in the fake game: old plugins present (E1, T-NEG-2/3),
# one pattern deliberately broken per plugin with the TEST build (T-NEG-1), another game exe (T-NEG-5),
# no C runtime DLLs (T-NEG-6), a renamed .asi (T-NEG-7), no code changes allowed (T-NEG-8),
# DIAGNOSTIC_MODE without a newmods folder.
# Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata mods/test newmods scripts plugins
echo hi > mods/test/file.txt

pass=0; fail=0
check() { if eval "$2"; then echo "PASS  $1"; pass=$((pass+1)); else echo "FAIL  $1"; fail=$((fail+1)); fi; }
setkey() { sed -b -i "0,/^$1=.*\r\$/s//$1=$2\r/" TweaksV.ini; }
addkey() { sed -b -i "/^\[TWEAKSV\]\r\$/a $1=$2\r" TweaksV.ini; }	# a key the generated INI does not have
fresh_ini() { sed 's/$/\r/' "$T/base.ini" > TweaksV.ini; setkey SHOW_ERROR_POPUPS 0; mkdir -p newmods; }
game() { local asi="$1"; shift; cp "$1" GTA5_Enhanced.exe; shift; rm -f harness.out TweaksV.log; LOCALAPPDATA="$AW" timeout 60 ./GTA5_Enhanced.exe "$asi" "$@" > harness.out 2>&1; tr -d '\r' < harness.out; }
log_has() { grep -q -- "$1" TweaksV.log; }
count() { grep -c -- "$1" TweaksV.log; }
has() { [[ "$out" == *"$1"* ]]; }
ALL_ON_HEAP='heap: 1024 MB'
GAME_HEAP='heap: 648 MB'
STORY_ON='splash: E9 86 01 00 00 90 | legal: 90 90 90 90 90 90 | landing: 31 C0 C3 24 08'
STORY_OFF='splash: 0F 84 85 01 00 00 | legal: 0F 84 07 05 00 00 | landing: 48 89 5C 24 08'
MODKIT_ON='modkit refs: new array (RW, 128 KB) | limits: 65535 65539'
MODKIT_OFF='modkit refs: game array | limits: 1023 1027'
PACK_ON='pack refs: 30 new 0 game 0 other, R31a new, R31b game (RW, 512 KB) | pack sizes: D3=12000'
PACK_OFF='pack refs: 0 new 30 game 0 other, R31a game, R31b game | pack sizes: D3=4500 D2=4500 D1=4500 I=4500 | hook: not set'
ROV_ON='file hooks 8/8, redirect 8/8, pass 8/8, spellings ok, folder ok, outside ok | open calls 5/5, behaviour ok | newmods hooks 2/2'
ROV_OFF='rov api hook: no'
summary_once() { [ "$(count 'Start-up summary not shown (SHOW_ERROR_POPUPS=0)')" -eq 1 ]; }

echo "== E1 / T-NEG-2 each old plugin on its own: only its part is skipped"
for old in HeapAdjuster.asi ZHeapAdjuster.asi ModkitLimitAdjusterEnhanced.asi PackfileLimitAdjusterEnhanced.asi RageOpenV.asi OpenRPF.asi OpenIV.asi StraightToStoryMode.asi DirectStorageFix.asi; do
	fresh_ini; touch "$old"; out=$(game TweaksV.asi full_game.exe); rm -f "$old"
	case "$old" in
		*HeapAdjuster.asi) ok="has '$GAME_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'" ;;
		Modkit*) ok="has '$ALL_ON_HEAP' && has '$MODKIT_OFF' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'" ;;
		Packfile*) ok="has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_OFF' && has '$ROV_ON' && has '$STORY_ON'" ;;
		RageOpenV.asi|OpenRPF.asi|OpenIV.asi) ok="has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_OFF' && has '$STORY_ON'" ;;
		StraightToStoryMode.asi) ok="has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_OFF'" ;;
		DirectStorageFix.asi) ok="has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON' && ! log_has '\[DirectStorage\]'" ;;
	esac
	check "$old: its part skipped, the rest applied, reported once in the summary" "$ok && log_has 'Old plugin found: .*$old' && summary_once"
done

echo "== E1 / T-NEG-2 all old plugins at once, in the game folder, scripts\\ and plugins\\: everything skipped"
fresh_ini
touch HeapAdjuster.asi ModkitLimitAdjusterEnhanced.asi plugins/PackfileLimitAdjusterEnhanced.asi scripts/OpenRPF.asi StraightToStoryMode.asi
out=$(game TweaksV.asi full_game.exe); echo "      $out"
rm -f HeapAdjuster.asi ModkitLimitAdjusterEnhanced.asi plugins/PackfileLimitAdjusterEnhanced.asi scripts/OpenRPF.asi StraightToStoryMode.asi
check "nothing applied" "has '$GAME_HEAP' && has '$MODKIT_OFF' && has '$PACK_OFF' && has '$ROV_OFF' && has '$STORY_OFF'"
check "all five reported, one summary with 5 problems, no crash" "[ \$(count 'PROBLEM: Old plugin found') -eq 5 ] && log_has 'Start-up summary not shown (SHOW_ERROR_POPUPS=0): 5 problem' && summary_once"

echo "== T-NEG-1 TEST build, one pattern broken per plugin: only that part is skipped, one summary"
breaktest() {	# label, pattern, expected-output check
	fresh_ini; addkey TEST_BREAK_PATTERN "$2"; out=$(game TweaksV_test.asi full_game.exe)
	check "$1" "$3 && log_has 'TEST BUILD - not for release' && log_has 'TEST BUILD: treated as not found' && summary_once && has 'protect: all restored'"
}
breaktest "H-E1 (heap): heap unchanged, the rest applied" "BA ? ? ? ? 45 31 C0 41 B1 01 E8 ? ? ? ? 48 8D 0D" \
	"has '$GAME_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'"
breaktest "M-E-R3 (a modkit reference): modkit untouched, the rest applied" "41 0f b7 c2 48 8d 0d ? ? ? ? 0f b7 04 41" \
	"has '$ALL_ON_HEAP' && has '$MODKIT_OFF' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'"
breaktest "M-E-L6 (a modkit limit): array moved, limits as in the game - harmless" "41 b8 ? ? ? ? b2 ? e8 ? ? ? ? 66 0f 76 c0" \
	"has 'modkit refs: new array (RW, 128 KB) | limits: 1023 1027 1027 1027 1027 2048' && has '$PACK_ON' && has '$ROV_ON'"
breaktest "P-E-R12 (a packfile reference): packfile untouched, the rest applied" "48 8d 0d ? ? ? ? 48 8b 0c 08 48 8b 01 ff 90 ? ? ? ? 48 8b 48" \
	"has '$PACK_OFF' && has '$MODKIT_ON' && has '$ROV_ON' && has '$STORY_ON'"
breaktest "P-E-H1 (the hooked call): array moved, limits as in the game, no hook - harmless" "89 48 ? 48 8d 0d ? ? ? ? 48 89 08 48 89 c1 e8" \
	"has 'pack refs: 30 new 0 game 0 other' && has 'pack sizes: D3=4500 D2=4500 D1=4500 I=4500 | hook: not set' && has '$ROV_ON'"
breaktest "R-E-F5 (a file function): no file hooks, OPEN and newmods on" "41 57 41 56 56 57 53 48 81 ec 60 02 00 00 48 89 d3 48 89 cf" \
	"has 'file hooks 0/8 | open calls 5/5, behaviour ok | newmods hooks 2/2' && has '$PACK_ON'"
breaktest "R-E-E3 (an encryption call): no OPEN calls, files and newmods on" "e8 ? ? ? ? 48 8b 46 ? 66 c7 00 00 ? eb" \
	"has 'file hooks 8/8, redirect 8/8, pass 8/8, spellings ok, folder ok, outside ok | open calls 0/5 | newmods hooks 2/2'"
breaktest "R-E-M1 (the mount call): no newmods hooks, files and OPEN on" "e8 ? ? ? ? 48 8d 05 ? ? ? ? 66 c7 44 58 02 00 00 66 89 3d" \
	"has 'file hooks 8/8, redirect 8/8, pass 8/8, spellings ok, folder ok, outside ok | open calls 5/5, behaviour ok | newmods hooks 0/2'"
breaktest "S-E2 (the legal screen): only that screen is left, the others skipped" "00 E9 ? 05 00 00 E8 ? 07" \
	"has 'splash: E9 86 01 00 00 90 | legal: 0F 84 07 05 00 00 | landing: 31 C0 C3 24 08' && has '$ROV_ON'"
breaktest "spacing, case and ?? do not matter" "ba ?? ?? ? ?  45 31 c0 41 b1 01 e8 ? ? ? ? 48 8d 0d" \
	"has '$GAME_HEAP' && has '$MODKIT_ON'"
fresh_ini; addkey TEST_BREAK_PATTERN "BA ? ? ? ? 45 31 C0 41 B1 01 E8 ? ? ? ? 48 8D 0D"; out=$(game TweaksV.asi full_game.exe)
check "the release build ignores TEST_BREAK_PATTERN" "has '$ALL_ON_HEAP' && ! log_has 'TEST BUILD' && ! log_has 'PROBLEM'"

echo "== T-NEG-5 loaded by another exe (GTA5.exe): TweaksV does nothing and creates nothing"
rm -f TweaksV.ini TweaksV.log; rm -rf appdata/TweaksV
cp full_game.exe GTA5.exe; LOCALAPPDATA="$AW" timeout 60 ./GTA5.exe TweaksV.asi > harness.out 2>&1; out=$(tr -d '\r' < harness.out); rm -f GTA5.exe
check "nothing applied" "has '$GAME_HEAP' && has '$MODKIT_OFF' && has '$PACK_OFF' && has '$ROV_OFF' && has '$STORY_OFF'"
check "no TweaksV.ini or TweaksV.log anywhere" "[ ! -e TweaksV.ini ] && [ ! -e TweaksV.log ] && [ ! -e appdata/TweaksV ]"

echo "== T-NEG-6 no Visual C++ runtime needed (/MT): only system DLLs imported"
deps=$(cmd //c "$(cygpath -w "$T/deps.cmd")" "$(cygpath -w TweaksV.asi)" 2>/dev/null | tr -d '\r' | grep -i '\.dll' | tr -d ' ' | sort | tr '\n' ' ')
echo "      imports: $deps"
check "KERNEL32, USER32 and VERSION only" "[ \"$deps\" = 'KERNEL32.dll USER32.dll VERSION.dll ' ]"

echo "== T-NEG-7 renamed to zTweaksV.asi: the same TweaksV.ini and TweaksV.log, everything works"
fresh_ini; cp TweaksV.asi zTweaksV.asi; out=$(game zTweaksV.asi full_game.exe); rm -f zTweaksV.asi
check "everything applied" "has '$ALL_ON_HEAP' && has '$MODKIT_ON' && has '$PACK_ON' && has '$ROV_ON' && has '$STORY_ON'"
check "TweaksV.log names zTweaksV.asi, no problems" "log_has 'TweaksV.asi: .*zTweaksV.asi' && ! log_has 'PROBLEM'"

echo "== T-NEG-8 no code may be changed (Windows' dynamic code policy): every write fails whole, each part reported"
fresh_ini; out=$(game TweaksV.asi full_game.exe nodyncode); echo "      $out"
check "harness survived to its report, the policy was set" "has 'protect: ' && ! has 'dynamic code policy not set'"
check "nothing written: game code, protection and call targets as they were" "has '$GAME_HEAP' && has '$STORY_OFF' && has '$MODKIT_OFF' && has '$PACK_OFF' && has 'stubs: 0, all RX' && has 'protect: all restored (RX)'"
check "every part reported" "log_has 'PROBLEM: The heap size could not be changed' && [ \$(count 'PROBLEM: The .* could not be patched') -eq 3 ] && log_has 'PROBLEM: .*modkit' && log_has 'PROBLEM: The packfile limit is not changed' && log_has 'PROBLEM: RageOpenV could not start'"
check "one start-up summary" "summary_once"

echo "== E2 DIAGNOSTIC_MODE without a newmods folder: the newmods patterns are still checked, nothing mounted"
fresh_ini; setkey DIAGNOSTIC_MODE 1; rmdir newmods; out=$(game TweaksV.asi full_game.exe); mkdir -p newmods
check "newmods not hooked, the rest applied" "has 'newmods hooks 0/2' && has 'file hooks 8/8' && has 'open calls 5/5'"
check "its 5 patterns found once each and logged, no problem" "[ \$(grep -c -E 'Pattern found: (e8 \? \? \? \? 48 8d 05|41 57 41 56 41 55 41 54|56 57 53 48 81 ec \? \? \? \? 48 83 79|56 57 53 48 81 ec \? \? \? \? 4c 89 cf).* 1 match,' TweaksV.log) -eq 5 ] && log_has 'the patterns were checked (DIAGNOSTIC_MODE), nothing is mounted' && ! log_has 'PROBLEM'"

echo
echo "PHASE E SCENARIOS: $pass passed, $fail failed"
