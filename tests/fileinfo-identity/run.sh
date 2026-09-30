#!/bin/sh
# run.sh -- build and run the FileInfo identity tests on Linux/WSL with clang,
# under AddressSanitizer and UndefinedBehaviorSanitizer.
#
# This is the run that matters for the POSIX implementations of fileGetINode()
# and fileGetDeviceID(): before they existed, platform/posix/psxfile.c simply
# did not define them, so anything off z/OS that touched a file's identity
# failed to link. Run run-zos.sh for the same cases on z/OS, where BPXYSTAT
# rather than struct stat backs FileInfo.
#
#   sh tests/fileinfo-identity/run.sh
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
COMMON="$(cd "$HERE/../.." && pwd)"
CC="${CC:-clang}"
OUT="$HERE/build"; mkdir -p "$OUT"

$CC -fsanitize=address,undefined -fno-sanitize-recover=all -g \
  -I "$COMMON/h" -I "$COMMON/platform/posix" \
  -D__ZOWE_OS_LINUX -D_GNU_SOURCE -DNOIBMHTTP=1 \
  "$HERE/fileinfo-identity-test.c" \
  "$COMMON/platform/posix/psxfile.c" \
  "$COMMON/c/utils.c" "$COMMON/c/alloc.c" "$COMMON/c/timeutls.c" \
  -o "$OUT/fileinfo-identity-test"

"$OUT/fileinfo-identity-test" "$OUT"
