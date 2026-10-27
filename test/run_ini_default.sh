#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The default TweaksV.ini: a first start without a TweaksV.ini creates exactly the default text
# (expected_default.ini, a comment after each key's value) and uses its defaults; a deleted key comes
# back with its default and no comment; crafted values are reported and clamped; a comment after a
# value, // or ;, is not part of it. A file without VERSION under [TWEAKSV] has the layout of TweaksV 1.x
# (layout1.ini is the 1.6.0 default): it is renamed to TweaksV.old.ini and replaced by the default file,
# nothing of it read - also in %LOCALAPPDATA%\TweaksV with a read-only game folder; a 1.x file inside
# the read-only game folder cannot be renamed, which is reported. Fake LOCALAPPDATA: the real one is
# never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
# the 1.x scenarios run the test build: the fresh file has ERROR_POPUP=1 and the rename is a problem
# for the start-up summary, and only a test build keeps that message box off the screen
cp "$R/object/Test/Release/TweaksV.asi" TweaksV_test.asi
mkdir -p appdata
sed 's/$/\r/' "$T/expected_default.ini" > expected.ini
RENAMED='TweaksV.ini had the layout of TweaksV 1.x. It was renamed to TweaksV.old.ini and a fresh TweaksV.ini written with the default settings - set your values again.'

echo "== first start without TweaksV.ini"
rm -f TweaksV.ini TweaksV.old.ini TweaksV.log; rm -rf appdata/TweaksV
out=$(game full_game.exe); echo "      ${out:0:160}"
check "TweaksV.ini is created" '[ -f TweaksV.ini ]'
check "it is exactly the new default text (CRLF)" 'cmp -s TweaksV.ini expected.ini'
check "the log says it was created" 'grep -q "Created TweaksV.ini with the default settings" TweaksV.log'
check "log header: HEAP_SIZE=2000, PACKFILEARRAY_SIZE=13000, TRAVERSAL_LIST_SIZE=10000, DECALDEFS_SIZE=2048, the [TWEAKSV] line with VERSION=2" 'grep -q "HeapAdjuster: HEAP_SIZE=2000" TweaksV.log && grep -q "PackfileLimit: PACKFILEARRAY_SIZE=13000" TweaksV.log && grep -q "BoxStreamer: TRAVERSAL_LIST_SIZE=10000" TweaksV.log && grep -q "Decals: DECALDEFS_SIZE=2048" TweaksV.log && grep -q "TweaksV: LOG=1 DIAGNOSTIC_MODE=0 DRY_RUN=0 CONSOLE=0 ERROR_POPUP=1 CONFLICT_CHECK=1 VERSION=2" TweaksV.log'
check "log header: no newmods line, no testing switch of a part" 'log_lacks -i "newmods" && log_lacks "LOG_DUMP\|LOG_SEARCHES\|LOG_DECALDEFS\|LOG_CALLS\|LOG_FILE_OPENS\|LOG_INFO\|LOG_MODS"'
check "the heap is set to 2000 MB" 'grep -q "heap: 2000 MB" harness.out'
check "the packfile limit is 13000" 'grep -q "I=12996 |" harness.out'
check "the box streamer list capacity is 10000" 'grep -q "box caps: 10000 10000 10000 10000 |" harness.out'
check "the decal definitions limit is 2048" 'grep -q "decal limits: 2047 2048 0x28198, 23 agree" harness.out'
check "no message box was needed (harness finished), no problem" 'grep -q "heap:" harness.out && log_lacks "PROBLEM"'

echo "== a deleted key comes back with its default, without a comment"
for k in HEAP_SIZE PACKFILEARRAY_SIZE OPEN_RPF CONSOLE; do sed -b -i "/^$k=/d" TweaksV.ini; done
out=$(game full_game.exe); echo "      ${out:0:160}"
check "HEAP_SIZE=2000 added back" 'grep -q "^HEAP_SIZE=2000" TweaksV.ini'
check "PACKFILEARRAY_SIZE=13000 added back" 'grep -q "^PACKFILEARRAY_SIZE=13000" TweaksV.ini'
check "OPEN_RPF=1 added back" 'grep -q "^OPEN_RPF=1" TweaksV.ini'
check "CONSOLE=0 added back" 'grep -q "^CONSOLE=0" TweaksV.ini'
check "a key added back carries no comment: TweaksV adds none to an existing file" 'tr -d "\r" < TweaksV.ini | grep -qx "HEAP_SIZE=2000"'
check "the log names all 4 re-added keys" '[ "$(grep -c "added the missing key" TweaksV.log)" = 4 ]'

