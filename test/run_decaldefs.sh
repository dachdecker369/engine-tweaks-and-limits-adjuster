#!/usr/bin/env bash
# DecalDefs scenarios (TweaksVDecalDefs): a fake GTA5_Enhanced.exe with copies of the patched game code
# loads the real TweaksV.asi. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
mkdir -p appdata

MOVED='decal refs: 31 new 0 game 0 other (RW, 1280 KB)'
UNMOVED='decal refs: 0 new 31 game 0 other |'
GAME_LIMITS='decal limits: 259 260 0x5918, 23 agree'
NEW_LIMITS='decal limits: 2047 2048 0x28198, 23 agree'
NOT_RAISED='PROBLEM: The decal definitions limit is not raised this session'
ANNOUNCED='DIAGNOSTIC: the number of registered decal definitions is logged after 10 seconds, then every minute while it changes'

echo "== D1 defaults: array moved, limit 2048, the loop end a whole number of passes below it, the other plugins still fine"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "all 31 references -> one new 1280 KB read/write array outside the game" "has '$MOVED'"
check "all 23 constants: 13 x 2047, 9 x 2048, the loop end 0x7D8 + 80 x 2028" "has '$NEW_LIMITS'"
check "status line" "log_has 'Decal definitions array moved and its limit raised from 260 to 2048 entries'"
check "the other plugins still applied" "has 'heap: 1024 MB' && has 'modkit refs: new array (RW, 128 KB)' && has 'pack refs: 30 new 0 game 0 other' && has 'box refs: 8 new 0 game 0 other'"
check "every patched page back to its own protection (read+execute, as the game's code)" "has 'protect: all restored (RX)'"
check "nothing is measured with DIAGNOSTIC_MODE=0, no problems" "! log_has 'registered decal definitions' && ! log_has 'PROBLEM'"

echo "== D2 DECALDEFS_SIZE=16384: the limit 16384, the loop end 0x7D8 + 80 x 16380, the array still full size"
fresh_ini; setkey DECALDEFS_SIZE 16384 DECALS; out=$(game full_game.exe)
check "limits 16383 and 16384, loop end 0x140698, array 1280 KB" "has 'decal limits: 16383 16384 0x140698, 23 agree' && has '$MOVED'"

echo "== D3 DECALDEFS_SIZE=100 is below the game's own 260: 260 used, reported for the start-up summary"
fresh_ini; setkey DECALDEFS_SIZE 100 DECALS; out=$(game full_game.exe)
check "array moved, the limits stay at the game's 260" "has '$MOVED' && has '$GAME_LIMITS'"
check "reported" "log_has 'PROBLEM: TweaksV.ini: \[DECALS\] DECALDEFS_SIZE=100 is outside 260..16384 - using 260.'"

echo "== D4 DECALDEFS_SIZE=20000 is clamped to 16384"
fresh_ini; setkey DECALDEFS_SIZE 20000 DECALS; out=$(game full_game.exe)
check "limits 16384" "has 'decal limits: 16383 16384 0x140698, 23 agree'"
check "reported" "log_has 'PROBLEM: TweaksV.ini: \[DECALS\] DECALDEFS_SIZE=20000 is outside 260..16384 - using 16384.'"

echo "== D5 DECALDEFS_SIZE=abc falls back to the default 2048"
fresh_ini; setkey DECALDEFS_SIZE abc DECALS; out=$(game full_game.exe)
check "limits 2048" "has '$NEW_LIMITS'"
check "reported" "log_has 'PROBLEM: TweaksV.ini: \[DECALS\] DECALDEFS_SIZE=abc is not a number - using 2048.'"

echo "== D6 DRY_RUN=1: everything found and checked, nothing written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe); echo "      $out"
check "references and limits untouched" "has '$UNMOVED' && has '$GAME_LIMITS'"
check "the 'would' line" "log_has 'DRY_RUN: would move the decal definitions array (31 references) and raise its limit from 260 to 2048 (23 sites).'"

