#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The siren count, a limit of the SirenSettings part measured first (TweaksVSirenCount): with DIAGNOSTIC_MODE=1 the fake game's
# four sites found, checked and logged, the vehicle lights' call of the siren lights' renderer redirected, and the fake's
# vehicles drawn through it from two threads at once - the game's renderer first, with its arguments, its result and its last
# error, then each model counted once and summed up 10 seconds after the first, and again when the models change, and with
# nothing drawn no summary at all; models that cannot be read, one whose names disagree with its bone structure and one whose
# lookup faults; more models than the table holds; each pattern missing, the two calls another mod redirected, the renderer's
# call to another function, find-bone-by-name not a function start and without unwind data, the plain game, DRY_RUN and
# Windows' dynamic code policy - each logged, never a problem, nothing redirected; without DIAGNOSTIC_MODE nothing looked for.
# Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata

# What the fake reports: its stand-ins, how its vehicles' sirens were drawn - 100 passes of four models from each of two
# threads - and whether its sites and data are as it placed them
BUILT='siren count: stand-ins built |'
COUNTED='siren draws: 800 through a TweaksV stub, the renderer 800 times, args ok, result and last error kept 800 of 800, 256 lookups in turn |'
UNCOUNTED='siren draws: 800 through the renderer, the renderer 800 times, args ok, result and last error kept 800 of 800, 0 lookups in turn |'
PLACED='siren sites and data: as placed |'
# The module's lines, word for word (an apostrophe as any character)
sc_line() { log_line "\[SirenSettings\] $1\$"; }
NOT_MEASURED='DIAGNOSTIC: the siren count is not measured:'
SUMMARY='DIAGNOSTIC: 4 vehicle models with sirens seen in 800 passes of the siren lights. renderer - the most sirens a model.s bone structure names: 20 of 20 (model 0x79FBB0C5); 1 with siren bones past siren20, which the game never lights; 1 losing siren bones past bone 127, which a bone structure cannot name; 0 that could not be read.'
LINE_32='DIAGNOSTIC: vehicle model 0xA1B2C3D4, 100 bones - siren and glass bones by name: 32 and 32, up to siren32 and siren_glass32; named by its bone structure: 20 and 20; past siren20, never lit: 12 and 12.'
LINE_LOST='DIAGNOSTIC: vehicle model 0x0C0FFEE0, 150 bones - siren and glass bones by name: 10 and 10, up to siren10 and siren_glass10; named by its bone structure: 8 and 10; lost past bone 127, found by name but not named by its bone structure: 2 and 0 (the highest bone 129).'
SITES="\[SirenSettings\] DIAGNOSTIC: .* (I-E-B[1-4])"

echo "== C1 DIAGNOSTIC_MODE=0: nothing looked for, the call not redirected, the sirens drawn as the game draws them"
fresh_ini; out=$(game full_game.exe scdraw); echo "      $out"
check "no siren count pattern looked for, no SirenSettings line" "log_lacks 'I-E-B' && log_lacks '\[SirenSettings\]'"
check "the stand-ins built; 800 passes through the renderer itself, its arguments, result and last error as the game's, no lookup" "has '$BUILT' && has '$UNCOUNTED'"
check "sites and data as placed, no call stub of its own (the 7 are the other parts'), every page its own protection" "has '$PLACED' && has 'stubs: 7, all RX' && has 'protect: all restored (RX)'"

