#!/bin/sh
# Under memory pressure configmgr must fail, not crash (zowe/zowe-common-c#685).
#
# makeShortLivedHeapInternal() wrote its eyecatcher through the NULL that
# safeMalloc() returns when memory runs out, so the first allocation to fail
# on the startup path was a segfault on Linux and a low-core store (0C4) on
# z/OS. This test runs a trivial script under a descending series of virtual
# memory limits and asserts that the process never dies by signal: it may
# succeed, or exit with a non-zero status and a message, but exit codes above
# 128 (signal deaths, 139 = SIGSEGV) are failures.
#
# Linux only: `ulimit -v` limits the address space there; on z/OS USS the
# equivalent is MEMLIMIT, which this harness does not drive. Skipped there.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "configmgr fails cleanly under memory pressure (#685)"

case "$(uname -s)" in
  OS/390)
    echo "  SKIP  ulimit -v is not the memory lever on z/OS; nothing asserted"
    end_suite
    exit $?
    ;;
esac

FX=fixtures
SCRIPT="$FX/oom_probe.js"
signal_deaths=0
runs=0
lowest_ok=""
first_fail=""

kb=9000
while [ $kb -ge 2000 ]; do
  # run in a subshell so the limit applies to configmgr alone
  ( ulimit -v $kb 2>/dev/null; "$CONFIGMGR" -script "$SCRIPT" ) >/dev/null 2>&1
  rc=$?
  runs=$((runs + 1))
  if [ $rc -eq 0 ]; then
    lowest_ok=$kb
  elif [ -z "$first_fail" ]; then
    first_fail="$kb (rc $rc)"
  fi
  if [ $rc -eq 127 ]; then
    # the dynamic loader could not map the C library: below this limit the
    # process is not ours to keep alive, so the sweep stops here
    echo "  (info: loader gives up at $kb KB; sweep stops)"
    break
  fi
  if [ $rc -gt 128 ]; then
    echo "  FAIL  ulimit -v $kb: died by signal (rc $rc)"
    signal_deaths=$((signal_deaths + 1))
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi
  kb=$((kb - 100))
done

echo "  (info: $runs limits tried; lowest that still succeeded: ${lowest_ok:-none} KB; first failure: ${first_fail:-none})"
assert_eq "no run died by signal across the sweep" "0" "$signal_deaths"

end_suite
