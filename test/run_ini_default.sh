#!/usr/bin/env bash
# The default TweaksV.ini: a first start without a TweaksV.ini creates exactly the default text
# (expected_default.ini) and uses its defaults; a deleted key comes back with its default.
# Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
mkdir -p appdata


echo "== first start without TweaksV.ini"
rm -f TweaksV.ini TweaksV.log; rm -rf appdata/TweaksV newmods
out=$(game full_game.exe); echo "      ${out:0:160}"
sed 's/$/\r/' "$T/expected_default.ini" > expected.ini
check "TweaksV.ini is created" '[ -f TweaksV.ini ]'
check "it is exactly the new default text (CRLF)" 'cmp -s TweaksV.ini expected.ini'
check "the log says it was created" 'grep -q "Created TweaksV.ini with the default settings" TweaksV.log'
check "log header: HEAP_SIZE=2000, PACKFILEARRAY_SIZE=13000 and INI_VERSION=1" 'grep -q "HeapAdjusterEnhanced: HEAP_SIZE=2000" TweaksV.log && grep -q "PackfileLimitAdjusterEnhanced: PACKFILEARRAY_SIZE=13000" TweaksV.log && grep -q "TweaksV: .*INI_VERSION=1" TweaksV.log'
check "log header: NEWMODS=0 and CONSOLE=0" 'grep -q "NEWMODS=0" TweaksV.log && grep -q "CONSOLE=0" TweaksV.log'
check "the heap is set to 2000 MB" 'grep -q "heap: 2000 MB" harness.out'
check "the packfile limit is 13000" 'grep -q "I=13000" harness.out'
check "newmods is not mounted" 'grep -q "NEWMODS=0 - the newmods folder is not mounted" TweaksV.log'
check "no message box was needed (harness finished)" 'grep -q "heap:" harness.out'

echo "== a deleted key comes back with its new default"
for k in HEAP_SIZE PACKFILEARRAY_SIZE NEWMODS CONSOLE; do sed -b -i "/^$k=/d" TweaksV.ini; done
out=$(game full_game.exe); echo "      ${out:0:160}"
check "HEAP_SIZE=2000 added back" 'grep -q "^HEAP_SIZE=2000" TweaksV.ini'
check "PACKFILEARRAY_SIZE=13000 added back" 'grep -q "^PACKFILEARRAY_SIZE=13000" TweaksV.ini'
check "NEWMODS=0 added back" 'grep -q "^NEWMODS=0" TweaksV.ini'
check "CONSOLE=0 added back" 'grep -q "^CONSOLE=0" TweaksV.ini'
check "the log names all 4 re-added keys" '[ "$(grep -c "added the missing key" TweaksV.log)" = 4 ]'


echo "== crafted values: each one is reported for the start-up summary, its default or nearest limit used"
cp expected.ini TweaksV.ini; setkey SHOW_ERROR_POPUPS 0
setkey HEAP_SIZE 5000; setkey CONSOLE maybe; setkey PACKFILEARRAY_SIZE -5; setkey INI_VERSION 2
setkey MODKITARRAY_SIZE "$(printf '9%.0s' $(seq 1 300))"	# longer than TweaksV reads
setkey SKIP_SPLASH_SCREEN '  TRUE  '; setkey UNLOCK_FILES 'Off'	# spaces and capitals are fine
out=$(game full_game.exe); echo "      ${out:0:160}"
check "HEAP_SIZE=5000: outside 1..2047, 2047 used" "grep -q 'PROBLEM: TweaksV.ini: \[HEAP_SETTINGS\] HEAP_SIZE=5000 is outside 1..2047 - using 2047.' TweaksV.log && grep -q 'heap: 2047 MB' harness.out"
check "MODKITARRAY_SIZE of 300 digits: not a number, the default used" "grep -q 'PROBLEM: TweaksV.ini: \[MODKIT_SETTINGS\] MODKITARRAY_SIZE=9* is not a number - using 65536.' TweaksV.log"
check "PACKFILEARRAY_SIZE=-5: outside, raised to the game's own size" "grep -q 'PACKFILEARRAY_SIZE=-5 is outside 1..65535 - using 1.' TweaksV.log && grep -q 'I=4500' harness.out"
check "CONSOLE=maybe: not on/off, off" "grep -q 'PROBLEM: TweaksV.ini: \[TWEAKSV\] CONSOLE=maybe is not a valid on/off value - using 0.' TweaksV.log"
check "INI_VERSION=2: a newer layout is reported" "grep -q 'PROBLEM: TweaksV.ini comes from a newer TweaksV (layout 2; this TweaksV knows layout 1)' TweaksV.log"
check "'  TRUE  ' and 'Off' are read, not reported" "grep -q 'SKIP_SPLASH_SCREEN=1' TweaksV.log && grep -q 'UNLOCK_FILES=0' TweaksV.log && ! grep -q 'PROBLEM: .*SKIP_SPLASH_SCREEN\|PROBLEM: .*UNLOCK_FILES' TweaksV.log"
check "all five in one start-up summary, nothing written back" "grep -q 'Start-up summary not shown (SHOW_ERROR_POPUPS=0): 5 problem' TweaksV.log && grep -q '^HEAP_SIZE=5000' TweaksV.ini"

echo "== a damaged TweaksV.ini (4 KB of random bytes) and an empty one: no crash, every key added, defaults used"
perl -e 'srand(42); print map { chr(int(rand(256))) } 1..4096' > TweaksV.ini
out=$(game full_game.exe); echo "      ${out:0:160}"
check "random bytes: the game runs on with the defaults" "grep -q 'heap: 2000 MB' harness.out && [ \$(grep -c 'added the missing key' TweaksV.log) -eq 30 ]"
: > TweaksV.ini
out=$(game full_game.exe); echo "      ${out:0:160}"
check "empty: every key added, the defaults used" "grep -q 'heap: 2000 MB' harness.out && [ \$(grep -c 'added the missing key' TweaksV.log) -eq 30 ]"

echo "INI DEFAULT CHECKS: $pass passed, $fail failed"
