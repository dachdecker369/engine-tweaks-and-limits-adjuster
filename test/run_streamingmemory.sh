#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The streaming memory, measured first for the Resources part to come (SPECIFICATIONS.md 11.10.7), in the full fake game:
# with DIAGNOSTIC_MODE=1 alone its four sites found once each, checked and logged with what each holds, and the fake's
# resource heap summed up 10 seconds in and again when its numbers change - the regions made of 32, their sizes and bytes
# in use, the streamed resources, the streaming budgets and what is counted against them, the refusals; nothing looked for
# without DIAGNOSTIC_MODE; a pattern missing, a call another mod hooked, an address outside the game and an object that
# cannot be read each logged, with no problem, no box and no summary; the plain game; DRY_RUN alike; and nothing written -
# the sites and the data as the fake left them, every page's protection its own. Fake LOCALAPPDATA: the real one is never
# touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata

LABELS="Q-E-H1 Q-E-A1 Q-E-S1 Q-E-R1"
SM='\[Resources\] DIAGNOSTIC: '
# untouched: the fake's four sites as it placed them, its heap, allocator and stamp as it left them, every page's
# protection its own
untouched() { has 'sites as placed 4 of 4, data as left 3 of 3' && has 'protect: all restored (RX)'; }
# not_measured WHY: the module's line, word for word; no problem and no box
not_measured() { log_line "${SM}the streaming memory is not measured: $1\$" && log_lacks 'PROBLEM' && log_lacks 'message box not shown'; }
# summaries: how many summaries of the streaming memory the log holds; summary TEXT: one of them, word for word from
# "the resource heap has"
summaries() { count "${SM}streaming memory: "; }
summary() { log_line "${SM}streaming memory: the resource heap has $1\$"; }
# at LABEL OFFSET: "RVA 0x..." of the byte OFFSET bytes into LABEL's match, from the scanner's line
at() {
	local m
	m=$(tr -d '\r' < TweaksV.log | sed -nE "s/.*Pattern $1 found: .* -> RVA 0x([0-9A-F]+), .*/\1/p" | head -n 1)
	[ -n "$m" ] && printf 'RVA 0x%X' $((16#$m + $2))
}
# reported WHAT: "RVA 0x..." of the fake's heap, allocator or stamp, from its report; called N: of its Nth function
reported() { sed -nE "s/.*streaming memory: .*$1 at (RVA 0x[0-9A-F]+),.*/\1/p" <<< "$out"; }
called() { sed -nE "s/.*streaming memory: .*calls to RVA (0x[0-9A-F]+), (0x[0-9A-F]+) and (0x[0-9A-F]+),.*/RVA \\$1/p" <<< "$out"; }
# expect_sites: the four site lines and the announcement as the last run must have logged them, every RVA from the
# scanner's lines and the fake's report: E_H1, E_A1, E_S1, E_R1, E_ANNOUNCED
expect_sites() {
	E_H1="${SM}resource heap reference at $(at Q-E-H1 3) -> $(reported heap), page size at $(at Q-E-H1 8) holds 8 KB, region size asked at $(at Q-E-H1 14) holds 1024 MB, constructor call at $(at Q-E-H1 18) -> $(called 1) (Q-E-H1)\$"
	E_A1="${SM}streaming allocator reference at $(at Q-E-A1 3) -> $(reported allocator), budgets' reset call at $(at Q-E-A1 7) -> $(called 2) (Q-E-A1)\$"
	E_S1="${SM}refusal stamp reference at $(at Q-E-S1 14) -> $(reported stamp), timeGetTime call at $(at Q-E-S1 7) -> $(called 3) (Q-E-S1)\$"
	E_R1="${SM}region size cap at $(at Q-E-R1 10) holds 256 MB and at $(at Q-E-R1 15) holds 256 MB, region size field at $(at Q-E-R1 3) holds 0x1D8, region size floor at $(at Q-E-R1 26) holds 16 MB (Q-E-R1)\$"
	E_ANNOUNCED="${SM}the resource heap at $(reported heap) (Q-E-H1), pages of 8 KB, regions of 256 MB, 32 at most; the streaming allocator at $(reported allocator) (Q-E-A1); the refusal stamp at $(reported stamp) (Q-E-S1): the regions, the bytes in use and the refusals are logged after 10 seconds, then every minute while they change.\$"
}
sites_logged() { log_line "$E_H1" && log_line "$E_A1" && log_line "$E_S1" && log_line "$E_R1"; }
# the fake's heap as it makes it once TweaksV has started, summed up
FIRST='9 of its 32 regions, 2304 of at most 8192 MB, 1864 MB of them in use, 1710 MB by streamed resources; the streaming budgets 2304 MB virtual and 2304 MB physical, nothing counted against them; no streamed resource refused for memory so far.'

echo "== M1 without DIAGNOSTIC_MODE: nothing looked for, nothing logged, nothing written"
fresh_ini; out=$(game full_game.exe)
check "none of the four patterns in the log, no line of the streaming memory; the sites and the data as the fake left them" "log_lacks 'Q-E-H1' && log_lacks 'Q-E-A1' && log_lacks 'Q-E-S1' && log_lacks 'Q-E-R1' && log_lacks 'streaming memory' && log_lacks 'resource heap' && untouched"

echo "== M2 DIAGNOSTIC_MODE=1: the four sites found and logged, and the fake's heap summed up 10 s in"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wait); echo "      $(grep -o 'streaming memory: heap at[^|]*' <<< "$out")"
expect_sites
check "the four patterns found once each" "(for l in $LABELS; do log_has \"Pattern \$l found: .* -> RVA 0x[0-9A-F]*, 1 match, bytes: \" || exit 1; done)"
check "Q-E-H1, word for word: the heap's reference leads to the fake's heap, 8 KB pages, 1024 MB asked, the constructor's call to the fake's" 'log_line "$E_H1"'
check "Q-E-A1, word for word: the allocator's reference leads to the fake's allocator, the budgets' reset call to the fake's" 'log_line "$E_A1"'
check "Q-E-S1, word for word: the stamp's reference leads to the fake's stamp, the timeGetTime call to the fake's" 'log_line "$E_S1"'
check "Q-E-R1, word for word: both caps 256 MB, the field 0x1D8, the floor 16 MB" 'log_line "$E_R1"'
check "the announcement, word for word, after the four site lines" 'log_line "$E_ANNOUNCED" && [ "$(grep -n "region size cap at" TweaksV.log | cut -d: -f1)" -lt "$(grep -n "the regions, the bytes in use and the refusals are logged" TweaksV.log | cut -d: -f1)" ]'
check "the summary 10 s in, word for word - 9 regions of 256 MB less 8 KB rounded, the budgets the heap's size - and no other" "summary '$FIRST' && [ \$(summaries) -eq 1 ]"
check "no problem; nothing written: the sites and the data as the fake left them, every page's protection its own" "log_lacks 'PROBLEM' && untouched"

echo "== M3 DRY_RUN=1 with DIAGNOSTIC_MODE=1: the same lines and the same summary"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe wait)
expect_sites
check "the four site lines and the announcement, word for word" 'sites_logged && log_line "$E_ANNOUNCED"'
check "the summary 10 s in, word for word" "summary '$FIRST' && [ \$(summaries) -eq 1 ]"
check "no problem; nothing written" "log_lacks 'PROBLEM' && untouched"

