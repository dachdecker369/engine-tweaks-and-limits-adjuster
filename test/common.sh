# Paths and helpers shared by the test suites; every run_*.sh sources this file.
# T: this folder. R: the repository. B: where the fake game runs (run_all.sh deletes it afterwards).
# AW: the fake LOCALAPPDATA inside B, as a Windows path - the real one is never touched.
T="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
R="$(cd "$T/.." && pwd)"
B="${TWEAKSV_TEST_DIR:-$R/object/TestRun}"
mkdir -p "$B/appdata" || exit 1
AW="$(cygpath -w "$B/appdata")"

# Test builds of TweaksV show no message box with this set, so no run waits on one (release builds
# ignore it).
export TWEAKSV_TEST_NO_BOXES=1

# check DESCRIPTION CONDITION: CONDITION is evaluated, the result printed and counted.
pass=0; fail=0
check() { if eval "$2"; then echo "PASS  $1"; pass=$((pass+1)); else echo "FAIL  $1"; fail=$((fail+1)); fi; }

# TweaksV.ini in B, with CRLF line endings, as TweaksV writes it.
# fresh_ini [FILE]: a copy of FILE from this folder (default $INI), without message boxes.
# setkey KEY VALUE [SECTION]: the first KEY in the file, or the KEY in [SECTION].
# addkey KEY VALUE: a key under [TWEAKSV] that the file does not have yet.
INI=base.ini
fresh_ini() { sed 's/$/\r/' "$T/${1:-$INI}" > TweaksV.ini; setkey ERROR_POPUP 0; }
setkey() {
	if [ -n "${3:-}" ]; then sed -b -i "/^\[$3\]/,/^\[/s/^$1=.*\r\$/$1=$2\r/" TweaksV.ini
	else sed -b -i "0,/^$1=.*\r\$/s//$1=$2\r/" TweaksV.ini; fi
}
addkey() { sed -b -i "/^\[TWEAKSV\]\r\$/a $1=$2\r" TweaksV.ini; }

# game EXE [OPTION...]: EXE, one of the fake game programs, runs as GTA5_Enhanced.exe and loads
# TweaksV.asi (or $ASI) with the fake LOCALAPPDATA, for 60 s at most; prints what it reported.
# The old TweaksV.log is deleted first, so a run in which TweaksV writes none leaves none.
game() {
	cp "$1" GTA5_Enhanced.exe; shift; rm -f harness.out TweaksV.log
	LOCALAPPDATA="$AW" timeout 60 ./GTA5_Enhanced.exe "${ASI:-TweaksV.asi}" "$@" > harness.out 2>&1
	tr -d '\r' < harness.out
}

# The last run: has TEXT - its report contains TEXT ($out); log_has PATTERN - a line of TweaksV.log
# matches; log_line PATTERN - the same without the CR, for patterns that end in $; count PATTERN -
# how many lines match.
has() { [[ "$out" == *"$1"* ]]; }
log_has() { grep -q -- "$1" TweaksV.log; }
log_line() { tr -d '\r' < TweaksV.log | grep -q -- "$1"; }
count() { grep -c -- "$1" TweaksV.log; }
