#!/bin/sh
# test-duplicate-keys.sh -- behavioral regression for zowe/zowe-common-c#581.
#
# A YAML mapping with a duplicate top-level `zowe:` key must be reduced to a
# SINGLE JSON property, with a warning, so that configmgr and the launcher --
# which both convert YAML via yaml2json.c -- evaluate the same config identically.
# Before the fix, yaml2json emitted duplicate JSON properties; the C accessors
# resolved the first while the QuickJS template evaluator (launcher) resolved the
# last, so `configmgr` and `launcher` diverged, and on current staging the load
# aborted with `TypeError: cannot read property 'myPrefix' of undefined`.
#
# Prototype semantics: FIRST-WINS (keep first duplicate, warn, ignore the rest),
# which resolves the `${{ zowe.environments.* }}` templates in Martin's example.
#
#   cd tests/dupkeytest && sh test-duplicate-keys.sh
#     CONFIGMGR=/path/to/configmgr sh test-duplicate-keys.sh   # to point at a binary
#
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
CM="${CONFIGMGR:-$HERE/../../bin/configmgr}"
Y="FILE($HERE/581-duplicate-zowe.yaml)"
S="$HERE/permissive.schema"

if [ ! -x "$CM" ]; then
  echo "SKIP: configmgr not built at '$CM' (build/build_cmgr_clang.sh, or set CONFIGMGR=...)"
  exit 0
fi

OUT="$("$CM" -s "$S" -p "$Y" validate 2>&1)"
fail=0
has()    { echo "$OUT" | grep -q "$1"; }
assert() { if [ "$2" -eq 0 ]; then echo "  ok  : $1"; else echo "  FAIL: $1"; fail=$((fail + 1)); fi; }

has "duplicate key 'zowe'";                  assert "warns about the duplicate 'zowe' key" $?
has '"runtimeDirectory": "/my/runtime"';     assert "first-wins keeps environments -> runtimeDirectory template resolves" $?
has '"logDirectory": "/my/runtime/log"';     assert "logDirectory template resolves" $?
has 'validate status = 0';                   assert "config loads and validates (status 0)" $?
if has 'TypeError'; then assert "no TypeError (load did not abort)" 1; else assert "no TypeError (load did not abort)" 0; fi
ZCOUNT="$(echo "$OUT" | grep -c '^  "zowe": {')"
[ "$ZCOUNT" -eq 1 ];                         assert "exactly one 'zowe' property in merged JSON (was 2 before the fix), got $ZCOUNT" $?

echo
if [ "$fail" -eq 0 ]; then echo "ALL PASS"; else echo "FAILURES: $fail"; fi
exit "$fail"
