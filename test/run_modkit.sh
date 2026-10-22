#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# ModkitLimit scenarios (TweaksVModkitLimit): a fake GTA5_Enhanced.exe with copies of the patched game
# code loads the real TweaksV.asi. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
mkdir -p appdata

GAME_LIMITS='limits: 1023 1027 1027 1027 1027 2048 1023 1023 |'
MAX_LIMITS='limits: 65535 65539 65539 65539 65539 131072 65535 65535 |'
NEW_ARRAY='modkit refs: new array (RW, 128 KB)'

echo "== K1 defaults: array moved, limits raised to 65536, the other plugins still fine"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "all six references -> one new 128 KB read/write array outside the game" "[[ \"\$out\" == *\"$NEW_ARRAY\"* ]]"
check "limits written for 65536 (size-1, size+3, size*2)" "[[ \"\$out\" == *\"$MAX_LIMITS\"* ]]"
check "the game's own entries are copied into the new array" "has 'modkit copy: ok'"
check "status line" "log_has 'The modkit array moved and its limit raised from 1024 to 65536 entries.'"
check "heap and story mode still applied" "[[ \"\$out\" == *'heap: 1024 MB'* && \"\$out\" == *'splash: E9 86 01 00 00 90 |'* ]]"
check "no problems" "log_lacks 'PROBLEM'"

echo "== K2 MODKITARRAY_SIZE=4096: limits for 4096, array still full size"
fresh_ini; setkey MODKITARRAY_SIZE 4096 MODKIT_LIMIT; out=$(game full_game.exe); echo "      $out"
check "limits 4095/4099/8192" "[[ \"\$out\" == *'limits: 4095 4099 4099 4099 4099 8192 4095 4095 |'* ]]"
check "array still 128 KB" "[[ \"\$out\" == *\"$NEW_ARRAY\"* ]]"

echo "== K3 MODKITARRAY_SIZE=500 is raised to the game's own 1024"
fresh_ini; setkey MODKITARRAY_SIZE 500 MODKIT_LIMIT; out=$(game full_game.exe); echo "      $out"
check "limits stay at the game's values" "[[ \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "clamp logged" "log_has 'MODKITARRAY_SIZE=500 is outside 1024..65536 - using 1024'"

echo "== K4 MODKITARRAY_SIZE=70000 is clamped to 65536"
fresh_ini; setkey MODKITARRAY_SIZE 70000 MODKIT_LIMIT; out=$(game full_game.exe)
check "limits for 65536" "[[ \"\$out\" == *\"$MAX_LIMITS\"* ]]"
check "clamp reported for the start-up summary" "log_has 'PROBLEM: TweaksV.ini: \[MODKIT_LIMIT\] MODKITARRAY_SIZE=70000 is outside 1..65536 - using 65536.'"

echo "== K5 MODKITARRAY_SIZE=abc falls back to the default 65536"
fresh_ini; setkey MODKITARRAY_SIZE abc MODKIT_LIMIT; out=$(game full_game.exe)
check "limits for 65536" "[[ \"\$out\" == *\"$MAX_LIMITS\"* ]]"
check "invalid value reported for the start-up summary" "log_has 'PROBLEM: TweaksV.ini: \[MODKIT_LIMIT\] MODKITARRAY_SIZE=abc is not a number - using 65536.'"

echo "== K6 DRY_RUN=1: everything found and checked, nothing written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe); echo "      $out"
check "references still on the game's array" "[[ \"\$out\" == *'modkit refs: game array'* ]]"
check "limits unchanged" "[[ \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "the 'would' line" "log_has 'DRY_RUN: would move the modkit array (6 references) and raise its limit from 1024 to 65536 (8 sites).'"

echo "== K7 DIAGNOSTIC_MODE=1: every pattern, reference and limit logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "the 14 modkit patterns each found exactly once" "[ \$(grep -c -E 'Pattern M-E-(R[1-6]|L[1-8]) found: .* 1 match,' TweaksV.log) -eq 14 ]"
check "6 references logged, and the scan finds the same 6 and no other" "[ \$(count 'ModkitLimit\] DIAGNOSTIC: reference at RVA .* (M-E-R[1-6])') -eq 6 ] && [ \$(count 'ModkitLimit\] DIAGNOSTIC: reference at RVA .* (scan)') -eq 6 ]"
check "8 limits logged, all decoding to 1024; the array logged" "[ \$(count 'ModkitLimit\] DIAGNOSTIC: limit at RVA .* -> game size 1024') -eq 8 ] && log_has 'DIAGNOSTIC: the modkit array is at RVA .* - 1024 entries of 2 bytes'"
check "the 14 writes logged" "[ \$(count 'ModkitLimit\] DIAGNOSTIC: modkit array reference at RVA') -eq 6 ] && [ \$(count 'ModkitLimit\] DIAGNOSTIC: modkit array limit at RVA') -eq 8 ]"

echo "== K8 [MODKIT_LIMIT] ENABLED=0"
fresh_ini; setkey ENABLED 0 MODKIT_LIMIT; out=$(game full_game.exe)
check "nothing changed" "[[ \"\$out\" == *'modkit refs: game array'* && \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "header: ModkitLimit off; no ModkitLimit lines" "log_has 'Features: .*, ModkitLimit off' && log_lacks '\[ModkitLimit\]'"

echo "== K9 an old ModkitLimitAdjusterEnhanced.asi in the folder: skipped"
fresh_ini; touch ModkitLimitAdjusterEnhanced.asi; out=$(game full_game.exe); rm -f ModkitLimitAdjusterEnhanced.asi
check "nothing changed" "[[ \"\$out\" == *'modkit refs: game array'* && \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "old plugin reported" "log_has 'Old plugin found: .*ModkitLimitAdjusterEnhanced.asi'"

echo "== K10 one limit disagrees about the game's size: nothing written"
fresh_ini; out=$(game full_game.exe modkitmismatch); echo "      $out"
check "limits unchanged" "[[ \"\$out\" == *'limits: 1023 1027 1027 1027 1027 2050 1023 1023 |'* ]]"
check "the array not moved" "[[ \"\$out\" == *'modkit refs: game array'* ]]"
check "reason logged and reported" "log_has 'the limits of the modkit array disagree about the game.s own size: 1024 and 1025 (M-E-L6)' && log_has 'PROBLEM: The modkit limit is not changed'"

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
check "reported" "log_has 'PROBLEM: The modkit limit is not changed this session: M-E-R1 was not found.'"

echo "== K14 a seventh reference to the array that no pattern names (a changed game): nothing written"
fresh_ini; out=$(game full_game.exe modkitextra); echo "      $out"
check "nothing changed" "[[ \"\$out\" == *'modkit refs: game array'* && \"\$out\" == *\"$GAME_LIMITS\"* ]]"
check "reason logged and reported" "log_has 'the game code holds 7 or more references to the modkit array, not the expected 6 (another mod may have moved some of them, or the game has changed).' && log_has 'PROBLEM: The modkit limit is not changed'"

echo
echo "MODKIT SCENARIOS: $pass passed, $fail failed"
