#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The weapon components measured first (TweaksVWeaponComponents, the WeaponLimits part's): with DIAGNOSTIC_MODE=1 the
# fake game's list of every weapon component definition - its rebuild, its sort, its update of the count, the pool's
# size call and the list's 15 other leas - found, checked and logged, and its count and the files' count summed up 10
# seconds in and again when they change: the fake's 412 definitions from 23 files at the game's sizes, a pack's 430
# from 24, the list full, the list overflowed; a pattern broken, the list another mod moved, a lea fewer, one more,
# one with another prefix, the count's references disagreeing, the count not a whole number of entries after the list
# and the pool size call another mod hooked - each logged, no problem, no box, no summary; the plain game; DRY_RUN
# alike; nothing looked for without DIAGNOSTIC_MODE; and nothing written in any of them: the fake's sites and data as
# placed, the size call to the game's lookup, every page's protection its own. Fake LOCALAPPDATA: the real one is
# never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata

# the fake's sites and data as it placed them, its size call to its stand-in for the game's lookup
PLACED='wc: sites as placed, data as placed, size call to the lookup'
RVA='RVA 0x[0-9A-F]*'
TAG='\[WeaponLimits\] DIAGNOSTIC: '
NOT_MEASURED="${TAG}the weapon components are not measured: "
ANNOUNCED="${TAG}the weapon component list at $RVA, 470 entries of 8 bytes up to its count at $RVA, and the count of weaponcomponents.meta files at $RVA: the definitions and the files are logged after 10 seconds, then every minute while the numbers change.\$"
# summary N F [END]: the summary of N definitions from F files, word for word (END: what follows the game's 470)
summary() { log_line "${TAG}$1 weapon component definitions from $2 weaponcomponents.meta files (the game.s own list holds 470)${3:-.}\$"; }
summaries() { count 'weapon component definitions from'; }
# target KIND LABEL: the RVA the module's line of that kind ("reference", "count reference", ...) leads to, for LABEL
target() { tr -d '\r' < TweaksV.log | sed -n "s/.*${TAG}$1 at RVA 0x[0-9A-F]* -> RVA \(0x[0-9A-F]*\) ($2)\$/\1/p"; }
# quiet: no problem of any part, no start-up summary and no box; unannounced: no announcement of the measurement;
# no_site_line: none of the module's site lines, each of which names a W-E-C pattern or the lea scan at its end - the
# weapon infos module logs its own sites under the same tag; loads: the module's lines of the list's loads
quiet() { log_lacks 'PROBLEM' && log_lacks -i 'start-up summary' && log_lacks 'message box'; }
unannounced() { log_lacks 'the definitions and the files are logged'; }
no_site_line() { log_lacks "${TAG}.*(W-E-C[1-4])" && log_lacks "${TAG}.*(lea scan)"; }
loads() { count "${TAG}reference at .* (lea scan)"; }
# checked: the four patterns found once each, every site's line, the 17 loads and the announcement
checked() {
	[ "$(grep -c -E 'Pattern W-E-C[1-4] found: .* 1 match,' TweaksV.log)" -eq 4 ] && [ "$(count "${TAG}reference at .* (W-E-C[12])\$")" -eq 2 ] &&
	[ "$(count "${TAG}count reference at ")" -eq 4 ] && [ "$(loads)" -eq 17 ] && log_line "$ANNOUNCED"
}

echo "== W1 the defaults, DIAGNOSTIC_MODE=0: nothing looked for, nothing written"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "none of the four patterns looked for, no line of the module" "log_lacks 'W-E-C' && log_lacks '\[WeaponLimits\]'"
check "the sites and the data as placed, the size call to the game's lookup, every page back to its own protection, no problem" "has '$PLACED' && has 'protect: all restored (RX)' && quiet"

