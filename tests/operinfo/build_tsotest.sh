#!/bin/sh
set -e

COMMON="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"

XLC_OPTS='-q64 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('"'"'CEE.SCEEMAC'"'"','"'"'SYS1.MACLIB'"'"','"'"'SYS1.MODGEN'"'"')"'
DEFINES="-D_OPEN_SYS_FILE_EXT=1 -D_OPEN_SYS=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1"
INCLUDES="-I ${COMMON}/h -I ${HERE} -I /usr/include/zos"

COMMON_SRCS="${COMMON}/c/alloc.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/dynalloc.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/scheduling.c"

echo "=== Building ztso_ikj ==="
eval xlclang $XLC_OPTS $DEFINES $INCLUDES -o ztso_ikj ztso_ikj.c $COMMON_SRCS
rc=$?
if [ $rc -eq 0 ]; then
  echo "  OK: ./ztso_ikj"
  ls -la ztso_ikj
else
  echo "  FAILED rc=$rc"
  exit $rc
fi
