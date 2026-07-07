#!/bin/sh
# Build + run the charset-streaming carry-forward / substitution tests under
# AddressSanitizer, natively on Linux/WSL with clang. Self-contained: compiles
# the handful of zowe-common-c TUs that convertCharsetStreaming needs directly
# from source, then links a tiny test driver -- no z/OS, no server, no certs.
#
#   sh run.sh          # clang + AddressSanitizer (default)
#   SAN= sh run.sh     # no sanitizer
#
# For the z/OS Open XL (ibm-clang64) compile check, see zos-compile-check.sh.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
CCROOT="$(cd "$HERE/../.." && pwd)"          # zowe-common-c
CLANG="${CLANG:-clang}"
SAN="${SAN:--fsanitize=address}"
FLAGS="-D__ZOWE_OS_LINUX=1 -D_GNU_SOURCE=1 -std=gnu99 -fms-extensions -g -O0 $SAN"
INC="-I $CCROOT/h -I $CCROOT/platform/posix"
OUT="$HERE/build"; mkdir -p "$OUT"

for f in charsets alloc utils logging timeutls collections; do
  $CLANG $FLAGS $INC -c "$CCROOT/c/$f.c" -o "$OUT/$f.o"
done
$CLANG $FLAGS $INC -c "$HERE/charset-streaming-test.c" -o "$OUT/test.o"
$CLANG $FLAGS "$OUT"/*.o -lpthread -lm -ldl -o "$OUT/charset-streaming-test"

echo "--- running charset-streaming-test ---"
ASAN_OPTIONS=detect_leaks=0 "$OUT/charset-streaming-test"
