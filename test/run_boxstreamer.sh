#!/usr/bin/env bash
# BoxStreamer scenarios (TweaksVBoxStreamer): a fake GTA5_Enhanced.exe with copies of the patched game
# code loads the real TweaksV.asi. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
mkdir -p appdata

MOVED='box refs: 8 new 0 game 0 other (RW, 784 KB)'
UNMOVED='box refs: 0 new 8 game 0 other |'
GAME_CAPS='box caps: 1000 1000 1000 1000'
NEW_CAPS='box caps: 10000 10000 10000 10000'
CALLS_GAME='box calls: 0 redirected 4 game 0 other, originals 2x+2x, args ok, returns 100 400 700 1000'
CALLS_COUNTED='box calls: 4 redirected 0 game 0 other, originals 2x+2x, args ok, returns 100 400 700 1000'
NOT_ENLARGED='PROBLEM: The box streamer.s traversal list is not enlarged this session'

echo "== B1 defaults: list moved, capacity 10000, the other plugins still fine"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "all 8 references -> one new 784 KB read/write list outside the game" "has '$MOVED'"
check "all 4 capacities 10000" "has '$NEW_CAPS'"
check "status line" "log_has 'Box streamer traversal list moved and its capacity raised from 1000 to 10000 entries'"
check "the other plugins still applied" "has 'heap: 1024 MB' && has 'modkit refs: new array (RW, 128 KB)' && has 'pack refs: 30 new 0 game 0 other'"
check "every patched page back to its own protection (read+execute, as the game's code)" "has 'protect: all restored (RX)'"
check "the tree-search calls untouched (LOG_SEARCHES=0): the game's own searches run" "has '$CALLS_GAME'"
check "no problems" "! log_has 'PROBLEM'"

echo "== B2 TRAVERSAL_LIST_SIZE=50000: capacities 50000, the list still full size"
fresh_ini; setkey TRAVERSAL_LIST_SIZE 50000 BOXSTREAMER_SETTINGS; out=$(game full_game.exe)
check "capacities 50000, list 784 KB" "has 'box caps: 50000 50000 50000 50000' && has '$MOVED'"

echo "== B3 TRAVERSAL_LIST_SIZE=500 is below the game's own 1000: 1000 used, reported for the start-up summary"
fresh_ini; setkey TRAVERSAL_LIST_SIZE 500 BOXSTREAMER_SETTINGS; out=$(game full_game.exe)
check "list moved, capacities stay at the game's 1000" "has '$MOVED' && has '$GAME_CAPS'"
check "reported" "log_has 'PROBLEM: TweaksV.ini: \[BOXSTREAMER_SETTINGS\] TRAVERSAL_LIST_SIZE=500 is outside 1000..100000 - using 1000.'"

echo "== B4 TRAVERSAL_LIST_SIZE=200000 is clamped to 100000"
fresh_ini; setkey TRAVERSAL_LIST_SIZE 200000 BOXSTREAMER_SETTINGS; out=$(game full_game.exe)
check "capacities 100000" "has 'box caps: 100000 100000 100000 100000'"
check "reported" "log_has 'PROBLEM: TweaksV.ini: \[BOXSTREAMER_SETTINGS\] TRAVERSAL_LIST_SIZE=200000 is outside 1000..100000 - using 100000.'"

echo "== B5 TRAVERSAL_LIST_SIZE=abc falls back to the default 10000"
fresh_ini; setkey TRAVERSAL_LIST_SIZE abc BOXSTREAMER_SETTINGS; out=$(game full_game.exe)
check "capacities 10000" "has '$NEW_CAPS'"
check "reported" "log_has 'PROBLEM: TweaksV.ini: \[BOXSTREAMER_SETTINGS\] TRAVERSAL_LIST_SIZE=abc is not a number - using 10000.'"

echo "== B6 DRY_RUN=1: everything found and checked, nothing written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe); echo "      $out"
check "references and capacities untouched" "has '$UNMOVED' && has '$GAME_CAPS'"
check "the 'would' line" "log_has 'DRY_RUN: would move the box streamer.s traversal list (8 references) and raise its capacity from 1000 to 10000 (4 sites).'"