echo "== C2 DIAGNOSTIC_MODE=1: the four sites found and logged, the call redirected, the models counted and summed up 10 s in"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe scdraw wait); echo "      $out"
renderer=$(tr -d '\r' < TweaksV.log | sed -n 's/.*siren lights. renderer call at RVA 0x[0-9A-F]* -> RVA \(0x[0-9A-F]*\), .*/\1/p')
function=$(tr -d '\r' < TweaksV.log | sed -n 's/.*, in the function at RVA \(0x[0-9A-F]*\) (unwind data) (I-E-B2)$/\1/p')
check "the 4 patterns each found exactly once" "[ \$(grep -c -E 'Pattern I-E-B[1-4] found: .* 1 match,' TweaksV.log) -eq 4 ]"
check "the vehicle lights' two calls, word for word" "sc_line 'DIAGNOSTIC: siren lights. renderer call at RVA 0x[0-9A-F]* -> RVA 0x[0-9A-F]*, siren settings getter call at RVA 0x[0-9A-F]* -> RVA 0x[0-9A-F]* (I-E-B1)'"
check "the renderer's bone read, its two offsets, in the function the renderer's call leads to, by the fake's unwind data" "sc_line 'DIAGNOSTIC: bone structure offset at RVA 0x[0-9A-F]* holds 0xB0, siren1.s bone offset at RVA 0x[0-9A-F]* holds 0x99, in the function at RVA 0x[0-9A-F]* (unwind data) (I-E-B2)' && [ -n '$renderer' ] && [ '$renderer' = '$function' ]"
check "the glass bones' offset and find-bone-by-name, a function start by the unwind data, its bone count offset" "sc_line 'DIAGNOSTIC: siren_glass1.s bone offset at RVA 0x[0-9A-F]* holds 0xAD (I-E-B3)' && sc_line 'DIAGNOSTIC: find-bone-by-name at RVA 0x[0-9A-F]*, a function start (unwind data), bone count offset at RVA 0x[0-9A-F]* holds 0x5E (I-E-B4)'"
check "the one write, the redirected call with its old and new bytes" "[ \$(count 'DIAGNOSTIC: siren lights. renderer call at RVA 0x[0-9A-F]*: E8') -eq 1 ] && sc_line 'DIAGNOSTIC: siren lights. renderer call at RVA 0x[0-9A-F]*: E8\( [0-9A-F][0-9A-F]\)\{4\} -> E8\( [0-9A-F][0-9A-F]\)\{4\}'"
check "the announcement, word for word" "sc_line 'DIAGNOSTIC: the siren lights. renderer call at RVA 0x[0-9A-F]* is redirected (I-E-B1): the vehicle models with sirens on, each counted once with its siren bones looked up by name up to siren32, are logged 10 seconds after the first, then every minute while they change.'"
check "800 passes through TweaksV's stub: the renderer each time, with the game's arguments, its result and last error back; 64 lookups for each of the 4 models, each after its vehicle's renderer" "has '$BUILT' && has '$COUNTED'"
check "the summary 10 s in, word for word: 4 models from two threads at once, each counted once - the police car's 20 sirens the most, one past siren20, one losing bones past 127" "sc_line '$SUMMARY' && [ \$(count 'vehicle models with sirens seen') -eq 1 ]"
check "a line for the model made for 32 sirens and for the one losing bones past 127, word for word, and for no other" "sc_line '$LINE_32' && sc_line '$LINE_LOST' && [ \$(count 'DIAGNOSTIC: vehicle model 0x') -eq 2 ]"
check "the call stub read+execute, 12 in all (the other parts' 11 and this one), every page its own protection" "has 'stubs: 12, all RX' && has 'protect: all restored (RX)'"
check "the sites but the redirected call and the models' data as placed" "has '$PLACED'"
check "no problem, no box" "log_lacks 'PROBLEM' && log_lacks 'Start-up summary'"

echo "== C2b DIAGNOSTIC_MODE=1 and nothing drawn for 11 s: the call redirected, no summary - its 10 seconds run from the first model counted"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe wait)
check "the call redirected and announced, its stub read+execute; no model counted, no summary; no problem" "sc_line 'DIAGNOSTIC: the siren lights. renderer call at RVA 0x[0-9A-F]* is redirected (I-E-B1): the vehicle models with sirens on, each counted once with its siren bones looked up by name up to siren32, are logged 10 seconds after the first, then every minute while they change.' && has 'siren draws: none, the call leads to a TweaksV stub |' && has 'stubs: 12, all RX' && log_lacks 'vehicle models with sirens seen' && log_lacks 'PROBLEM'"

