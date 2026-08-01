#!/bin/sh
# run.sh -- build and run the bounded string-helper tests on Linux/WSL with
# clang, under AddressSanitizer and UndefinedBehaviorSanitizer.
#
# The test allocates every buffer at exactly the size it passes to the function
# under test, so an off-by-one is a heap overflow the sanitizer reports rather
# than a silent pass. Run run-zos.sh for the same cases on z/OS, which is where
# EBCDIC char mode and the __LONGNAME__ name mappings are actually exercised.
#
#   sh tests/utils-safe-strings/run.sh
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
COMMON="$(cd "$HERE/../.." && pwd)"
CC="${CC:-clang}"
OUT="$HERE/build"; mkdir -p "$OUT"

$CC -fsanitize=address,undefined -fno-sanitize-recover=all -g \
  -I "$COMMON/h" -I "$COMMON/platform/posix" \
  -D__ZOWE_OS_LINUX -DNOIBMHTTP=1 \
  "$HERE/safe-strings-test.c" \
  "$COMMON/c/utils.c" "$COMMON/c/alloc.c" "$COMMON/c/timeutls.c" \
  -o "$OUT/safe-strings-test"

"$OUT/safe-strings-test"
