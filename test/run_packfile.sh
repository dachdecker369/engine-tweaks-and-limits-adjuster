#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# PackfileLimit scenarios (TweaksVPackfileLimit): a fake GTA5_Enhanced.exe with copies of the patched game
# code loads the real TweaksV.asi. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
mkdir -p appdata

MOVED='pack refs: 32 new 0 game 0 other, R31a new, R31b game (RW, 512 KB)'
UNMOVED='pack refs: 0 new 32 game 0 other, R31a game, R31b game |'
GAME_SIZES='pack sizes: D3=4500 D2=4500 D1=4500 I=4496 |'
NO_HOOK='hook: not set'

echo "== P1 defaults (game 1.0.1158.16): array moved, limits 12000, configuration count replaced"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "all 32 common references + the 1158.13+ one -> one new 512 KB read/write array; the older build's one untouched" "has '$MOVED'"
check "direct size (1158.13+ site) 12000 and the indirect size 11996 - the game's margin of 4 kept; the older builds' sites untouched" "has 'pack sizes: D3=12000 D2=4500 D1=4500 I=11996 |'"
check "hooked call replaces the configured 3000 with 12000 and calls the game's function with it" "has 'hook: config 3000 -> 12000, original called 1x with 12000 |'"
check "the hook's line" "log_has 'The archive count of the game.s configuration, 3000, is replaced by 12000.'"
check "status lines" "log_has 'The packfile array moved and its limit raised from 4500 to 12000 entries.' && log_has 'The streaming configuration call at RVA .* is redirected: the archive count the game.s configuration holds is replaced by 12000 when the game reads it.'"
check "the other plugins still applied" "has 'heap: 1024 MB' && has 'splash: E9 86 01 00 00 90 |' && has 'modkit refs: new array (RW, 128 KB)'"
check "every patched page back to its own protection (read+execute, as the game's code)" "has 'protect: all restored (RX)'"
check "the redirected call leads to a call stub that is read+execute, never writable" "has ', all RX' && ! has 'stubs: 0,'"
check "no problems" "log_lacks 'PROBLEM'"

echo "== P2 PACKFILEARRAY_SIZE=20000"
fresh_ini; setkey PACKFILEARRAY_SIZE 20000 PACKFILE_LIMIT; out=$(game full_game.exe); echo "      $out"
check "limits and configuration count 20000, array still full size" "has 'D3=20000 D2=4500 D1=4500 I=19996 |' && has 'config 3000 -> 20000,' && has '$MOVED'"

echo "== P3 PACKFILEARRAY_SIZE=1000 is raised to the game's own 4500"
fresh_ini; setkey PACKFILEARRAY_SIZE 1000 PACKFILE_LIMIT; out=$(game full_game.exe)
check "limits stay 4500, configuration count 4500" "has 'D3=4500 D2=4500 D1=4500 I=4496 |' && has 'config 3000 -> 4500,'"
check "clamp logged" "log_has 'PACKFILEARRAY_SIZE=1000 is outside 4500..65535 - using 4500'"

echo "== P4 PACKFILEARRAY_SIZE=70000 is clamped to 65535"
fresh_ini; setkey PACKFILEARRAY_SIZE 70000 PACKFILE_LIMIT; out=$(game full_game.exe)
check "limits 65535" "has 'D3=65535 D2=4500 D1=4500 I=65531 |' && has 'config 3000 -> 65535,'"
check "clamp reported for the start-up summary" "log_has 'PROBLEM: TweaksV.ini: \[PACKFILE_LIMIT\] PACKFILEARRAY_SIZE=70000 is outside 1..65535 - using 65535.'"

echo "== P5 PACKFILEARRAY_SIZE=abc falls back to the default 13000"
fresh_ini; setkey PACKFILEARRAY_SIZE abc PACKFILE_LIMIT; out=$(game full_game.exe)
check "limits 13000" "has 'D3=13000 D2=4500 D1=4500 I=12996 |'"
check "invalid value reported for the start-up summary" "log_has 'PROBLEM: TweaksV.ini: \[PACKFILE_LIMIT\] PACKFILEARRAY_SIZE=abc is not a number - using 13000.'"

echo "== P6 DRY_RUN=1: everything found and checked, nothing written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe); echo "      $out"
check "references, sizes and call untouched" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "the 'would' line" "log_has 'DRY_RUN: would move the packfile array (33 references) and raise its limit from 4500 to 12000 (2 sites) and redirect the streaming configuration call at RVA'"

