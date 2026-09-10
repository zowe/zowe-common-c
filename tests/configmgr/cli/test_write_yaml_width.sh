#!/bin/sh
# writeYAMLWithWidth() folds long double-quoted values so that no output line
# exceeds the given width (a PARMLIB record length), and refuses when a line
# cannot be folded (zowe/zowe-common-c#550).

cd "$(dirname "$0")"
. ./lib.sh

start_suite "writeYAML honours a maximum line width (#550)"

FX=$(pwd)/fixtures
OUT=${TMPDIR:-/tmp}/cmgr-folded-$$.yaml
run_case "width 60" \
  "$CONFIGMGR" -script "$FX/write_yaml_width.js" "$FX/any_object_schema.json" "$FX/long_values.yaml" 60 "$OUT" "$FX/long_key_unfoldable.yaml"
assert_exit "script exits 0" 0
assert_contains "all checks passed" "write_yaml_width: all checks passed" "$LAST_STDOUT"
rm -f "$OUT"

end_suite
