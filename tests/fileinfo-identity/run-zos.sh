#!/bin/sh
# run-zos.sh -- compile, link and run the FileInfo identity tests on z/OS with
# ibm-clang64. The Linux/WSL equivalent is run.sh, which additionally runs them
# under AddressSanitizer.
#
# On z/OS these accessors were already implemented (c/zosfile.c, over BPXYSTAT)
# and this PR does not touch them. This run exists to pin the contract on the
# platform that defines it: if the POSIX implementations added here ever drift
# from what z/OS does, one of the two runs starts failing.
#
# The compiler flags below are lifted VERBATIM from the `zos` mode of
# build/build_cmgr_clang.sh, following tests/zosfile-mkdir/run-zos.sh. The $VAR
# strings are expanded UNQUOTED so the -mzos-asmlib=//'DATASET' quotes reach
# the compiler intact.
#
# The test creates three small files (and one hard link) in the directory given
# as argv[1], defaulting to its own build directory, and removes them again.
#
# Prereq: interactive shell with env.sh sourced (ibm-clang64 on PATH).
#   cd <zowe-common-c> && sh tests/fileinfo-identity/run-zos.sh
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
PR="${PR:-$(cd "$HERE/../.." && pwd)}"          # zowe-common-c root
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
# zosfile.c holds the z/OS fileGetINode/fileGetDeviceID; the rest are what it
# links against. Same list as tests/zosfile-mkdir/run-zos.sh.
for f in zosfile zos utils alloc logging timeutls le collections recovery \
         xlate charsets xlate_tables scheduling; do
  # NOTE: $BASE_CFLAGS etc. are intentionally UNQUOTED so the //'DATASET'
  # quotes reach the compiler as fully-qualified dataset names.
  if $CC -c $BASE_CFLAGS $MAIN_CHARSET_CFLAGS $MAIN_DEFS $MAIN_INCLUDES \
        "$PR/c/$f.c" -o "$OUT/$f.o" 2>"$OUT/$f.err"; then
    echo "  OK   $f.c"
  else
    echo "  FAIL $f.c"; sed -n '1,6p' "$OUT/$f.err"; rc=1
  fi
done
# psxfile.c is guarded to compile to nothing off its own platforms; building it
# here is what proves the guard still holds and that the two new functions have
# not become duplicate symbols on z/OS.
if $CC -c $BASE_CFLAGS $MAIN_CHARSET_CFLAGS $MAIN_DEFS $MAIN_INCLUDES \
      "$PR/platform/posix/psxfile.c" -o "$OUT/psxfile.o" 2>"$OUT/psxfile.err"; then
  echo "  OK   psxfile.c"
else
  echo "  FAIL psxfile.c"; sed -n '1,6p' "$OUT/psxfile.err"; rc=1
fi
if $CC -c $BASE_CFLAGS $MAIN_CHARSET_CFLAGS $MAIN_DEFS $MAIN_INCLUDES \
      "$HERE/fileinfo-identity-test.c" -o "$OUT/test.o" 2>"$OUT/test.err"; then
  echo "  OK   fileinfo-identity-test.c"
else
  echo "  FAIL fileinfo-identity-test.c"; sed -n '1,6p' "$OUT/test.err"; rc=1
fi

echo
if [ $rc != 0 ]; then
  echo "COMPILE FAILURES above (per-file .err files in $OUT)."
  exit $rc
fi
echo "ALL COMPILED under ibm-clang64."
echo
echo "== linking + running =="
# Link with just the codegen flags (-fasm/-mzos-asmlib are compile-only and
# warn as unused at link); the objects are already assembled.
LIBOBJS="$OUT/zosfile.o $OUT/zos.o $OUT/utils.o $OUT/alloc.o $OUT/logging.o \
$OUT/timeutls.o $OUT/le.o $OUT/collections.o $OUT/recovery.o $OUT/xlate.o \
$OUT/charsets.o $OUT/xlate_tables.o $OUT/scheduling.o $OUT/psxfile.o"
if $CC -m64 -mzos-float-kind=ieee $LIBOBJS "$OUT/test.o" \
      -o "$OUT/fileinfo-identity-test" 2>"$OUT/link.err"; then
  # Capture the test's own exit status BEFORE transcoding -- in a pipeline $?
  # is the last command's status (iconv/tr), which is always 0 and would report
  # a failing run as a pass.
  "$OUT/fileinfo-identity-test" "$OUT" >"$OUT/report.raw" 2>&1
  rc=$?
  # The test compiles EBCDIC (like the code under test), so report.raw is
  # IBM-1047. Transcode it for an ordinary terminal: to ISO8859-1 rather than
  # UTF-8 so everything stays single-byte, then map any EBCDIC NL (0x15) or
  # NEL (0x85) to LF, since which one a line end survives as depends on the
  # shell's conversion state (_BPXK_AUTOCVT, _TAG_REDIR_OUT, file tags).
  #
  # Set ZOWE_TEST_RAW=1 if something in your toolchain ALREADY transcodes this
  # command's output -- converting twice produces line noise. report.raw always
  # holds the untouched EBCDIC either way.
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
