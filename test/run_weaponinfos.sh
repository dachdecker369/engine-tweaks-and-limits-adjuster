#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The weapon infos (TweaksVWeaponInfos), a limit of the WeaponLimits part to come, measured first: the full fake game's
# initializer of the weapon info blob array, its allocation and its loader's head, the array in the fake's data empty
# while TweaksV starts and set up by the fake afterwards, as the game's initializer does. Without DIAGNOSTIC_MODE nothing
# is looked for; with it both patterns found once and every site's line at its place, the array still empty at the
# start, the measurement announced and its summary 10 seconds in with the fake's numbers at the game's sizes, and again
# as packs are mounted - the array full, then past its end - but not while nothing changes; an array set up already,
# and one with more room than the game's code gives; a pattern missing, each site not as expected and an address
# outside the game, each logged in its words with no problem, no box and no summary; the plain game; DRY_RUN alike; and
# nothing written - the sites and the array as the fake laid them, every page its own protection. Fake LOCALAPPDATA: the
# real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata

W='\[WeaponLimits\] DIAGNOSTIC:'
# the fake's sites and array as it laid them: its initializer's array, 152 files loaded of the game's 160
LAID='weapon infos: sites as laid, array 152 of 160 |'
# a summary line, not the announcement (. for an apostrophe, as in every pattern below)
SUMMARY='weapons.meta files loaded (the game.s own array holds'
# the summary of the fake's array at the game's sizes, word for word
GAME_SUMMARY="152 weapons.meta files loaded (the game's own array holds 160, this one 160)."

# site TEXT: the module's line "DIAGNOSTIC: TEXT", whole (a regex)
site() { log_line "$W $1\$"; }
# rva LABEL: the RVA of a pattern's one match, from the scanner's line; hex N: N as the log writes an RVA
rva() { tr -d '\r' < TweaksV.log | sed -nE "s/.*Pattern $1 found: .* -> RVA (0x[0-9A-F]+), 1 match,.*/\1/p" | head -n 1; }
hex() { printf '0x%X' "$1"; }
# summaries: the summary lines, without their time and tag
summaries() { tr -d '\r' < TweaksV.log | grep -- "$SUMMARY" | sed 's/.*\] DIAGNOSTIC: //'; }
# mine: the module's lines and its patterns' lines, without their time, sorted
mine() { tr -d '\r' < TweaksV.log | grep -E "W-E-I[12]|weapon info|weapons\.meta|-> RVA $arr \(scan\)" | sed -E 's/^[0-9:.]+ //' | sort; }
# not_measured REASON: the module's line for why, word for word, and nothing measured: no announcement, no summary
not_measured() { log_line "$W the weapon infos are not measured: $1\$" && log_lacks 'and its room are logged after' && log_lacks "$SUMMARY"; }
# no_box: no problem and no start-up summary box (the test build, with ERROR_POPUP=1, would log the one it keeps off the screen)
no_box() { log_lacks 'PROBLEM' && log_lacks 'message box not shown' && log_lacks 'Showing the start-up summary'; }
# failing: TweaksV.ini for a run of the test build with DIAGNOSTIC_MODE=1 and ERROR_POPUP=1; run_test OPTION...: that run
failing() { fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey ERROR_POPUP 1; }
run_test() { out=$(ASI=TweaksV_test.asi game full_game.exe "$@"); }

echo "== WI1 DIAGNOSTIC_MODE=0, as by default: nothing looked for, no line of the module, nothing written"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "neither pattern looked for, no line of the module" "log_lacks 'W-E-I1' && log_lacks 'W-E-I2' && log_lacks 'weapon info' && log_lacks 'weapons.meta'"
check "the sites and the array as laid - the fake's initializer set it up after TweaksV's start; every page its own protection; no problem" "has '$LAID' && has 'protect: all restored (RX)' && log_lacks 'PROBLEM'"

