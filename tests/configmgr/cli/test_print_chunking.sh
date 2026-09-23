#!/bin/sh
# Regression coverage for js_native_print() (c/embeddedjs.c), the native
# console.log() implementation. The fix uses a 256-byte stack buffer for
# short arguments and a 64KB (MAX_PRINT_ELT_SIZE) heap buffer -- chunked in
# 64KB pieces -- for longer ones. This test pins that no bytes are dropped,
# duplicated, or corrupted at those size transitions, plus a couple of
# ordinary multi-argument console.log() calls. Driven through
# fixtures/print_chunking.js, which brackets each payload in
# "<<<label:len>>>" / "<<<end>>>" markers for this script to verify.
#
# NOTE: js_native_print() is only wired up as console.log() under __ZOWE_OS_ZOS.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "console.log native print chunking"

FX=fixtures

run_case "configmgr -script print_chunking.js" \
  "$CONFIGMGR" -script "$FX/print_chunking.js"

assert_exit "script exits 0" 0

boundary_report=$(echo "$LAST_STDOUT" | awk '
  BEGIN { bad = 0 }
  /^<<<[A-Za-z0-9_-]+:[0-9]+>>>$/ {
    line = $0
    sub(/^<<</, "", line); sub(/>>>$/, "", line)
    split(line, parts, ":")
    label = parts[1]; len = parts[2] + 0
    if ((getline payload) <= 0) { print "FAIL: " label " missing payload line"; bad = 1; next }
    if ((getline endmark) <= 0) { print "FAIL: " label " missing end marker"; bad = 1; next }
    plen = length(payload)
    if (plen != len) {
      print "FAIL: " label " length mismatch: expected " len " got " plen
      bad = 1
      next
    }
    body = (plen > 1) ? substr(payload, 1, plen - 1) : ""
    lastch = substr(payload, plen, 1)
    if ((plen > 0 && lastch != "Z") || body ~ /[^A]/) {
      print "FAIL: " label " content corrupted"
      bad = 1
      next
    }
    if (endmark != "<<<end>>>") {
      print "FAIL: " label " end marker missing/shifted (byte count changed downstream)"
      bad = 1
      next
    }
    print "PASS: " label " (" len " bytes)"
  }
  END { exit bad }
')

echo "$boundary_report" | sed 's/^/    /'
boundary_fail=$(echo "$boundary_report" | grep -c '^FAIL: ')
assert_eq "no FAIL lines in boundary checks" "0" "$boundary_fail"

mixed_line=$(echo "$LAST_STDOUT" | awk '/^<<<mixed-args>>>$/{getline p; print p; exit}')
assert_eq "mixed text/number/bool/null/undefined arguments join correctly" \
  "count: 42 pi: 3.14159 flag: true nothing: null missing: undefined" "$mixed_line"

several_line=$(echo "$LAST_STDOUT" | awk '/^<<<several-params>>>$/{getline p; print p; exit}')
assert_eq "several plain parameters join correctly" "1 2 3 four 5 six" "$several_line"

end_suite