echo "== W2 DIAGNOSTIC_MODE=1: the four patterns found once each, every site checked and logged, the measurement announced"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe); echo "      $out"
list=$(target reference W-E-C1); counted=$(target 'count reference' W-E-C1); files=$(target "files' count reference" W-E-C1)
check "W-E-C1 to W-E-C4 found exactly once each" "[ \$(grep -c -E 'Pattern W-E-C[1-4] found: .* 1 match,' TweaksV.log) -eq 4 ]"
check "the rebuild's and the sort's leas lead to one list" "[ -n '$list' ] && [ \"\$(target reference W-E-C2)\" = '$list' ]"
check "the count's four references - the rebuild's reset, the sort's read, the update's read and write - lead to one count, 470 x 8 bytes after the list" "[ \$(count '${TAG}count reference at ') -eq 4 ] && [ \"\$(target 'count reference' 'W-E-C[123]' | sort -u)\" = '$counted' ] && [ \$(( $counted - $list )) -eq 3760 ]"
check "the files' count 16 bytes after the count, as the game's lies; the entry size 8" "[ \$(( $files - $counted )) -eq 16 ] && log_line '${TAG}entry size at $RVA holds 8 (W-E-C2)\$'"
check "the pool's size call, its name and its default, word for word" "log_line '${TAG}pool size call at $RVA -> $RVA for the pool 0xBAE33CCF, its default at $RVA holds 470, the game.s configuration at $RVA (W-E-C4)\$'"
check "every lea of the list logged: 17, each leading to it" "[ \$(grep -c '${TAG}reference at $RVA -> RVA $list (lea scan)' TweaksV.log) -eq 17 ] && [ \$(loads) -eq 17 ]"
check "the announcement, word for word" "log_line '$ANNOUNCED'"
check "no summary within the first seconds" "log_lacks 'weapon component definitions'"
check "nothing written - the sites and the data as placed, the size call to the game's lookup, every page its own protection; no problem" "has '$PLACED' && has 'protect: all restored (RX)' && quiet"

echo "== W3 DIAGNOSTIC_MODE=1 for 12 seconds: the summary 10 s in, the fake's numbers at the game's sizes"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wait)
check "the summary, word for word: 412 definitions from 23 files against the game's own 470, once" "summary 412 23 && [ \$(summaries) -eq 1 ]"
check "nothing written, no problem" "has '$PLACED' && quiet"

echo "== W4 the summary again when the numbers change: a pack's file loads after the first (test build, TWEAKSV_TEST_SUMMARY_MS=1000)"
export TWEAKSV_TEST_SUMMARY_MS=1000
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(ASI=TweaksV_test.asi game full_game.exe wcpack wait)
unset TWEAKSV_TEST_SUMMARY_MS
check "the test build's line, once" "[ \$(count 'TEST BUILD: the weapon components summary comes every 1000 ms after its first (TWEAKSV_TEST_SUMMARY_MS=1000).') -eq 1 ]"
check "412 from 23 first, then 430 from 24, and nothing more while the numbers stay" "summary 412 23 && summary 430 24 && [ \$(summaries) -eq 2 ] && [ \$(grep -n 'from 23 weaponcomponents' TweaksV.log | cut -d: -f1) -lt \$(grep -n 'from 24 weaponcomponents' TweaksV.log | cut -d: -f1) ]"
check "nothing written by TweaksV, no problem" "has '$PLACED' && quiet"

echo "== W5 DRY_RUN=1 alike, the list full: 470 definitions from 25 files"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe wcfull wait)
check "every pattern, site and load as without DRY_RUN, the announcement" "checked"
check "the summary, word for word: the list full" "summary 470 25 ' - it is full: one more definition crashes the game while it loads.' && [ \$(summaries) -eq 1 ]"
check "nothing written, no problem" "has '$PLACED' && quiet"

echo "== W6 the list overflowed: the 471st definition's pointer over the count, which reads its low half"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wcover wait)
check "every pattern, site and load as before, the announcement" "checked"
check "the summary, word for word: no count within the game's 470" "log_line '${TAG}the weapon component list.s count reads -1547607936, from 25 weaponcomponents.meta files - outside the game.s own list of 470: the list has overflowed, a definition past its end written over the count and the memory after it, and the game is likely to crash.\$'"
check "nothing written, no problem" "has '$PLACED' && quiet"

echo "== W7 TEST_BREAK_PATTERN=W-E-C1 (test build, ERROR_POPUP=1) for 12 seconds: logged, no problem, no box, no summary"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey ERROR_POPUP 1; addkey TEST_BREAK_PATTERN W-E-C1; out=$(ASI=TweaksV_test.asi game full_game.exe wait)
check "the scanner's line and the module's, word for word; the other three still looked for and found" "log_has 'TEST BUILD: pattern W-E-C1 is treated as not found (TEST_BREAK_PATTERN).' && log_line '${NOT_MEASURED}W-E-C1 was not found.\$' && [ \$(grep -c -E 'Pattern W-E-C[234] found: .* 1 match,' TweaksV.log) -eq 3 ]"
check "no site line, no announcement, no summary" "no_site_line && unannounced && log_lacks 'weapon component definitions'"
check "no problem, no start-up summary, no box; nothing written" "quiet && has '$PLACED'"