echo "== C3 the test build, its summaries every 2 s: again when the models change - two more models 11 s in, then 3 s more"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(TWEAKSV_TEST_SUMMARY_MS=2000 ASI=TweaksV_test.asi game full_game.exe scdraw scmore); echo "      $out"
check "the test build's line for this measurement" "log_line '\[TweaksV\] TEST BUILD: the siren count summary comes every 2000 ms after its first (TWEAKSV_TEST_SUMMARY_MS=2000).\$'"
check "the first summary, then a second with 6 models in 820 passes, 2 of them past siren20 - and no third, the models unchanged" "sc_line '$SUMMARY' && sc_line 'DIAGNOSTIC: 6 vehicle models with sirens seen in 820 passes of the siren lights. renderer - the most sirens a model.s bone structure names: 20 of 20 (model 0x79FBB0C5); 2 with siren bones past siren20, which the game never lights; 1 losing siren bones past bone 127, which a bone structure cannot name; 0 that could not be read.' && [ \$(count 'vehicle models with sirens seen') -eq 2 ]"
check "a line for the new model made for 32 sirens, none for the one with 16; the earlier two lines not written again" "sc_line 'DIAGNOSTIC: vehicle model 0xB2C3D4E5, 110 bones - siren and glass bones by name: 32 and 32, up to siren32 and siren_glass32; named by its bone structure: 20 and 20; past siren20, never lit: 12 and 12.' && [ \$(count 'vehicle model 0xA1B2C3D4,') -eq 1 ] && [ \$(count 'vehicle model 0x0C0FFEE0,') -eq 1 ] && [ \$(count 'DIAGNOSTIC: vehicle model 0x') -eq 3 ]"
check "820 passes through the stub with the renderer's arguments, result and last error; 64 lookups for each of the 6 models" "has 'siren draws: 820 through a TweaksV stub, the renderer 820 times, args ok, result and last error kept 820 of 820, 384 lookups in turn |' && log_lacks 'PROBLEM'"

echo "== C4 four more models: a bone structure that cannot be read, a vehicle drawn without a skeleton, names that disagree with the bone structure, a lookup that faults"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe scdraw scodd scfault wait); echo "      $out"
check "1410 passes, the one without a skeleton handed on as it was; no lookup for the two whose data cannot be read, one for the one whose lookup faults" "has 'siren draws: 1410 through a TweaksV stub, the renderer 1410 times, args ok, result and last error kept 1410 of 1410, 321 lookups in turn |'"
check "the summary: 8 models, 3 that could not be read" "sc_line 'DIAGNOSTIC: 8 vehicle models with sirens seen in 1410 passes of the siren lights. renderer - the most sirens a model.s bone structure names: 20 of 20 (model 0x79FBB0C5); 1 with siren bones past siren20, which the game never lights; 1 losing siren bones past bone 127, which a bone structure cannot name; 3 that could not be read.'"
check "the two whose data could not be read, each with what could not" "sc_line 'DIAGNOSTIC: vehicle model 0x0BAD0001 could not be read: its bone structure (the model info.s +0xB0).' && sc_line 'DIAGNOSTIC: vehicle model 0x0BAD0002 could not be read: its skeleton data (the renderer.s 7th argument, its first field).'"
check "the lookup that faults: the access violation caught, the model one that could not be read and looked at no more, the game going on" "sc_line 'DIAGNOSTIC: vehicle model 0x0BAD0003 could not be read: find-bone-by-name faulted on its skeleton data (an access violation).' && has '$PLACED'"
check "the one whose names disagree with its bone structure: siren2 below bone 128 not named, a glass bone not found by name, siren1 found at another bone" "sc_line 'DIAGNOSTIC: vehicle model 0x0DD0DD00, 50 bones - siren and glass bones by name: 4 and 0, up to siren4 and no siren_glass; named by its bone structure: 3 and 1; found by name below bone 128 but not named by its bone structure: 1 and 0; named by its bone structure but not found by name: 0 and 1; found by name at another bone than its bone structure names: 1 and 0.' && [ \$(count 'DIAGNOSTIC: vehicle model 0x') -eq 6 ]"
check "no problem, no box" "log_lacks 'PROBLEM' && log_lacks 'Start-up summary'"