echo "== a file with VERSION alone: every other key is added back"
printf '[TWEAKSV]\r\nVERSION=2\r\n' > TweaksV.ini
out=$(game full_game.exe); echo "      ${out:0:160}"
check "27 keys added, the defaults used, nothing renamed" '[ $(grep -c "added the missing key" TweaksV.log) -eq 27 ] && grep -q "heap: 2000 MB" harness.out && log_lacks "TweaksV.old.ini" && [ ! -e TweaksV.old.ini ]'

echo "== crafted values: each one is reported for the start-up summary, its default or nearest limit used"
cp expected.ini TweaksV.ini; setkey ERROR_POPUP 0
setkey HEAP_SIZE 5000; setkey CONSOLE maybe; setkey PACKFILEARRAY_SIZE -5; setkey VERSION 3
setkey MODKITARRAY_SIZE "$(printf '0%.0s' $(seq 1 250))99999$(printf 'x%.0s' $(seq 1 45))"	# 300 characters, longer than TweaksV reads
setkey SKIP_SPLASH_SCREEN '  TRUE  '; setkey UNLOCK_FILES 'Off'	# spaces and capitals are fine
out=$(game full_game.exe); echo "      ${out:0:160}"
check "HEAP_SIZE=5000: outside 1..2047, 2047 used" "grep -q 'PROBLEM: TweaksV.ini: \[HEAP_ADJUSTER\] HEAP_SIZE=5000 is outside 1..2047 - using 2047.' TweaksV.log && grep -q 'heap: 2047 MB' harness.out"
check "MODKITARRAY_SIZE of 300 characters: only the first 255 are read, 250 zeros and 99999 - a number outside its limits, 65536 used (all 300 would be no number)" "grep -q 'PROBLEM: TweaksV.ini: \[MODKIT_LIMIT\] MODKITARRAY_SIZE=99999 is outside 1..65536 - using 65536.' TweaksV.log && log_lacks 'MODKITARRAY_SIZE=.* is not a number'"
check "PACKFILEARRAY_SIZE=-5: outside, 1 used, which is then raised to the game's own size - both reported" "grep -q 'PROBLEM: TweaksV.ini: \[PACKFILE_LIMIT\] PACKFILEARRAY_SIZE=-5 is outside 1..65535 - using 1.' TweaksV.log && grep -q 'PROBLEM: TweaksV.ini: PACKFILEARRAY_SIZE=1 is outside 4500..65535 - using 4500.' TweaksV.log && grep -q 'I=4496 |' harness.out"
check "CONSOLE=maybe: not on/off, off" "grep -q 'PROBLEM: TweaksV.ini: \[TWEAKSV\] CONSOLE=maybe is not a valid on/off value - using 0.' TweaksV.log"
check "VERSION=3: a newer layout is reported" "grep -q 'PROBLEM: TweaksV.ini comes from a newer TweaksV (layout 3; this TweaksV knows layout 2)' TweaksV.log"
check "'  TRUE  ' and 'Off' are read, not reported" "grep -q 'SKIP_SPLASH_SCREEN=1' TweaksV.log && grep -q 'UNLOCK_FILES=0' TweaksV.log && log_lacks 'PROBLEM: .*SKIP_SPLASH_SCREEN\|PROBLEM: .*UNLOCK_FILES'"
check "all six in one start-up summary, nothing written back" "grep -q 'Start-up summary not shown (ERROR_POPUP=0): 6 problem' TweaksV.log && grep -q '^HEAP_SIZE=5000' TweaksV.ini"

