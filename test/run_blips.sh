#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The Blips part: blip_controller's guard in the fake game's script program, raised each time the fake places the
# program - as the game does at the start and at every save load - through the script programs' hook; after each
# placement the guard is printed from its count's NATIVE through its branch, and the rest of the code compared with
# what the fake placed. The guard as the game has it, NOP-padded, fused with its body behind the branch, and with
# too little room; the threshold the configured limit already, and another mod's; the guard twice, a jump, a call or
# a SWITCH case into its span, a CATCH in the program, a byte that is no instruction, a native the program does not
# have, a page that cannot be read, the guard changed while the natives are resolved - each left as placed, a
# problem the first time, after the start-up summary in a box of its own; another program and
# blip_controller's hash under another name left alone; the old plugin, ENABLED=0, DRY_RUN, DIAGNOSTIC_MODE with the
# natives' line and the blip pool's summary, a broken pattern, the call another mod redirected; the Release and the
# test build alike, and the test build's copy of the program next to the part. Fake LOCALAPPDATA: the real one is
# never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata

# The guard from its count's NATIVE through its branch: as the game has it - the count (native 1 in the fake),
# PUSH_CONST_U8 150, ILT, NOT, JZ to the exit - and raised to 1900: PUSH_CONST_S16 1900 and IGE in the place of
# the push, ILT and NOT, the count and the JZ as they were
GAME='2C 01 00 01 25 96 0C 06 56 0A 00'
RAISED='2C 01 00 01 43 6C 07 0B 56 0A 00'
# guard_after BYTES [N]: the fake placed blip_controller N times (default 1), its guard holds BYTES after the last
# placement, and the rest of every placement's code is as placed
guard_after() { has "blips: ${2:-1} placed, guard $1, the rest as placed ${2:-1} of ${2:-1}"; }
# raised_line N LIMIT: the part's line for placement N, word for word
raised_line() { log_line "\[Blips\] blip_controller.s guard at 0x3FE of its code: the threshold 150 -> $2 (placement $1).\$"; }
# refused WHY: the part's problem for placement 1, word for word up to WHY's end
refused() { log_has "PROBLEM: blip_controller.s threshold is not raised (placement 1): $1"; }

echo "== L1 defaults: the guard raised where the game placed it, PUSH_CONST_S16 1900 and IGE; the JZ and the rest as placed"
fresh_ini; out=$(game full_game.exe script); echo "      $out"
check "the header: Blips on, its line holds ACTIVE_BLIP_LIMIT=1900" "log_has 'Features: .*, Blips on' && log_line '\[TweaksV\] Blips: ACTIVE_BLIP_LIMIT=1900\$'"
check "the hook made by the part, and its line" "log_line '\[Blips\] The natives. resolution call at RVA 0x[0-9A-F]* is redirected: every script program the game places passes TweaksV, before and after the game resolves its natives.\$' && log_line '\[Blips\] Each time the game places blip_controller, its guard.s threshold is set to 1900 (ACTIVE_BLIP_LIMIT; the game.s own: 150).\$'"
check "placed through TweaksV's stub; 43 6C 07 0B in the place of 25 96 0C 06, the count and the JZ as they were, the rest of the code as placed" "has 'script: 1 placed on thread' && has 'through a TweaksV stub' && guard_after '$RAISED'"
check "the part's line, word for word; no problem" "raised_line 1 1900 && log_lacks 'PROBLEM'"

echo "== L2 a second placement, as a save load makes one: raised again"
fresh_ini; out=$(game full_game.exe scripttwice)
check "both placements raised, each with its line; the rest of each as placed" "guard_after '$RAISED' 2 && raised_line 1 1900 && raised_line 2 1900 && log_lacks 'PROBLEM'"

echo "== L3 ACTIVE_BLIP_LIMIT=200: the compact form, NOP and PUSH_CONST_U8"
fresh_ini; setkey ACTIVE_BLIP_LIMIT 200; out=$(game full_game.exe script)
check "00 25 C8 0B, the line with 200" "guard_after '2C 01 00 01 00 25 C8 0B 56 0A 00' && raised_line 1 200"

