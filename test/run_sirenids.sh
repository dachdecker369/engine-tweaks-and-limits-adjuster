#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The siren setting IDs, measured first (the SirenSettings part to come): with DIAGNOSTIC_MODE=1 only, the 14 patterns
# found in the full fake game, every site checked and logged word for word, and the measurement over the fake's carcols
# data and ID table at the game's sizes - 10 seconds in, again when the numbers change (the test build's
# TWEAKSV_TEST_SUMMARY_MS), before the base files are read, with the table full and with the settings unreadable; a
# pattern missing, the plain game, a site another mod changed, the table moved, split or loaded a fifth time, a member's
# type, count, offset or data, a jump, a fill, a call and a pointer not as the findings have them - each logged, with no
# problem, no box and nothing measured; DRY_RUN alike; nothing written - the fake's sites and data as placed, every page's
# protection its own. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata

# the module's lines: under [SirenSettings], with DIAGNOSTIC: in front
L='\[SirenSettings\] DIAGNOSTIC: '
RVA='RVA 0x[0-9A-F]*'
# its 14 patterns, for the scanner's lines
LABELS='I-E-(P[1-3]|T[1-4]|D[12]|C[1-3]|R1|M1)'
# untouched: TweaksV wrote nothing into the fake's siren sites and data, and every page has its own protection back
untouched() { has 'siren ids: sites as placed 15 of 15, data as placed' && has 'protect: all restored (RX)'; }
# lines LINE...: every line is in the log, word for word
lines() { local l; for l in "$@"; do log_line "${L}$l\$" || { echo "      missing: $l"; return 1; }; done; }
# measured: the announcement, word for word; summary LOADED INUSE PAST TWICE: the summary, word for word; summaries: how many
measured() { lines "the siren settings, through the carcols pointer at $RVA (I-E-M1), and the siren setting ID table at $RVA, 256 IDs (I-E-T1 to I-E-T4): the settings loaded and the IDs in use are logged after 10 seconds, then every minute while the numbers change."; }
summary() { lines "siren settings loaded: $1 (vehicles can use 255 at most); siren setting IDs in use: $2 of 256; settings past position 255, which no vehicle can reach: $3; IDs given to more than one setting: $4."; }
summaries() { grep -c -E "${L}(siren settings loaded: |no siren settings loaded yet|the siren settings at .* could not be read)" TweaksV.log; }
# not_measured WHY: the line, word for word up to WHY's end; no problem, no announcement, no summary
not_measured() { lines "the siren settings are not measured: $1" && log_lacks 'PROBLEM' && log_lacks 'the settings loaded and the IDs in use are logged' && [ "$(summaries)" -eq 0 ]; }
# no_box: nothing reported, so no start-up summary and no box (ERROR_POPUP=1, test build)
no_box() { log_lacks 'Showing the start-up summary' && log_lacks 'TEST BUILD: message box not shown'; }