echo "== D7 DIAGNOSTIC_MODE=1: every pattern, constant, reference and write logged; the measurement announced"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "15 patterns found exactly once, the shared 'add decal' check exactly 7 times, each of its matches logged with its bytes" "[ \$(grep -c -E 'Pattern D-E-L[0-9]+ found: .* 1 match,' TweaksV.log) -eq 15 ] && [ \$(grep -c -E 'Pattern D-E-L12 found: .* 7 matches,' TweaksV.log) -eq 1 ] && [ \$(grep -c -E 'D-E-L12 match [1-7] of 7 at RVA 0x[0-9A-F]+:( [0-9A-F]{2}){16} \| E8' TweaksV.log) -eq 7 ]"
check "the array and its count logged" "log_has 'DIAGNOSTIC: the decal definitions array is at RVA .* - 260 entries of 80 bytes; its count is at RVA'"
check "23 constants logged: 13 holding 259, 9 holding 260, the loop end 22808" "[ \$(count 'DecalDefs\] DIAGNOSTIC: limit at RVA .* holds') -eq 23 ] && [ \$(count 'DecalDefs\] DIAGNOSTIC: limit at RVA .* holds 259 (N-1') -eq 13 ] && [ \$(count 'DecalDefs\] DIAGNOSTIC: limit at RVA .* holds 260 (N,') -eq 9 ] && log_has 'DIAGNOSTIC: limit at RVA .* holds 22808 (loop end, D-E-L11): the start-up loop clears 26 definitions per pass from offset 2008'"
check "the 31 places that load the array logged" "[ \$(count 'DecalDefs\] DIAGNOSTIC: the array is loaded at RVA') -eq 31 ]"
check "the 54 writes logged" "[ \$(count 'DecalDefs\] DIAGNOSTIC: decal definitions array reference at RVA') -eq 31 ] && [ \$(count 'DecalDefs\] DIAGNOSTIC: decal definitions limit at RVA') -eq 22 ] && [ \$(count 'DecalDefs\] DIAGNOSTIC: decal definitions loop end at RVA') -eq 1 ]"
check "still applied" "has '$MOVED' && has '$NEW_LIMITS'"
check "the measurement announced, no summary within the first seconds (nothing registered yet)" "log_has '$ANNOUNCED.' && ! log_has 'decal definitions registered'"

echo "== D8 [DECALS] ENABLED=0"
fresh_ini; setkey ENABLED 0 DECALS; out=$(game full_game.exe)
check "nothing changed" "has '$UNMOVED' && has '$GAME_LIMITS'"
check "header: DecalDefs off; no DecalDefs lines" "log_has 'Features: .*, DecalDefs off' && ! log_has '\[DecalDefs\]'"

echo "== D9 another mod already moved the array (every reference points outside the game): nothing written"
fresh_ini; out=$(game full_game.exe decalmoved); echo "      $out"
check "references untouched (all elsewhere), limits unchanged" "has 'decal refs: 0 new 0 game 31 other |' && has '$GAME_LIMITS'"
check "reason logged and reported" "log_has 'decal definitions array or its count is not inside the game: the array at 0x.* (outside the game), the count at RVA .* (another mod may have moved the array already)' && log_has '$NOT_RAISED'"
check "every page back to its own protection" "has 'protect: all restored'"

echo "== D10 one reference points elsewhere in the game: only 30 load the array, nothing written"
fresh_ini; out=$(game full_game.exe decalsplit); echo "      $out"
check "references untouched (one elsewhere), limits unchanged" "has 'decal refs: 0 new 30 game 1 other |' && has '$GAME_LIMITS'"
check "reason logged and reported" "log_has 'loads the decal definitions array in 30 places, not the expected 31' && log_has '$NOT_RAISED'"

echo "== D11 a 32nd lea loads the array (a changed game): nothing written"
fresh_ini; out=$(game full_game.exe decalextra); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has '$GAME_LIMITS'"
check "reason logged and reported" "log_has 'loads the decal definitions array in 32 or more places, not the expected 31' && log_has '$NOT_RAISED'"

echo "== D12 one lea has a REX prefix the game's never have (49): nothing written"
fresh_ini; out=$(game full_game.exe decalprefix); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has '$GAME_LIMITS'"
check "reason logged and reported" "log_has 'load of the decal definitions array at RVA .* is not a \"lea r64, \[rip+disp32\]\" with a 48 or 4C prefix: 49 8D' && log_has '$NOT_RAISED'"