echo "== B7 DIAGNOSTIC_MODE=1: every pattern, capacity, reference and write logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "the 4 patterns each found exactly once" "[ \$(grep -c -E 'Pattern B-E-L[1-4] found: .* 1 match,' TweaksV.log) -eq 4 ]"
check "4 capacities logged, all holding 1000" "[ \$(count 'BoxStreamer\] DIAGNOSTIC: capacity at RVA .* holds 1000') -eq 4 ]"
check "the list and the 8 places that load it logged" "log_has 'DIAGNOSTIC: the traversal list is at RVA .* - 1000 entries of 8 bytes' && [ \$(count 'BoxStreamer\] DIAGNOSTIC: the list is loaded at RVA') -eq 8 ]"
check "the 12 writes logged" "[ \$(count 'BoxStreamer\] DIAGNOSTIC: box streamer list reference at RVA') -eq 8 ] && [ \$(count 'BoxStreamer\] DIAGNOSTIC: box streamer list capacity at RVA') -eq 4 ]"
check "still applied" "has '$MOVED' && has '$NEW_CAPS'"

echo "== B8 [BOXSTREAMER_SETTINGS] ENABLED=0"
fresh_ini; setkey ENABLED 0 BOXSTREAMER_SETTINGS; out=$(game full_game.exe)
check "nothing changed" "has '$UNMOVED' && has '$GAME_CAPS'"
check "header: BoxStreamer off; no BoxStreamer lines" "log_has 'Features: .*, BoxStreamer off' && ! log_has '\[BoxStreamer\]'"

echo "== B9 another mod already moved the list (every reference points outside the game): nothing written"
fresh_ini; out=$(game full_game.exe boxmoved); echo "      $out"
check "references untouched (all elsewhere), capacities unchanged" "has 'box refs: 0 new 0 game 8 other |' && has '$GAME_CAPS'"
check "reason logged and reported" "log_has 'traversal list is not inside the game: 0x.* (outside the game) (another mod may have moved it already)' && log_has '$NOT_ENLARGED'"
check "every page back to its own protection" "has 'protect: all restored'"

echo "== B10 one reference points elsewhere in the game: only 7 load the list, nothing written"
fresh_ini; out=$(game full_game.exe boxsplit); echo "      $out"
check "references untouched (one elsewhere), capacities unchanged" "has 'box refs: 0 new 7 game 1 other |' && has '$GAME_CAPS'"
check "reason logged and reported" "log_has 'loads the box streamer.s traversal list in 7 places, not the expected 8' && log_has '$NOT_ENLARGED'"

echo "== B11 a ninth lea loads the list (a changed game): nothing written"
fresh_ini; out=$(game full_game.exe boxninth); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has '$GAME_CAPS'"
check "reason logged and reported" "log_has 'loads the box streamer.s traversal list in 9 or more places, not the expected 8' && log_has '$NOT_ENLARGED'"

echo "== B12 one capacity disagrees about the game's own size (1001): nothing written"
fresh_ini; out=$(game full_game.exe boxmismatch); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has 'box caps: 1000 1000 1001 1000'"
check "reason logged and reported" "log_has 'capacities of the box streamer.s traversal list disagree about the game.s own size: 1000 and 1001 (B-E-L3)' && log_has '$NOT_ENLARGED'"

echo "== B13 implausible capacities (200000): nothing written"
fresh_ini; out=$(game full_game.exe boxbadsize); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has 'box caps: 200000 200000 200000 200000'"
check "reason logged and reported" "log_has 'box streamer traversal list size, 200000, is not plausible' && log_has '$NOT_ENLARGED'"

echo "== B14 none of the game code present (plain fake game): logged and skipped, no crash"
fresh_ini; out=$(game plain_game.exe)
check "harness survived" "grep -q 'loaded' harness.out"
check "reported" "log_has 'A capacity of the box streamer.s traversal list was not found in the game code: B-E-L1' && log_has '$NOT_ENLARGED'"