echo "== M4 TEST_BREAK_PATTERN (test build, ERROR_POPUP=1): each pattern missing logged, no problem, no box, nothing measured"
for l in $LABELS; do
	fresh_ini; setkey ERROR_POPUP 1; setkey DIAGNOSTIC_MODE 1; addkey TEST_BREAK_PATTERN "$l"
	if [ "$l" = Q-E-R1 ]; then out=$(ASI=TweaksV_test.asi game full_game.exe wait); else out=$(ASI=TweaksV_test.asi game full_game.exe); fi
	check "$l: the line word for word, no problem, no box, no announcement; nothing written" "log_has 'TEST BUILD: pattern $l is treated as not found' && not_measured '$l was not found.' && log_lacks 'the regions, the bytes in use and the refusals are logged' && untouched"
done
check "Q-E-R1, the writing half's: no summary 10 s in" "[ \$(summaries) -eq 0 ]"

echo "== M5 the timeGetTime call another mod has hooked: logged, nothing measured"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe smhooked wait)
check "its site line names the address outside the game; the line word for word, no problem" "log_has '${SM}refusal stamp reference at RVA 0x[0-9A-F]* -> RVA 0x[0-9A-F]*, timeGetTime call at RVA 0x[0-9A-F]* -> 0x[0-9A-F]* (outside the game) (Q-E-S1)' && not_measured 'the timeGetTime call at RVA 0x[0-9A-F]* is not a call into the game (another mod may have hooked it already) (Q-E-S1).'"
check "no announcement, no summary 10 s in; nothing written" "log_lacks 'the regions, the bytes in use and the refusals are logged' && [ \$(summaries) -eq 0 ] && untouched"