echo "== W8 TEST_BREAK_PATTERN=W-E-C4 (test build): the last of the four, logged by its label"
fresh_ini; setkey DIAGNOSTIC_MODE 1; addkey TEST_BREAK_PATTERN W-E-C4; out=$(ASI=TweaksV_test.asi game full_game.exe)
check "the module's line, word for word; the other three found, no site line" "log_line '${NOT_MEASURED}W-E-C4 was not found.\$' && [ \$(grep -c -E 'Pattern W-E-C[123] found: .* 1 match,' TweaksV.log) -eq 3 ] && no_site_line"
check "no problem, nothing written" "quiet && has '$PLACED'"

echo "== W9 the list another mod moved: its leas outside the game (test build, ERROR_POPUP=1)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey ERROR_POPUP 1; out=$(ASI=TweaksV_test.asi game full_game.exe wcmoved)
check "the reason, word for word, after the rebuild's lea" "log_line '${TAG}reference at $RVA -> 0x[0-9A-F]* (outside the game) (W-E-C1)\$' && log_line '${NOT_MEASURED}the weapon component list is not inside the game: 0x[0-9A-F]* (outside the game) (another mod may have moved it already).\$'"
check "no announcement; no problem, no box; nothing written" "unannounced && quiet && has '$PLACED'"

echo "== W10 one lea of the list leads elsewhere in the game: only 16 load it"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wcsplit)
check "16 loads logged, the reason word for word" "[ \$(loads) -eq 16 ] && log_line '${NOT_MEASURED}the game code loads the weapon component list in 16 places, not the expected 17 (another mod may have moved some of them, or the game has changed).\$'"
check "no announcement, no problem, nothing written" "unannounced && quiet && has '$PLACED'"

echo "== W11 an 18th lea loads the list (a changed game)"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wcextra)
check "18 loads logged, the reason word for word" "[ \$(loads) -eq 18 ] && log_line '${NOT_MEASURED}the game code loads the weapon component list in 18 or more places, not the expected 17 (another mod may have moved some of them, or the game has changed).\$'"
check "no announcement, no problem, nothing written" "unannounced && quiet && has '$PLACED'"

echo "== W12 one lea of the list with a 49 prefix, which the game's never have"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wcprefix)
check "the reason, word for word, with the lea's bytes" "log_line '${NOT_MEASURED}the load of the weapon component list at $RVA is not a \"lea r64, \[rip+disp32\]\" with a 48 or 4C prefix: 49 8D 05 [0-9A-F ]*\.\$'"
check "no announcement, no problem, nothing written" "unannounced && quiet && has '$PLACED'"

echo "== W13 the update's store of the count leads elsewhere: the count's references disagree"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wccount)
check "the reason, word for word, naming the update" "log_line '${NOT_MEASURED}the references to the weapon component list.s count do not all point to one count: $RVA and $RVA (W-E-C3).\$'"
check "no announcement, no problem, nothing written" "unannounced && quiet && has '$PLACED'"

echo "== W14 the count 4 bytes further: not a whole number of 8-byte entries after the list"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wcgap)
check "the reason, word for word" "log_line '${NOT_MEASURED}the weapon component list.s count at $RVA does not lie a whole number of 8-byte entries after the list at $RVA.\$'"
check "no announcement, no problem, nothing written" "unannounced && quiet && has '$PLACED'"

echo "== W15 the pool size call another mod hooked: it leads outside the game"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wchooked); echo "      $out"
check "the reason, word for word" "log_line '${NOT_MEASURED}the pool size call at $RVA is not a call into the game (another mod may have hooked it already) (W-E-C4).\$'"
check "no announcement, no problem; the call left where the other mod's hook sent it, nothing written" "unannounced && quiet && has 'wc: sites as placed, data as placed, size call elsewhere'"

echo "== W16 none of the game code present (plain fake game): each pattern logged missing, no problem of the module"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game plain_game.exe)
check "the harness survived" "grep -q 'harness: .* loaded' harness.out"
check "all four looked for and missing, the module's line names the first" "[ \$(grep -c 'Pattern W-E-C[1-4] not found: ' TweaksV.log) -eq 4 ] && log_line '${NOT_MEASURED}W-E-C1 was not found.\$' && log_lacks '\[WeaponLimits\] PROBLEM'"

echo
echo "WEAPON COMPONENTS SCENARIOS: $pass passed, $fail failed"