echo "== a comment after a value - // as the default file writes it, or ; - is not part of the value"
cp expected.ini TweaksV.ini; setkey ERROR_POPUP 0
commented() { sed -b -i "s|^$1=.*\r\$|$1=$2\r|" TweaksV.ini; }	# the whole line with its comment: setkey takes no /
commented HEAP_SIZE '1500   //my own heap'; commented SKIP_LEGAL_SCREEN '0 ; I want to see it'; commented DECALDEFS_SIZE 'abc //oops'
out=$(game full_game.exe); echo "      ${out:0:160}"
check "HEAP_SIZE=1500 with a // comment: 1500 used, nothing reported" "grep -q 'heap: 1500 MB' harness.out && log_lacks 'PROBLEM: .*HEAP_SIZE'"
check "SKIP_LEGAL_SCREEN=0 with a ; comment: read as off, nothing reported" "grep -q 'SKIP_LEGAL_SCREEN=0' TweaksV.log && log_lacks 'PROBLEM: .*SKIP_LEGAL_SCREEN'"
check "DECALDEFS_SIZE=abc with a comment: reported without the comment, the default used" "grep -q 'PROBLEM: TweaksV.ini: \[DECALS\] DECALDEFS_SIZE=abc is not a number - using 2048.' TweaksV.log"

echo "== a 1.x TweaksV.ini (the 1.6.0 default with HEAP_SIZE=1500): renamed, replaced, reported once, nothing of it read"
rm -f TweaksV.old.ini; sed 's/$/\r/' "$T/layout1.ini" > TweaksV.ini; setkey HEAP_SIZE 1500; cp TweaksV.ini old.before
out=$(ASI=TweaksV_test.asi game full_game.exe); echo "      ${out:0:160}"
check "the old file is TweaksV.old.ini now, byte for byte" 'cmp -s TweaksV.old.ini old.before'
check "TweaksV.ini is the fresh default text" 'cmp -s TweaksV.ini expected.ini'
check "reported once, for the start-up summary (a message box in a release build)" "[ \$(count \"$RENAMED\") -eq 1 ] && grep -q 'PROBLEM: TweaksV.ini had the layout' TweaksV.log && grep -q 'message box not shown (TWEAKSV_TEST_NO_BOXES=1): TweaksV could not apply everything' TweaksV.log"
check "nothing of the old file was read: the heap is 2000 MB, not 1500" 'grep -q "heap: 2000 MB" harness.out && log_lacks "HEAP_SIZE=1500"'
check "the log header says so" 'grep -q "TweaksV.ini: .*TweaksV.ini (the 1.x file was renamed to TweaksV.old.ini and this fresh file written)" TweaksV.log'

echo "== a second 1.x file while TweaksV.old.ini is there (HEAP_SIZE=1600): the old one is replaced"
sed 's/$/\r/' "$T/layout1.ini" > TweaksV.ini; setkey HEAP_SIZE 1600
out=$(ASI=TweaksV_test.asi game full_game.exe)
check "TweaksV.old.ini holds the second file, TweaksV.ini is fresh again" 'grep -q "^HEAP_SIZE=1600" TweaksV.old.ini && cmp -s TweaksV.ini expected.ini'
check "reported once" "[ \$(count \"$RENAMED\") -eq 1 ]"

echo "== any file without a VERSION line counts as 1.x: a layout-2 file that lost it, an empty file, 4 KB of random bytes"
cp expected.ini TweaksV.ini; sed -b -i '/^VERSION=/d' TweaksV.ini; setkey HEAP_SIZE 1700
out=$(ASI=TweaksV_test.asi game full_game.exe)
check "VERSION deleted: renamed and replaced, nothing of it read" 'grep -q "^HEAP_SIZE=1700" TweaksV.old.ini && cmp -s TweaksV.ini expected.ini && grep -q "heap: 2000 MB" harness.out'
: > TweaksV.ini
out=$(ASI=TweaksV_test.asi game full_game.exe)
check "empty: renamed and replaced, the defaults used" '[ -e TweaksV.old.ini ] && [ ! -s TweaksV.old.ini ] && cmp -s TweaksV.ini expected.ini && grep -q "heap: 2000 MB" harness.out'
perl -e 'srand(42); print map { chr(int(rand(256))) } 1..4096' > TweaksV.ini
out=$(ASI=TweaksV_test.asi game full_game.exe)
check "random bytes: renamed and replaced, no crash, the defaults used" '[ $(stat -c %s TweaksV.old.ini) -eq 4096 ] && cmp -s TweaksV.ini expected.ini && grep -q "heap: 2000 MB" harness.out'
rm -f TweaksV.old.ini old.before