echo "== WI2 DIAGNOSTIC_MODE=1, 11 seconds: both patterns once, every site's line at its place, the array empty at the start, the summary 10 s in"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wait)
i1=$(rva W-E-I1); i2=$(rva W-E-I2)
arr=$(tr -d '\r' < TweaksV.log | sed -nE "s/.* reference at RVA $(hex $((i1 + 23))) -> RVA (0x[0-9A-F]+) \(W-E-I1\): the block.s store - the weapon info blob array$/\1/p")
alloc=$(tr -d '\r' < TweaksV.log | sed -nE "s/.* allocation call at RVA $(hex $((i1 + 15))) -> RVA (0x[0-9A-F]+) \(W-E-I1\): .*/\1/p")
check "W-E-I1 and W-E-I2 found, 1 match each; the array and the allocation where their lines say" "[ -n '$i1' ] && [ -n '$i2' ] && [ -n '$arr' ] && [ -n '$alloc' ]"
check "the number allocated at W-E-I1 + 11 and the capacity at + 34, 160 each" "site 'limit at RVA $(hex $((i1 + 11))) holds 160 (W-E-I1): the blobs the game allocates' && site 'limit at RVA $(hex $((i1 + 34))) holds 160 (W-E-I1): the array.s capacity'"
check "the call at + 15 to the allocation, its first 14 bytes and its element size 0xF8" "site 'allocation call at RVA $(hex $((i1 + 15))) -> RVA $alloc (W-E-I1): 56 57 48 83 EC 28 89 D6 48 63 FA 48 69 CF F8 00 00 00'"
check "the block's store at + 23 giving the array, the capacity's store at + 30 leading to the array + 0xA" "site 'reference at RVA $(hex $((i1 + 23))) -> RVA $arr (W-E-I1): the block.s store - the weapon info blob array' && site 'reference at RVA $(hex $((i1 + 30))) -> RVA $(hex $((arr + 0xA))) (W-E-I1): the capacity.s store'"
check "the loader's count read at W-E-I2 + 3 and written at + 13, both at the array + 8; its block read at + 27, at the array" "site 'reference at RVA $(hex $((i2 + 3))) -> RVA $(hex $((arr + 8))) (W-E-I2): the loader.s read of the count' && site 'reference at RVA $(hex $((i2 + 13))) -> RVA $(hex $((arr + 8))) (W-E-I2): the loader.s write of the count' && site 'reference at RVA $(hex $((i2 + 27))) -> RVA $arr (W-E-I2): the loader.s read of the block'"
check "the blob size at + 20, 0xF8; the loader's start, 0x18 before the match, a function start (no unwind data in the fake, a NOP before it)" "site 'blob size at RVA $(hex $((i2 + 20))) holds 0xF8 (W-E-I2)' && site 'the loader at RVA $(hex $((i2 - 0x18))) (W-E-I2 - 0x18): no unwind data, byte before it 90'"
check "the scan: exactly the 2 references to the array, the block's store one of them" "[ \$(count '$W reference at RVA 0x[0-9A-F]* -> RVA $arr (scan)') -eq 2 ] && site 'reference at RVA $(hex $((i1 + 23))) -> RVA $arr (scan)'"
check "the array still empty at TweaksV's start, the proof that its initializer runs later; the measurement announced" "site 'the weapon info blob array at RVA $arr is still empty at TweaksV.s start (no block, 0 loaded, capacity 0): its initializer runs after TweaksV.s DllMain, so the part to come can raise its limit in time.' && site 'the weapon info blob array at RVA $arr (W-E-I1), 160 blobs of 248 bytes: the weapons.meta files loaded and its room are logged after 10 seconds, then every minute while the numbers change.'"
check "the summary 10 s in, word for word, once: 152 files of the game's 160, the capacity read back" '[ "$(summaries)" = "$GAME_SUMMARY" ]'
check "nothing written: the sites and the array as laid, every page its own protection; no problem" "has '$LAID' && has 'protect: all restored (RX)' && log_lacks 'PROBLEM'"
mine > weaponinfos_diagnostic.txt

