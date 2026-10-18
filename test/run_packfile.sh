#!/usr/bin/env bash
# M4 (PackfileLimitAdjusterEnhanced) scenarios: a fake GTA5_Enhanced.exe with copies of the
# patched game code loads the real TweaksV.asi. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
mkdir -p appdata

pass=0; fail=0
check() { if eval "$2"; then echo "PASS  $1"; pass=$((pass+1)); else echo "FAIL  $1"; fail=$((fail+1)); fi; }
setkey() { sed -b -i "0,/^$1=.*\r\$/s//$1=$2\r/" TweaksV.ini; }
setpackkey() { sed -b -i "/^\[PACKFILE_SETTINGS\]/,/^\[/s/^$1=.*\r\$/$1=$2\r/" TweaksV.ini; }
fresh_ini() { sed 's/$/\r/' "$T/base.ini" > TweaksV.ini; setkey SHOW_ERROR_POPUPS 0; }
game() { cp "$1" GTA5_Enhanced.exe; shift; rm -f harness.out; LOCALAPPDATA="$AW" ./GTA5_Enhanced.exe TweaksV.asi "$@" > harness.out 2>&1; tr -d '\r' < harness.out; }
log_has() { grep -q -- "$1" TweaksV.log; }
count() { grep -c -- "$1" TweaksV.log; }
has() { [[ "$out" == *"$1"* ]]; }
MOVED='pack refs: 30 new 0 game 0 other, R31a new, R31b game (RW, 512 KB)'
UNMOVED='pack refs: 0 new 30 game 0 other, R31a game, R31b game |'
GAME_SIZES='pack sizes: D3=4500 D2=4500 D1=4500 I=4500'
NO_HOOK='hook: not set'

echo "== P1 defaults (game 1.0.1158.16): array moved, limits 12000, configuration count replaced"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "all 30 common references + the 1158.13+ one -> one new 512 KB read/write array; the older build's one untouched" "has '$MOVED'"
check "direct size (1158.13+ site) and indirect size 12000, the older builds' sites untouched" "has 'pack sizes: D3=12000 D2=4500 D1=4500 I=12000'"
check "hooked call replaces the configured 3000 with 12000 and calls the game's function with it" "has 'hook: config 3000 -> 12000, original called 1x with 12000'"
check "original log lines (4500 / 3000 / 12000)" "log_has 'Vanilla Packfile List Size is: 4500' && log_has 'Vanilla Packfile List Size From ConfigFile is: 3000' && log_has 'Packfile List Size Adjusted to: 12000'"
check "status line" "log_has 'Packfile array moved and its limits raised from 4500 to 12000'"
check "the other plugins still applied" "has 'heap: 1024 MB' && has 'splash: E9 86 01 00 00 90' && has 'modkit refs: new array (RW, 128 KB)'"
check "every patched page back to its own protection" "has 'protect: all restored'"
check "no problems" "! log_has 'PROBLEM'"

echo "== P2 PACKFILEARRAY_SIZE=20000"
fresh_ini; setpackkey PACKFILEARRAY_SIZE 20000; out=$(game full_game.exe); echo "      $out"
check "limits and configuration count 20000, array still full size" "has 'D3=20000 D2=4500 D1=4500 I=20000' && has 'config 3000 -> 20000' && has '$MOVED'"

echo "== P3 PACKFILEARRAY_SIZE=1000 is raised to the game's own 4500"
fresh_ini; setpackkey PACKFILEARRAY_SIZE 1000; out=$(game full_game.exe)
check "limits stay 4500, configuration count 4500" "has 'D3=4500 D2=4500 D1=4500 I=4500' && has 'config 3000 -> 4500'"
check "clamp logged" "log_has 'PACKFILEARRAY_SIZE=1000 is outside 4500..65535 - using 4500'"

