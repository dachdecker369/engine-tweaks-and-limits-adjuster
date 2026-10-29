#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The ped props, measured first (TweaksVPedProps, the PedProps part to come): with DIAGNOSTIC_MODE=1 the full fake game's
# eleven narrowings found and checked as the part will check them, each with the call in its match; the complete sets
# through the scanner's FindCallsTo - the per-anchor count called exactly 7 times and the metadata lookup exactly 17,
# the calls that lead elsewhere not counted, one call more refused; the model info store walked 10 seconds in and again
# when its numbers change, every ped model's props per anchor at the game's offsets, only the first entry of an anchor
# in a file counted; anchors and files past 255 named, the lines capped; a bound broken and memory that cannot be read
# ending a pass with a line; A-E-G1 once and three times, a call that leads elsewhere, a call another mod hooked, the
# store outside the game, a pattern missing, the plain game - each logged, never a problem, never a box, no summary;
# DRY_RUN alike; nothing written to the game, ever. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata

ASIS='pedprops: sites as placed, data as placed |'
STORE='(the model info store.s 60010 indices, its stride 16)'
SUMMARY="\[PedProps\] DIAGNOSTIC: ped props: 43 ped models with variation data in 99 files $STORE; the most props on one anchor: 212 (mp_m_freemode_01, anchor 0, from 30 files, at most 38 in one); no anchor and no file past 255.\$"
ANNOUNCED='\[PedProps\] DIAGNOSTIC: the model info store at RVA 0x[0-9A-F]* (A-E-S1): the props per anchor of its ped models are logged after 10 seconds, then every minute while they change.$'
# not_measured WHY: the module's line, word for word, and nothing more of the measurement: not announced, no problem, no
# box, the fake's sites and data as placed
not_measured() { log_line "\[PedProps\] DIAGNOSTIC: the ped props are not measured: $1\$" && log_lacks 'DIAGNOSTIC: the model info store at' && log_lacks 'PROBLEM' && log_lacks 'Start-up summary' && has "$ASIS"; }
# not_counted WHY: the measurement announced, its pass ended with this line and no summary; no problem, nothing written
not_counted() { log_line "\[PedProps\] DIAGNOSTIC: the ped props are not counted: $1\$" && log_line "$ANNOUNCED" && log_lacks 'DIAGNOSTIC: ped props: ' && log_lacks 'PROBLEM' && has "$ASIS"; }
# targets KIND: the targets of the KIND call lines, the patterns' and the scan's, each once; count_start: the per-anchor
# count's own RVA; first_line PATTERN: the number of the first line that matches
targets() { tr -d '\r' < TweaksV.log | sed -n "s/.*\] DIAGNOSTIC: $1 call at RVA 0x[0-9A-F]* -> \(RVA 0x[0-9A-F]*\) (.*/\1/p" | sort -u; }
count_start() { tr -d '\r' < TweaksV.log | sed -n 's/.*\] DIAGNOSTIC: per-anchor count at \(RVA 0x[0-9A-F]*\):.*/\1/p'; }
first_line() { tr -d '\r' < TweaksV.log | grep -n -- "$1" | head -n 1 | cut -d: -f1; }
# the sites, each line word for word but its RVA - every narrowing with its bytes, every call; then the scans and the layout
sites_logged() {
	local n
	for n in "0F B6 C3 (an anchor.s total, A-E-T1)" "0F B6 C0 (an anchor.s total, A-E-T2)" "0F B6 C0 (an anchor.s total, A-E-T3)" \
		"0F B6 C0 (an anchor.s total, A-E-T4)" "0F B6 C0 (an anchor.s total, A-E-T5)" "0F B6 E8 (a worn prop.s index, A-E-G3)" \
		"44 0F B6 C0 (a worn prop.s index, A-E-G4)" "44 0F B6 C0 (a worn prop.s index, A-E-G5)" "44 0F B6 F7 (a worn prop.s index, A-E-G6)"; do
		[ "$(count "\[PedProps\] DIAGNOSTIC: narrowing at RVA 0x[0-9A-F]* holds $n")" -eq 1 ] || return 1
	done
	[ "$(count '\[PedProps\] DIAGNOSTIC: narrowing at RVA 0x[0-9A-F]* holds 44 0F B6 C6 (a worn prop.s index, A-E-G1)')" -eq 2 ] &&
	[ "$(count '\[PedProps\] DIAGNOSTIC: texture count call at RVA 0x[0-9A-F]* -> RVA 0x[0-9A-F]* (A-E-T1)')" -eq 1 ] &&
	[ "$(count '\[PedProps\] DIAGNOSTIC: per-anchor count call at RVA 0x[0-9A-F]* -> RVA 0x[0-9A-F]* (A-E-T[2-5])')" -eq 4 ] &&
	[ "$(count '\[PedProps\] DIAGNOSTIC: metadata lookup call at RVA 0x[0-9A-F]* -> RVA 0x[0-9A-F]* (A-E-G[13-6])')" -eq 6 ] &&
	[ "$(count '\[PedProps\] DIAGNOSTIC: per-anchor count call at RVA 0x[0-9A-F]* -> RVA 0x[0-9A-F]* (call scan)')" -eq 7 ] &&
	[ "$(count '\[PedProps\] DIAGNOSTIC: metadata lookup call at RVA 0x[0-9A-F]* -> RVA 0x[0-9A-F]* (call scan)')" -eq 17 ] &&
	log_line '\[PedProps\] DIAGNOSTIC: per-anchor count at RVA 0x[0-9A-F]*: a model info.s variation collection at +0x240, its count of variation files at +0x8 (A-E-C1)$' &&
	log_line '\[PedProps\] DIAGNOSTIC: its loop at RVA 0x[0-9A-F]*: a variation file.s anchors at +0x58, their count at +0x60, entries of 0x18 bytes, an entry.s props at +0x8 (A-E-C2)$' &&
	log_line '\[PedProps\] DIAGNOSTIC: model info store at RVA 0x[0-9A-F]*: its count at RVA 0x[0-9A-F]*, its bitmap.s pointer at RVA 0x[0-9A-F]*, its array.s pointer at RVA 0x[0-9A-F]*, its stride at RVA 0x[0-9A-F]*; a model info.s type in the low 5 bits of +0x9D, 6 for a ped (A-E-S1)$'
}
# each kind of call leads to one function, the per-anchor count's to its start
one_target_each() { [ "$(targets 'per-anchor count' | wc -l)" -eq 1 ] && [ "$(targets 'per-anchor count')" = "$(count_start)" ] && [ "$(targets 'metadata lookup' | wc -l)" -eq 1 ]; }

