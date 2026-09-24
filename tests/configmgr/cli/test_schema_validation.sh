#!/bin/sh
# Pins schema-validation outcomes for representative violation classes.
# Each case runs `configmgr validate` and asserts on:
#   - the program exit code (0 = valid, 99 = ZCFG_CONFIG_FAILED_VALIDATION)
#   - the presence/absence of "No validity Exceptions" in the trace output
#   - for failure cases, a substring matching the specific exception that
#     should have been raised
#
# This catches silent validator regressions (e.g., dropping support for a
# constraint type) that the existing test_configmgr makefile target misses
# because it only runs the happy path.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "configmgr schema validation"

FX=fixtures
PORT_SCHEMA="$FX/port_range_schema.json"

# --- 1: a good config validates clean ---
run_case "good: validate exits 0" "$CONFIGMGR" \
  -s "$PORT_SCHEMA" -p "FILE($FX/port_good.yaml)" validate
assert_exit "good: exit 0" 0
assert_contains "good: 'No validity Exceptions' in output" \
  "No validity Exceptions" "$LAST_STDOUT"

# --- 2: required field missing ---
run_case "required-missing: validate" "$CONFIGMGR" \
  -s "$PORT_SCHEMA" -p "FILE($FX/port_missing.yaml)" validate
assert_exit "required-missing: exit 99" 99
assert_contains "required-missing: validity exceptions present" \
  "Validity Exceptions" "$LAST_STDOUT"
# Schema requires "port"; some message should call it out.
assert_contains "required-missing: mentions 'port'" "port" "$LAST_STDOUT"

# --- 3: wrong type (string where integer required) ---
run_case "wrong-type: validate" "$CONFIGMGR" \
  -s "$PORT_SCHEMA" -p "FILE($FX/port_wrong_type.yaml)" validate
assert_exit "wrong-type: exit 99" 99
assert_contains "wrong-type: validity exceptions present" \
  "Validity Exceptions" "$LAST_STDOUT"
# The validator should mention either 'string' or 'integer'
case "$LAST_STDOUT" in
  *integer*|*string*|*type*)
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "  PASS  wrong-type: diagnostic mentions type"
    ;;
  *)
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo "  FAIL  wrong-type: diagnostic does NOT mention type-name"
    echo "          actual: $LAST_STDOUT"
    ;;
esac

# --- 4: out-of-range integer (maximum: 65535, given 999999) ---
run_case "out-of-range: validate" "$CONFIGMGR" \
  -s "$PORT_SCHEMA" -p "FILE($FX/port_too_large.yaml)" validate
assert_exit "out-of-range: exit 99" 99
assert_contains "out-of-range: 'too large' in message" \
  "too large" "$LAST_STDOUT"
assert_contains "out-of-range: mentions the offending value" \
  "999999" "$LAST_STDOUT"
assert_contains "out-of-range: mentions the limit" \
  "65535" "$LAST_STDOUT"

end_suite