echo "== P4 PACKFILEARRAY_SIZE=70000 is clamped to 65535"
fresh_ini; setpackkey PACKFILEARRAY_SIZE 70000; out=$(game full_game.exe)
check "limits 65535" "has 'D3=65535 D2=4500 D1=4500 I=65535' && has 'config 3000 -> 65535'"
check "clamp logged" "log_has 'PACKFILEARRAY_SIZE=70000 is outside 4500..65535 - using 65535'"

echo "== P5 PACKFILEARRAY_SIZE=abc falls back to the default 13000"
fresh_ini; setpackkey PACKFILEARRAY_SIZE abc; out=$(game full_game.exe)
check "limits 13000" "has 'D3=13000 D2=4500 D1=4500 I=13000'"
check "invalid value logged" "log_has 'PACKFILEARRAY_SIZE is not a valid size - using the default, 13000'"

echo "== P6 DRY_RUN=1: everything found and checked, nothing written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe); echo "      $out"
check "references, sizes and call untouched" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "both 'would' lines" "log_has 'DRY_RUN: would move the packfile array (31 references)' && log_has 'DRY_RUN: would change the packfile limits from 4500 to 12000 and hook the streaming configuration call at RVA'"

echo "== P7 DIAGNOSTIC_MODE=1: every pattern, reference and site logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "the 34 packfile patterns of this build each found exactly once" "[ \$(grep -c -E 'Pattern found: (48 83 C6|3b 05|89 48|48 8d 15|48 8d 3d \? \? \? \? 48 8b|89 c0 48 8d 0d|48 8d 0d|48 83 ec|48 c1 e8|48 89 2d|48 89 35|48 89 3d|BF).* 1 match,' TweaksV.log) -eq 34 ]"
check "31 references logged" "[ \$(count 'PackfileLimitAdjusterEnhanced\] DIAGNOSTIC: reference at RVA') -eq 31 ]"
check "direct size, indirect variable and call logged" "log_has 'DIAGNOSTIC: direct size at RVA .* holds 4500' && log_has 'DIAGNOSTIC: indirect size variable at RVA .* holds 4500' && log_has 'DIAGNOSTIC: streaming configuration call at RVA .* -> RVA'"
check "still applied" "has 'D3=12000 D2=4500 D1=4500 I=12000'"
check "patterns found once get no per-match lines (T2)" "! log_has ' match 1 of '"

echo "== P7b DIAGNOSTIC_MODE=1, a second identical copy of the 1158.13+ reference: each match logged with its bytes (T2)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe twor31a)
check "its line counts 2 matches" "log_has 'Pattern found: BF 08 00 00 00 BE 10 00 00 00 4C 8D 05 -> RVA 0x[0-9A-F]*, 2 matches, bytes: BF '"
check "one line per match: 16 bytes before it, | at its start, 48 bytes from it" "[ \$(tr -d '\r' < TweaksV.log | grep -c -E '\[MemScanner\]   match [12] of 2 at RVA 0x[0-9A-F]+:( [0-9A-F]{2}){16} \| BF( [0-9A-F]{2}){47}\$') -eq 2 ]"
check "both copies' bytes shown" "[ \$(count '| BF 08 00 00 00 BE 10 00 00 00 4C 8D 05 ') -eq 2 ]"
check "no other pattern gets per-match lines" "[ \$(count ' match 1 of ') -eq 1 ]"
check "the array still moved, no problem" "has 'pack refs: 30 new 0 game 0 other' && ! log_has 'PROBLEM'"

echo "== P7c the game's obfuscated code that the old 1158.13+ pattern also matched (RVA 0x582A227): not matched now (T2)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe junkr31a)
check "found exactly once, no per-match lines" "log_has 'Pattern found: BF 08 00 00 00 BE 10 00 00 00 4C 8D 05 -> RVA 0x[0-9A-F]*, 1 match,' && ! log_has ' match 1 of '"
check "the array moved, no problem" "has '$MOVED' && ! log_has 'PROBLEM'"