echo "== D13 a constant beyond its pattern disagrees (FindById's second compare holds 261): nothing written"
fresh_ini; out=$(game full_game.exe decalmismatch); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has 'decal limits: 259 260 0x5918, 1 differ'"
check "reason logged and reported" "log_has 'decal definitions limit at RVA .* holds 261, not the 260 the parser.s check implies (D-E-L3)' && log_has '$NOT_RAISED'"

echo "== D14 the parser's check implies 261 (holds 260), which the start-up loop's end contradicts: nothing written"
fresh_ini; out=$(game full_game.exe decaloffbyone); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has 'decal limits: 260 260 0x5918, 12 differ'"
check "reason logged and reported" "log_has 'end of the decal manager.s start-up loop at RVA .* holds 22808, not its start offset 2008 + 261 x 80' && log_has '$NOT_RAISED'"

echo "== D15 the parser's check implies an implausible 131072: nothing written"
fresh_ini; out=$(game full_game.exe decalbadsize); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has 'decal limits: 131071 260 0x5918, 12 differ'"
check "reason logged and reported" "log_has 'game.s own decal definitions limit, 131072, is not plausible' && log_has '$NOT_RAISED'"

echo "== D16 none of the game code present (plain fake game): logged and skipped, no crash"
fresh_ini; out=$(game plain_game.exe)
check "harness survived" "grep -q 'loaded' harness.out"
check "reported" "log_has 'parser.s check of the decal definitions limit was not found in the game code: D-E-L1' && log_has '$NOT_RAISED'"

echo "== D17 DIAGNOSTIC_MODE=1 with 300 definitions registered: the count is read and summed up 10 s in, nothing hooked for it"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe decalmany wait); echo "      $out"
check "array moved and limit raised as before; no call stub added for it (the 10 are the other parts': the packfile hook, RageOpenV's 5 calls, the box streamer's 4 counted calls)" "has '$MOVED' && has '$NEW_LIMITS' && has 'stubs: 10, all RX'"
check "the log says what is logged" "log_has '$ANNOUNCED.'"
check "the summary: 300 registered, 40 more than the game's own array holds" "log_has 'DIAGNOSTIC: 300 decal definitions registered (the game.s own array holds 260, this one 2048) - the game.s own array would have dropped 40 of them.'"
check "no problems" "! log_has 'PROBLEM'"

echo "== D18 DIAGNOSTIC_MODE=1 and DRY_RUN=1 with the game's own array full: nothing written, the count is still logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe decalfull wait)
check "nothing changed" "has '$UNMOVED' && has '$GAME_LIMITS'"
check "the log says the game's own array is in use" "log_has '$ANNOUNCED (DRY_RUN: the game.s own array is in use).'"
check "the summary: 260 registered, the array full" "log_has 'DIAGNOSTIC: 260 decal definitions registered (the game.s own array holds 260 and is in use) - it is full: any further definitions were dropped.'"

echo "== D19 DecalPatch.asi next to TweaksV: this part and the box streamer part are switched off and reported"
fresh_ini; touch DecalPatch.asi; out=$(game full_game.exe); rm -f DecalPatch.asi; echo "      $out"
check "neither part applied, the rest is" "has '$UNMOVED' && has '$GAME_LIMITS' && has 'box refs: 0 new 8 game 0 other' && has 'heap: 1024 MB' && has 'modkit refs: new array (RW, 128 KB)'"
check "reported once per part, one start-up summary" "[ \$(count 'PROBLEM: Old plugin found: .*DecalPatch.asi. TweaksV.s BoxStreamer part is switched off') -eq 1 ] && [ \$(count 'PROBLEM: Old plugin found: .*DecalPatch.asi. TweaksV.s DecalDefs part is switched off') -eq 1 ] && [ \$(count 'Start-up summary not shown (ERROR_POPUP=0)') -eq 1 ]"
check "neither part ran: no line of either" "! log_has '\[DecalDefs\]' && ! log_has '\[BoxStreamer\]'"

echo
echo "DECALDEFS SCENARIOS: $pass passed, $fail failed"