echo "== WI3 packs mounted while the game runs (test build, TWEAKSV_TEST_SUMMARY_MS=1000): the summary again each time the numbers change - the array full, then past its end - and not while they do not"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(TWEAKSV_TEST_SUMMARY_MS=1000 ASI=TweaksV_test.asi game full_game.exe wipacks)
PACKS="$GAME_SUMMARY
160 weapons.meta files loaded (the game's own array holds 160, this one 160) - it is full: the next file would be written past its end.
165 weapons.meta files loaded (the game's own array holds 160, this one 160) - past its end: the game's memory is already damaged."
check "the test build's switch said once for this measurement" "[ \$(count 'TEST BUILD: the weapon infos summary comes every 1000 ms after its first (TWEAKSV_TEST_SUMMARY_MS=1000).') -eq 1 ]"
check "three summaries, word for word, in order: 152 of 160, then 160 - full -, then 165 - past its end -, and no fourth while nothing changes" '[ "$(summaries)" = "$PACKS" ]'
check "nothing written by TweaksV; no problem" "has 'weapon infos: sites as laid, array 165 of 160 |' && log_lacks 'PROBLEM'"

echo "== WI4 DRY_RUN=1 with DIAGNOSTIC_MODE=1, 11 seconds: alike - the module writes nothing either way"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe wait)
check "every line of the module and of its two patterns as without DRY_RUN, the summary 10 s in among them" '[ -s weaponinfos_diagnostic.txt ] && [ "$(mine)" = "$(cat weaponinfos_diagnostic.txt)" ]'
check "nothing written; no problem" "has '$LAID' && has 'protect: all restored (RX)' && log_lacks 'PROBLEM'"
rm -f weaponinfos_diagnostic.txt

echo "== WI5 the array set up before TweaksV starts (an initializer that ran first): logged as such, not a problem, measured all the same"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wisetup wait)
check "the line, word for word" "site 'the weapon info blob array at RVA $arr is set up already at TweaksV.s start (a block, 152 loaded, capacity 160): its initializer ran before TweaksV.s DllMain, so the part to come could not raise its limit this session - measured all the same.'"
check "the announcement and the summary 10 s in all the same; nothing written; no problem" "site 'the weapon info blob array at RVA $arr (W-E-I1), 160 blobs of 248 bytes: the weapons.meta files loaded and its room are logged after 10 seconds, then every minute while the numbers change.' && [ \"\$(summaries)\" = \"\$GAME_SUMMARY\" ] && has '$LAID' && log_lacks 'PROBLEM'"

echo "== WI6 an array with room for 512, more than the game's code gives (another mod's raise after TweaksV's checks), 170 files loaded"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wiraised wait)
check "the summary, word for word: what the game's own array would not have held" "[ \"\$(summaries)\" = \"170 weapons.meta files loaded (the game's own array holds 160, this one 512) - the game's own array would have overflowed by 10 of them.\" ] && has 'weapon infos: sites as laid, array 170 of 512 |'"

echo "== WI7 TEST_BREAK_PATTERN=W-E-I1 (test build, ERROR_POPUP=1), 11 seconds: logged, no problem, no box, no summary; W-E-I2 still looked for"
failing; addkey TEST_BREAK_PATTERN W-E-I1; run_test wait
check "the scanner's test line and the module's line, word for word; W-E-I2 found all the same" "log_has 'TEST BUILD: pattern W-E-I1 is treated as not found (TEST_BREAK_PATTERN).' && not_measured 'W-E-I1 was not found.' && log_has 'Pattern W-E-I2 found: .* 1 match'"
check "no problem, no box, no summary 10 s in; nothing written" "no_box && has '$LAID' && has 'protect: all restored (RX)'"

echo "== WI8 TEST_BREAK_PATTERN=W-E-I2 (test build, ERROR_POPUP=1)"
failing; addkey TEST_BREAK_PATTERN W-E-I2; run_test
check "the module's line, word for word, W-E-I1 found; no problem, no box; nothing written" "not_measured 'W-E-I2 was not found.' && log_has 'Pattern W-E-I1 found: .* 1 match' && no_box && has '$LAID'"

echo "== WI9 the two limits disagree, 160 and 161 (another mod changed one)"
failing; run_test wimismatch
check "the line, word for word; no problem, no box; nothing written" "not_measured 'the two limits of the weapon info blob array disagree: 160 at RVA $(hex $((i1 + 11))) and 161 at RVA $(hex $((i1 + 34))) (W-E-I1; another mod may have changed one).' && no_box && has 'weapon infos: sites as laid, array 152 of 161 |'"

echo "== WI10 both limits 0"
failing; run_test wizero
check "the line, word for word; no problem, no box; nothing written" "not_measured 'the limits of the weapon info blob array hold 0 (W-E-I1).' && no_box && has 'weapon infos: sites as laid,'"

