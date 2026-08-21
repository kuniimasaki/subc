#!/bin/bash
# subc test harness: runs demofiles/*.c against their .expect sidecar files
# and reports pass/fail. Each demofiles/<name>.c with a matching
# demofiles/<name>.expect is a test case:
#
#   EXIT=<0|1>              required exit code
#   CONTAINS=<substring>     optional; must appear somewhere in stdout+stderr
#
# Files with no .expect are skipped (not yet formalised as a test case).
#
# Usage: scripts/run-tests.sh [path-to-main-binary]

set -u
cd "$(dirname "$0")/.."
BIN="${1:-./main}"

if [ ! -x "$BIN" ]; then
  echo "error: interpreter binary not found or not executable: $BIN" >&2
  echo "(run 'make' first)" >&2
  exit 2
fi

pass=0
fail=0
skipped=0
failures=()

for c in demofiles/*.c; do
  name=$(basename "$c" .c)
  expect="demofiles/$name.expect"

  if [ ! -f "$expect" ]; then
    skipped=$((skipped + 1))
    continue
  fi

  exp_exit=$(sed -n 's/^EXIT=//p' "$expect")
  exp_contains=$(sed -n 's/^CONTAINS=//p' "$expect")

  out=$("$BIN" "$c" 2>&1)
  act_exit=$?

  reason=""
  if [ "$act_exit" != "$exp_exit" ]; then
    reason="exit=$act_exit (expected $exp_exit)"
  fi
  if [ -n "$exp_contains" ] && ! grep -qF -- "$exp_contains" <<<"$out"; then
    if [ -n "$reason" ]; then reason="$reason; "; fi
    reason="${reason}missing '$exp_contains' in output"
  fi

  if [ -z "$reason" ]; then
    pass=$((pass + 1))
    echo "PASS  $name"
  else
    fail=$((fail + 1))
    failures+=("$name: $reason")
    echo "FAIL  $name -- $reason"
  fi
done

echo
echo "$pass passed, $fail failed, $skipped skipped (no .expect)"

if [ "$fail" -gt 0 ]; then
  echo
  echo "Failures:"
  for f in "${failures[@]}"; do
    echo "  - $f"
  done
  exit 1
fi
