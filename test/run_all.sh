#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Builds TweaksV (Release and the test build) and the fake game programs, runs every suite, and
# deletes what the run made (object\TestRun, object\TestTemp and object\Test). From Git Bash, in the
# repository:
#   bash test/run_all.sh [--no-build] [--keep]
# --no-build tests the TweaksV.asi files already built, so it works after a --keep run (every other run
# deletes the test build); --keep leaves the run folders for a look. The run fails when a check fails, when
# a suite stops early, when a build warns, and when the number of checks is not the one NOTES.md gives.
set -u
T="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
R="$(cd "$T/.." && pwd)"
export TWEAKSV_TEST_DIR="$R/object/TestRun"
# the suites, one a line, run in this order: the tables first, the parts in the order of the parts, the parts
# measured first in the order of SPECIFICATIONS.md 11.10, the whole of TweaksV last
SUITES=(
	patterns
	ini_default
	story
	modkit
	packfile
	boxstreamer
	decals
	rageopenv
	directstorage
	blips
	# [weapon infos]
	weaponinfos
	# [weapon components]
	# [siren ids]
	# [siren count]
	# [ped props]
	# [texture budget]
	# [streaming memory]
	integration
)

build=1; keep=0
for a in "$@"; do
	case "$a" in
		--no-build) build=0 ;;
		--keep) keep=1 ;;
		*) echo "unknown option: $a"; exit 2 ;;
	esac
done

cleanup() {
	[ "$keep" = 1 ] && { echo "kept: $TWEAKSV_TEST_DIR, $R/object/TestTemp and $R/object/Test"; return; }
	rm -rf -- "${TWEAKSV_TEST_DIR:?}" "${R:?}/object/TestTemp" "${R:?}/object/Test"
}

# check_warnings LOG: the warnings a build log holds are printed, with the log's last lines, and counted
warnings=0
check_warnings() {
	local found
	found=$(tr -d '\r' < "$1" | grep -c ': warning ')
	[ "$found" -gt 0 ] || return 0
	echo "== $found warning(s) in $(basename "$1"):"
	tr -d '\r' < "$1" | grep ': warning ' | head -n 20
	echo "   the last lines of $(basename "$1"):"
	tr -d '\r' < "$1" | tail -n 5
	warnings=$((warnings + found))
}

rm -rf -- "${TWEAKSV_TEST_DIR:?}" "${R:?}/object/TestTemp"
mkdir -p "$TWEAKSV_TEST_DIR" || exit 1

if [ "$build" = 1 ]; then
	echo "== building TweaksV (Release and test build)"
	cmd //c "$(cygpath -w "$T/build_tweaksv.cmd")" > "$TWEAKSV_TEST_DIR/build_tweaksv.txt" 2>&1 \
		|| { tr -d '\r' < "$TWEAKSV_TEST_DIR/build_tweaksv.txt" | tail -n 30; echo "TweaksV build failed"; cleanup; exit 1; }
	check_warnings "$TWEAKSV_TEST_DIR/build_tweaksv.txt"
fi
for f in "$R/x64/Release/TweaksV.asi" "$R/object/Test/Release/TweaksV.asi"; do
	[ -f "$f" ] || { echo "missing: $f (run without --no-build)"; cleanup; exit 1; }
done

echo "== building the test programs"
OUT_DIR="$(cygpath -w "$TWEAKSV_TEST_DIR")" cmd //c "$(cygpath -w "$T/build.cmd")" > "$TWEAKSV_TEST_DIR/build_tests.txt" 2>&1 \
	|| { tr -d '\r' < "$TWEAKSV_TEST_DIR/build_tests.txt" | tail -n 30; echo "test program build failed"; cleanup; exit 1; }
check_warnings "$TWEAKSV_TEST_DIR/build_tests.txt"

total_pass=0; total_fail=0; summary=""
for s in "${SUITES[@]}"; do
	echo; echo "######## $s"
	bash "$T/run_$s.sh" 2>&1 | tee "$TWEAKSV_TEST_DIR/$s.out"
	line=$(grep -E '[0-9]+ passed, [0-9]+ failed' "$TWEAKSV_TEST_DIR/$s.out" | tail -n 1)
	p=$(sed -nE 's/.* ([0-9]+) passed, ([0-9]+) failed.*/\1/p' <<< "$line")
	f=$(sed -nE 's/.* ([0-9]+) passed, ([0-9]+) failed.*/\2/p' <<< "$line")
	stopped=""
	if [ -z "$p" ]; then p=0; f=1; stopped=" - no result line (the suite stopped early)"; fi
	total_pass=$((total_pass + p)); total_fail=$((total_fail + f))
	summary+="  $(printf '%-14s' "$s") $p passed, $f failed$stopped"$'\n'
done

# the number of checks NOTES.md gives ("Tests: ... (N checks)"): a check that silently stops running shows
expected=$(grep -oE '[0-9]+ checks' "$R/NOTES.md" | head -n 1 | grep -oE '[0-9]+')
ok=1
[ "$total_fail" = 0 ] || ok=0
echo; echo "======== RESULTS"
printf '%s' "$summary"
echo "  all            $total_pass passed, $total_fail failed"
if [ "$((total_pass + total_fail))" != "${expected:-none}" ]; then
	echo "  the count: NOTES.md gives ${expected:-no number of} checks, this run made $((total_pass + total_fail))"; ok=0
fi
if [ "$warnings" != 0 ]; then echo "  build warnings: $warnings (listed above)"; ok=0; fi
cleanup
[ "$ok" = 1 ]
