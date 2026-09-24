#!/bin/sh
# run-zos.sh -- compile, link and run the bounded string-helper tests on z/OS
# with ibm-clang64. The Linux/WSL equivalent is run.sh, which additionally runs
# them under AddressSanitizer; this one exercises them on the target platform,
# in EBCDIC char mode, with the __LONGNAME__ name mappings active -- neither of
# which the Linux build covers.
#
# The compiler flags below are lifted VERBATIM from the `zos` mode of
# build/build_cmgr_clang.sh, following the convention in
# tests/charset-streaming/run-zos.sh. The $VAR strings are expanded UNQUOTED so
# the -mzos-asmlib=//'DATASET' quotes reach the compiler intact.
#
# The test allocates every buffer at exactly the size it passes to the function
# under test and touches no files.
#
# Prereq: interactive shell with env.sh sourced (ibm-clang64 on PATH).
#   cd <zowe-common-c> && sh tests/utils-safe-strings/run-zos.sh
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
PR="${PR:-$(cd "$HERE/../.." && pwd)}"
CC="${CC:-ibm-clang64}"
OUT="$HERE/zbuild"; mkdir -p "$OUT"

# ---- flags: VERBATIM from build/build_cmgr_clang.sh, mode: zos ----
BASE_CFLAGS="-m64 -mzos-float-kind=ieee -std=gnu99 -fasm \
  -mzos-asmlib=//'CEE.SCEEMAC' -mzos-asmlib=//'SYS1.MACLIB' -mzos-asmlib=//'SYS1.MODGEN' \
  -mzos-no-asm-implicit-clobber-reg -Wno-trigraphs -D_EXT=1"
MAIN_CHARSET_CFLAGS="-fzos-le-char-mode=ebcdic -fexec-charset=IBM-1047"
MAIN_DEFS="-D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 \
  -DNOIBMHTTP=1 -DUSE_ZOWE_TLS=1 -DNEW_CAA_LOCATIONS=1"
MAIN_INCLUDES="-I $PR/h -I $PR/platform/posix"

echo "== compiling with $CC (flags lifted from build_cmgr_clang.sh zos) =="
rc=0
for f in utils alloc logging timeutls le collections recovery zos scheduling; do
  if $CC -c $BASE_CFLAGS $MAIN_CHARSET_CFLAGS $MAIN_DEFS $MAIN_INCLUDES \
        "$PR/c/$f.c" -o "$OUT/$f.o" 2>"$OUT/$f.err"; then
    echo "  OK   $f.c"
  else
    echo "  FAIL $f.c"; sed -n '1,6p' "$OUT/$f.err"; rc=1
  fi
done
if $CC -c $BASE_CFLAGS $MAIN_CHARSET_CFLAGS $MAIN_DEFS $MAIN_INCLUDES \
      "$HERE/safe-strings-test.c" -o "$OUT/test.o" 2>"$OUT/test.err"; then
  echo "  OK   safe-strings-test.c"
else
  echo "  FAIL safe-strings-test.c"; sed -n '1,6p' "$OUT/test.err"; rc=1
fi

echo
if [ $rc != 0 ]; then
  echo "COMPILE FAILURES above (per-file .err files in $OUT)."
  exit $rc
fi
echo "ALL COMPILED under ibm-clang64."
echo
echo "== linking + running =="
LIBOBJS="$OUT/utils.o $OUT/alloc.o $OUT/logging.o $OUT/timeutls.o $OUT/le.o \
$OUT/collections.o $OUT/recovery.o $OUT/zos.o $OUT/scheduling.o"
if $CC -m64 -mzos-float-kind=ieee $LIBOBJS "$OUT/test.o" \
      -o "$OUT/safe-strings-test" 2>"$OUT/link.err"; then
  # Capture the exit status BEFORE transcoding: in a pipeline $? is the last
  # command's status, which would report a failing run as a pass.
  "$OUT/safe-strings-test" >"$OUT/report.raw" 2>&1
  rc=$?
  # See tests/zosfile-mkdir/run-zos.sh for why this is written the way it is.
  # ZOWE_TEST_RAW=1 skips it when something upstream already transcodes.
  if [ -n "${ZOWE_TEST_RAW:-}" ]; then
    cat "$OUT/report.raw"
  else
    iconv -f IBM-1047 -t ISO8859-1 <"$OUT/report.raw" | tr '\025\205' '\012\012'
  fi
else
  echo "  LINK FAILED:"; sed -n '1,12p' "$OUT/link.err"; rc=1
fi

echo
echo "(exit $rc -- nonzero means at least one test failed)"
exit $rc
