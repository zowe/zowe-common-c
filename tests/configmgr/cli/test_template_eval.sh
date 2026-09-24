#!/bin/sh
# Pins ${{ ... }} template evaluation for every JSON type the evaluator can
# return: integer, boolean, string, null, array, object, plus templates that
# call into stdlib (Math.*, String methods, JSON.stringify). Each assertion
# is a separate `extract` invocation so a regression on any one type
# surfaces locally.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "configmgr template eval"

FX=fixtures
SCHEMA="$FX/permissive_schema.json"
YAML="FILE($FX/types.yaml)"

extract_value() {
  # extract_value /pointer -> stdout (LAST_STDOUT/LAST_EXIT set)
  run_case "extract $1" "$CONFIGMGR" -s "$SCHEMA" -p "$YAML" extract "$1"
}

# Literal int (control)
extract_value "/literal_int"
assert_exit "/literal_int exit" 0
assert_contains "/literal_int value" "117" "$LAST_STDOUT"

# Template int: 9*13
extract_value "/template_int"
assert_exit "/template_int exit" 0
assert_contains "/template_int value" "117" "$LAST_STDOUT"

# Template negative int
extract_value "/template_neg_int"
assert_exit "/template_neg_int exit" 0
assert_contains "/template_neg_int value" "-42" "$LAST_STDOUT"

# Literal bool
extract_value "/literal_bool_false"
assert_exit "/literal_bool_false exit" 0
assert_contains "/literal_bool_false value" "false" "$LAST_STDOUT"

# Template bool: 1===1 -> true
extract_value "/template_bool"
assert_exit "/template_bool exit" 0
assert_contains "/template_bool value" "true" "$LAST_STDOUT"

# Literal string
extract_value "/literal_string"
assert_exit "/literal_string exit" 0
assert_contains "/literal_string value" "Hello, World!" "$LAST_STDOUT"

# Template string: 'a'.repeat(16)
extract_value "/template_string"
assert_exit "/template_string exit" 0
assert_contains "/template_string value" "aaaaaaaaaaaaaaaa" "$LAST_STDOUT"

# Template string concat
extract_value "/template_concat"
assert_exit "/template_concat exit" 0
assert_contains "/template_concat value" "abc" "$LAST_STDOUT"

# Literal null
extract_value "/literal_null"
assert_exit "/literal_null exit" 0
assert_contains "/literal_null prints 'null'" "null" "$LAST_STDOUT"

# Template null
extract_value "/template_null"
assert_exit "/template_null exit" 0
assert_contains "/template_null prints 'null'" "null" "$LAST_STDOUT"

# Template array - index 0, 1, 2
extract_value "/template_array_simple/0"
assert_contains "/template_array_simple/0 == 1" "1" "$LAST_STDOUT"
extract_value "/template_array_simple/1"
assert_contains "/template_array_simple/1 == 2" "2" "$LAST_STDOUT"
extract_value "/template_array_simple/2"
assert_contains "/template_array_simple/2 == 3" "3" "$LAST_STDOUT"

# Template array of strings
extract_value "/template_array_strings/1"
assert_contains "/template_array_strings/1 == y" "y" "$LAST_STDOUT"

# Template array mixed types
extract_value "/template_array_mixed/0"
assert_contains "/template_array_mixed/0 == 1" "1" "$LAST_STDOUT"
extract_value "/template_array_mixed/1"
assert_contains "/template_array_mixed/1 == two" "two" "$LAST_STDOUT"
extract_value "/template_array_mixed/2"
assert_contains "/template_array_mixed/2 == true" "true" "$LAST_STDOUT"

# Template returning object
extract_value "/template_object/foo"
assert_contains "/template_object/foo == bar" "bar" "$LAST_STDOUT"
extract_value "/template_object/n"
assert_contains "/template_object/n == 42" "42" "$LAST_STDOUT"

# Stdlib: Math
extract_value "/template_math"
assert_contains "/template_math == 314" "314" "$LAST_STDOUT"

# Stdlib: String.prototype.replace
extract_value "/template_string_replace"
assert_contains "/template_string_replace == hello_world" "hello_world" "$LAST_STDOUT"

# Stdlib: JSON.stringify produces a string
extract_value "/template_json_stringify"
assert_contains "/template_json_stringify mentions key:1" "\"k\":1" "$LAST_STDOUT"

# Nested-object template
extract_value "/nested/inner_template"
assert_contains "/nested/inner_template == 12" "12" "$LAST_STDOUT"
extract_value "/nested/inner_object/deepest_template"
assert_contains "/nested/inner_object/deepest_template == xy" "xy" "$LAST_STDOUT"

end_suite
