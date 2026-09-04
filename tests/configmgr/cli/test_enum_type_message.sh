#!/bin/sh
# Validation messages for enum/const mismatches name the right type
# (zowe/zowe-common-c#563). A property without a "type" keyword, common inside
# oneOf alternatives, used to be reported as 'integer' regardless of what the
# schema's own values were.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "enum/const mismatch messages name the schema's type (#563)"

FX=fixtures

run_case "untyped oneOf certificate.type" \
  "$CONFIGMGR" -s "$FX/oneof_untyped_schema.json" -p "FILE($FX/oneof_untyped_bad.yaml)" validate
assert_exit "invalid config exits 99" 99
assert_contains "enum message names string" "expecting one of values '[JCEKS, JCECCAKS, JCERACFKS, JCECCARACFKS, JCEHYBRIDRACFKS]' of type 'string'" "$LAST_STDOUT"
assert_contains "const message names string" "expecting value 'PKCS12' of type 'string'" "$LAST_STDOUT"
case "$LAST_STDOUT" in
  *"of type 'integer'"*) echo "  FAIL  no message claims type 'integer'"; FAIL_COUNT=$((FAIL_COUNT + 1)) ;;
  *) echo "  PASS  no message claims type 'integer'" ;;
esac


run_case "typed integer enum and string const" \
  "$CONFIGMGR" -s "$FX/typed_int_enum_schema.json" -p "FILE($FX/typed_int_enum_bad.yaml)" validate
assert_exit "invalid config exits 99" 99
assert_contains "integer enum still says integer" "expecting one of values '[7554, 7555, 7556]' of type 'integer'" "$LAST_STDOUT"
assert_contains "string const says string" "expecting value 'STANDARD' of type 'string'" "$LAST_STDOUT"

end_suite
