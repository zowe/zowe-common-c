#!/bin/sh
# Tiny portable assertion library for configmgr CLI tests.
# POSIX-only (no bash-isms). Should run identically on Linux and z/OS USS.
#
# Usage:
#   . ./lib.sh
#   start_suite "my suite"
#   run_case "label" "configmgr ... extract /foo"
#     # uses $LAST_STDOUT, $LAST_EXIT after the run
#   assert_eq "label" "<expected>" "$LAST_STDOUT"
#   assert_exit "label" 0
#   end_suite   # returns 1 if any failure

PASS_COUNT=0
FAIL_COUNT=0
SUITE_NAME=""

# CONFIGMGR can be overridden from the environment. The default discovers
# the binary built by build_cmgr_clang.sh (zowe-common-c/bin/configmgr).
if [ -z "$CONFIGMGR" ]; then
  # tests/configmgr/cli/lib.sh is three dirs deep; bin is at root.
  cli_dir=$(cd "$(dirname "$0")" && pwd)
  CONFIGMGR="$cli_dir/../../../bin/configmgr"
fi
if [ ! -x "$CONFIGMGR" ]; then
  echo "lib.sh: configmgr binary not found or not executable: $CONFIGMGR" >&2
  echo "lib.sh: set \$CONFIGMGR to override, or build first via build/build_cmgr_clang.sh" >&2
  exit 2
fi

start_suite() {
  SUITE_NAME="$1"
  PASS_COUNT=0
  FAIL_COUNT=0
  echo "==== suite: $SUITE_NAME ===="
}

# run_case "label" "shell command line"
# Captures stdout+stderr into LAST_STDOUT and exit into LAST_EXIT.
# Does NOT itself assert; let the caller decide.
run_case() {
  CASE_LABEL="$1"
  shift
  # $@ is the command and its args, split as the shell sees them.
  LAST_STDOUT=$("$@" 2>&1)
  LAST_EXIT=$?
}

# assert_eq label expected actual
assert_eq() {
  label=$1; expected=$2; actual=$3
  if [ "$expected" = "$actual" ]; then
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "  PASS  $label"
  else
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo "  FAIL  $label"
    echo "          expected: $expected"
    echo "          actual:   $actual"
  fi
}

# assert_contains label needle haystack
assert_contains() {
  label=$1; needle=$2; haystack=$3
  case "$haystack" in
    *"$needle"*)
      PASS_COUNT=$((PASS_COUNT + 1))
      echo "  PASS  $label"
      ;;
    *)
      FAIL_COUNT=$((FAIL_COUNT + 1))
      echo "  FAIL  $label  (substring not found)"
      echo "          needle:   $needle"
      echo "          haystack: $haystack"
      ;;
  esac
}

# assert_exit label expected_code
# Uses $LAST_EXIT from the most recent run_case.
assert_exit() {
  label=$1; expected=$2
  if [ "$LAST_EXIT" = "$expected" ]; then
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "  PASS  $label (exit=$expected)"
  else
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo "  FAIL  $label  exit expected=$expected actual=$LAST_EXIT"
  fi
}

end_suite() {
  total=$((PASS_COUNT + FAIL_COUNT))
  echo "---- $SUITE_NAME: $PASS_COUNT/$total pass, $FAIL_COUNT fail ----"
  if [ "$FAIL_COUNT" -gt 0 ]; then
    return 1
  fi
  return 0
}

# cli_mktemp: portable temp-file creation. z/OS USS does not ship mktemp
# in /bin; fall back to a $$-and-counter path under TMPDIR (or /tmp).
# Always echoes a path and creates the file, matching mktemp's contract.
cli_mktemp() {
  if command -v mktemp >/dev/null 2>&1; then
    mktemp
  else
    CLI_MKTEMP_SEQ=$((${CLI_MKTEMP_SEQ:-0} + 1))
    _p="${TMPDIR:-/tmp}/cli.$$.${CLI_MKTEMP_SEQ}"
    : > "$_p"
    echo "$_p"
  fi
}
