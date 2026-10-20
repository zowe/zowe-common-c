#!/bin/sh
# Pins the embedded-JS Configuration native module contract via the
# fixtures/configmgr_api.js driver. The script-side does the assertions;
# this shell wrapper just verifies clean exit and that we saw a final
# PASS-count line.
#
# Notable contract details captured in the JS payload:
#   - validate() ALWAYS returns ok:true (bug, configmgr.c line ~1603)
#   - validate() response includes shoeSize: 11 (debugging artifact)
#   - validate(badConfig) returns ok:true PLUS exceptionTree
#   - getConfigData() returns the POST-template-eval JSON
#   - makeModifiedConfiguration and copyConfigurationAndDeleteKey don't
#     mutate the source config

cd "$(dirname "$0")"
. ./lib.sh

start_suite "configmgr JS API contract"

FX=fixtures
FX_ABS=$(cd "$FX" && pwd)

# All paths are passed to the JS as absolute, so the script doesn't care
# about its own cwd or where it was launched from.
run_case "configmgr -script configmgr_api.js" \
  "$CONFIGMGR" -script "$FX/configmgr_api.js" \
  "$FX_ABS/port_range_schema.json" \
  "$FX_ABS/port_good.yaml" \
  "$FX_ABS/port_too_large.yaml" \
  "$FX_ABS/permissive_schema.json" \
  "$FX_ABS/types.yaml"

# Inside-JS assertions each printed "PASS:" or "FAIL:" lines, and a final
# summary line. Verify by counting.
pass_in_js=$(echo "$LAST_STDOUT" | grep -c '^PASS: ')
fail_in_js=$(echo "$LAST_STDOUT" | grep -c '^FAIL: ')

echo "  (JS-side pass count: $pass_in_js, fail count: $fail_in_js)"

if [ "$fail_in_js" -gt 0 ]; then
  echo "$LAST_STDOUT" | grep '^FAIL: '
  FAIL_COUNT=$((FAIL_COUNT + fail_in_js))
fi

# Embedded JS in -script mode currently falls off main() with no explicit
# return value; exit code is therefore not reliable. Don't assert on it.
# Instead, key off the JS-side counts.
assert_contains "summary line present" "JS API CONTRACT:" "$LAST_STDOUT"

if [ "$fail_in_js" -eq 0 ] && [ "$pass_in_js" -gt 0 ]; then
  PASS_COUNT=$((PASS_COUNT + pass_in_js))
  echo "  PASS  JS-side $pass_in_js assertions all green"
fi

end_suite