echo "== PP1 DIAGNOSTIC_MODE=0: nothing looked for, nothing measured"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "no line of the module, none of its patterns looked for; the sites and the data as placed, every page's protection its own" "log_lacks '\[PedProps\]' && log_lacks 'A-E-' && has '$ASIS' && has 'protect: all restored (RX)' && log_lacks 'PROBLEM'"

echo "== PP2 DIAGNOSTIC_MODE=1: every pattern found as often as it must match, every site logged, the complete sets, the summary 10 s in"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wait)
check "12 patterns found once each, A-E-G1 twice with a line per match" "[ \$(grep -c -E 'Pattern A-E-(T[1-5]|G[3-6]|C[12]|S1) found: .* 1 match,' TweaksV.log) -eq 12 ] && log_has 'Pattern A-E-G1 found: .* 2 matches,' && [ \$(count 'A-E-G1 match [12] of 2 at RVA') -eq 2 ]"
check "the eleven narrowings, each with its bytes, and the calls in their matches; the layout's sites and the store's four globals" "sites_logged"
check "the complete sets: exactly 7 calls of the per-anchor count and 17 of the metadata lookup - the calls that lead elsewhere not counted - each kind to one function, the count's to its start" "one_target_each"
check "announced, word for word" "log_line '$ANNOUNCED'"
check "the summary 10 s in, word for word: 43 ped models of 60010 indices - the other models, the ped without variation data, the null model info, the bit that is clear and the index past the count not counted - the most on one anchor 212, the second entry of anchor 0 in a file not counted" "log_line \"$SUMMARY\" && [ \$(count 'DIAGNOSTIC: ped props: ') -eq 1 ]"
check "no line past 255, no problem; nothing written - the sites and the data as placed, so no call redirected - every page's protection its own" "log_lacks 'past the game.s own limit' && log_lacks 'PROBLEM' && has '$ASIS' && has 'protect: all restored (RX)'"

