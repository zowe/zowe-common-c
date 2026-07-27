#!/bin/sh
#
# build.sh -- build the three clang-readiness tests under xlclang and/or
# ibm-clang64 on z/OS USS. Flag set mirrors build/build_cmgr_xlclang.sh and
# build/build_cmgr_clang.sh so any TU these tests link will compile under
# the same conditions as the production configmgr build.
#
# Run from this directory:
#    cd tests/clang_readiness
#    sh build.sh                # both compilers, all three tests
#    sh build.sh xlclang        # xlclang only
#    sh build.sh clang          # ibm-clang64 only
#
# Each test compiles to two binaries: test_<name>_xlc and test_<name>_clang.
# The bpxnet test requires outbound HTTP to example.com:80.
#

set -eu

WHICH="${1:-both}"

H=../../h
PXSX=../../platform/posix
C=../../c

# ---- Sources -----------------------------------------------------------
# Common library TUs every test transitively needs. Same TUs the configmgr
# build pulls; the linker drops unreferenced ones.
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

# bpxnet test additionally pulls bpxskt.c (the socket layer under test).
# socketmgmt.c is deliberately NOT pulled -- it's the TTLS / peer-cert
# extension that calls back into bpxskt's tcpIOControl, but we're testing
# plain HTTP and have no business linking certificate code.
NET_C="
  $C/bpxskt.c
"

# dynalloc test pulls dynalloc.c (SVC 99 wrapper). It exercises the
# GETDSAB inline-asm sites in zos.c via ddnameExists() and getDSAB()
# after dynamically allocating a fresh DD.
DYN_C="
  $C/dynalloc.c
"

# ---- xlclang flags (mirrors build_cmgr_xlclang.sh stage 2) -------------
XLC_FLAGS="\
  -q64 \
  -Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN') \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DNEW_CAA_LOCATIONS=1 \
  -DCMGRTEST=1 \
  -I $H \
  -I $PXSX"

# ---- ibm-clang64 flags (mirrors build_cmgr_clang.sh mode=zos stage 2) --
IBC_FLAGS="\
  -m64 \
  -mzos-float-kind=ieee \
  -std=gnu99 \
  -fasm \
  -mzos-asmlib=//'CEE.SCEEMAC' \
  -mzos-asmlib=//'SYS1.MACLIB' \
  -mzos-asmlib=//'SYS1.MODGEN' \
  -mzos-no-asm-implicit-clobber-reg \
  -Wno-trigraphs \
  -fzos-le-char-mode=ebcdic \
  -fexec-charset=IBM-1047 \
  -D_EXT=1 \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DNEW_CAA_LOCATIONS=1 \
  -DCMGRTEST=1 \
  -I $H \
  -I $PXSX"

# ---- Build helpers -----------------------------------------------------
build_xlc() {
  TEST=$1
  EXTRA=$2
  echo "===> xlclang: $TEST"
  # -o must come BEFORE the source list under xlclang on z/OS, otherwise
  # the driver parses "-o<path>" as an input file with an unknown suffix
  # and rejects it with FSUM3008.
  # shellcheck disable=SC2086
  xlclang $XLC_FLAGS -o "${TEST}_xlc" $COMMON_C $EXTRA "$TEST.c"
}

build_clang() {
  TEST=$1
  EXTRA=$2
  echo "===> ibm-clang64: $TEST"
  # shellcheck disable=SC2086
  ibm-clang64 $IBC_FLAGS -o "${TEST}_clang" $COMMON_C $EXTRA "$TEST.c"
}

build_one() {
  TEST=$1
  EXTRA=$2
  case "$WHICH" in xlclang|both) build_xlc   "$TEST" "$EXTRA" ;; esac
  case "$WHICH" in clang|both)   build_clang "$TEST" "$EXTRA" ;; esac
}

build_one test_charconv ""
build_one test_recovery ""
build_one test_bpxnet   "$NET_C"
build_one test_dynalloc "$DYN_C"
build_one test_signalcontrol ""

# ---- Optional: dump assembly to inspect inline-asm codegen. NOTE: under a
# clang compiler (xlclang or ibm-clang64) charsets.c now takes the iconv branch
# and has NO __troo -- the __troo HLASM path is metal-C only. recovery.c ->
# storageObtain still carries inline asm. Enable with `ASM=1 sh build.sh`.
#
# xlclang refuses -S outside Metal C mode (CCN0458 "-S invalid because
# METAL not specified"). Its LE-mode equivalent is -Wc,asmlist which
# writes <basename>.asm next to the source file. ibm-clang64 is
# clang-based and accepts -S directly.
#
# Each emission is soft-failed (|| true) so a failure on one compiler
# does not abort the others.
if [ "${ASM:-0}" = "1" ]; then
  echo
  echo "===> dumping assembly for charsets.c and recovery.c (ASM=1)"
  set +e
  for src in charsets recovery; do
    case "$WHICH" in xlclang|both)
      echo "  xlclang -Wc,asmlist $C/${src}.c"
      # -Wc,asmlist drops ${src}.asm in cwd. Compile-only via -c.
      # shellcheck disable=SC2086
      xlclang $XLC_FLAGS -c -Wc,asmlist "$C/${src}.c" -o "${src}_xlc.o" \
        || echo "    (xlclang asm emission failed; check messages above)"
      # If a .asm landed, give it a compiler-tagged name.
      if [ -f "${src}.asm" ]; then mv "${src}.asm" "${src}_xlc.asm"; fi
      ;;
    esac
    case "$WHICH" in clang|both)
      echo "  ibm-clang64 -mzos-listing=json $C/${src}.c -> ${src}_clang.json"
      # ibm-clang64 emits the JSON listing alongside the .o. Compile-only
      # via -c. The listing-file-name option pins the output path so we
      # do not have to guess what auto-derivation produced.
      # shellcheck disable=SC2086
      ibm-clang64 $IBC_FLAGS \
        -c -mzos-listing=json \
        -mzos-listing-file-name="${src}_clang.json" \
        "$C/${src}.c" -o "${src}_clang.o" \
        || echo "    (ibm-clang64 -mzos-listing=json failed)"
      ;;
    esac
  done
  set -e
  echo
  echo "  produced listing files:"
  ls -1 ./*.s ./*.asm ./*_clang.json 2>/dev/null || echo "    (none)"
  echo
  echo "  to render the JSON listing on Linux:"
  echo "    python3 format_listing.py charsets_clang.json"
  echo "    python3 format_listing.py recovery_clang.json"
  echo "  or on z/OS with IBM's tool:"
  echo "    ibm-clang-listing-formatter charsets_clang.json -o charsets_clang.txt"
fi

echo
echo "Built. Run with:"
echo "  ./test_charconv_xlc    ./test_charconv_clang"
echo "  ./test_recovery_xlc    ./test_recovery_clang"
echo "  ./test_bpxnet_xlc      ./test_bpxnet_clang"
echo "  ./test_dynalloc_xlc    ./test_dynalloc_clang"
if [ "${ASM:-0}" != "1" ]; then
  echo
  echo "Add ASM=1 to also dump charsets.c / recovery.c assembly:"
  echo "  ASM=1 sh build.sh"
fi
