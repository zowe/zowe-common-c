#!/bin/sh
# run-zos.sh -- compile, link, and run the #828 streaming-charset test on z/OS
# with ibm-clang64: confirms charsets.c (our fix) builds on the Open XL compiler
# AND that convertCharsetStreaming behaves correctly on real z/OS iconv.
#
# The flags below are lifted VERBATIM from the `zos` mode of
# build/build_cmgr_clang.sh (Joe's known-good configmgr build). This script
# does NOT invent flags -- if build_cmgr_clang.sh's zos flags change, mirror
# them here (better yet, factor them into a shared sourced file and source it
# from both; see the note at the bottom).
#
# Why a script and not a pasted command line: the -mzos-asmlib=//'DATASET'
# values must keep their single quotes so the dataset is treated as
# fully-qualified (else it gets your userid prefixed -> SVC99 alloc error).
# Those quotes only survive as literal characters via unquoted shell-variable
# expansion; typed directly at a prompt the shell strips them. So keep the
# flags in "$VAR" strings and expand them UNQUOTED at the call, as below.
#
# Prereq: interactive shell with env.sh sourced (ibm-clang64 on PATH).
#   cd /ZOWE/joezowe/pr593 && sh tests/charset-streaming/run-zos.sh
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
  -DNOIBMHTTP=1 -DUSE_ZOWE_TLS=1 -DNEW_CAA_LOCATIONS=1 -DCMGRTEST=1"
GSKINC=/usr/lpp/gskssl/include
MAIN_INCLUDES="-I $PR/h -I $PR/platform/posix -I $GSKINC"

echo "== compiling with $CC (flags lifted from build_cmgr_clang.sh zos) =="
rc=0
# charsets.c is our fix; the rest are the TUs the test links against.
for f in charsets alloc utils logging timeutls collections; do
  # NOTE: $BASE_CFLAGS etc. are intentionally UNQUOTED so the //'DATASET'
  # quotes reach the compiler as fully-qualified dataset names.
  if $CC -c $BASE_CFLAGS $MAIN_CHARSET_CFLAGS $MAIN_DEFS $MAIN_INCLUDES \
        "$PR/c/$f.c" -o "$OUT/$f.o" 2>"$OUT/$f.err"; then
    echo "  OK   $f.c"
  else
    echo "  FAIL $f.c"; sed -n '1,6p' "$OUT/$f.err"; rc=1
  fi
done
if $CC -c $BASE_CFLAGS $MAIN_CHARSET_CFLAGS $MAIN_DEFS \
      -I "$PR/h" -I "$PR/platform/posix" \
      "$HERE/charset-streaming-test.c" -o "$OUT/test.o" 2>"$OUT/test.err"; then
  echo "  OK   charset-streaming-test.c"
else
  echo "  FAIL charset-streaming-test.c"; sed -n '1,6p' "$OUT/test.err"; rc=1
fi
# link-time LE stubs (getCAA/abortIfUnsupportedCAA are referenced by logging.o
# but never called on the streaming path -- see zos-le-stub.c).
if $CC -c $BASE_CFLAGS $MAIN_CHARSET_CFLAGS -I "$PR/h" \
      "$HERE/zos-le-stub.c" -o "$OUT/zos-le-stub.o" 2>"$OUT/stub.err"; then
  echo "  OK   zos-le-stub.c"
else
  echo "  FAIL zos-le-stub.c"; sed -n '1,6p' "$OUT/stub.err"; rc=1
fi

echo
if [ $rc != 0 ]; then
  echo "COMPILE FAILURES above (per-file .err files in $OUT)."
  exit $rc
fi
echo "ALL COMPILED under ibm-clang64."
echo
echo "== linking + running the streaming test on real z/OS iconv =="
# Link with just the codegen flags (-fasm/-mzos-asmlib are compile-only and warn
# as unused at link); the objects are already assembled.
if $CC -m64 -mzos-float-kind=ieee "$OUT"/*.o -o "$OUT/charset-streaming-test" 2>"$OUT/link.err"; then
  # The test compiles EBCDIC (like charsets.c), so its printed messages are
  # IBM-1047 -- transcode to UTF-8 for reading. All conversion DATA it checks is
  # numeric, so pass/fail is char-mode independent. Substitute bytes are accepted
  # as either 0x1A (z/OS iconv SUB) or 0x3F (WSL '?').
  "$OUT/charset-streaming-test" | iconv -f IBM-1047 -t UTF-8
else
  echo "  LINK FAILED:"; sed -n '1,8p' "$OUT/link.err"; rc=1
fi
echo
echo "DRY note: these flags are copied verbatim from build/build_cmgr_clang.sh."
echo "The clean fix is to factor that script's per-mode flag block into a sourceable"
echo "file and source it from both; ask and I'll do that refactor."
exit $rc