echo "== TweaksV.old.ini is a hard link to another file: the 1.x file is not renamed over it, reported, the defaults used"
sed 's/$/\r/' "$T/layout1.ini" > TweaksV.ini; echo "not an old ini" > victim.txt; ln victim.txt TweaksV.old.ini
out=$(ASI=TweaksV_test.asi game full_game.exe)
check "the other file is unchanged, the 1.x file still in place" '[ "$(cat victim.txt)" = "not an old ini" ] && grep -q "^\[HEAP_SETTINGS\]" TweaksV.ini'
check "reported in its own words, the defaults used" 'grep -q "PROBLEM: TweaksV.ini (.*) has the layout of TweaksV 1.x and could not be renamed to TweaksV.old.ini (the TweaksV.old.ini already there is not replaced: it is a hard link" TweaksV.log && grep -q "heap: 2000 MB" harness.out'
rm -f TweaksV.old.ini victim.txt TweaksV.ini

# Read-only as under C:\Program Files: a deny entry for this user on the folder alone (not on the files
# in it) lets nothing be added to it, which also stops a rename inside it.
echo "== a read-only game folder: a 1.x file in %LOCALAPPDATA%\\TweaksV is renamed and replaced there"
rm -rf ro appdata/TweaksV; mkdir -p ro appdata/TweaksV; cp full_game.exe ro/GTA5_Enhanced.exe; cp TweaksV_test.asi ro/TweaksV.asi
sed 's/$/\r/' "$T/layout1.ini" > appdata/TweaksV/TweaksV.ini
deny_writes ro
ro_run() { run_fake -C ro GTA5_Enhanced.exe TweaksV.asi; out=$(tr -d '\r' < harness.out); }
fb=appdata/TweaksV
check "the folder is read-only (a check of the test itself)" "[ -n '$sid' ] && ! touch ro/probe.txt 2>/dev/null"
ro_run
check "renamed and replaced in %LOCALAPPDATA%\\TweaksV, reported once, nothing in the game folder" "grep -q '^\[HEAP_SETTINGS\]' $fb/TweaksV.old.ini && cmp -s $fb/TweaksV.ini expected.ini && [ \$(grep -c \"$RENAMED\" $fb/TweaksV.log) -eq 1 ] && [ ! -e ro/TweaksV.ini ] && [ ! -e ro/TweaksV.old.ini ]"
check "the game is patched with the defaults" "has 'heap: 2000 MB' && has 'splash: E9 86 01 00 00 90 |'"
allow_writes ro

echo "== a 1.x file inside the read-only game folder: cannot be renamed, reported in its own words, not read"
rm -rf appdata/TweaksV; sed 's/$/\r/' "$T/layout1.ini" > ro/TweaksV.ini; setkey HEAP_SIZE 1500 "" ro/TweaksV.ini
deny_writes ro
ro_run
check "the 1.x file is still there, unchanged; no fresh file anywhere" "grep -q '^HEAP_SIZE=1500' ro/TweaksV.ini && [ ! -e ro/TweaksV.old.ini ] && [ ! -e $fb/TweaksV.ini ]"
check "reported, the defaults used" "grep -q 'PROBLEM: TweaksV.ini (.*ro.TweaksV.ini) has the layout of TweaksV 1.x and could not be renamed to TweaksV.old.ini (Windows error 5, access denied). Delete or rename it yourself - TweaksV uses its built-in defaults until then.' $fb/TweaksV.log && has 'heap: 2000 MB'"
check "the log header says it is not used" "grep -q 'TweaksV.ini: .*ro.TweaksV.ini (not used: it has the layout of TweaksV 1.x and could not be renamed' $fb/TweaksV.log"
allow_writes ro
check "the folder is writable again (the test's clean-up)" "touch ro/probe.txt"
rm -rf ro appdata/TweaksV

echo "INI DEFAULT CHECKS: $pass passed, $fail failed"