# every site's line, word for word, in the order of the patterns
SITES=(
	"siren settings' parser registration at $RVA: name hash 0x84EB9B0D, member data table at $RVA, offsets table at $RVA (I-E-P1)"
	"siren setting's ID member: member 0 at +0x0, member 2 at +0x4; its data at $RVA, offset field 0x0, type 0x1 (I-E-P1)"
	"vehicle variations' parser registration at $RVA: name hash 0x18EAFEA9, member data table at $RVA, offsets table at $RVA (I-E-P2)"
	"variation's siren setting ID member: member 6 of 7 at +0x4B, member 5 at +0x4A; its data at $RVA, offset field 0x4B, type 0x1 (I-E-P2)"
	"simple member's XML reader at $RVA: the member data at +0x8 of the member, its type at +0x10, types up to 0x21 (I-E-P3)"
	"reference at $RVA -> $RVA (I-E-T1)"
	"siren setting ID load at $RVA: 0F B6 2C 0F (I-E-T1)"
	"siren setting ID check for none at $RVA: 48 81 FD FF 00 00 00 (I-E-T1)"
	"siren setting ID load at $RVA: 0F B6 1C 2B (I-E-T1)"
	"variation's siren setting ID store at $RVA: 88 1C 0F (I-E-T1)"
	"reference at $RVA -> $RVA (I-E-T2)"
	"siren setting ID store at $RVA: 40 88 31 (I-E-T2)"
	"siren setting ID load at $RVA: 0F B6 2C 39 (I-E-T2)"
	"siren setting ID check for none at $RVA: 48 81 FD FF 00 00 00 (I-E-T2)"
	"limit at $RVA holds 256 (N, I-E-T2) -> game size 256"
	"siren setting ID given when none is free at $RVA: 40 B6 FF (I-E-T2)"
	"reference at $RVA -> $RVA (I-E-T3)"
	"siren setting ID load at $RVA: 0F B6 3C 03 (I-E-T3)"
	"siren setting ID check for none at $RVA: 48 81 FF FF 00 00 00 (I-E-T3)"
	"siren setting ID load at $RVA: 0F B6 14 17 (I-E-T3)"
	"variation's siren setting ID store at $RVA: 88 14 03 (I-E-T3)"
	"carcols pointer load at $RVA -> $RVA (I-E-T4)"
	"reference at $RVA -> $RVA (I-E-T4)"
	"siren setting ID store at $RVA: 40 88 33 (I-E-T4)"
	"siren setting ID load at $RVA: 0F B6 0C 3B (I-E-T4)"
	"siren setting ID load at $RVA: 0F B6 34 3B (I-E-T4)"
	"siren setting ID check for none at $RVA: 48 81 FE FF 00 00 00 (I-E-T4)"
	"limit at $RVA holds 256 (N, I-E-T4) -> game size 256"
	"siren setting ID given when none is free at $RVA: 40 B6 FF (I-E-T4)"
	"the siren setting ID table is at $RVA - 256 entries of 1 byte"
	"pack dedupe's siren setting ID load at $RVA: 43 0F B6 54 3D 00 (I-E-D1)"
	"pack dedupe's check for none at $RVA: 80 FA FF (I-E-D1)"
	"pack dedupe's compare loop at $RVA, its jz -> $RVA (I-E-D2)"
	"siren setting constructor's first store at +0x1C at $RVA: C7 41 1C 00 00 00 00 (I-E-C1)"
	"siren setting constructor's ID default at $RVA: C6 06 FF (I-E-C1)"
	"siren setting constructor's second store at +0x1C at $RVA: 89 46 1C (I-E-C1)"
	"variation's in-place constructor's stores at $RVA: C7 41 30 00 00 00 00 48 C7 41 38 00 00 00 00 C7 41 40 00 00 00 00 C7 41 48 00 00 FF FF C3 (I-E-C2)"
	"variation's allocating constructor's stores at $RVA: C7 40 30 00 00 00 00 48 C7 40 38 00 00 00 00 C7 40 40 00 00 00 00 C7 40 48 00 00 FF FF 48 83 C4 28 C3 (I-E-C3)"
	"base loader call at $RVA -> $RVA, after the siren setting ID table's last fill at $RVA -> $RVA (I-E-R1)"
	"siren settings getter at $RVA: a model's siren setting index at +0x53B, the settings at +0x48 of the carcols data, 0x5E8 bytes each; the carcols pointer at $RVA (I-E-M1)"
)
# the table's RVA, as the module names it; how many of the module's lines lead to it
table_rva() { tr -d '\r' < TweaksV.log | sed -n 's/.*the siren setting ID table is at \(RVA 0x[0-9A-F]*\) - 256 entries of 1 byte$/\1/p'; }
to_table() { tr -d '\r' < TweaksV.log | grep -c -E "${L}reference at RVA 0x[0-9A-F]+ -> $(table_rva) \($1\)$"; }

echo "== I1 without DIAGNOSTIC_MODE: nothing looked for, nothing measured"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "none of the 14 patterns looked for, no line of the module; nothing written" "[ \$(grep -c -E 'Pattern $LABELS' TweaksV.log) -eq 0 ] && log_lacks '\[SirenSettings\]' && untouched"

