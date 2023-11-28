#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

echo "********************************************************************************"
echo "Building getesm..."

WORKING_DIR=$(cd $(dirname "$0") && pwd)
COMMON="$WORKING_DIR/.."

date_stamp=$(date +%Y%m%d%S)
TMP_DIR="${WORKING_DIR}/tmp-${date_stamp}"
mkdir -p "${TMP_DIR}" && cd "${TMP_DIR}"

rm -f "${COMMON}/bin/getesm"

xlclang \
  -q64 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('SYS1.MACLIB')" \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DNOIBMHTTP=1 \
  -DCMGRTEST=1 \
  -I "${COMMON}/h" \
  -o "${COMMON}/bin/getesm" \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/getesm.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/zos.c 

rm -rf "${TMP_DIR}"

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.
