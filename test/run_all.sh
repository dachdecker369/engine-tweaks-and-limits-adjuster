#!/usr/bin/env bash
# Builds TweaksV (Release and the test build) and the fake game programs, runs every suite, and
# deletes what the run made (object\TestRun and object\Test). From Git Bash, in the repository:
#   bash test/run_all.sh [--no-build] [--keep]
# --no-build tests the TweaksV.asi files already built; --keep leaves the run folders for a look.
set -u
T="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
R="$(cd "$T/.." && pwd)"
export TWEAKSV_TEST_DIR="$R/object/TestRun"
SUITES="ini_default story modkit packfile rageopenv directstorage phase_e"

build=1; keep=0
for a in "$@"; do
	case "$a" in
		--no-build) build=0 ;;
		--keep) keep=1 ;;
		*) echo "unknown option: $a"; exit 2 ;;
	esac
done

cleanup() {
	[ "$keep" = 1 ] && { echo "kept: $TWEAKSV_TEST_DIR and $R/object/Test"; return; }
	rm -rf -- "${TWEAKSV_TEST_DIR:?}" "${R:?}/object/Test"
}

rm -rf -- "${TWEAKSV_TEST_DIR:?}"
mkdir -p "$TWEAKSV_TEST_DIR" || exit 1

if [ "$build" = 1 ]; then
	echo "== building TweaksV (Release and test build)"
	cmd //c "$(cygpath -w "$T/build_tweaksv.cmd")" > "$TWEAKSV_TEST_DIR/build_tweaksv.txt" 2>&1 \
		|| { tr -d '\r' < "$TWEAKSV_TEST_DIR/build_tweaksv.txt" | tail -n 30; echo "TweaksV build failed"; cleanup; exit 1; }
fi
for f in "$R/x64/Release/TweaksV.asi" "$R/object/Test/Release/TweaksV.asi"; do
	[ -f "$f" ] || { echo "missing: $f (run without --no-build)"; cleanup; exit 1; }
done

echo "== building the test programs"
OUT_DIR="$(cygpath -w "$TWEAKSV_TEST_DIR")" cmd //c "$(cygpath -w "$T/build.cmd")" > "$TWEAKSV_TEST_DIR/build_tests.txt" 2>&1 \
	|| { tr -d '\r' < "$TWEAKSV_TEST_DIR/build_tests.txt" | tail -n 30; echo "test program build failed"; cleanup; exit 1; }

total_pass=0; total_fail=0; summary=""
for s in $SUITES; do
	echo; echo "######## $s"
	bash "$T/run_$s.sh" 2>&1 | tee "$TWEAKSV_TEST_DIR/$s.out"
	line=$(grep -E '[0-9]+ passed, [0-9]+ failed' "$TWEAKSV_TEST_DIR/$s.out" | tail -n 1)
	p=$(sed -nE 's/.* ([0-9]+) passed, ([0-9]+) failed.*/\1/p' <<< "$line")
	f=$(sed -nE 's/.* ([0-9]+) passed, ([0-9]+) failed.*/\2/p' <<< "$line")
	if [ -z "$p" ]; then p=0; f=1; line="$s: no result line (the suite stopped early)"; fi
	total_pass=$((total_pass + p)); total_fail=$((total_fail + f))
	summary+="  $(printf '%-14s' "$s") $p passed, $f failed"$'\n'
done

echo; echo "======== RESULTS"
printf '%s' "$summary"
echo "  all            $total_pass passed, $total_fail failed"
cleanup
[ "$total_fail" = 0 ]