echo "== L4 ACTIVE_BLIP_LIMIT=5000, past its range: 1997 used, reported"
fresh_ini; setkey ACTIVE_BLIP_LIMIT 5000; out=$(game full_game.exe script)
check "reported, 1997 used: 43 CD 07 0B" "log_has 'PROBLEM: TweaksV.ini: \[BLIPS\] ACTIVE_BLIP_LIMIT=5000 is outside 150..1997 - using 1997.' && guard_after '2C 01 00 01 43 CD 07 0B 56 0A 00' && raised_line 1 1997"

echo "== L5 ACTIVE_BLIP_LIMIT=150, the game's own threshold: left as it is, one line"
fresh_ini; setkey ACTIVE_BLIP_LIMIT 150; out=$(game full_game.exe script)
check "nothing written, the line word for word, no problem" "guard_after '$GAME' && log_line '\[Blips\] blip_controller.s guard at 0x3FE of its code holds the threshold 150 already (ACTIVE_BLIP_LIMIT=150) - nothing to patch (placement 1).\$' && log_lacks 'PROBLEM'"

echo "== L6 a threshold of 240, not the game's own: left as it is and logged, not a problem"
fresh_ini; out=$(game full_game.exe script blip240)
check "nothing written, the line word for word, no problem" "guard_after '2C 01 00 01 25 F0 0C 06 56 0A 00' && log_line '\[Blips\] blip_controller.s threshold is not raised (placement 1): its guard at 0x3FE of its code holds 240, not the game.s own 150 - left as it is.\$' && log_lacks 'PROBLEM'"

echo "== L7 the fused ILT_JZ, its body behind the branch (the polarity the other way round): ILT, and a plain JZ with the same offset"
fresh_ini; out=$(game full_game.exe script blipfused)
check "43 6C 07 0C 56 in the place of 00 00 25 96 5B, the offset 03 00 kept; the rest as placed" "guard_after '2C 01 00 01 43 6C 07 0C 56 03 00' && raised_line 1 1900"

echo "== L8 two NOPs after the count: the push right-aligned against the branch, NOPs before it"
fresh_ini; out=$(game full_game.exe script blippadded)
check "00 00 43 6C 07 0B in the place of 00 00 25 96 0C 06; the rest as placed" "guard_after '2C 01 00 01 00 00 43 6C 07 0B 56 0A 00' && raised_line 1 1900"

echo "== L9 IGE and JZ, no NOT: three bytes before the branch - too few for PUSH_CONST_S16, enough for PUSH_CONST_U8"
fresh_ini; out=$(game full_game.exe script bliproom)
check "1900: left as placed, the problem word for word" "guard_after '2C 01 00 01 25 96 0B 56 0A 00' && refused 'its guard at 0x3FE of its code has 3 bytes before its branch, and the threshold 1900 needs 4.'"
fresh_ini; setkey ACTIVE_BLIP_LIMIT 200; out=$(game full_game.exe script bliproom)
check "200: 25 C8 0B, the room exactly" "guard_after '2C 01 00 01 25 C8 0B 56 0A 00' && raised_line 1 200 && log_lacks 'PROBLEM'"

echo "== L10 the guard twice: left as placed"
fresh_ini; out=$(game full_game.exe script bliptwice)
check "nothing written; the problem names both, word for word" "guard_after '$GAME' && refused 'its guard was found twice in its code, at 0x3FE and 0x800 - which one is meant cannot be told.'"

echo "== L11 a jump from elsewhere into the span: left as placed"
fresh_ini; out=$(game full_game.exe script blipjump)
check "nothing written; the problem names the jump, word for word" "guard_after '$GAME' && refused 'a jump at 0x100 leads into its guard at 0x3FE of its code, between 0x402 and 0x408.'"

