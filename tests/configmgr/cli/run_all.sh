#!/bin/sh
# Run every test_*.sh in this directory, accumulate pass/fail counts,
# exit non-zero if any test failed. Portable POSIX shell.
#
# Usage:
#   sh run_all.sh                  # use ../../../bin/configmgr by default
#   CONFIGMGR=/path/to/cm sh run_all.sh
#
# Each test_*.sh script is expected to exit non-zero on any failure.
# Exit codes:
#   0  - all tests passed
#   1  - at least one test failed
#   2  - configmgr binary missing (lib.sh exits 2 in this case)

cd "$(dirname "$0")"

total_pass=0
total_fail=0
suite_fail=0

for t in test_*.sh; do
  [ -r "$t" ] || continue
  echo
  echo "########################################################################"
  echo "# Running $t"
  echo "########################################################################"
  if sh "$t"; then
    :
  else
    suite_fail=$((suite_fail + 1))
  fi
done

echo
echo "========================================================================"
if [ "$suite_fail" -eq 0 ]; then
  echo "ALL configmgr CLI tests passed."
else
  echo "$suite_fail test script(s) reported failure."
fi
echo "========================================================================"

[ "$suite_fail" -eq 0 ]