echo "== P8 [PACKFILE_SETTINGS] ENABLED=0"
fresh_ini; setpackkey ENABLED 0; out=$(game full_game.exe)
check "nothing changed" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "no packfile lines" "! log_has '\[PackfileLimitAdjusterEnhanced\]'"

echo "== P9 an old PackfileLimitAdjusterEnhanced.asi in the folder: skipped"
fresh_ini; touch PackfileLimitAdjusterEnhanced.asi; out=$(game full_game.exe); rm -f PackfileLimitAdjusterEnhanced.asi
check "nothing changed" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "old plugin reported" "log_has 'Old plugin found: .*PackfileLimitAdjusterEnhanced.asi'"

echo "== P10 another mod already moved the array: nothing written"
fresh_ini; out=$(game full_game.exe packmoved); echo "      $out"
check "references untouched (one elsewhere), sizes and call untouched" "has 'pack refs: 0 new 29 game 1 other' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "reason logged and reported" "log_has 'do not all point to the game.s own array: RVA .* and 0x.* (outside the game)' && log_has 'PROBLEM: The packfile limit is not changed'"
check "every page back to its own protection" "has 'protect: all restored'"

echo "== P11 another mod already hooked the streaming configuration call: limits left alone"
fresh_ini; out=$(game full_game.exe packhooked); echo "      $out"
check "the array was moved (the earlier step), sizes untouched, the other mod's call kept" "has '$MOVED' && has '$GAME_SIZES' && has \"hook: other mod's\""
check "reason logged and reported" "log_has 'is not a call into the game (another mod may have hooked it already)' && log_has 'PROBLEM: The packfile limit is not changed'"

echo "== P12 implausible direct size (70000): limits left alone"
fresh_ini; out=$(game full_game.exe packbadsize); echo "      $out"
check "array moved, direct sizes as they were, indirect untouched, call untouched" "has '$MOVED' && has 'pack sizes: D3=70000 D2=70000 D1=70000 I=4500' && has '$NO_HOOK'"
check "reason logged" "log_has 'packfile list size, 70000, is not plausible'"

echo "== P13 no version resource: the build cannot be told, nothing done"
fresh_ini; out=$(game full_game_nover.exe); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "reason logged and reported" "log_has 'file version could not be read' && log_has 'PROBLEM: The packfile limit is not changed'"

echo "== P14 game 1.0.1158.13 (first build with the new patterns)"
fresh_ini; out=$(game full_game_1158_13.exe); echo "      $out"
check "1158.13+ site and reference used" "has '$MOVED' && has 'D3=12000 D2=4500 D1=4500 I=12000' && has 'config 3000 -> 12000'"

echo "== P15 game 1.0.1013.17: the Mansions-era direct size and the older build's reference"
fresh_ini; out=$(game full_game_1013.exe); echo "      $out"
check "older build's reference moved, the 1158.13+ one untouched" "has 'pack refs: 30 new 0 game 0 other, R31a game, R31b new (RW, 512 KB)'"
check "1013.17+ direct size used" "has 'pack sizes: D3=4500 D2=12000 D1=4500 I=12000' && has 'config 3000 -> 12000'"
check "no problems" "! log_has 'PROBLEM'"

echo "== P16 game 1.0.889.22: the oldest direct size"
fresh_ini; out=$(game full_game_889.exe); echo "      $out"
check "older build's reference moved" "has 'R31a game, R31b new (RW, 512 KB)'"
check "oldest direct size used" "has 'pack sizes: D3=4500 D2=4500 D1=12000 I=12000' && has 'config 3000 -> 12000'"

echo "== P17 none of the game code present (plain fake game): logged and skipped, no crash"
fresh_ini; out=$(game plain_game.exe)
check "harness survived" "grep -q 'loaded' harness.out"
check "reported" "log_has 'A reference to the packfile array was not found' && log_has 'PROBLEM: The packfile limit is not changed'"

echo
echo "PACKFILE SCENARIOS: $pass passed, $fail failed"