echo "== L12 a byte that is no instruction: the code cannot be decoded, left as placed"
fresh_ini; out=$(game full_game.exe script blipbadbyte)
check "nothing written; the problem names the byte, word for word" "guard_after '$GAME' && refused 'its code cannot be decoded: the byte 0xA0 at 0x200 is no instruction.'"

echo "== L13 WAIT called through native 3, not below the program's native count: no guard"
fresh_ini; out=$(game full_game.exe script blipnative)
check "nothing written; the problem says why, word for word" "guard_after '$GAME' && refused 'its guard was not found in its code - the count and a comparison with a threshold are at 0x3FE, but its WAIT calls native 3, which is not below its native count, 3.'"

echo "== L14 a page of its code that cannot be read: left as placed"
fresh_ini; out=$(game full_game.exe scriptpage)
check "nothing written; the problem names the page" "guard_after '$GAME' && refused 'page 1 of its code, at 0x[0-9A-F]* (outside the game), cannot be read.'"

echo "== L15 the same failure at each placement: reported once, then counted in the log"
fresh_ini; out=$(game full_game.exe scripttwice blipbadbyte)
check "one problem, shown after the start-up summary; the second placement a line with the count" "[ \$(count 'PROBLEM: blip_controller') -eq 1 ] && log_line '\[Blips\] blip_controller.s threshold is not raised (placement 2): its code cannot be decoded: the byte 0xA0 at 0x200 is no instruction - 2 times this session, reported once.\$' && log_has 'Problem after the start-up summary not shown (ERROR_POPUP=0): 1 problem(s), see above.' && guard_after '$GAME' 2"

echo "== L16 a problem after the start-up summary, test build, ERROR_POPUP=1: a box of its own"
fresh_ini; setkey ERROR_POPUP 1; out=$(ASI=TweaksV_test.asi game full_game.exe script bliptwice)
check "the box of a part that stopped working after the start-up" "log_has 'Showing a problem after the start-up summary (1 problem(s)).' && log_has 'TEST BUILD: message box not shown (TWEAKSV_TEST_NO_BOXES=1): TweaksV: a part stopped working after the start-up:'"

echo "== L17 another program placed first: left alone; blip_controller raised as the game's 2nd program"
fresh_ini; out=$(game full_game.exe scriptother)
check "other_script as placed, no line of the part about it; blip_controller raised" "has 'other programs as placed 1 of 1' && guard_after '$RAISED' && raised_line 1 1900 && log_lacks 'other_script'"

echo "== L18 blip_controller's hash under another name, placed twice: left alone, logged once"
fresh_ini; out=$(game full_game.exe scripthash)
check "both as placed; the line once, word for word; no problem" "has 'blips: 0 placed' && has 'other programs as placed 2 of 2' && [ \$(count '\[Blips\] A script program has the hash of blip_controller, 0x80CFE974, but is named blip_imposter - left as it is (logged once).') -eq 1 ] && log_lacks 'PROBLEM'"

echo "== L19 BlipLimitFix.asi in the game folder: the part off, the old plugin's line"
fresh_ini; touch BlipLimitFix.asi; out=$(game full_game.exe script); rm -f BlipLimitFix.asi
check "reported as the old plugin, the part off in the header, nothing of it" "log_has 'PROBLEM: Old plugin found: .*BlipLimitFix.asi. TweaksV.s Blips part is switched off for this session so nothing is patched twice. Delete the old file - TweaksV replaces it.' && log_has 'Features: .*, Blips off' && log_lacks '\[Blips\]' && has 'through the game function' && guard_after '$GAME'"

echo "== L20 [BLIPS] ENABLED=0, DIAGNOSTIC_MODE=1: nothing, not even its patterns"
fresh_ini; setkey ENABLED 0 BLIPS; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe script)
check "the part off in the header; no line of it, no pattern of its own or of the hook looked for; nothing written" "log_has 'Features: .*, Blips off' && log_lacks '\[Blips\]' && log_lacks 'Y-E-H1' && log_lacks 'L-E-P1' && has 'through the game function' && guard_after '$GAME' && log_lacks 'PROBLEM'"

