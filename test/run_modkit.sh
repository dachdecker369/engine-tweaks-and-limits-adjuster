#!/usr/bin/env bash
# M3 (ModkitLimitAdjusterEnhanced) scenarios: a fake GTA5_Enhanced.exe with copies of the
# patched game code loads the real TweaksV.asi. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
mkdir -p appdata

pass=0; fail=0
check() { if eval "$2"; then echo "PASS  $1"; pass=$((pass+1)); else echo "FAIL  $1"; fail=$((fail+1)); fi; }
setkey() { sed -b -i "0,/^$1=.*\r\$/s//$1=$2\r/" TweaksV.ini; }
setmodkitkey() { sed -b -i "/^\[MODKIT_SETTINGS\]/,/^\[/s/^$1=.*\r\$/$1=$2\r/" TweaksV.ini; }
fresh_ini() { sed 's/$/\r/' "$T/base.ini" > TweaksV.ini; setkey SHOW_ERROR_POPUPS 0; }
game() { cp "$1" GTA5_Enhanced.exe; shift; rm -f harness.out; LOCALAPPDATA="$AW" ./GTA5_Enhanced.exe TweaksV.asi "$@" > harness.out 2>&1; tr -d '\r' < harness.out; }
log_has() { grep -q -- "$1" TweaksV.log; }
count() { grep -c -- "$1" TweaksV.log; }
GAME_LIMITS='limits: 1023 1027 1027 1027 1027 2048 1023 1023'
MAX_LIMITS='limits: 65535 65539 65539 65539 65539 131072 65535 65535'
NEW_ARRAY='modkit refs: new array (RW, 128 KB)'

echo "== K1 defaults: array moved, limits raised to 65536, the other plugins still fine"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "all six references -> one new 128 KB read/write array outside the game" "[[ \"\$out\" == *\"$NEW_ARRAY\"* ]]"
check "limits written for 65536 (size-1, size+3, size*2)" "[[ \"\$out\" == *\"$MAX_LIMITS\"* ]]"
check "original log lines (vanilla 1024, adjusted 65536)" "log_has 'Vanilla Modkit List Size is: 1024' && log_has 'Modkit List Size Adjusted to: 65536'"
check "heap and story mode still applied" "[[ \"\$out\" == *'heap: 1024 MB'* && \"\$out\" == *'splash: E9 86 01 00 00 90'* ]]"
check "no problems" "! log_has 'PROBLEM'"

echo "== K2 MODKITARRAY_SIZE=4096: limits for 4096, array still full size"
fresh_ini; setmodkitkey MODKITARRAY_SIZE 4096; out=$(game full_game.exe); echo "      $out"
check "limits 4095/4099/8192" "[[ \"\$out\" == *'limits: 4095 4099 4099 4099 4099 8192 4095 4095'* ]]"
check "array still 128 KB" "[[ \"\$out\" == *\"$NEW_ARRAY\"* ]]"

echo "== K3 MODKITARRAY_SIZE=500 is raised to the game's own 1024"
fresh_ini; setmodkitkey MODKITARRAY_SIZE 500; out=$(game full_game.exe); echo "      $out"
check "limits stay at the game's values" "[[ \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "clamp logged" "log_has 'MODKITARRAY_SIZE=500 is outside 1024..65536 - using 1024'"

echo "== K4 MODKITARRAY_SIZE=70000 is clamped to 65536"
fresh_ini; setmodkitkey MODKITARRAY_SIZE 70000; out=$(game full_game.exe)
check "limits for 65536" "[[ \"\$out\" == *\"$MAX_LIMITS\"* ]]"
check "clamp logged" "log_has 'MODKITARRAY_SIZE=70000 is outside 1024..65536 - using 65536'"

echo "== K5 MODKITARRAY_SIZE=abc falls back to the default 65536"
fresh_ini; setmodkitkey MODKITARRAY_SIZE abc; out=$(game full_game.exe)
check "limits for 65536" "[[ \"\$out\" == *\"$MAX_LIMITS\"* ]]"
check "invalid value logged" "log_has 'MODKITARRAY_SIZE is not a valid size - using the default, 65536'"