echo "== C5 1,100 more models after the four, one pass each: more than the table of 1,024 holds"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe scdraw scfull wait); echo "      $out"
summary=$(tr -d '\r' < TweaksV.log | grep 'vehicle models with sirens seen')
seen=$(sed -n 's/.*DIAGNOSTIC: \([0-9]*\) vehicle models with sirens seen in 1900 passes .*/\1/p' <<< "$summary")
held=$(sed -n 's/.*; the table full, \([0-9]*\) passes of models it could not hold not counted\.$/\1/p' <<< "$summary")
check "1900 passes through the stub, the renderer's arguments, result and last error each time" "has 'siren draws: 1900 through a TweaksV stub, the renderer 1900 times, args ok, result and last error kept 1900 of 1900, '"
check "the summary: at most 1,024 models held, each of the 1,104 either held or its one pass counted as not held" "[ -n '$seen' ] && [ -n '$held' ] && [ '$seen' -le 1024 ] && [ \$(( $seen + $held )) -eq 1104 ]"
check "no problem" "log_lacks 'PROBLEM'"

echo "== C6 TEST_BREAK_PATTERN (test build): each of the four patterns missing - logged, never a problem, nothing redirected"
for label in I-E-B1 I-E-B2 I-E-B3 I-E-B4; do
	more=""; [ "$label" = I-E-B1 ] && more=wait	# once with 11 s more: no summary comes
	fresh_ini; setkey DIAGNOSTIC_MODE 1; addkey TEST_BREAK_PATTERN $label; out=$(ASI=TweaksV_test.asi game full_game.exe scdraw $more)
	check "$label: the scanner's test line and the module's, word for word; drawn through the renderer itself, nothing counted, no stub of its own; no problem, no box, no summary" "grep -q 'TEST BUILD: pattern $label is treated as not found' TweaksV.log && sc_line '$NOT_MEASURED $label was not found.' && has '$UNCOUNTED' && has 'stubs: 11, all RX' && log_lacks 'PROBLEM' && log_lacks 'Start-up summary' && log_lacks 'vehicle models with sirens seen'"
done

echo "== C7 the call of the renderer another mod redirected already: left alone, logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe scdraw schooked)
check "the line, word for word; the other mod's call kept and nothing drawn through it here; no stub of its own; no problem" "sc_line '$NOT_MEASURED the siren lights. renderer call at RVA 0x[0-9A-F]* is not a call into the game (another mod may have hooked it already) (I-E-B1).' && has 'siren draws: none, the call leads to another mod |' && has 'stubs: 11, all RX' && log_lacks 'PROBLEM'"

echo "== C8 the call of the getter another mod redirected already: logged, nothing redirected"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe scdraw scgetterhooked)
check "the line, word for word; the renderer's call not redirected; no problem" "sc_line '$NOT_MEASURED the siren settings getter call at RVA 0x[0-9A-F]* is not a call into the game (another mod may have hooked it already) (I-E-B1).' && has '$UNCOUNTED' && has 'stubs: 11, all RX' && log_lacks 'PROBLEM'"

echo "== C9 the renderer's call leads to another function of the game than the one its bone read lies in: logged, nothing redirected"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe scdraw scelsewhere)
check "the line, word for word, naming the function the unwind data gives; the call as it was; no problem" "sc_line '$NOT_MEASURED I-E-B2 at RVA 0x[0-9A-F]* does not lie in the siren lights. renderer at RVA 0x[0-9A-F]*: the game.s unwind data puts it in the function at RVA 0x[0-9A-F]*.' && has 'siren draws: none, the call leads to another function of the game |' && has 'stubs: 11, all RX' && log_lacks 'PROBLEM'"

