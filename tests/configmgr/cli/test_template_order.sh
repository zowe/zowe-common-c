#!/bin/sh
# Pins ${{ ... }} template DEPENDENCY-ORDER semantics. Three sub-cases:
#
#   1. chain_ok      - dependency declared before consumer  -> resolves cleanly
#   2. forward_ref   - dependency declared AFTER consumer   -> resolves via
#                      fixed-point iteration (added 2026-05-14)
#   3. cycle         - mutual reference                     -> cfgLoadConfiguration
#                      fails with ZCFG_EVAL_FAILURE (exit 2), evaluator emits
#                      a "stalled at pass N" diagnostic naming the cycle
#
# History:
#   2026-05-12: Initial pin asserted forward refs SILENTLY produced
#               "[object Object]" and cycles produced exit 16 with no
#               diagnostic. Both were documented as current-broken-behavior
#               that needed fixing.
#   2026-05-14: evaluateJsonTemplates rewritten in c/embeddedjs.c with
#               fixed-point iteration capped at MAX_TEMPLATE_PASSES (16).
#               Top-level unresolved markers are no longer published as
#               JS globals, so a forward reference produces a clean
#               ReferenceError on the failing pass and is retried after
#               the dependency resolves. A pass that sees markers but
#               resolves none triggers a "stalled" diagnostic and
#               propagates ZCFG_EVAL_FAILURE.
#
# Residual limitation: a template that forward-references a NESTED
# (non-top-level) marker can still produce "undefined"-tainted output
# on first pass and won't be retried because pass 1 replaces the marker
# with a string (which is no longer a marker, so the visitor doesn't
# revisit it). Top-level forward refs are the common case and are fully
# fixed. Document this if you ship a config relying on nested forward
# refs.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "configmgr template order"

FX=fixtures
SCHEMA="$FX/permissive_schema.json"

# --- case 1: chain in dependency-correct order resolves cleanly ---

run_case "chain_ok: base literal" "$CONFIGMGR" -s "$SCHEMA" \
  -p "FILE($FX/chain_ok.yaml)" extract /base
assert_exit "chain_ok /base exit" 0
assert_contains "chain_ok /base == MYHOST" "MYHOST" "$LAST_STDOUT"

run_case "chain_ok: 1-deep template" "$CONFIGMGR" -s "$SCHEMA" \
  -p "FILE($FX/chain_ok.yaml)" extract /hlq
assert_exit "chain_ok /hlq exit" 0
assert_contains "chain_ok /hlq == MYHOST.ZWE" "MYHOST.ZWE" "$LAST_STDOUT"

run_case "chain_ok: 2-deep template" "$CONFIGMGR" -s "$SCHEMA" \
  -p "FILE($FX/chain_ok.yaml)" extract /proclib
assert_exit "chain_ok /proclib exit" 0
assert_contains "chain_ok /proclib == MYHOST.ZWE.PROCLIB" \
  "MYHOST.ZWE.PROCLIB" "$LAST_STDOUT"

run_case "chain_ok: 3-deep template" "$CONFIGMGR" -s "$SCHEMA" \
  -p "FILE($FX/chain_ok.yaml)" extract /deep_chain
assert_exit "chain_ok /deep_chain exit" 0
assert_contains "chain_ok /deep_chain == MYHOST.ZWE.PROCLIB.MEMBER" \
  "MYHOST.ZWE.PROCLIB.MEMBER" "$LAST_STDOUT"

# --- case 2: forward-reference (known broken; pin current behavior) ---

run_case "forward_ref: /base resolves (no fwd ref)" "$CONFIGMGR" -s "$SCHEMA" \
  -p "FILE($FX/chain_forward_ref.yaml)" extract /base
assert_exit "forward_ref /base exit" 0
assert_contains "forward_ref /base == base" "base" "$LAST_STDOUT"

# /middle depends on /base which is earlier-resolved by the time middle's
# visitor runs (because base has no template at all). middle works.
run_case "forward_ref: /middle (1-step ok)" "$CONFIGMGR" -s "$SCHEMA" \
  -p "FILE($FX/chain_forward_ref.yaml)" extract /middle
assert_exit "forward_ref /middle exit" 0
assert_contains "forward_ref /middle == 'base middle'" "base middle" \
  "$LAST_STDOUT"

# /result depends on /middle which is declared LATER in the doc. With
# fixed-point iteration: pass 1 fails (middle is a marker, not published
# as a global -> ReferenceError); middle resolves on pass 1 because its
# own dependency (base) is a literal; pass 2 retries result and succeeds.
run_case "forward_ref: /result (resolves via fixed-point)" \
  "$CONFIGMGR" -s "$SCHEMA" \
  -p "FILE($FX/chain_forward_ref.yaml)" extract /result
assert_exit "forward_ref /result exit" 0
assert_contains \
  "forward_ref /result == 'base middle z'" \
  "base middle z" "$LAST_STDOUT"

# --- case 3: cycle (cfgLoadConfiguration fails with diagnostic) ---

run_case "cycle: /a (cyclic key)" "$CONFIGMGR" -s "$SCHEMA" \
  -p "FILE($FX/cycle.yaml)" extract /a
assert_exit "cycle /a exits 2 (ZCFG_EVAL_FAILURE from cfgLoadConfiguration)" 2
assert_contains "cycle /a output mentions evaluator stall" "stalled at pass" \
  "$LAST_STDOUT"
assert_contains "cycle /a output mentions circular or undefined" \
  "circular reference or undefined variable" "$LAST_STDOUT"

# /b is symmetric; same outcome
run_case "cycle: /b (cyclic key)" "$CONFIGMGR" -s "$SCHEMA" \
  -p "FILE($FX/cycle.yaml)" extract /b
assert_exit "cycle /b exits 2" 2

end_suite
