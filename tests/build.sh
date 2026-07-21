#!/bin/sh
#
# tests/build.sh <area> -- build and run zowe-common-c unit tests on Linux/WSL
# under AddressSanitizer (+ UndefinedBehaviorSanitizer), off the mainframe.
#
# It compiles the common-c translation units that build off-platform (z/OS-only
# ones, and ones needing the OSS deps, self-exclude by failing to compile) into
# a cached object set, then links and runs each test in tests/<area>/ against
# them. No c89, no _C89_L6SYSLIB, no copied zis folder. ASan is the point: it is
# where a buffer overrun / use-after-free actually gets caught. See the ZSS repo
# test-support/TESTING.md for how this fits the review flow.
#
#   sh tests/build.sh httpserver              # every *.c in tests/httpserver/
#   sh tests/build.sh httpserver/bigreqtest.c # just that one
#   sh tests/build.sh jsonmergetest.c         # a test .c at tests/ root
#   SAN= sh tests/build.sh httpserver         # disable sanitizers
#
# A test is a C program with its own main() that returns nonzero on failure.
# If the code under test is z/OS-only, guard the mainframe parts with
# #ifdef __ZOWE_OS_ZOS so the portable half still builds and runs here; a link
# error for an excluded TU means you need such a guard or a tests/<area>/stubs.c.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"       # tests/
ROOT="$(cd "$HERE/.." && pwd)"              # repo root
CLANG="${CLANG:-clang}"
SAN="${SAN--fsanitize=address,undefined}"   # unset SAN (SAN=) to disable
# -ffunction-sections + --gc-sections: dead-strip functions the test does not
# reach, so a z/OS-only helper living in an otherwise-portable TU (e.g. a
# user-info lookup in utils.c that references BPX4GGN) does not force the whole
# test to satisfy that symbol. Only what the test transitively calls is kept.
CFLAGS="-D__ZOWE_OS_LINUX=1 -D_GNU_SOURCE=1 -std=gnu99 -fms-extensions -g -O0 -fno-omit-frame-pointer -ffunction-sections -fdata-sections -Wno-implicit-function-declaration $SAN"
INC="-I $ROOT/h -I $ROOT/platform/posix -I $ROOT/jwt/jwt"
LDLIBS="-lpthread -lm -ldl -lcrypto"   # -lcrypto: MD5_*/SHA1_* used by crypto.c off z/OS
GCSECT="-Wl,--gc-sections"

ARG="${1:-}"
[ -n "$ARG" ] || { echo "usage: sh tests/build.sh <area>[/<test.c>] | <test.c>"; exit 2; }

# Resolve the area directory and an optional single test file.
case "$ARG" in
  */*.c) AREADIR="$ROOT/tests/${ARG%/*}"; ONLY="$ROOT/tests/$ARG" ;;
  *.c)   AREADIR="$ROOT/tests";           ONLY="$ROOT/tests/$ARG" ;;
  *)     AREADIR="$ROOT/tests/$ARG";      ONLY="" ;;
esac
[ -d "$AREADIR" ] || { echo "no such test area: $AREADIR"; exit 1; }

OUT="$HERE/.build"; OBJ="$OUT/obj"; mkdir -p "$OBJ"

# ---- 1. compile the portable common-c TUs (cached by modification time) ----
echo "== compiling common-c TUs (off-platform-incompatible ones self-exclude) =="
: > "$OUT/excluded.txt"
LIBOBJS=""
for src in "$ROOT"/c/*.c "$ROOT"/platform/posix/*.c; do
  base="$(basename "${src%.c}")"
  o="$OBJ/$base.o"
  if [ -f "$o" ] && [ "$o" -nt "$src" ]; then LIBOBJS="$LIBOBJS $o"; continue; fi
  if $CLANG -c $CFLAGS $INC "$src" -o "$o" 2>"$OBJ/$base.err"; then
    LIBOBJS="$LIBOBJS $o"
  else
    first="$(grep -m1 'error:' "$OBJ/$base.err" | sed 's/.*error: //' | cut -c1-58)"
    printf '  skip %-22s %s\n' "$base" "$first" >> "$OUT/excluded.txt"
    rm -f "$o"
  fi
done
echo "  linked $(echo $LIBOBJS | wc -w) TUs, excluded $(grep -c . "$OUT/excluded.txt" 2>/dev/null || echo 0) (see $OUT/excluded.txt)"

# Shared stubs for z/OS-only symbols referenced from portable code the tests link
# (e.g. logging.c -> bpxnet.c networking). Plus optional per-area stubs.c.
STUB=""
if [ -f "$HERE/wsl-stubs.c" ]; then
  $CLANG -c $CFLAGS $INC "$HERE/wsl-stubs.c" -o "$OBJ/wsl-stubs.o" 2>/dev/null && STUB="$OBJ/wsl-stubs.o"
fi
if [ -f "$AREADIR/stubs.c" ]; then
  $CLANG -c $CFLAGS $INC "$AREADIR/stubs.c" -o "$OBJ/areastubs.o" 2>/dev/null && STUB="$STUB $OBJ/areastubs.o"
fi

# ---- 2. build + run each test ----
if [ -n "$ONLY" ]; then TESTS="$ONLY"; else TESTS="$(ls "$AREADIR"/*.c 2>/dev/null | grep -v '/stubs\.c$' || true)"; fi
[ -n "$TESTS" ] || { echo "no test .c files in $AREADIR"; exit 1; }

rc=0
for t in $TESTS; do
  name="$(basename "${t%.c}")"
  echo "== $name =="
  if ! $CLANG $CFLAGS $INC -c "$t" -o "$OBJ/$name.o" 2>"$OBJ/$name.cc.err"; then
    echo "  COMPILE FAILED:"; sed -n '1,8p' "$OBJ/$name.cc.err" | sed 's/^/    /'; rc=1; continue
  fi
  if ! $CLANG $CFLAGS "$OBJ/$name.o" $LIBOBJS $STUB $LDLIBS $GCSECT -o "$OUT/$name" 2>"$OBJ/$name.ld.err"; then
    echo "  LINK FAILED. Undefined symbols usually mean a z/OS-only TU was excluded"
    echo "  above; add tests/$(basename "$AREADIR")/stubs.c or #ifdef __ZOWE_OS_ZOS the call:"
    grep -oE "undefined reference to \`[^']+'" "$OBJ/$name.ld.err" | sed 's/.*`/    /; s/.$//' | sort -u | head -20
    [ -s "$OBJ/$name.ld.err" ] || cat "$OBJ/$name.ld.err" | head -6
    rc=1; continue
  fi
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 "$OUT/$name"
  st=$?
  [ $st -eq 0 ] && echo "  PASS" || { echo "  FAIL (exit $st)"; rc=1; }
done
exit $rc
