#!/bin/sh
# `configmgr -script` must report a failing script and exit non-zero
# (zowe/zowe-install-packaging#3639). Since the QuickJS 2024-01-13 update a
# module's top-level throw rejects a promise instead of returning an
# exception, and configmgr dropped it silently and exited 0; the "no errors"
# report in zowe/zowe-common-c#585 is the same symptom.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "configmgr -script exit status and error reporting (#3639)"

FX=fixtures

run_case "throwing script" "$CONFIGMGR" -script "$FX/throws.js"
assert_exit "throwing script exits 2 (ZCFG_EVAL_FAILURE)" 2
assert_contains "throwing script prints the error" "boom from throws.js" "$LAST_STDOUT"

run_case "module with a ReferenceError" "$CONFIGMGR" -script "$FX/reference_error_module.js"
assert_exit "ReferenceError module exits 2" 2
assert_contains "ReferenceError module prints the error" "noSuchFunction" "$LAST_STDOUT"

run_case "missing script file" "$CONFIGMGR" -script "$FX/does_not_exist.js"
assert_exit "missing script exits 2" 2

run_case "healthy script" "$CONFIGMGR" -script "$FX/configmgr_api.js" \
  "$(cd $FX && pwd)/port_range_schema.json" "$(cd $FX && pwd)/port_good.yaml" \
  "$(cd $FX && pwd)/port_too_large.yaml" "$(cd $FX && pwd)/permissive_schema.json" \
  "$(cd $FX && pwd)/types.yaml"
assert_exit "healthy script still exits 0" 0

end_suite