echo "== I2 DIAGNOSTIC_MODE=1: every pattern found once, every site's line, the announcement, the summary 10 s in"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wait)
check "the 14 patterns each found exactly once" "[ \$(grep -c -E 'Pattern $LABELS found: .* 1 match,' TweaksV.log) -eq 14 ]"
check "every site's line, word for word: the registrations, the reader, the loaders, the dedupe, the constructors, the init, the getter" "lines \"\${SITES[@]}\""
check "the 2 limits of each builder and the 4 lea of the table, to one table, and the scan's 4 to the same; the IDs' bytes 8 loads and 2 stores" "[ \$(count '${L}limit at ') -eq 4 ] && [ \$(to_table 'I-E-T[1-4]') -eq 4 ] && [ \$(to_table 'lea scan') -eq 4 ] && [ \$(count '${L}siren setting ID load at ') -eq 8 ] && [ \$(count '${L}siren setting ID store at ') -eq 2 ]"
check "the announcement, word for word" "measured"
check "the summary 10 s in, once, at the game's sizes: 24 settings, 24 of the 256 IDs in use, none past position 255, no ID given twice" "summary 24 24 0 0 && [ \$(summaries) -eq 1 ]"
check "no problem; nothing written - the sites and the data as placed, every page's protection its own" "log_lacks 'PROBLEM' && untouched"

echo "== I3 the test build, TWEAKSV_TEST_SUMMARY_MS=500: a pack's 6 settings at 12 s, the summary again when the numbers change"
export TWEAKSV_TEST_SUMMARY_MS=500
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(ASI=TweaksV_test.asi game full_game.exe sipack)
check "the test build's line, once for the summary of the siren settings" "[ \$(count 'TEST BUILD: the siren settings summary comes every 500 ms after its first (TWEAKSV_TEST_SUMMARY_MS=500).') -eq 1 ]"
check "two summaries: 24 settings 10 s in, then 30, the pack's IDs in use too - and no more" "summary 24 24 0 0 && summary 30 30 0 0 && [ \$(summaries) -eq 2 ] && grep -m 1 'siren settings loaded: ' TweaksV.log | grep -q 'loaded: 24 '"
check "nothing written; the data as the fake left it after the pack" "log_lacks 'PROBLEM' && untouched"

echo "== I4 the same, the game's base files not read yet at 10 s (the carcols pointer null), read at 12 s"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(ASI=TweaksV_test.asi game full_game.exe sinone)
check "the first summary says so, word for word; the second has the 24 settings - and no more" "lines 'no siren settings loaded yet - the game has not read carcols.ymt.' && summary 24 24 0 0 && [ \$(summaries) -eq 2 ] && grep -m 1 -E 'siren settings loaded' TweaksV.log | grep -q 'no siren settings loaded yet'"
check "nothing written" "log_lacks 'PROBLEM' && untouched"
unset TWEAKSV_TEST_SUMMARY_MS

echo "== I5 300 settings, given their IDs as the game gives them: the table full, 44 past position 255, the ID 255 to the last 45"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe simany wait)
check "the summary, word for word: 256 of 256 IDs in use, 44 past position 255, one ID given to more than one setting" "summary 300 256 44 1 && untouched"

echo "== I6 the settings' array in memory that cannot be read: said, not counted"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe siunreadable wait)
check "the summary says so, word for word; no problem" "lines 'the siren settings at 0x[0-9A-F]* (outside the game) could not be read this time (the game may have been changing them).' && [ \$(summaries) -eq 1 ] && log_lacks 'PROBLEM' && untouched"

echo "== I7 DRY_RUN=1 with DIAGNOSTIC_MODE=1: alike - nothing of the module is written either way"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe wait)
check "the 14 patterns, every site's line, the announcement and the summary as without DRY_RUN; nothing written" "[ \$(grep -c -E 'Pattern $LABELS found: .* 1 match,' TweaksV.log) -eq 14 ] && lines \"\${SITES[@]}\" && measured && summary 24 24 0 0 && untouched"

echo "== I8 TEST_BREAK_PATTERN=I-E-T3 (test build), ERROR_POPUP=1: logged, no problem, no box, no summary"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey ERROR_POPUP 1; addkey TEST_BREAK_PATTERN I-E-T3; out=$(ASI=TweaksV_test.asi game full_game.exe wait)
check "the pattern treated as not found, every other looked for; the line, word for word" "log_has 'TEST BUILD: pattern I-E-T3 is treated as not found (TEST_BREAK_PATTERN).' && [ \$(grep -c -E 'Pattern $LABELS found: ' TweaksV.log) -eq 13 ] && not_measured 'I-E-T3 was not found.'"
check "no box, nothing written" "no_box && untouched"

