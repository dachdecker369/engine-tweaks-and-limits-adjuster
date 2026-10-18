# Paths shared by the test suites; every run_*.sh sources this file.
# T: this folder. R: the repository. B: where the fake game runs (run_all.sh deletes it afterwards).
# AW: the fake LOCALAPPDATA inside B, as a Windows path - the real one is never touched.
T="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
R="$(cd "$T/.." && pwd)"
B="${TWEAKSV_TEST_DIR:-$R/object/TestRun}"
mkdir -p "$B/appdata" || exit 1
AW="$(cygpath -w "$B/appdata")"
