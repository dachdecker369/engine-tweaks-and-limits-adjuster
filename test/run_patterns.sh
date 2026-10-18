#!/usr/bin/env bash
# The pattern table: source\TweaksVPatterns.h and PATTERNS.md list the same patterns - the same labels
# with the same bytes - and the code uses every pattern of the table exactly once. Needs no fake game.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

# "LABEL TEXT" per line, sorted: the table's entries, PATTERNS.md's rows (but the removed ones) and the
# labels the code names with Pattern("...") (the table's own comment does not count)
table=$(sed -nE 's/^\s*\{ "([A-Za-z0-9-]+)", "([^"]+)" \},.*/\1 \2/p' "$R/source/TweaksVPatterns.h" | sort)
docs=$(grep -E '^\| [A-Z]-E[-A-Za-z0-9]* \| `' "$R/PATTERNS.md" | grep -v '| removed |' | sed -E 's/^\| ([^ ]+) \| `([^`]+)` \|.*/\1 \2/' | sort)
uses=$(grep -ohE 'Pattern\("[A-Za-z0-9-]+"\)' $(ls "$R"/source/*.cpp "$R"/source/*.h | grep -v TweaksVPatterns.h) | sed -E 's/Pattern\("(.*)"\)/\1/' | sort)
labels=$(cut -d' ' -f1 <<< "$table")
echo "      $(wc -l <<< "$table") patterns in the table, $(wc -l <<< "$docs") in PATTERNS.md, $(wc -l <<< "$uses") uses in the code"

echo "== the table and PATTERNS.md"
check "PATTERNS.md lists exactly the table's patterns, with the same bytes" '[ -n "$table" ] && [ "$table" = "$docs" ]'
[ "$table" = "$docs" ] || diff <(echo "$table") <(echo "$docs") | sed 's/^/      /'

echo "== the table and the code"
check "every pattern of the table is used, and only once" '[ "$labels" = "$uses" ]'
[ "$labels" = "$uses" ] || diff <(echo "$labels") <(echo "$uses") | sed 's/^/      /'

echo
echo "PATTERN TABLE CHECKS: $pass passed, $fail failed"
