#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

echo "********************************************************************************"
echo "Building and running unit tests..."

WORKING_DIR=$(cd $(dirname "$0") && pwd)
COMMON="$WORKING_DIR/../.."

date_stamp=$(date +%Y%m%d%S)
TMP_DIR="${WORKING_DIR}/tmp-${date_stamp}"
mkdir -p "${TMP_DIR}" && cd "${TMP_DIR}"

COMMON_SRCS="\
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/timeutls.c"

CC_FLAGS="\
  -q64 \
  -qascii \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -I ${COMMON}/h \
  -I ${COMMON}/platform/posix \
  -I ${WORKING_DIR}"

WC_FLAGS="-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('SYS1.MACLIB')"

FAILED=0
TOTAL=0
PASS=0

# --- Build and run test_collections ---
echo ""
echo "======================================"
echo "Building test_collections..."
echo "======================================"
xlclang ${CC_FLAGS} "${WC_FLAGS}" \
  -o "${TMP_DIR}/test_collections" \
  ${WORKING_DIR}/test_collections.c \
  ${COMMON_SRCS}

if [ $? -eq 0 ]; then
  echo "Running test_collections..."
  "${TMP_DIR}/test_collections"
  TOTAL=$((TOTAL + 1))
  if [ $? -eq 0 ]; then
    PASS=$((PASS + 1))
  else
    FAILED=$((FAILED + 1))
  fi
else
  echo "FAILED to compile test_collections"
  TOTAL=$((TOTAL + 1))
  FAILED=$((FAILED + 1))
fi

# --- Build and run test_utils ---
echo ""
echo "======================================"
echo "Building test_utils..."
echo "======================================"
xlclang ${CC_FLAGS} "${WC_FLAGS}" \
  -o "${TMP_DIR}/test_utils" \
  ${WORKING_DIR}/test_utils.c \
  ${COMMON_SRCS}

if [ $? -eq 0 ]; then
  echo "Running test_utils..."
  "${TMP_DIR}/test_utils"
  TOTAL=$((TOTAL + 1))
  if [ $? -eq 0 ]; then
    PASS=$((PASS + 1))
  else
    FAILED=$((FAILED + 1))
  fi
else
  echo "FAILED to compile test_utils"
  TOTAL=$((TOTAL + 1))
  FAILED=$((FAILED + 1))
fi

# --- Summary ---
echo ""
echo "======================================"
echo "Test suites: ${TOTAL} total, ${PASS} passed, ${FAILED} failed"
echo "======================================"

cd "${WORKING_DIR}"
rm -rf "${TMP_DIR}"

if [ ${FAILED} -ne 0 ]; then
  exit 1
fi
exit 0

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.