echo "== P7 DIAGNOSTIC_MODE=1: every pattern, reference and site logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "the 36 packfile patterns of this build each found exactly once" "[ \$(grep -c -E 'Pattern P-E-[A-Za-z0-9]+ found: .* 1 match,' TweaksV.log) -eq 36 ]"
check "33 references logged, and the scan finds the same 33 and no other" "[ \$(count 'PackfileLimit\] DIAGNOSTIC: reference at RVA .* (P-E-R') -eq 33 ] && [ \$(count 'PackfileLimit\] DIAGNOSTIC: reference at RVA .* (scan)') -eq 33 ]"
check "direct size, indirect variable, array and call logged" "log_has 'PackfileLimit\] DIAGNOSTIC: limit at RVA .* holds 4500 (N, P-E-D3) -> game size 4500' && log_has 'PackfileLimit\] DIAGNOSTIC: limit variable at RVA .* holds 4496 (N-4, P-E-I1) -> game size 4500' && log_has 'DIAGNOSTIC: the packfile array is at RVA .* - 4500 entries of 8 bytes' && log_has 'PackfileLimit\] DIAGNOSTIC: streaming configuration call at RVA .* -> RVA'"
check "the 36 writes logged" "[ \$(count 'PackfileLimit\] DIAGNOSTIC: packfile array reference at RVA') -eq 33 ] && [ \$(count 'PackfileLimit\] DIAGNOSTIC: packfile array limit at RVA') -eq 2 ] && [ \$(count 'PackfileLimit\] DIAGNOSTIC: streaming configuration call at RVA .*: E8 .* -> E8') -eq 1 ]"
check "still applied" "has 'D3=12000 D2=4500 D1=4500 I=11996 |'"
check "patterns found once get no per-match lines (only D-E-L12, which matches 7 times, gets them)" "[ \$(count ' match 1 of ') -eq 1 ] && log_has 'D-E-L12 match 1 of 7 at RVA'"

echo "== P7b DIAGNOSTIC_MODE=1, a second identical copy of the 1158.13+ reference: each match logged with its bytes, nothing written"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe twor31a)
check "its line counts 2 matches" "log_has 'Pattern P-E-R31a found: BF 08 00 00 00 BE 10 00 00 00 4C 8D 05 -> RVA 0x[0-9A-F]*, 2 matches, bytes: BF '"
check "one line per match, naming the pattern: 16 bytes before it, | at its start, 48 bytes from it" "[ \$(tr -d '\r' < TweaksV.log | grep -c -E '\[Scanner\]   P-E-R31a match [12] of 2 at RVA 0x[0-9A-F]+:( [0-9A-F]{2}){16} \| BF( [0-9A-F]{2}){47}\$') -eq 2 ]"
check "both copies' bytes shown" "[ \$(count '| BF 08 00 00 00 BE 10 00 00 00 4C 8D 05 ') -eq 2 ]"
check "no other pattern gets per-match lines (but D-E-L12, which matches 7 times)" "[ \$(count ' match 1 of ') -eq 2 ] && log_has 'D-E-L12 match 1 of 7 at RVA'"
check "a reference pattern that matches twice is not used: nothing written, reported" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK' && log_has 'the game code holds P-E-R31a 2 or more times, not the expected 1.' && log_has 'PROBLEM: The packfile limit is not changed'"

echo "== P7c the game's obfuscated code that the old 1158.13+ pattern also matched (RVA 0x582A227): not matched now"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe junkr31a)
check "found exactly once, no per-match lines of its own" "log_has 'Pattern P-E-R31a found: BF 08 00 00 00 BE 10 00 00 00 4C 8D 05 -> RVA 0x[0-9A-F]*, 1 match,' && log_lacks 'P-E-R31a match 1 of '"
check "the array moved, no problem" "has '$MOVED' && log_lacks 'PROBLEM'"

echo "== P8 [PACKFILE_LIMIT] ENABLED=0"
fresh_ini; setkey ENABLED 0 PACKFILE_LIMIT; out=$(game full_game.exe)
check "nothing changed" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "header: PackfileLimit off; no PackfileLimit lines" "log_has 'Features: .*, PackfileLimit off' && log_lacks '\[PackfileLimit\]'"

echo "== P9 an old PackfileLimitAdjusterEnhanced.asi in the folder: skipped"
fresh_ini; touch PackfileLimitAdjusterEnhanced.asi; out=$(game full_game.exe); rm -f PackfileLimitAdjusterEnhanced.asi
check "nothing changed" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "old plugin reported" "log_has 'Old plugin found: .*PackfileLimitAdjusterEnhanced.asi'"