echo "== M6 the resource heap's lea leading outside the game: logged, nothing measured"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe smoutside)
check "the line word for word, no problem, no announcement; nothing written" "not_measured 'the resource heap at 0x[0-9A-F]* (outside the game) (Q-E-H1) is not inside the game.' && log_lacks 'the regions, the bytes in use and the refusals are logged' && untouched"

echo "== M7 the streaming allocator's page that cannot be read while TweaksV starts: logged, nothing measured"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe smunreadable)
check "the line word for word, no problem, no announcement; nothing written" "not_measured 'the streaming allocator at RVA 0x[0-9A-F]* (Q-E-A1) cannot be read.' && log_lacks 'the regions, the bytes in use and the refusals are logged' && untouched"

echo "== M8 the heap grown to all 32 regions with a refusal (test build, TWEAKSV_TEST_SUMMARY_MS=3000): summed up again"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(TWEAKSV_TEST_SUMMARY_MS=3000 ASI=TweaksV_test.asi game full_game.exe smgrow)
check "the test build's line: the summaries after the first every 3000 ms" "log_line '\[TweaksV\] TEST BUILD: the streaming memory summary comes every 3000 ms after its first (TWEAKSV_TEST_SUMMARY_MS=3000).\$'"
check "the first summary 10 s in, as without the switch" "summary '$FIRST'"
check "again once the numbers changed, word for word - 32 of 32 regions, the refusal since the last summary 4 to 7 s before it - then no more" "summary '32 of its 32 regions, 8192 of at most 8192 MB, 7384 MB of them in use, 7200 MB by streamed resources; the streaming budgets 8192 MB virtual and 8192 MB physical, nothing counted against them; streamed resources were refused for memory since the last summary, the last [4-7] s ago - the heap has all 32 of its regions.' && [ \$(summaries) -eq 2 ] && log_lacks 'PROBLEM' && untouched"

echo "== M9 a refusal before the first summary, the budgets counting their use, more in use later (test build, 3000 ms)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(TWEAKSV_TEST_SUMMARY_MS=3000 ASI=TweaksV_test.asi game full_game.exe smrefused smcounted smmore)
check "the first summary, word for word: what is counted against the budgets, the refusal 11 to 15 s before it with room in the heap" "summary '9 of its 32 regions, 2304 of at most 8192 MB, 1864 MB of them in use, 1710 MB by streamed resources; the streaming budgets 2304 MB virtual and 2304 MB physical, 12 MB and 40 MB counted against them - so on this build they may bind (PATTERNS.md, \"Resources\"); streamed resources were refused for memory before this summary, the last 1[1-5] s ago - the heap still had room, so another budget refused them.'"
check "the second, word for word: more in use, no refusal since the first, the last 14 to 18 s before it" "summary '9 of its 32 regions, 2304 of at most 8192 MB, 1989 MB of them in use, 1835 MB by streamed resources; the streaming budgets 2304 MB virtual and 2304 MB physical, 12 MB and 40 MB counted against them - so on this build they may bind (PATTERNS.md, \"Resources\"); no streamed resource refused for memory since the last summary, the last 1[4-8] s ago.'"
check "two summaries and no more; no problem; nothing written" "[ \$(summaries) -eq 2 ] && log_lacks 'PROBLEM' && untouched"

echo "== M10 the plain game: every pattern looked for and missing, logged, not a problem of the streaming memory's"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game plain_game.exe)
check "the scanner's line for each of the four" "(for l in $LABELS; do log_has \"Pattern \$l not found: \" || exit 1; done)"
check "the line word for word, the first one named; no problem under its tag" "log_line '${SM}the streaming memory is not measured: Q-E-H1 was not found.\$' && log_lacks '\[Resources\] PROBLEM'"

echo
echo "STREAMING MEMORY SCENARIOS: $pass passed, $fail failed"
