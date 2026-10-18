#!/usr/bin/env bash
# M2 (StraightToStoryMode) scenarios: a fake GTA5_Enhanced.exe with copies of the patched
# game code loads the real TweaksV.asi. Fake LOCALAPPDATA: the real one is never touched.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$B" || exit 1
cp "$R/x64/Release/TweaksV.asi" .
mkdir -p appdata

pass=0; fail=0
check() { if eval "$2"; then echo "PASS  $1"; pass=$((pass+1)); else echo "FAIL  $1"; fail=$((fail+1)); fi; }
setkey() { sed -b -i "0,/^$1=.*\r\$/s//$1=$2\r/" TweaksV.ini; }   # first occurrence of KEY
setstorykey() { sed -b -i "/^\[STRAIGHTTOSTORYMODE_SETTINGS\]/,/^\[/s/^$1=.*\r\$/$1=$2\r/" TweaksV.ini; }
fresh_ini() { sed 's/$/\r/' "$T/base.ini" > TweaksV.ini; setkey SHOW_ERROR_POPUPS 0; }  # no message boxes in tests
game() { cp "$1" GTA5_Enhanced.exe; shift; rm -f harness.out; LOCALAPPDATA="$AW" ./GTA5_Enhanced.exe TweaksV.asi "$@" > harness.out 2>&1; tr -d '\r' < harness.out; }
log_has() { grep -q -- "$1" TweaksV.log; }
count() { grep -c -- "$1" TweaksV.log; }
ORIG_SPLASH='splash: 0F 84 85 01 00 00'; NEW_SPLASH='splash: E9 86 01 00 00 90'
ORIG_LEGAL='legal: 0F 84 07 05 00 00';   NEW_LEGAL='legal: 90 90 90 90 90 90'
ORIG_LANDING='landing: 48 89 5C 24 08';  NEW_LANDING='landing: 31 C0 C3 24 08'

echo "== S1 defaults: all three screens skipped, heap adjusted"
fresh_ini; out=$(game full_game.exe); echo "      $out"
check "splash: jz +0x185 -> jmp +0x186 / nop (computed, same as upstream's bytes)" "[[ \"\$out\" == *\"$NEW_SPLASH\"* ]]"
check "legal: 6 NOPs" "[[ \"\$out\" == *\"$NEW_LEGAL\"* ]]"
check "landing page: xor eax,eax / ret" "[[ \"\$out\" == *\"$NEW_LANDING\"* ]]"
check "heap 1024 MB" "[[ \"\$out\" == *'heap: 1024 MB'* ]]"
check "'Patch successful!' logged" "log_has '\[StraightToStoryMode\] Patch successful!'"
check "no problems" "! log_has 'PROBLEM'"

echo "== S2 the three switches off: nothing changed"
fresh_ini; setstorykey SKIP_SPLASH_SCREEN 0; setstorykey SKIP_LEGAL_SCREEN 0; setstorykey SKIP_LANDING_PAGE 0
out=$(game full_game.exe); echo "      $out"
check "all three sites unchanged" "[[ \"\$out\" == *\"$ORIG_SPLASH\"* && \"\$out\" == *\"$ORIG_LEGAL\"* && \"\$out\" == *\"$ORIG_LANDING\"* ]]"
check "each switch logged" "log_has 'SKIP_SPLASH_SCREEN=0' && log_has 'SKIP_LEGAL_SCREEN=0' && log_has 'SKIP_LANDING_PAGE=0'"

echo "== S3 only SKIP_LEGAL_SCREEN=0"
fresh_ini; setstorykey SKIP_LEGAL_SCREEN 0; out=$(game full_game.exe); echo "      $out"
check "splash and landing patched, legal left alone" "[[ \"\$out\" == *\"$NEW_SPLASH\"* && \"\$out\" == *\"$ORIG_LEGAL\"* && \"\$out\" == *\"$NEW_LANDING\"* ]]"

echo "== S4 [STRAIGHTTOSTORYMODE_SETTINGS] ENABLED=0"
fresh_ini; setstorykey ENABLED 0; out=$(game full_game.exe); echo "      $out"
check "nothing changed" "[[ \"\$out\" == *\"$ORIG_SPLASH\"* && \"\$out\" == *\"$ORIG_LEGAL\"* && \"\$out\" == *\"$ORIG_LANDING\"* ]]"
check "no StraightToStoryMode lines" "! log_has '\[StraightToStoryMode\]'"

echo "== S5 DRY_RUN=1: found and checked, nothing written"
fresh_ini; setkey DRY_RUN 1; out=$(game full_game.exe); echo "      $out"
check "nothing changed" "[[ \"\$out\" == *\"$ORIG_SPLASH\"* && \"\$out\" == *\"$ORIG_LEGAL\"* && \"\$out\" == *\"$ORIG_LANDING\"* ]]"
check "three 'would patch' lines with old -> new bytes" "[ \$(count 'DRY_RUN: would patch the') -eq 3 ] && log_has 'would patch the splash screen: 0F 84 85 01 00 00 -> E9 86 01 00 00 90'"
check "no 'Patch successful!'" "! log_has 'Patch successful!'"

