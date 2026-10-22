# SPDX-License-Identifier: GPL-3.0-only
# Paths and helpers shared by the test suites; every run_*.sh sources this file.
# T: this folder. R: the repository. B: where the fake game runs, its game folder (run_all.sh deletes it
# afterwards). AW: the fake LOCALAPPDATA inside B, as a Windows path - the real one is never touched.
# TW: the fake TEMP and TMP, object\TestTemp next to B, as a Windows path: outside the game folder, as the
# real %TEMP% is, which is never touched either (run_all.sh deletes this one too).
T="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
R="$(cd "$T/.." && pwd)"
B="${TWEAKSV_TEST_DIR:-$R/object/TestRun}"
mkdir -p "$B/appdata" "$(dirname "$B")/TestTemp" || exit 1
AW="$(cygpath -w "$B/appdata")"
TW="$(cygpath -w "$(dirname "$B")/TestTemp")"
rm -f "$B/harness.state"	# no run of this suite yet

# Test builds of TweaksV show no message box with this set, so no run waits on one (release builds
# ignore it).
export TWEAKSV_TEST_NO_BOXES=1

# check DESCRIPTION CONDITION: CONDITION is evaluated, the result printed and counted - unless the last run
# of a fake (run_fake) was stopped or printed no report: then every check after it fails, printed as
# TIMEOUT or NO REPORT, until the next run. skip DESCRIPTION: a check this machine cannot make, printed as
# SKIP and counted as passed, so that the number of checks is the same on every machine.
pass=0; fail=0
check() {
	local state=""
	[ -f "$B/harness.state" ] && state=$(< "$B/harness.state")
	case "$state" in
		timeout) echo "TIMEOUT  $1"; fail=$((fail+1)); return ;;
		noreport*) echo "NO REPORT  $1 (exit status ${state#noreport })"; fail=$((fail+1)); return ;;
	esac
	if eval "$2"; then echo "PASS  $1"; pass=$((pass+1)); else echo "FAIL  $1"; fail=$((fail+1)); fi
}
skip() { echo "SKIP  $1"; pass=$((pass+1)); }

# TweaksV.ini in B, with CRLF line endings, as TweaksV writes it.
# fresh_ini [FILE]: a copy of FILE from this folder (default $INI), without message boxes.
# setkey KEY VALUE [SECTION [FILE]]: the first KEY in the file, or the KEY in [SECTION] ("" for the first
# KEY), in FILE (default TweaksV.ini).
# addkey KEY VALUE: a key under [TWEAKSV] that the file does not have yet.
INI=base.ini
fresh_ini() { sed 's/$/\r/' "$T/${1:-$INI}" > TweaksV.ini; setkey ERROR_POPUP 0; }
setkey() {
	if [ -n "${3:-}" ]; then sed -b -i "/^\[$3\]/,/^\[/s/^$1=.*\r\$/$1=$2\r/" "${4:-TweaksV.ini}"
	else sed -b -i "0,/^$1=.*\r\$/s//$1=$2\r/" "${4:-TweaksV.ini}"; fi
}
addkey() { sed -b -i "/^\[TWEAKSV\]\r\$/a $1=$2\r" TweaksV.ini; }

# run_fake [-C DIR] EXE [ARG...]: runs EXE in DIR (default here), as the game runs, with the fake
# LOCALAPPDATA (or $FAKE_APPDATA), TEMP and TMP, for RUN_TIMEOUT seconds at most; what it prints goes to
# harness.out here. How the run ended goes to harness.state, which check reads: "timeout" when it was
# stopped (status 124), "noreport STATUS" when it printed no report (the full game's last field is
# "protect: ...", the plain game's "harness: ..."), else "ok STATUS".
RUN_TIMEOUT=60
run_fake() {
	local dir=. status
	if [ "$1" = -C ]; then dir=$2; shift 2; fi
	rm -f harness.out "$B/harness.state"
	(cd "$dir" && LOCALAPPDATA="${FAKE_APPDATA:-$AW}" TEMP="$TW" TMP="$TW" timeout "$RUN_TIMEOUT" "./$1" "${@:2}") > harness.out 2>&1
	status=$?
	if [ "$status" = 124 ]; then echo timeout
	elif ! grep -qE '(protect|harness): ' harness.out; then echo "noreport $status"
	else echo "ok $status"; fi > "$B/harness.state"
}

# game EXE [OPTION...]: EXE, one of the fake game programs, runs as GTA5_Enhanced.exe and loads
# TweaksV.asi (or $ASI), through run_fake; prints what it reported. The old TweaksV.log is deleted first,
# so a run in which TweaksV writes none leaves none.
game() {
	cp "$1" GTA5_Enhanced.exe; shift; rm -f TweaksV.log
	run_fake GTA5_Enhanced.exe "${ASI:-TweaksV.asi}" "$@"
	tr -d '\r' < harness.out
}

# The last run: has TEXT - its report contains TEXT ($out); log_has PATTERN - a line of TweaksV.log
# matches; log_lacks [-i] [-F] PATTERN [FILE] - TweaksV.log (or FILE) is there and no line matches, the
# options as grep's; log_line PATTERN - log_has without the CR, for patterns that end in $; count PATTERN -
# how many lines match.
has() { [[ "$out" == *"$1"* ]]; }
log_has() { grep -q -- "$1" TweaksV.log; }
log_lacks() {
	local options=()
	while [[ "${1:-}" == -[iF] ]]; do options+=("$1"); shift; done
	[ -f "${2:-TweaksV.log}" ] && ! grep -q "${options[@]}" -- "$1" "${2:-TweaksV.log}"
}
log_line() { tr -d '\r' < TweaksV.log | grep -q -- "$1"; }
count() { grep -c -- "$1" TweaksV.log; }

# deny_writes DIR: DIR becomes read-only for this user, as a folder under C:\Program Files is - a deny entry
# on the folder alone (not on the files in it) lets nothing be added to it, which also stops a rename inside
# it. allow_writes DIR takes this user's deny entries off DIR again. sid: this user's SID.
sid=""
deny_writes() {
	[ -n "$sid" ] || sid=$(MSYS_NO_PATHCONV=1 "$(cygpath -S)/whoami.exe" /user /fo csv /nh | tr -d '\r' | sed -n 's/.*,"\(S-[0-9-]*\)"$/\1/p')
	MSYS_NO_PATHCONV=1 icacls "$(cygpath -w "$1")" /deny "*$sid:(WD,AD)" > /dev/null
}
allow_writes() { MSYS_NO_PATHCONV=1 icacls "$(cygpath -w "$1")" /remove:d "*$sid" > /dev/null; }