echo "== PP3 two anchors and a file past 255: each named, word for word"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppmany wait)
check "the summary: the most on one anchor 356, two anchors and one file past 255" "log_line \"\[PedProps\] DIAGNOSTIC: ped props: 43 ped models with variation data in 115 files $STORE; the most props on one anchor: 356 (mp_f_freemode_01, anchor 2, from 29 files, at most 270 in one); past 255: 2 anchors and 1 file, each in a line of its own.\\\$\""
check "mp_m_freemode_01's anchor 0: 302 props, refused from index 46 on" "log_line '\[PedProps\] DIAGNOSTIC: ped props: mp_m_freemode_01, anchor 0: 302 props from 45 files - past the game.s own limit of 255: the game.s own checks refuse its props from index 46 on.$'"
check "mp_f_freemode_01's file 29 of 29 with 270 props on anchor 2" "log_line '\[PedProps\] DIAGNOSTIC: ped props: mp_f_freemode_01, anchor 2: its file 29 of 29 holds 270 props - a file holds 255 at most, its props past 255 stay unusable with or without TweaksV.$'"
check "mp_f_freemode_01's anchor 2: 356 props, refused from index 100 on" "log_line '\[PedProps\] DIAGNOSTIC: ped props: mp_f_freemode_01, anchor 2: 356 props from 29 files - past the game.s own limit of 255: the game.s own checks refuse its props from index 100 on.$'"
check "in that order, after the summary; no problem, nothing written" "[ \$(first_line 'DIAGNOSTIC: ped props: 43') -lt \$(first_line 'mp_m_freemode_01, anchor 0: 302') ] && [ \$(first_line 'mp_m_freemode_01, anchor 0: 302') -lt \$(first_line 'its file 29 of 29') ] && [ \$(first_line 'its file 29 of 29') -lt \$(first_line 'anchor 2: 356 props') ] && log_lacks 'PROBLEM' && has '$ASIS'"

echo "== PP4 the test build, TWEAKSV_TEST_SUMMARY_MS=2000: the summary 10 s in and again when the numbers change - 5 more files at 12 s - and not while they stay; an entry of anchor 13, said once"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(TWEAKSV_TEST_SUMMARY_MS=2000 ASI=TweaksV_test.asi game full_game.exe ppgrow ppunknown wait)
check "the switch said once for this measurement" "[ \$(count 'TEST BUILD: the ped props summary comes every 2000 ms after its first (TWEAKSV_TEST_SUMMARY_MS=2000).') -eq 1 ]"
check "the first summary, word for word: 212 on anchor 0 from 30 files; the entry of anchor 13 counted apart" "log_line \"\[PedProps\] DIAGNOSTIC: ped props: 43 ped models with variation data in 99 files $STORE; the most props on one anchor: 212 (mp_m_freemode_01, anchor 0, from 30 files, at most 38 in one); 1 anchor entry names no anchor of 0..12; no anchor and no file past 255.\\\$\""
check "the second, once the numbers changed: 242 from 35 files of 104" "log_line \"\[PedProps\] DIAGNOSTIC: ped props: 43 ped models with variation data in 104 files $STORE; the most props on one anchor: 242 (mp_m_freemode_01, anchor 0, from 35 files, at most 38 in one); 1 anchor entry names no anchor of 0..12; no anchor and no file past 255.\\\$\""
check "two summaries, in that order - none while the numbers stayed, over 11 s of passes every 2 s" "[ \$(count 'DIAGNOSTIC: ped props: 43') -eq 2 ] && [ \$(first_line 'in 99 files') -lt \$(first_line 'in 104 files') ]"
check "the entry of anchor 13, word for word, logged once over both summaries" "[ \$(count 'names anchor 13') -eq 1 ] && log_line '\[PedProps\] DIAGNOSTIC: ped props: an anchor entry of mp_m_freemode_01 names anchor 13, not one of 0..12 - such entries are counted apart (logged once).$'"
check "no problem; nothing written - the data as the fake grew it" "log_lacks 'PROBLEM' && has '$ASIS'"