echo "== B15 LOG_SEARCHES=1: the four tree-search calls go through TweaksV's stubs, the searches are counted and summed up 10 s in"
fresh_ini; setkey LOG_SEARCHES 1 BOXSTREAMER_SETTINGS; out=$(game full_game.exe wait); echo "      $out"
check "list moved and capacities raised as before" "has '$MOVED' && has '$NEW_CAPS'"
check "all four calls redirected; the game's searches still run with the game's arguments and their results come back unchanged" "has '$CALLS_COUNTED'"
check "the four stubs are read+execute, every page back to its own protection" "has 'stubs: 10, all RX' && has 'protect: all restored (RX)'"
check "the log says what is counted" "log_has 'LOG_SEARCHES: the 4 tree-search calls are counted; a summary follows after 10 seconds, then every minute while the numbers change.'"
check "the summary: 4 searches, the largest 1000 of 10000, one more than the game's own list could hold" "log_has 'LOG_SEARCHES: 4 searches so far; the largest returned 1000 of 10000 entries; 1 returned more than the game.s own list (1000 entries) could hold.'"
check "no problems" "! log_has 'PROBLEM'"

echo "== B16 LOG_SEARCHES=1 and DIAGNOSTIC_MODE=1: every call and its redirection logged"
fresh_ini; setkey LOG_SEARCHES 1 BOXSTREAMER_SETTINGS; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "the four calls and their targets logged" "[ \$(count 'BoxStreamer\] DIAGNOSTIC: tree-search call at RVA .* -> RVA') -eq 4 ]"
check "the four redirections logged as writes" "[ \$(count 'BoxStreamer\] DIAGNOSTIC: tree-search call at RVA .*: E8 .* -> E8') -eq 4 ]"
check "still applied" "has '$MOVED' && has '$CALLS_COUNTED'"

echo "== B17 LOG_SEARCHES=1, another mod already hooked a tree-search call: nothing written"
fresh_ini; setkey LOG_SEARCHES 1 BOXSTREAMER_SETTINGS; out=$(game full_game.exe boxcallhooked); echo "      $out"
check "nothing changed, the other mod's call kept" "has '$UNMOVED' && has '$GAME_CAPS' && has 'box calls: 0 redirected 3 game 1 other'"
check "reason logged and reported" "log_has 'tree-search call at RVA .* is not a call into the game (another mod may have hooked it already) (B-E-L2)' && log_has '$NOT_ENLARGED'"

echo "== B18 LOG_SEARCHES=1, a pair of calls leads to two different functions: nothing written"
fresh_ini; setkey LOG_SEARCHES 1 BOXSTREAMER_SETTINGS; out=$(game full_game.exe boxcallsplit); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has '$GAME_CAPS' && has 'box calls: 0 redirected 4 game 0 other'"
check "reason logged and reported" "log_has 'tree-search calls of the box streamer do not lead to the same function: RVA .* and RVA .* (B-E-L3)' && log_has '$NOT_ENLARGED'"

echo "== B19 LOG_SEARCHES=1 and DRY_RUN=1: the calls are checked, nothing written"
fresh_ini; setkey LOG_SEARCHES 1 BOXSTREAMER_SETTINGS; setkey DRY_RUN 1; out=$(game full_game.exe)
check "nothing changed" "has '$UNMOVED' && has '$GAME_CAPS' && has '$CALLS_GAME'"
check "the 'would' line names the calls" "log_has 'DRY_RUN: would move the box streamer.s traversal list (8 references) and raise its capacity from 1000 to 10000 (4 sites) and redirect the 4 tree-search calls (LOG_SEARCHES).'"

echo "== B20 LOG_SEARCHES=0 with a hooked call: the calls are not looked at, the list is moved all the same"
fresh_ini; out=$(game full_game.exe boxcallhooked)
check "list moved, the other mod's call kept, no problem" "has '$MOVED' && has 'box calls: 0 redirected 3 game 1 other' && ! log_has 'PROBLEM'"

echo
echo "BOXSTREAMER SCENARIOS: $pass passed, $fail failed"
