#!/usr/bin/env bash
# The default TweaksV.ini: a first start without a TweaksV.ini creates exactly the default text
# (expected_default.ini) and uses its defaults; a deleted key comes back with its default.
# Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" TweaksV.asi
mkdir -p appdata

pass=0; fail=0
check() { if eval "$2"; then echo "  PASS $1"; pass=$((pass+1)); else echo "  FAIL $1"; fail=$((fail+1)); fi; }
game() { cp full_game.exe GTA5_Enhanced.exe; rm -f harness.out; LOCALAPPDATA="$AW" timeout 60 ./GTA5_Enhanced.exe TweaksV.asi > harness.out 2>&1; echo "  (harness exit $?: $(tr -d '\r' < harness.out | cut -c1-160))"; }

echo "== first start without TweaksV.ini"
rm -f TweaksV.ini TweaksV.log; rm -rf appdata/TweaksV newmods
game
sed 's/$/\r/' "$T/expected_default.ini" > expected.ini
check "TweaksV.ini is created" '[ -f TweaksV.ini ]'
check "it is exactly the new default text (CRLF)" 'cmp -s TweaksV.ini expected.ini'
check "the log says it was created" 'grep -q "Created TweaksV.ini with the default settings" TweaksV.log'
check "log header: HEAP_SIZE=2000 ... PACKFILEARRAY_SIZE=13000" 'grep -q "HEAP_SIZE=2000.*PACKFILEARRAY_SIZE=13000" TweaksV.log'
check "log header: NEWMODS=0 and CONSOLE=0" 'grep -q "NEWMODS=0" TweaksV.log && grep -q "CONSOLE=0" TweaksV.log'
check "the heap is set to 2000 MB" 'grep -q "heap: 2000 MB" harness.out'
check "the packfile limit is 13000" 'grep -q "I=13000" harness.out'
check "newmods is not mounted" 'grep -q "NEWMODS=0 - the newmods folder is not mounted" TweaksV.log'
check "no message box was needed (harness finished)" 'grep -q "heap:" harness.out'

echo "== a deleted key comes back with its new default"
for k in HEAP_SIZE PACKFILEARRAY_SIZE NEWMODS CONSOLE; do sed -b -i "/^$k=/d" TweaksV.ini; done
game
check "HEAP_SIZE=2000 added back" 'grep -q "^HEAP_SIZE=2000" TweaksV.ini'
check "PACKFILEARRAY_SIZE=13000 added back" 'grep -q "^PACKFILEARRAY_SIZE=13000" TweaksV.ini'
check "NEWMODS=0 added back" 'grep -q "^NEWMODS=0" TweaksV.ini'
check "CONSOLE=0 added back" 'grep -q "^CONSOLE=0" TweaksV.ini'
check "the log names all 4 re-added keys" '[ "$(grep -c "added the missing key" TweaksV.log)" = 4 ]'

echo "INI DEFAULT CHECKS: $pass passed, $fail failed"