echo "== P10 another mod already moved the array: nothing written"
fresh_ini; out=$(game full_game.exe packmoved); echo "      $out"
check "references untouched (one elsewhere), sizes and call untouched" "has 'pack refs: 0 new 31 game 1 other' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "reason logged and reported" "log_has 'do not all point to the game.s own array: RVA .* and 0x.* (outside the game)' && log_has 'PROBLEM: The packfile limit is not changed'"
check "every page back to its own protection" "has 'protect: all restored'"

echo "== P11 another mod already hooked the streaming configuration call: nothing written"
fresh_ini; out=$(game full_game.exe packhooked); echo "      $out"
check "references and sizes untouched, the other mod's call kept" "has '$UNMOVED' && has '$GAME_SIZES' && has \"hook: other mod's\""
check "reason logged and reported" "log_has 'is not a call into the game (another mod may have hooked it already)' && log_has 'PROBLEM: The packfile limit is not changed'"

echo "== P12 implausible direct size (70000): nothing written"
fresh_ini; out=$(game full_game.exe packbadsize); echo "      $out"
check "references untouched, direct sizes as they were, indirect and call untouched" "has '$UNMOVED' && has 'pack sizes: D3=70000 D2=70000 D1=70000 I=4496 |' && has '$NO_HOOK'"
check "reason logged" "log_has 'packfile list size, 70000, is not plausible'"

echo "== P12b the indirect size variable holds the array's size itself, not 4 less (a changed margin): nothing written"
fresh_ini; out=$(game full_game.exe packindirect); echo "      $out"
check "references, sizes and call untouched" "has '$UNMOVED' && has 'pack sizes: D3=4500 D2=4500 D1=4500 I=4500 |' && has '$NO_HOOK'"
check "reason logged and reported" "log_has 'the limits of the packfile array disagree about the game.s own size: 4500 and 4504 (P-E-I1)' && log_has 'PROBLEM: The packfile limit is not changed'"

echo "== P13 no version resource: the build cannot be told, nothing done"
fresh_ini; out=$(game full_game_nover.exe); echo "      $out"
check "nothing changed" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "the header names no version; reason logged and reported" "log_has 'file version unknown,' && log_has 'file version could not be read' && log_has 'PROBLEM: The packfile limit is not changed'"

echo "== P14 game 1.0.1158.13 (first build with the new patterns)"
fresh_ini; out=$(game full_game_1158_13.exe); echo "      $out"
check "the header names 1.0.1158.13; its site and reference used" "log_has 'file version 1.0.1158.13,' && has '$MOVED' && has 'D3=12000 D2=4500 D1=4500 I=11996 |' && has 'config 3000 -> 12000,'"

echo "== P15 game 1.0.1013.17: the Mansions-era direct size and the older build's reference"
fresh_ini; out=$(game full_game_1013.exe); echo "      $out"
check "older build's reference moved, the 1158.13+ one untouched" "has 'pack refs: 32 new 0 game 0 other, R31a game, R31b new (RW, 512 KB)'"
check "the header names 1.0.1013.17; the 1013.17+ direct size used" "log_has 'file version 1.0.1013.17,' && has 'pack sizes: D3=4500 D2=12000 D1=4500 I=11996 |' && has 'config 3000 -> 12000,'"
check "no problems" "log_lacks 'PROBLEM'"

echo "== P16 game 1.0.889.22: the oldest direct size"
fresh_ini; out=$(game full_game_889.exe); echo "      $out"
check "older build's reference moved" "has 'R31a game, R31b new (RW, 512 KB)'"
check "the header names 1.0.889.22; the oldest direct size used" "log_has 'file version 1.0.889.22,' && has 'pack sizes: D3=4500 D2=4500 D1=12000 I=11996 |' && has 'config 3000 -> 12000,'"

echo "== P17 none of the game code present (plain fake game): logged and skipped, no crash"
fresh_ini; out=$(game plain_game.exe)
check "harness survived" "grep -q 'loaded' harness.out"
check "reported" "log_has 'PROBLEM: The packfile limit is not changed this session: P-E-R01 was not found.'"

echo "== P18 a 34th reference to the array that no pattern names (a changed game): nothing written"
fresh_ini; out=$(game full_game.exe packextra); echo "      $out"
check "references, sizes and call untouched" "has '$UNMOVED' && has '$GAME_SIZES' && has '$NO_HOOK'"
check "reason logged and reported" "log_has 'the game code holds 34 or more references to the packfile array, not the expected 33 (another mod may have moved some of them, or the game has changed).' && log_has 'PROBLEM: The packfile limit is not changed'"

echo
echo "PACKFILE SCENARIOS: $pass passed, $fail failed"
