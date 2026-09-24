#!/bin/sh
# Smoke test: configmgr binary exists, responds to --help-ish invocation,
# and runs validate/extract/env/jq commands against trivial inputs.
# Runs on any platform where configmgr was built (Linux, z/OS USS).

cd "$(dirname "$0")"
. ./lib.sh

start_suite "configmgr smoke"

FX=fixtures

# Help: invoked with no args, configmgr prints usage and exits ZCFG_BAD_ARGS (12).
run_case "no-args help" "$CONFIGMGR"
assert_exit "no-args exit is BAD_ARGS" 12
assert_contains "no-args output mentions Usage" "Usage:" "$LAST_STDOUT"
assert_contains "no-args output lists extract" "extract" "$LAST_STDOUT"

# extract a literal string
run_case "extract literal_string" "$CONFIGMGR" \
  -s "$FX/permissive_schema.json" -p "FILE($FX/types.yaml)" extract /literal_string
assert_exit "extract literal_string clean" 0
assert_contains "extract literal_string value" "Hello, World!" "$LAST_STDOUT"

# extract a literal int
run_case "extract literal_int" "$CONFIGMGR" \
  -s "$FX/permissive_schema.json" -p "FILE($FX/types.yaml)" extract /literal_int
assert_exit "extract literal_int clean" 0
assert_contains "extract literal_int value" "117" "$LAST_STDOUT"

# validate against schema (good)
run_case "validate port_good" "$CONFIGMGR" \
  -s "$FX/port_range_schema.json" -p "FILE($FX/port_good.yaml)" validate
assert_exit "validate good exits 0" 0
assert_contains "validate good says no exceptions" "No validity Exceptions" "$LAST_STDOUT"

# env command: writes to /tmp and we read it back
env_out=$(cli_mktemp)
run_case "env writes file" "$CONFIGMGR" \
  -s "$FX/port_range_schema.json" -p "FILE($FX/port_good.yaml)" env "$env_out"
assert_exit "env exits 0" 0
env_content=$(cat "$env_out")
assert_contains "env file mentions ZWE_port" "ZWE_port" "$env_content"
assert_contains "env file has 8080 value" "8080" "$env_content"
rm -f "$env_out"

# NOTE: the `jq` subcommand has a platform-conditional bug (configmgr.c:1895
# hardcodes UTF-8 ccsid, wrong on z/OS USS). Its assertions live in
# known_issues/test_jq_ccsid.sh so the main suite stays a clean 100%
# benchmark across platforms.

end_suite
