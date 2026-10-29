#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The texture budget, measured first for the Resources part to come (TweaksVTextureBudget): with DIAGNOSTIC_MODE=1 only,
# the five patterns found in the full fake game, every site checked and logged with the addresses the fake reports, and the
# counters summed up 10 seconds in, at the game's own sizes - the budgets 1500, 1500, 3000 and 3000 MB, 98304 objects - and
# again once they grow past the budget (the test build's TWEAKSV_TEST_SUMMARY_MS), not while nothing changes; a pattern
# missing (TEST_BREAK_PATTERN), a site not as expected, a call another mod hooked, an address outside the game, sites that
# disagree on the counters and a factor's multiplication changed - each logged, no problem, no box, no summary; the plain
# game; DRY_RUN alike; and in every run nothing written: the fake's sites and data as they were, no call redirected, every
# page's protection its own. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata

# The summaries: the counters as the fake sets them, then grown by 200 MB and 800 textures past the 3000 MB budget, the
# update check's flag up; the update check's switch and factors, read once
SUMMARY1='\[Resources\] DIAGNOSTIC: texture budget: 2845 MB of the 3000 MB budget for the texture quality.s index 2 (MaxStreamingMemory_0..3_Base: 1500, 1500, 3000 and 3000 MB) - textures 2456 MB, buffers 389 MB; 23456 of the 98304 objects allowed (MaxStreamingResources) - 22222 textures, 1234 buffers; the most so far 2900 MB by the game.s own mark, 2845 MB and 23456 objects at TweaksV.s readings; the update check.s flag is down.$'
SUMMARY2='\[Resources\] DIAGNOSTIC: texture budget: 3045 MB of the 3000 MB budget for the texture quality.s index 2 (MaxStreamingMemory_0..3_Base: 1500, 1500, 3000 and 3000 MB) - textures 2656 MB, buffers 389 MB; 24256 of the 98304 objects allowed (MaxStreamingResources) - 23022 textures, 1234 buffers; the most so far 3045 MB by the game.s own mark, 3045 MB and 24256 objects at TweaksV.s readings; the update check.s flag is up: it evicts ahead of time - at the budget: the streaming engine evicts what it cannot place (texture loss).$'
UPDATE_CHECK='\[Resources\] DIAGNOSTIC: texture budget: the update check.s switch holds 1 (on), its factors 0.9 of the budget to start evicting, 0.8 to stop, and 0.95 of the video memory Windows gives the game to start evicting too (read once).$'
NOT_MEASURED='\[Resources\] DIAGNOSTIC: the texture budget is not measured: '

