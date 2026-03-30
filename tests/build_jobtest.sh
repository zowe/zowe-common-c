#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.

set -e

COMMON="$(cd "$(dirname "$0")/.." && pwd)"

echo "Building jobtest (SSI 80 job enumeration test)"
echo "Common dir: $COMMON"

xlclang \
  -q64 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -I "${COMMON}/h" \
  -I /usr/include/zos \
  -o jobtest \
  jobtest.c \
  ${COMMON}/c/jobservice.c \
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
  ${COMMON}/c/dynalloc.c

rc=$?
if [ $rc -eq 0 ]; then
  echo "Build successful: ./jobtest"
  ls -la jobtest
else
  echo "Build failed rc=$rc"
fi
exit $rc
