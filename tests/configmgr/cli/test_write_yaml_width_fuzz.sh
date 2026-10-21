#!/bin/sh
# Randomized round trip of writeYAMLWithWidth(): random values, random
# widths, folded text must load back identical (zowe/zowe-common-c#550).

cd "$(dirname "$0")"
. ./lib.sh

start_suite "writeYAMLWithWidth randomized round trip (#550)"

FX=$(pwd)/fixtures
SCRATCH=${TMPDIR:-/tmp}/cmgr-fuzz-$$
mkdir -p "$SCRATCH"
run_case "200 random documents" \
  "$CONFIGMGR" -script "$FX/write_yaml_width_fuzz.js" "$FX/any_object_schema.json" 200 "$SCRATCH"
assert_exit "script exits 0" 0
assert_contains "all iterations passed" "write_yaml_width_fuzz: all iterations passed" "$LAST_STDOUT"
rm -rf "$SCRATCH"

end_suite