# rva NAME [PLUS]: "RVA 0x..." of what the fake's report places at NAME (stores, getter, placement, update, next step,
# counters, index, flag, switch), plus PLUS
rva() {
	local at
	at=$(sed -nE "s/.*[ ,;]$1 RVA (0x[0-9A-F]+)[ ,].*/\1/p" <<< "$out" | head -n 1)
	[ -n "$at" ] || { echo "RVA-of-$1-not-reported"; return; }
	printf 'RVA 0x%X' $(( at + ${2:-0} ))
}
# untouched: the fake's report - its four sites as placed, its data as set, their pages' protection their own
untouched() { has 'texture budget: sites as placed 4 of 4, data as set, its page RW' && has 'protect: all restored (RX)'; }
# found LABEL NAME [PLUS]: the scanner's line for LABEL, found once where the fake has it
found() { log_has "Pattern $1 found: .* -> $(rva "$2" "${3:-0}"), 1 match"; }
patterns_found() { found Q-E-T1 stores && found Q-E-T2 stores 34 && found Q-E-T3 getter && found Q-E-T4 placement && found Q-E-T5 update; }
# sites_logged: the module's eight lines, word for word, with the addresses of the fake's sites and data
sites_logged() {
	log_line "\[Resources\] DIAGNOSTIC: store of the most objects at $(rva stores 6) -> $(rva counters 0x20), from the configuration.s +0x54C (Q-E-T1)\$" &&
	log_line "\[Resources\] DIAGNOSTIC: store of budgets 0 and 1 at $(rva stores 26) -> $(rva counters 0x28), from the configuration.s +0x550 in MB (Q-E-T1)\$" &&
	log_line "\[Resources\] DIAGNOSTIC: store of budgets 2 and 3 at $(rva stores 48) -> $(rva counters 0x38), from the configuration.s +0x558 in MB (Q-E-T2)\$" &&
	log_line "\[Resources\] DIAGNOSTIC: call after the budgets. stores at $(rva stores 74) -> $(rva 'next step') (Q-E-T2)\$" &&
	log_line "\[Resources\] DIAGNOSTIC: budget getter at $(rva getter): the index at $(rva index), the budgets at +0x28 of the counters (Q-E-T3)\$" &&
	log_line "\[Resources\] DIAGNOSTIC: placement check at $(rva placement): the numbers at $(rva counters 0x10) and $(rva counters 0x14), the most at $(rva counters 0x20), the bytes at $(rva counters) and $(rva counters 8) (Q-E-T4)\$" &&
	log_line "\[Resources\] DIAGNOSTIC: update check at $(rva update): its switch at $(rva switch), the counters at $(rva counters), the getter.s call at $(rva update 0x22) -> $(rva getter) (Q-E-T5)\$" &&
	log_line "\[Resources\] DIAGNOSTIC: update check.s flag at $(rva update 0x4B) -> $(rva flag), its factors at $(rva update 0x54) -> $(rva switch 8) and $(rva update 0xA0) -> $(rva switch 4) (Q-E-T5)\$"
}
announced() { log_line "\[Resources\] DIAGNOSTIC: the texture budget.s counters at $(rva counters) (Q-E-T4): the streamed textures and buffers are logged after 10 seconds, then every minute while their highs, the update check.s flag or the budget in use change.\$"; }
# nothing_measured: no announcement, no summary, no line of the update check
nothing_measured() { log_lacks 'texture budget.s counters at' && log_lacks 'DIAGNOSTIC: texture budget:'; }
# at_ms PATTERN: the time of the first log line that matches, in ms of the day
at_ms() {
	tr -d '\r' < TweaksV.log | grep -m1 -- "$1" | sed -nE 's/^([0-9]{2}):([0-9]{2}):([0-9]{2})\.([0-9]{3}) .*/\1 \2 \3 \4/p' |
		{ read -r h m s f && echo $(( (10#$h * 3600 + 10#$m * 60 + 10#$s) * 1000 + 10#$f )); }
}
# first_after_10s: the first summary 10 seconds after the announcement - 9.9 at least, 20 at most on a busy machine
first_after_10s() {
	local a f d
	a=$(at_ms 'texture budget.s counters at'); f=$(at_ms 'objects allowed (MaxStreamingResources)')
	[ -n "$a" ] && [ -n "$f" ] || return 1
	d=$(( f - a )); (( d < 0 )) && d=$(( d + 86400000 ))
	(( d >= 9900 && d <= 20000 ))
}
summaries() { count 'objects allowed (MaxStreamingResources)'; }

echo "== TB1 defaults, DIAGNOSTIC_MODE=0: nothing looked for, nothing written"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "no texture budget pattern looked for, no line of the module" "log_lacks 'Q-E-T' && log_lacks '\[Resources\]' && log_lacks -i 'texture budget'"
check "the fake's sites as placed, its data as set, every page's protection its own" "untouched"

echo "== TB2 DIAGNOSTIC_MODE=1: the five patterns found once each, every site logged, the counters summed up 10 s in"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wait)
check "Q-E-T1 to Q-E-T5 found once each, where the fake has them - Q-E-T2 34 bytes after Q-E-T1" "patterns_found"
check "the eight lines of the sites, word for word, with the addresses of the fake's sites and data" "sites_logged"
check "the announcement, word for word, with the counters' address" "announced"
check "the update check's switch and factors, read once" "log_line '$UPDATE_CHECK' && [ \$(count 'the update check.s switch holds') -eq 1 ]"
check "the summary, word for word, with the fake's numbers at the game's own sizes, 10 s after the announcement" "log_line '$SUMMARY1' && first_after_10s"
check "one summary, no problem; nothing written, no call redirected" "[ \$(summaries) -eq 1 ] && log_lacks 'PROBLEM' && untouched"

echo "== TB3 the test build, TWEAKSV_TEST_SUMMARY_MS=500: once the first summary is out, the textures grow past the budget and the flag goes up - a summary says so"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(TWEAKSV_TEST_SUMMARY_MS=500 ASI=TweaksV_test.asi game full_game.exe tbgrow)
check "the test build's line: the summaries after the first every 500 ms" "log_line '\[TweaksV\] TEST BUILD: the texture budget summary comes every 500 ms after its first (TWEAKSV_TEST_SUMMARY_MS=500).\$'"
check "the first summary as the Release build's, word for word, then the update check's line once" "log_line '$SUMMARY1' && [ \$(count 'the update check.s switch holds') -eq 1 ]"
check "the fake grew the counters after it; the next summary, word for word: past the budget, the flag up" "has 'grown after the first summary' && log_line '$SUMMARY2'"
check "no problem; the sites as placed, the data as the fake set it" "log_lacks 'PROBLEM' && untouched"

echo "== TB4 the test build, TWEAKSV_TEST_SUMMARY_MS=500, nothing changes: no summary after the first"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(TWEAKSV_TEST_SUMMARY_MS=500 ASI=TweaksV_test.asi game full_game.exe wait)
check "the first summary, then none in the readings every 500 ms after it" "log_line '$SUMMARY1' && [ \$(summaries) -eq 1 ] && log_lacks 'PROBLEM' && untouched"

echo "== TB5 TEST_BREAK_PATTERN=Q-E-T3 (test build, ERROR_POPUP=1): logged, not reported - no problem, no box, no summary"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey ERROR_POPUP 1; addkey TEST_BREAK_PATTERN Q-E-T3; out=$(ASI=TweaksV_test.asi game full_game.exe wait)
check "the scanner's line and the module's, word for word; the other four found all the same" "log_has 'TEST BUILD: pattern Q-E-T3 is treated as not found (TEST_BREAK_PATTERN).' && log_line '${NOT_MEASURED}Q-E-T3 was not found.\$' && [ \$(grep -c -E 'Pattern Q-E-T[1245] found: .* 1 match,' TweaksV.log) -eq 4 ]"
check "no problem, no box, no site line, no announcement, no summary; nothing written" "log_lacks 'PROBLEM' && log_lacks 'message box not shown' && log_lacks 'DIAGNOSTIC: store of' && nothing_measured && untouched"

echo "== TB6 a site not as expected: the shift between the budgets' stores by 21, not 20"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe tbshift wait)
check "the reason with the bytes found, word for word; the store of the most logged before it, nothing after it" "log_line '${NOT_MEASURED}the code after Q-E-T1 at $(rva stores 21) is not the store of budgets 0 and 1: 66 0F 73 F0 15 F3 0F 7F 05 [0-9A-F ]*.\$' && log_has 'DIAGNOSTIC: store of the most objects' && log_lacks 'DIAGNOSTIC: store of budgets'"
check "no problem, no announcement, no summary; nothing written" "log_lacks 'PROBLEM' && nothing_measured && untouched"

echo "== TB7 the call after the budgets' stores already hooked by another mod: logged, nothing measured"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe tbcallhooked)
check "the call's line and the reason, word for word" "log_line '\[Resources\] DIAGNOSTIC: call after the budgets. stores at $(rva stores 74) -> 0x[0-9A-F]* (outside the game) (Q-E-T2)\$' && log_line '${NOT_MEASURED}the call after the budgets. stores at $(rva stores 74) is not a call into the game (another mod may have hooked it already) (Q-E-T2).\$'"
check "no problem, no announcement; nothing written" "log_lacks 'PROBLEM' && nothing_measured && untouched"

echo "== TB8 an address outside the game: the getter's index"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe tbindexout)
check "the reason, word for word" "log_line '${NOT_MEASURED}the index at 0x[0-9A-F]* (outside the game) (Q-E-T3) is not inside the game.\$'"
check "no problem, no announcement; nothing written" "log_lacks 'PROBLEM' && nothing_measured && untouched"