echo "== I9 a site another mod changed - the base loop's first ID load a word load already - ERROR_POPUP=1 (test build)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey ERROR_POPUP 1; out=$(ASI=TweaksV_test.asi game full_game.exe siwidened wait)
check "the line, word for word, with the bytes found and those expected" "not_measured 'the siren setting ID load at $RVA holds 0F B7 2C 0F, not 0F B6 2C 0F (I-E-T1).'"
check "no box, nothing written" "no_box && untouched"

echo "== I10 an address outside the game: the four lea of the table lead outside it (another mod moved it)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe simoved)
check "the line, word for word; nothing written" "not_measured 'the siren setting ID table is not inside the game: 0x[0-9A-F]* (outside the game) (another mod may have moved it already).' && untouched"

echo "== I11 the plain game: every pattern missing, the first named"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game plain_game.exe)
check "the 14 not found, the line names I-E-P1; no problem of the module's (the parts report theirs), nothing measured" "[ \$(grep -c -E 'Pattern $LABELS not found: ' TweaksV.log) -eq 14 ] && lines 'the siren settings are not measured: I-E-P1 was not found.' && log_lacks '\[SirenSettings\] PROBLEM' && log_lacks 'the settings loaded and the IDs in use are logged'"

echo "== I12 the parser registrations not as the findings have them, each logged and nothing measured"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe sitype)
check "the variations' ID member of type 0x3, a word: its line, then the reason, word for word" "lines \"variation's siren setting ID member: member 6 of 7 at +0x4B, member 5 at +0x4A; its data at $RVA, offset field 0x4B, type 0x3 (I-E-P2)\" && not_measured \"the variation's siren setting ID member's type at $RVA is 0x3, not 0x1 or 0x2, which store a byte (I-E-P2).\" && untouched"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe simembers)
check "an eighth member of the variations" "not_measured \"the vehicle variations' parser registration has 8 or more members, not 7 (I-E-P2).\" && untouched"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe sioffsets)
check "the siren settings' member 2 at +0x8" "not_measured \"the siren settings' members 0 and 2 are at +0x0 and +0x8, not +0x0 and +0x4 (I-E-P1).\" && untouched"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe sidataout)
check "the ID member's data outside the game" "not_measured \"the siren setting's ID member's data is not inside the game: 0x[0-9A-F]* (outside the game) (I-E-P1).\" && untouched"

echo "== I13 the ID table split or loaded a fifth time, each logged and nothing measured"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe sisplit)
check "a pack's builder's lea 16 bytes into the table" "not_measured \"the references to the siren setting ID table do not all point to the game's own table: $RVA and $RVA (I-E-T4).\" && untouched"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe sififth)
check "a fifth lea: the scan's 5 lines, then the reason" "[ \$(to_table 'lea scan') -eq 5 ] && not_measured 'the game code loads the siren setting ID table in 5 or more places, not the expected 4 (another mod may have moved some of them, or the game has changed).' && untouched"

echo "== I14 a jump, a fill, a call and a pointer not as the findings have them, each logged and nothing measured"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe sijz)
check "the dedupe loop's jz outside the game" "not_measured \"the jz of the pack dedupe's compare loop at $RVA does not lead into the game (another mod may have changed it) (I-E-D2).\" && untouched"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe sifill)
check "the init's last fill 16 bytes below the table's last 16" "not_measured \"the store before the base loader call at $RVA fills $RVA, not the siren setting ID table's last 16 bytes at $RVA (I-E-R1).\" && untouched"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe sicallhooked)
check "the base loader call another mod redirected" "not_measured 'the base loader call at $RVA is not a call into the game (another mod may have hooked it already) (I-E-R1).' && untouched"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe sicarcols)
check "the getter's carcols pointer not a pack builder's" "not_measured \"the carcols pointer the siren settings getter reads, $RVA, is not the one a pack's builder reads, $RVA (I-E-M1).\" && untouched"

echo
echo "SIREN IDS SCENARIOS: $pass passed, $fail failed"