echo "== C10 find-bone-by-name's pattern 16 bytes into its function: not a function start - logged, nothing redirected"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe scdraw scnotstart)
check "its site line and the module's line, word for word, by the unwind data; the call not redirected; no problem" "sc_line 'DIAGNOSTIC: find-bone-by-name at RVA 0x[0-9A-F]*, inside the function at RVA 0x[0-9A-F]* (unwind data), bone count offset at RVA 0x[0-9A-F]* holds 0x5E (I-E-B4)' && sc_line '$NOT_MEASURED find-bone-by-name at RVA 0x[0-9A-F]* is not a function start (inside the function at RVA 0x[0-9A-F]* (unwind data)) (I-E-B4).' && has '$UNCOUNTED' && log_lacks 'PROBLEM'"

echo "== C10b find-bone-by-name's pattern where no unwind data names a function, CC before it: not a start by the unwind data - logged, nothing redirected"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe scdraw scnounwind)
check "its site line and the module's line, word for word; the call not redirected; no problem" "sc_line 'DIAGNOSTIC: find-bone-by-name at RVA 0x[0-9A-F]*, no unwind data, bone count offset at RVA 0x[0-9A-F]* holds 0x5E (I-E-B4)' && sc_line '$NOT_MEASURED find-bone-by-name at RVA 0x[0-9A-F]* is not a function start (no unwind data) (I-E-B4).' && has '$UNCOUNTED' && has 'stubs: 11, all RX' && log_lacks 'PROBLEM'"

echo "== C11 none of the game code present (plain fake game): logged, not a problem"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game plain_game.exe)
check "the scanner's four misses and the module's line, word for word; none of it a problem" "[ \$(grep -c -E 'Pattern I-E-B[1-4] not found: ' TweaksV.log) -eq 4 ] && sc_line '$NOT_MEASURED I-E-B1 was not found.' && log_lacks '\[SirenSettings\] PROBLEM'"

echo "== C12 DRY_RUN=1 with DIAGNOSTIC_MODE=1: the sites found and checked, the call not redirected, nothing counted"
fresh_ini; setkey DIAGNOSTIC_MODE 1; setkey DRY_RUN 1; out=$(game full_game.exe scdraw)
check "the four patterns found once each, the four site lines" "[ \$(grep -c -E 'Pattern I-E-B[1-4] found: .* 1 match,' TweaksV.log) -eq 4 ] && [ \$(count '$SITES') -eq 4 ]"
check "the would line and the module's line, word for word; nothing written, drawn through the renderer itself; no stub; no problem" "sc_line 'DRY_RUN: would redirect the siren lights. renderer call at RVA 0x[0-9A-F]*.' && sc_line '$NOT_MEASURED DRY_RUN - the siren lights. renderer call is not redirected, so nothing is counted.' && log_lacks 'renderer call at RVA 0x[0-9A-F]*: E8' && has '$UNCOUNTED' && has '$PLACED' && has 'stubs: 0,' && log_lacks 'PROBLEM'"

echo "== C13 Windows' dynamic code policy: no call stub can be made - logged under the part, not its problem, nothing redirected"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe scdraw nodyncode)
check "the stubs' line and the module's line, word for word; the call as it was; no problem of the part's" "sc_line 'The call stubs could not be made read+execute (.*) - nothing is changed.' && sc_line '$NOT_MEASURED the siren lights. renderer call at RVA 0x[0-9A-F]* could not be redirected (details above).' && has '$UNCOUNTED' && log_lacks '\[SirenSettings\] PROBLEM'"

echo
echo "SIREN COUNT SCENARIOS: $pass passed, $fail failed"
