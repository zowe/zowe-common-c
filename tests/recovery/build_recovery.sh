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

# Compiler options — full listings with cross-reference, offset map, and assembler
XLC_OPTS='-q64 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('"'"'CEE.SCEEMAC'"'"','"'"'SYS1.MACLIB'"'"','"'"'SYS1.MODGEN'"'"'),LIST(./),XREF,SOURCE,OFFSET"'
DEFINES="-D_OPEN_SYS_FILE_EXT=1 -D_OPEN_SYS=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1"
INCLUDES="-I ${COMMON}/h -I /usr/include/zos"

COMMON_SRCS="${COMMON}/c/zos.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/scheduling.c \
  ${COMMON}/c/mvscmd.c"

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
    # listing goes to .lst in current dir
    _lst="$(basename "$_src" .c).lst"
    if [ -f "$_lst" ]; then
      echo "  Listing: ./$_lst"
    fi
  else
    echo "  FAILED rc=$rc"
    exit $rc
  fi
}

cd "$HERE"

echo "Building recovery test programs"
echo "Common dir: $COMMON"
echo "Local dir:  $HERE"

build_one dumptest1 dumptest1.c
build_one sliptest1 sliptest1.c

echo ""
echo "=== All builds OK ==="
