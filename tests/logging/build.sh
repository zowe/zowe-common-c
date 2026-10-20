#!/bin/sh
# build.sh -- build and run component_id_test on Linux/WSL with clang (the
# byte-order case the test exists for) or on z/OS with xlclang/ibm-clang64
# (where it must keep passing).
#
#   cd tests/logging && sh build.sh            # build with clang, run
#   LOGGING_C=/path/to/other/logging.c sh build.sh   # test another logging.c
#                                                    # (e.g. the pre-fix one)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
C="$ROOT/c"; H="$ROOT/h"
LOGGING_C="${LOGGING_C:-$C/logging.c}"
OUT="$HERE/build"; mkdir -p "$OUT"

case "$(uname -s)" in
  OS/390)
    CC="${CC:-xlclang}"
    CFLAGS="-q64 -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -I $H -I $ROOT/platform/posix"
    ;;
  *)
    CC="${CC:-clang}"
    CFLAGS="-std=gnu99 -g -O0 -D__ZOWE_OS_LINUX=1 -D_GNU_SOURCE=1 -fsanitize=address,undefined -I $H -I $ROOT/platform/posix"
    ;;
esac

# shellcheck disable=SC2086
$CC $CFLAGS -o "$OUT/component_id_test" \
  "$HERE/component_id_test.c" "$LOGGING_C" \
  "$C/alloc.c" "$C/utils.c" "$C/collections.c" "$C/timeutls.c"
"$OUT/component_id_test"