echo "== WI11 the call to the allocation leads outside the game (another mod's hook)"
failing; run_test wihooked
check "the call's line and the module's line, word for word; no problem, no box; nothing written" "site 'allocation call at RVA $(hex $((i1 + 15))) -> 0x[0-9A-F]* (outside the game) (W-E-I1)' && not_measured 'the allocation call at RVA $(hex $((i1 + 15))) is not a call into the game (another mod may have hooked it already) (W-E-I1).' && no_box && has '$LAID'"

echo "== WI12 the allocation begins with a jump, not with its own bytes"
failing; run_test wibytes
check "the call's line with the bytes found, the module's line, word for word; no problem, no box; nothing written" "site 'allocation call at RVA $(hex $((i1 + 15))) -> RVA $alloc (W-E-I1): E9 57 48 83 EC 28 89 D6 48 63 FA 48 69 CF F8 00 00 00' && not_measured 'the allocation at RVA $alloc does not begin with the bytes it is expected to (W-E-I1).' && no_box && has '$LAID'"

echo "== WI13 the block's store leads outside the game: an address outside the game"
failing; run_test wioutside
check "the store's line and the module's line, word for word; no problem, no box; nothing written" "site 'reference at RVA $(hex $((i1 + 23))) -> 0x[0-9A-F]* (outside the game) (W-E-I1): the block.s store - the weapon info blob array' && not_measured 'the weapon info blob array at 0x[0-9A-F]* (outside the game) (W-E-I1) is not inside the game.' && no_box && has '$LAID'"

echo "== WI14 the capacity's store leads to the array + 0xC, not to its capacity"
failing; run_test wicapacity
check "the line, word for word; no problem, no box; nothing written" "not_measured 'the capacity.s store at RVA $(hex $((i1 + 30))) leads to RVA $(hex $((arr + 0xC))), not to the array.s capacity at RVA $(hex $((arr + 0xA))) (W-E-I1).' && no_box && has '$LAID'"

echo "== WI15 the loader's read of the count leads to the array + 0x10, not to its count"
failing; run_test wicount
check "the line, word for word; no problem, no box; nothing written" "not_measured 'the loader.s read of the count at RVA $(hex $((i2 + 3))) leads to RVA $(hex $((arr + 0x10))), not to RVA $(hex $((arr + 8))) (W-E-I2).' && no_box && has '$LAID'"

echo "== WI16 the loader's blob size 0x100, not the allocation's element size 0xF8"
failing; run_test wisize
check "the size's line and the module's line, word for word; no problem, no box; nothing written" "site 'blob size at RVA $(hex $((i2 + 20))) holds 0x100 (W-E-I2)' && not_measured 'the loader.s blob size, 0x100 at RVA $(hex $((i2 + 20))), is not the allocation.s element size, 0xF8 (W-E-I2).' && no_box && has '$LAID'"

echo "== WI17 the loader's start is no function start (48 before it, no unwind data)"
failing; run_test winotstart
check "the line, word for word; no problem, no box; nothing written" "not_measured 'the loader at RVA $(hex $((i2 - 0x18))) (W-E-I2 - 0x18) is not a function start (no unwind data, byte before it 48).' && no_box && has '$LAID'"

echo "== WI18 a third reference to the array, a second store of a block (another mod, or a changed game)"
failing; run_test wiextra
check "the three references logged, the module's line word for word; no problem, no box; nothing written" "[ \$(count '$W reference at RVA 0x[0-9A-F]* -> RVA $arr (scan)') -eq 3 ] && not_measured 'the game code holds 3 or more references to the weapon info blob array, not the expected 2 (another mod may have changed one, or the game has changed).' && no_box && has '$LAID'"

echo "== WI19 the plain game, no game code, DIAGNOSTIC_MODE=1: both patterns missing, logged, no problem of the module"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game plain_game.exe)
check "both looked for, the module's line, word for word; no problem of its own" "log_has 'Pattern W-E-I1 not found' && log_has 'Pattern W-E-I2 not found' && not_measured 'W-E-I1 was not found.' && log_lacks '\[WeaponLimits\] PROBLEM'"

echo
echo "WEAPON INFOS SCENARIOS: $pass passed, $fail failed"