echo "== TB9 the sites disagree on the counters: the update check's lea 8 bytes past them"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe tbcounters)
check "the reason, word for word, the placement's check logged before it" "log_line '${NOT_MEASURED}the update check at $(rva update) does not read the counters at $(rva counters) (Q-E-T5).\$' && log_has 'DIAGNOSTIC: placement check at'"
check "no problem, no announcement; nothing written" "log_lacks 'PROBLEM' && nothing_measured && untouched"

echo "== TB10 the update check's multiplication by its raise factor turned into an addition"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe tbfactor)
check "the reason with the bytes found, word for word" "log_line '${NOT_MEASURED}the update check.s code at $(rva update 0xA0) is not the multiplication by its factor at $(rva switch 4): F3 44 0F 58 0D [0-9A-F ]* (Q-E-T5).\$'"
check "no problem, no announcement; nothing written" "log_lacks 'PROBLEM' && nothing_measured && untouched"

echo "== TB11 the plain game, DIAGNOSTIC_MODE=1: every pattern missing - logged, not the module's problem"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game plain_game.exe)
check "the scanner's five lines and the module's, word for word" "[ \$(grep -c -E 'Pattern Q-E-T[1-5] not found: ' TweaksV.log) -eq 5 ] && log_line '${NOT_MEASURED}Q-E-T1 was not found.\$'"
check "no problem of the module's, no announcement" "log_lacks '\[Resources\] PROBLEM' && nothing_measured"

echo "== TB12 DRY_RUN=1 with DIAGNOSTIC_MODE=1: the same lines and the same summary; nothing written"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe wait)
check "the five patterns, the eight lines of the sites, the announcement and the summary 10 s in, word for word" "patterns_found && sites_logged && announced && log_line '$SUMMARY1' && first_after_10s"
check "no problem; nothing written" "log_lacks 'PROBLEM' && untouched"

echo
echo "TEXTURE BUDGET SCENARIOS: $pass passed, $fail failed"