echo "== L21 DRY_RUN=1: the call found and checked, its would line; nothing redirected or written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe script)
check "the would line and the part's line, word for word; nothing written; no problem" "log_has '\[Blips\] DRY_RUN: would redirect the natives. resolution call at RVA 0x[0-9A-F]*.' && log_line '\[Blips\] DRY_RUN: the natives. resolution call is not redirected, so blip_controller is not looked at - its guard is found and checked on a boot without DRY_RUN.\$' && has 'through the game function' && guard_after '$GAME' && log_lacks 'PROBLEM'"

echo "== L22 DIAGNOSTIC_MODE=1: the hook's sites, the natives' line, the write, and the blip pool summed up 10 s in"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe script wait)
check "the hook's three sites under [Blips], with what each holds, the strings' length offset among them" "log_has '\[Blips\] DIAGNOSTIC: natives. resolution call at RVA 0x[0-9A-F]* -> RVA 0x[0-9A-F]*, strings. length offset at RVA 0x[0-9A-F]* holds 0x70 (Y-E-H1)' && log_has '\[Blips\] DIAGNOSTIC: code length offset at RVA 0x[0-9A-F]* holds 0x1C, native table offset at RVA 0x[0-9A-F]* holds 0x40 (Y-E-N1)' && log_has '\[Blips\] DIAGNOSTIC: code page table offset at RVA 0x[0-9A-F]* holds 0x10 (Y-E-I1)'"
check "the natives' line: the count native index 1 and WAIT index 2, their hashes before the resolution and their handlers after it" "log_line '\[Blips\] DIAGNOSTIC: blip_controller.s count native index 1, hash 0x1122334455667788, handler RVA 0x[0-9A-F]*; WAIT index 2, hash 0x99AABBCCDDEEFF00, handler RVA 0x[0-9A-F]*.\$'"
check "the write with its old and new bytes" "log_has '\[Blips\] DIAGNOSTIC: threshold in blip_controller.s guard at 0x3FE of its code (placement 1) at 0x[0-9A-F]* (outside the game): 25 96 0C 06 -> 43 6C 07 0B'"
check "the pool's two patterns found once each, the pool and its mark inside the game, the measurement announced" "log_has 'Pattern L-E-P1 found: .* 1 match' && log_has 'Pattern L-E-P2 found: .* 1 match' && log_line '\[Blips\] DIAGNOSTIC: the blip pool at RVA 0x[0-9A-F]* (L-E-P1), 2000 slots, and its high-water mark at RVA 0x[0-9A-F]* (L-E-P2): the slots in use are logged after 10 seconds, then every minute while the numbers change.\$'"
check "the summary 10 s in, word for word: 162 slots in use below the mark at 170 - the two in use beyond it not counted - against the limit" "log_line '\[Blips\] DIAGNOSTIC: blips: 162 of the pool.s 2000 slots in use, the high-water mark at 170; blip_controller makes blips up to 1900 (ACTIVE_BLIP_LIMIT).\$'"
check "the guard raised as without it; no problem" "guard_after '$RAISED' && log_lacks 'PROBLEM'"

echo "== L23 TEST_BREAK_PATTERN=Y-E-H1 (test build): the part's problem; nothing hooked or written"
fresh_ini; addkey TEST_BREAK_PATTERN Y-E-H1; out=$(ASI=TweaksV_test.asi game full_game.exe script)
check "the problem, word for word; the call not redirected; the guard as placed" "log_has 'PROBLEM: blip_controller.s threshold is not raised this session: Y-E-H1 was not found.' && has 'through the game function' && guard_after '$GAME'"

