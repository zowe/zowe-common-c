#!/bin/sh
# cfgSetConfigPath() sets the path; a second call replaces the first instead
# of appending to it (zowe/zowe-common-c#571). Driven through the JS API in
# fixtures/config_path_reset.js, which prints PASS:/FAIL: lines and exits
# non-zero on any failure.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "configmgr setConfigPath replaces, not appends (#571)"

FX=fixtures
FX_ABS=$(cd "$FX" && pwd)

run_case "configmgr -script config_path_reset.js" \
  "$CONFIGMGR" -script "$FX/config_path_reset.js" \
  "$FX_ABS/permissive_schema.json" \
  "$FX_ABS/path_reset_a.yaml" \
  "$FX_ABS/path_reset_b.yaml"

fail_in_js=$(echo "$LAST_STDOUT" | grep -c '^FAIL: ')
echo "$LAST_STDOUT" | grep -E '^(PASS|FAIL): ' | sed 's/^/    /'
assert_exit "script exits 0" 0
assert_eq "no FAIL lines from the script" "0" "$fail_in_js"

end_suite