echo "== PP5 10,001 more peds, each with a file of 300 props on one anchor: 20,000 lines past 255, then only counted"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppflood wait)
check "the summary has the numbers: 10001 anchors and 10001 files past 255" "log_line \"\[PedProps\] DIAGNOSTIC: ped props: 10044 ped models with variation data in 10100 files $STORE; the most props on one anchor: 300 (model 0x2C000000, anchor 1, from 1 file, at most 300 in one); past 255: 10001 anchors and 10001 files, each in a line of its own.\\\$\""
check "a model by its hash: its file, then its anchor, word for word; refused from index 44 on" "log_line '\[PedProps\] DIAGNOSTIC: ped props: model 0x2C000000, anchor 1: its file 1 of 1 holds 300 props - a file holds 255 at most, its props past 255 stay unusable with or without TweaksV.$' && log_line '\[PedProps\] DIAGNOSTIC: ped props: model 0x2C000000, anchor 1: 300 props from 1 file - past the game.s own limit of 255: the game.s own checks refuse its props from index 44 on.$'"
check "20000 lines, the 10000th model's the last, then the cap line once" "[ \$(count '\[PedProps\] DIAGNOSTIC: ped props: model 0x2C[0-9A-F]*, anchor 1: ') -eq 20000 ] && log_has 'model 0x2C00270F, anchor 1: 300 props' && log_lacks 'model 0x2C002710' && [ \$(count 'DIAGNOSTIC: 20000 lines of ped props past 255 written - from now on they are only counted; the summary every minute has the numbers.') -eq 1 ]"
check "no problem; nothing written" "log_lacks 'PROBLEM' && has '$ASIS'"

echo "== PP6 the store's count holds 2,000,000: the pass ends with a line, no summary"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppbadcount wait)
check "the line, word for word; no summary, no problem, nothing written" "not_counted 'the model info store.s count, 2000000, is not plausible.'"

echo "== PP7 the store's stride holds 12: the pass ends with a line, no summary"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppbadstride wait)
check "the line, word for word; no summary, no problem, nothing written" "not_counted 'the model info store.s stride, 12, is not plausible.'"

echo "== PP8 a variation file in memory that cannot be read: the access caught, the pass ended with a line, no summary"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppunreadable wait)
check "the line names the model and the file; no summary, no problem, nothing written" "not_counted 'a variation file of mp_f_freemode_01 at 0x[0-9A-F]* (outside the game) cannot be read.'"

echo "== PP9 TEST_BREAK_PATTERN=A-E-T1 (the test build): logged, no problem, no box, no summary"
fresh_ini; setkey DIAGNOSTIC_MODE 1; addkey TEST_BREAK_PATTERN A-E-T1; out=$(ASI=TweaksV_test.asi game full_game.exe wait)
check "the pattern treated as not found; the others still looked for; the line, word for word" "log_has 'TEST BUILD: pattern A-E-T1 is treated as not found (TEST_BREAK_PATTERN).' && [ \$(grep -c -E 'Pattern A-E-(T[2-5]|G[13-6]|C[12]|S1) found: ' TweaksV.log) -eq 12 ] && not_measured 'A-E-T1 was not found.'"
check "no site checked, no summary after 10 s" "log_lacks 'DIAGNOSTIC: narrowing at' && log_lacks 'DIAGNOSTIC: ped props: '"

echo "== PP10 TEST_BREAK_PATTERN=A-E-G1, the pattern that must match twice"
fresh_ini; setkey DIAGNOSTIC_MODE 1; addkey TEST_BREAK_PATTERN A-E-G1; out=$(ASI=TweaksV_test.asi game full_game.exe)
check "the line, word for word; nothing more" "not_measured 'A-E-G1 was not found.'"

echo "== PP11 TEST_BREAK_PATTERN=A-E-S1, a pattern of the measurement's layout"
fresh_ini; setkey DIAGNOSTIC_MODE 1; addkey TEST_BREAK_PATTERN A-E-S1; out=$(ASI=TweaksV_test.asi game full_game.exe)
check "the line, word for word; no site checked" "not_measured 'A-E-S1 was not found.' && log_lacks 'DIAGNOSTIC: narrowing at'"

echo "== PP12 A-E-G1 only once (a changed game)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppg1once)
check "the line, word for word" "not_measured 'the game code holds A-E-G1 1 time, not the expected 2.'"