echo "== L24 TEST_BREAK_PATTERN=L-E-P1 with DIAGNOSTIC_MODE=1: the measurement off, logged, not a problem; the guard still raised"
fresh_ini; setkey DIAGNOSTIC_MODE 1; addkey TEST_BREAK_PATTERN L-E-P1; out=$(ASI=TweaksV_test.asi game full_game.exe script)
check "the line, word for word; no problem; the guard raised" "log_line '\[Blips\] DIAGNOSTIC: the blip pool is not measured: L-E-P1 was not found.\$' && log_lacks 'PROBLEM' && guard_after '$RAISED'"

echo "== L25 the call another mod redirected already: left alone, the part's problem"
fresh_ini; out=$(game full_game.exe scripthooked)
check "the problem, word for word; nothing placed through it" "log_has 'PROBLEM: blip_controller.s threshold is not raised this session: the natives. resolution call at RVA 0x[0-9A-F]* is not a call into the game (another mod may have hooked it already).' && has 'script: none placed, call to another mod'"

echo "== L26 the test build: as the Release build"
fresh_ini; out=$(ASI=TweaksV_test.asi game full_game.exe scripttwice)
check "both placements raised, each with its line, no problem" "log_has 'TEST BUILD - not for release' && guard_after '$RAISED' 2 && raised_line 1 1900 && raised_line 2 1900 && log_lacks 'PROBLEM'"

echo "== L27 the test build with TEST_DUMP_SCRIPT: the switch's copy holds the program as placed, the program the raised guard"
d=appdata/TweaksV
fresh_ini; addkey TEST_DUMP_SCRIPT blip_controller; rm -rf $d; rm -f script_placed_*.bin; out=$(ASI=TweaksV_test.asi game full_game.exe script)
check "after its header the copy is byte for byte what the fake placed - 25 96 0C 06 at 0x402 of the code; the program holds 43 6C 07 0B there" "tail -c +177 $d/Script-blip_controller-1.bin | cmp -s - script_placed_1.bin && [ \"\$(od -An -tx1 -j\$((304 + 0x402)) -N4 $d/Script-blip_controller-1.bin | tr -d ' ')\" = 25960c06 ] && guard_after '$RAISED'"
check "one hook, made by the switch, which joins first: its line under [TweaksV]; the part raises the guard after it" "log_has '\[TweaksV\] The natives. resolution call at RVA 0x[0-9A-F]* is redirected' && log_lacks '\[Blips\] The natives. resolution call' && raised_line 1 1900 && has 'stubs: 7, all RX' && log_lacks 'PROBLEM'"
rm -rf $d; rm -f script_placed_*.bin

echo "== L28 a call from elsewhere into the span: left as placed"
fresh_ini; out=$(game full_game.exe script blipcall)
check "nothing written; the problem names the call, word for word" "guard_after '$GAME' && refused 'a call at 0x100 leads into its guard at 0x3FE of its code, between 0x402 and 0x408.'"

echo "== L29 a SWITCH case from elsewhere into the span, its offset unsigned: left as placed"
fresh_ini; out=$(game full_game.exe script blipswitch)
check "nothing written; the problem names the SWITCH, word for word" "guard_after '$GAME' && refused 'a SWITCH case at 0x100 leads into its guard at 0x3FE of its code, between 0x402 and 0x408.'"

echo "== L30 a CATCH in the program, its target computed while the script runs: left as placed"
fresh_ini; out=$(game full_game.exe script blipcatch)
check "nothing written; the problem names it, word for word" "guard_after '$GAME' && refused 'its code holds a CATCH, THROW or CALLINDIRECT at 0x200, whose target is computed while the script runs and could lead into its guard at 0x3FE of its code.'"

echo "== L31 the threshold changed while the game resolves the natives, as another mod could: nothing written over it"
fresh_ini; out=$(game full_game.exe script blipchanged)
check "the change as it was made, nothing of the part's; the problem word for word" "guard_after '2C 01 00 01 25 97 0C 06 56 0A 00' && refused 'its guard at 0x3FE of its code changed between the game.s placement and the write (another mod may be at work) - left as it is.'"

echo
echo "BLIPS SCENARIOS: $pass passed, $fail failed"
