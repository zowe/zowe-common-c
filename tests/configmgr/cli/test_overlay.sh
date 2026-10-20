#!/bin/sh
# Pins multi-source overlay (merge) semantics.
#
# configmgr merges config sources from RIGHT TO LEFT: each leftward source
# overlays the merged result of those to its right via jsonMerge. The
# default array-merge policy is JSON_MERGE_FLAG_CONCATENATE_ARRAYS, and
# scalars/objects use leftmost-wins on conflict.
#
# Fixtures (under fixtures/):
#   overlay_base.yaml    port:7000  name:base   extras:{fromBase:true}    colors:[red,green]
#   overlay_middle.yaml  port:8000              extras:{fromMiddle:true}  colors:[yellow]
#   overlay_top.yaml     port:9000                                        colors:[orange]
#
# With config path "FILE(top):FILE(middle):FILE(base)", expectations:
#   /port       == 9000           (top wins)
#   /name       == "base"         (only in base, survives leftward overlay)
#   /extras/fromBase   == true    (object keys union)
#   /extras/fromMiddle == true
#   /colors            == [red, green, yellow, orange]
#
# *** ASYMMETRY ALERT ***: arrays concatenate RIGHTMOST source first
# (base's [red, green] come before middle's [yellow] which comes before
# top's [orange]). This is opposite to the scalar precedence direction
# (where leftmost source wins). Documented here because it's surprising:
# the source code does
#     jsonMerge(overlay, configData, CONCATENATE_ARRAYS)
# which appends overlay's array elements AFTER configData's. configData
# starts as the rightmost source and grows leftward, so each leftward
# source's array elements end up later in the combined array.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "configmgr overlay/merge"

FX=fixtures
SCHEMA="$FX/permissive_schema.json"
P="FILE($FX/overlay_top.yaml):FILE($FX/overlay_middle.yaml):FILE($FX/overlay_base.yaml)"

# Scalar precedence: leftmost source wins on conflict
run_case "scalar: /port (top wins)" "$CONFIGMGR" -s "$SCHEMA" -p "$P" extract /port
assert_exit "scalar /port exit" 0
assert_contains "scalar /port == 9000" "9000" "$LAST_STDOUT"

# Key only in base survives
run_case "scalar: /name (only in base)" "$CONFIGMGR" -s "$SCHEMA" -p "$P" extract /name
assert_exit "scalar /name exit" 0
assert_contains "scalar /name == base" "base" "$LAST_STDOUT"

# Object-merge: keys union across all three sources
run_case "object: /extras/fromBase" "$CONFIGMGR" -s "$SCHEMA" -p "$P" extract /extras/fromBase
assert_exit "object /extras/fromBase exit" 0
assert_contains "object /extras/fromBase == true" "true" "$LAST_STDOUT"

run_case "object: /extras/fromMiddle" "$CONFIGMGR" -s "$SCHEMA" -p "$P" extract /extras/fromMiddle
assert_exit "object /extras/fromMiddle exit" 0
assert_contains "object /extras/fromMiddle == true" "true" "$LAST_STDOUT"

# Array concatenate: rightmost (base) first, then leftward sources appended.
# This is the inverse of scalar precedence -- pinning it here so any change
# to jsonMerge's array semantics or to the overload recursion is caught.
run_case "array: /colors/0 (base's first)" "$CONFIGMGR" -s "$SCHEMA" -p "$P" extract /colors/0
assert_exit "array /colors/0 exit" 0
assert_contains "array /colors/0 == red" "red" "$LAST_STDOUT"

run_case "array: /colors/1 (base's second)" "$CONFIGMGR" -s "$SCHEMA" -p "$P" extract /colors/1
assert_contains "array /colors/1 == green" "green" "$LAST_STDOUT"

run_case "array: /colors/2 (middle's first)" "$CONFIGMGR" -s "$SCHEMA" -p "$P" extract /colors/2
assert_contains "array /colors/2 == yellow" "yellow" "$LAST_STDOUT"

run_case "array: /colors/3 (top's first)" "$CONFIGMGR" -s "$SCHEMA" -p "$P" extract /colors/3
assert_contains "array /colors/3 == orange" "orange" "$LAST_STDOUT"

# Reverse order: base first should change scalar precedence to "base wins"
P_REV="FILE($FX/overlay_base.yaml):FILE($FX/overlay_middle.yaml):FILE($FX/overlay_top.yaml)"
run_case "reverse: /port (base wins)" "$CONFIGMGR" -s "$SCHEMA" -p "$P_REV" extract /port
assert_exit "reverse /port exit" 0
assert_contains "reverse /port == 7000" "7000" "$LAST_STDOUT"

end_suite