echo "== K6 DRY_RUN=1: everything found and checked, nothing written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe); echo "      $out"
check "references still on the game's array" "[[ \"\$out\" == *'modkit refs: game array'* ]]"
check "limits unchanged" "[[ \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "both 'would' lines" "log_has 'DRY_RUN: would move the modkit array (6 references)' && log_has 'DRY_RUN: would change 8 modkit limits from 1024 to 65536'"

echo "== K7 DIAGNOSTIC_MODE=1: every pattern, reference and limit logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "the 14 modkit patterns each found exactly once" "[ \$(grep -c -E 'Pattern found: (31 c9 48 8d 05|89 c2 48 8d 0d|41 0f b7 c2 48 8d 0d|57 48 83 ec|48 8d 3d \? \? \? \? 0f b7|77 \? 48 8d 15|0f b7 c0 66 ba|48 83 c5|74 \? 48 83 c5|48 83 c7 \? 48 83 c2 \? 48 83 c5|48 83 c7 \? 48 83 c2 \? 48 83 c6|41 b8|0f b7 41|41 0f b7 ca).* 1 match,' TweaksV.log) -eq 14 ]"
check "6 references logged" "[ \$(count 'ModkitLimitAdjusterEnhanced\] DIAGNOSTIC: reference at RVA') -eq 6 ]"
check "8 limits logged, all decoding to 1024" "[ \$(count 'DIAGNOSTIC: limit at RVA .* -> game size 1024') -eq 8 ]"

echo "== K8 [MODKIT_SETTINGS] ENABLED=0"
fresh_ini; setmodkitkey ENABLED 0; out=$(game full_game.exe)
check "nothing changed" "[[ \"\$out\" == *'modkit refs: game array'* && \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "no modkit lines" "! log_has '\[ModkitLimitAdjusterEnhanced\]'"

echo "== K9 an old ModkitLimitAdjusterEnhanced.asi in the folder: skipped"
fresh_ini; touch ModkitLimitAdjusterEnhanced.asi; out=$(game full_game.exe); rm -f ModkitLimitAdjusterEnhanced.asi
check "nothing changed" "[[ \"\$out\" == *'modkit refs: game array'* && \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "old plugin reported" "log_has 'Old plugin found: .*ModkitLimitAdjusterEnhanced.asi'"

echo "== K10 one limit disagrees about the game's size: limits left alone (array already moved - harmless)"
fresh_ini; out=$(game full_game.exe modkitmismatch); echo "      $out"
check "limits unchanged" "[[ \"\$out\" == *'limits: 1023 1027 1027 1027 1027 2050 1023 1023'* ]]"
check "the array was moved (the earlier step)" "[[ \"\$out\" == *\"$NEW_ARRAY\"* ]]"
check "reason logged and reported" "log_has 'The modkit limits disagree about the game.s own size: 1024 and 1025' && log_has 'PROBLEM: The modkit limit is not changed'"

echo "== K11 another mod already moved the array: nothing written"
fresh_ini; out=$(game full_game.exe modkitmoved); echo "      $out"
check "references untouched (still mixed), limits unchanged" "[[ \"\$out\" == *'modkit refs: mixed'* && \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "reason logged" "log_has 'do not all point to the game.s own array: RVA .* and 0x.* (outside the game)'"

echo "== K12 one reference points elsewhere in the game: nothing written"
fresh_ini; out=$(game full_game.exe modkitsplit)
check "limits unchanged" "[[ \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "reason logged" "log_has 'do not all point to the game.s own array'"

echo "== K13 none of the game code present (plain fake game): logged and skipped, no crash"
fresh_ini; out=$(game plain_game.exe)
check "harness survived" "grep -q 'loaded' harness.out"
check "reported" "log_has 'A reference to the modkit array was not found' && log_has 'PROBLEM: The modkit limit is not changed'"

echo
echo "MODKIT SCENARIOS: $pass passed, $fail failed"
