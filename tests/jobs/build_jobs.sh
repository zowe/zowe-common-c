#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.

set -e

COMMON="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"

XLC_OPTS='-q64 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('"'"'CEE.SCEEMAC'"'"','"'"'SYS1.MACLIB'"'"','"'"'SYS1.MODGEN'"'"')"'
DEFINES="-D_OPEN_SYS_FILE_EXT=1 -D_OPEN_SYS=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1"
INCLUDES="-I ${COMMON}/h -I ${HERE} -I /usr/include/zos"

COMMON_SRCS="${COMMON}/c/tui.c \
  ${COMMON}/c/jobservice.c \
  ${COMMON}/c/logreader.c \
  ${COMMON}/c/ssi.c \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/scheduling.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/dynalloc.c"

build_one() {
  _prog="$1"
  _src="$2"
  _extra_src="$3"
  echo ""
  echo "=== Building $_prog ==="
  eval xlclang $XLC_OPTS $DEFINES $INCLUDES -o "$_prog" "$_src" $_extra_src $COMMON_SRCS
  rc=$?
  if [ $rc -eq 0 ]; then
    echo "  OK: ./$_prog"
    ls -la "$_prog"
  else
    echo "  FAILED rc=$rc"
    exit $rc
  fi
}

echo "Building SDSF-like TUI tools"
echo "Common dir: $COMMON"
echo "Local dir:  $HERE"

# Phase 1: ST panel (job status)
build_one zst zst.c

# Phase 3: DA panel (display active)
build_one zda zda.c "${HERE}/asinfo.c"

# Phase 4: MVS command console (ON HOLD — sys/console.h not available)
# build_one zcmd zcmd.c "${HERE}/mvscmd.c"

# Phase 5: SYSLOG browser
build_one zlog zlog.c

# Phase 6: Process viewer
build_one zps zps.c "${HERE}/procinfo.c"

# Phase 7: MVS Display command browser
build_one zdisp zdisp.c "${COMMON}/c/zdisplay.c"

echo ""
echo "All builds complete."