echo "== S6 DIAGNOSTIC_MODE=1: every pattern and write logged"
fresh_ini; setkey DIAGNOSTIC_MODE 1; out=$(game full_game.exe)
check "three story patterns found once each" "log_has 'Pattern found: 0F 85 A9 00 00 00 48 8D 54 -> RVA 0x.*, 1 match' && log_has 'Pattern found: 00 E9 ? 05 00 00 E8 ? 07 -> RVA 0x.*, 1 match' && log_has 'Pattern found: E8 ? ? ? ? 8B 0D ? ? ? ? 83 C1 ? 84 C0 -> RVA 0x.*, 1 match'"
check "three writes logged with old -> new bytes" "log_has 'DIAGNOSTIC: splash screen at RVA 0x.*: 0F 84 85 01 00 00 -> E9 86 01 00 00 90' && log_has 'DIAGNOSTIC: legal screen at RVA' && log_has 'DIAGNOSTIC: landing page at RVA 0x.*: 48 89 5C -> 31 C0 C3'"
check "patterns found once get no per-match lines (T2)" "! log_has ' match 1 of '"

echo "== S7 already patched (an old StraightToStoryMode ran first under another name): skipped quietly"
fresh_ini; out=$(game full_game.exe prepatched); echo "      $out"
check "bytes stay patched" "[[ \"\$out\" == *\"$NEW_SPLASH\"* && \"\$out\" == *\"$NEW_LEGAL\"* && \"\$out\" == *'landing: 31 C0 C3'* ]]"
check "three 'already patched - skipped' lines, no problems" "[ \$(count 'already patched - skipped') -eq 3 ] && ! log_has 'PROBLEM'"

echo "== S8 unexpected bytes at the splash site: refused, the others still applied"
fresh_ini; out=$(game full_game.exe badsplash); echo "      $out"
check "splash left alone" "[[ \"\$out\" == *'splash: 48 89 85 01 00 00'* ]]"
check "refusal logged with the bytes found" "log_has 'The splash screen cannot be skipped: the game code found is not the expected instruction (48 89 85 01 00 00)'"
check "legal and landing still patched" "[[ \"\$out\" == *\"$NEW_LEGAL\"* && \"\$out\" == *\"$NEW_LANDING\"* ]]"

echo "== S9 unexpected bytes at the legal site: refused with the bytes logged"
fresh_ini; out=$(game full_game.exe badlegal); echo "      $out"
check "legal left alone" "[[ \"\$out\" == *'legal: 48 89 07 05 00 00'* ]]"
check "refusal logged with the bytes found" "log_has 'The legal screen cannot be skipped: the game code found is not the expected instruction (48 89 07 05 00 00)'"

echo "== S9b a jnz instead of the game's jz at the legal site: refused (T3), the others still applied"
fresh_ini; out=$(game full_game.exe jnzlegal); echo "      $out"
check "legal left alone" "[[ \"\$out\" == *'legal: 0F 85 07 05 00 00'* ]]"
check "refusal logged with the bytes found" "log_has 'The legal screen cannot be skipped: the game code found is not the expected instruction (0F 85 07 05 00 00)'"
check "splash and landing still patched, no 'Patch successful!'" "[[ \"\$out\" == *\"$NEW_SPLASH\"* && \"\$out\" == *\"$NEW_LANDING\"* ]] && ! log_has 'Patch successful!'"

echo "== S10 the landing page function is hooked by another mod: left alone"
fresh_ini; out=$(game full_game.exe hookedlanding); echo "      $out"
check "hook left alone" "[[ \"\$out\" == *'landing: E9 11 22 33 44'* ]]"
check "reported" "log_has 'another mod has already hooked'"

echo "== S11 an old StraightToStoryMode.asi in the folder: TweaksV's part is skipped"
fresh_ini; touch StraightToStoryMode.asi; out=$(game full_game.exe); echo "      $out"; rm -f StraightToStoryMode.asi
check "nothing changed" "[[ \"\$out\" == *\"$ORIG_SPLASH\"* && \"\$out\" == *\"$ORIG_LEGAL\"* && \"\$out\" == *\"$ORIG_LANDING\"* ]]"
check "old plugin reported" "log_has 'Old plugin found: .*StraightToStoryMode.asi'"

echo "== S12 none of the game code present (plain fake game): each screen reported, no crash"
fresh_ini; out=$(game plain_game.exe); echo "      $out"
check "harness survived" "grep -q 'loaded' harness.out"
check "three 'not found' problems" "log_has 'The splash screen cannot be skipped: its game code was not found' && log_has 'The legal screen cannot be skipped: its game code was not found' && log_has 'The landing page cannot be skipped: its game code was not found'"

echo
echo "STORY SCENARIOS: $pass passed, $fail failed"
