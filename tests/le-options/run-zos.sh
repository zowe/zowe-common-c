#!/bin/sh
#
# run-zos.sh -- build test_le_options on z/OS USS and run it under several
# Language Environment runtime-option regimes, asserting what getLEHeapOptions
# must report for each.
#
# Builds, in this order, skipping a compiler that is not on the PATH:
#   xlclang, 64-bit   (the configmgr build: build/build_cmgr_xlclang.sh)
#   xlc, 31-bit       (the ZSS build: classic xlc, XPLINK; exercises the
#                      AMODE 31 offsets. xlclang -Wc,ILP32 cannot compile
#                      zos.c/timeutls.c: CLC5400 on long long asm operands)
#   ibm-clang64       (build/build_cmgr_clang.sh, mode zos)
#
# Run from this directory:
#    cd tests/le-options
#    sh run-zos.sh              # everything
#    sh run-zos.sh xlclang64    # one build: xlclang64 | xlc31 | clang64
#
# Output is EBCDIC (the library's own char mode), so nothing here transcodes.
# Exit status is the number of failing (build, case) pairs.

set -u

ONLY="${1:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
H="$ROOT/h"
PXSX="$ROOT/platform/posix"
C="$ROOT/c"
OUT="$HERE/zbuild"; mkdir -p "$OUT"

# Same library TUs tests/clang_readiness links; the linker drops the unused.
COMMON_C="
  $C/alloc.c
  $C/charsets.c
  $C/collections.c
  $C/fdpoll.c
  $C/le.c
  $C/logging.c
  $C/recovery.c
  $C/scheduling.c
  $C/signalcontrol.c
  $C/timeutls.c
  $C/utils.c
  $C/xlate.c
  $C/zos.c
  $C/zosfile.c
"

DEFS="-D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DNEW_CAA_LOCATIONS=1 -DCMGRTEST=1 -I $H -I $PXSX"

XLC64_FLAGS="-q64 \
  -Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN') \
  $DEFS"
# Mirrors zss/build/build_zss.sh's xlc step, minus its product defines and
# its listing options (agg, exp, list, so, off, xref), which only decorate a
# listing nobody asked for and warn without one.
XLC31_FLAGS="\
  -Wc,dll,expo,langlvl(extc99),gonum,goff,hgpr,roconst,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN') \
  -Wc,xplink \
  $DEFS"
IBC64_FLAGS="-m64 -mzos-float-kind=ieee -std=gnu99 -fasm \
  -mzos-asmlib=//'CEE.SCEEMAC' -mzos-asmlib=//'SYS1.MACLIB' -mzos-asmlib=//'SYS1.MODGEN' \
  -mzos-no-asm-implicit-clobber-reg -Wno-trigraphs \
  -fzos-le-char-mode=ebcdic -fexec-charset=IBM-1047 -D_EXT=1 $DEFS"

failures=0

# run_cases BINARY -- one row per runtime-option regime:
#   _CEE_RUNOPTS | expect HEAPPOOLS | expect HEAPPOOLS64 | expect HEAPZONES size64
run_cases() {
  bin="$1"
  while IFS='|' read -r runopts pools pools64 zones64; do
    echo "  --- _CEE_RUNOPTS='$runopts' ---"
    if [ -n "$runopts" ]; then
      _CEE_RUNOPTS="$runopts" "$bin" "$pools" "$pools64" "$zones64"
    else
      ( unset _CEE_RUNOPTS; "$bin" "$pools" "$pools64" "$zones64" )
    fi
    rc=$?
    if [ "$rc" -ne 0 ]; then failures=$((failures + 1)); fi
  done <<'EOF'
|0|0|0
HEAPPOOLS64(ON)|0|1|0
HEAPPOOLS(ON)|1|0|0
HEAPZONES(32,MSG,32,MSG)|0|0|32
HEAPPOOLS(ON) HEAPPOOLS64(ON)|1|1|0
EOF
}

# build_and_run NAME COMPILER FLAGS...
build_and_run() {
  name="$1"; shift
  cc="$1"; shift
  if [ -n "$ONLY" ] && [ "$ONLY" != "$name" ]; then return 0; fi
  echo
  echo "================================================================"
  echo "== $name: $cc"
  echo "================================================================"
  if ! command -v "$cc" >/dev/null 2>&1; then
    echo "  SKIPPED: $cc not on PATH"
    return 0
  fi
  bin="$OUT/test_le_options_$name"
  # -o before the sources: the xlclang driver rejects it afterwards (FSUM3008).
  # shellcheck disable=SC2086
  if "$cc" "$@" -o "$bin" $COMMON_C "$HERE/test_le_options.c" >"$OUT/build_$name.log" 2>&1; then
    echo "  build OK"
  else
    echo "  build FAILED, see $OUT/build_$name.log:"
    grep -E 'ERROR|error:|SEVERE' "$OUT/build_$name.log" | head -15 | sed 's/^/    /'
    failures=$((failures + 1))
    return 1
  fi
  run_cases "$bin"
}

# shellcheck disable=SC2086
build_and_run xlclang64 xlclang     $XLC64_FLAGS
# shellcheck disable=SC2086
build_and_run xlc31     xlc         $XLC31_FLAGS
# shellcheck disable=SC2086
build_and_run clang64   ibm-clang64 $IBC64_FLAGS

echo
echo "$failures failing (build, case) pair(s)"
exit $failures