echo "== PP13 A-E-G1 three times (a changed game)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppg1thrice)
check "the scanner's 3 matches; the line, word for word" "log_has 'Pattern A-E-G1 found: .* 3 matches,' && not_measured 'the game code holds A-E-G1 3 or more times, not the expected 2.'"

echo "== PP14 an 8th call of the per-anchor count (a changed game): the scan finds it, refused"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppcountcall)
check "8 calls logged; the line, word for word" "[ \$(count 'per-anchor count call at RVA .* (call scan)') -eq 8 ] && not_measured 'the game code calls the per-anchor count at RVA 0x[0-9A-F]* in 8 or more places, not the expected 7 (another mod may have redirected some of them, or the game has changed).'"

echo "== PP15 an 18th call of the metadata lookup (a changed game): the scan finds it, refused"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe pplookupcall)
check "the count's 7 calls, then 18 of the lookup logged; the line, word for word" "[ \$(count 'per-anchor count call at RVA .* (call scan)') -eq 7 ] && [ \$(count 'metadata lookup call at RVA .* (call scan)') -eq 18 ] && not_measured 'the game code calls the metadata lookup at RVA 0x[0-9A-F]* in 18 or more places, not the expected 17 (another mod may have redirected some of them, or the game has changed).'"

echo "== PP16 A-E-T3's call leads into the per-anchor count's loop, not to its start: a site not as expected"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppcountsplit)
check "T2's call passed, T3's line, word for word; T4's never looked at" "log_has 'per-anchor count call at RVA .* (A-E-T2)' && log_lacks 'per-anchor count call at RVA .* (A-E-T4)' && not_measured 'the per-anchor count call at RVA 0x[0-9A-F]* leads to RVA 0x[0-9A-F]*, not to the per-anchor count at RVA 0x[0-9A-F]* (A-E-C1) (A-E-T3).'"

echo "== PP17 A-E-G4's call leads to another function than the other metadata lookup calls"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe pplookupsplit)
texture=$(tr -d '\r' < TweaksV.log | sed -n 's/.*texture count call at RVA 0x[0-9A-F]* -> \(RVA 0x[0-9A-F]*\) (A-E-T1).*/\1/p')
check "the line names both functions, the second the texture count's, word for word" "[ -n '$texture' ] && not_measured 'the metadata lookup calls do not lead to the same function: RVA 0x[0-9A-F]* and $texture (A-E-G4).'"

echo "== PP18 A-E-T1's call leads outside the game (another mod hooked it)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe pphooked)
check "the call's line names the address outside the game; the line, word for word" "log_has 'texture count call at RVA 0x[0-9A-F]* -> 0x[0-9A-F]* (outside the game) (A-E-T1)' && not_measured 'the texture count call at RVA 0x[0-9A-F]* is not a call into the game (another mod may have hooked it already) (A-E-T1).'"

echo "== PP19 A-E-S1's reference to the store's array pointer leads outside the game"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe ppstoreout)
check "the line, word for word" "not_measured 'the model info store.s array.s pointer at 0x[0-9A-F]* (outside the game) (A-E-S1) is not inside the game.'"

echo "== PP20 the plain game, DIAGNOSTIC_MODE=1: every pattern missing, logged; no problem of the module"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game plain_game.exe)
check "the 13 patterns each named by the scanner; the line, word for word; no problem of the module, nothing announced" "[ \$(grep -c -E 'Pattern A-E-(T[1-5]|G[13-6]|C[12]|S1) not found: ' TweaksV.log) -eq 13 ] && log_line '\[PedProps\] DIAGNOSTIC: the ped props are not measured: A-E-T1 was not found.$' && log_lacks '\[PedProps\] PROBLEM' && log_lacks 'DIAGNOSTIC: the model info store at'"

echo "== PP21 DRY_RUN=1 with DIAGNOSTIC_MODE=1: the same, a dry run changes nothing in the module"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe wait)
check "every site logged and checked, the complete sets, announced" "sites_logged && one_target_each && log_line '$ANNOUNCED'"
check "the summary 10 s in, word for word; no problem, nothing written" "log_line \"$SUMMARY\" && log_lacks 'PROBLEM' && has '$ASIS' && has 'protect: all restored (RX)'"

echo
echo "PED PROPS SCENARIOS: $pass passed, $fail failed"
