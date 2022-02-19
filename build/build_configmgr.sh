#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

WORKING_DIR=$(dirname "$0")


echo "********************************************************************************"
echo "Building configmgr..."

rm -f "${COMMON}/bin/configmgr"

mkdir -p "${WORKING_DIR}/tmp-configmgr" && cd "$_"
COMMON="../.."

LIBYAML="libyaml"
MAJOR=0
MINOR=2
PATCH=5
VERSION="\"${MAJOR}.${MINOR}.${PATCH}\""

if [ ! -d "${LIBYAML}" ]; then
  git clone git@github.com:yaml/libyaml.git
fi

export _C89_ACCEPTABLE_RC=0

if ! c89 \
  -c \
  -Wc,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,list\(\),so\(\),off,xref \
  -Wc,ascii,noxplink \
  -D_ENHANCED_ASCII_EXT=0xFFFFFFFF \
  -DYAML_VERSION_MAJOR="${MAJOR}" \
  -DYAML_VERSION_MINOR="${MINOR}" \
  -DYAML_VERSION_PATCH="${PATCH}" \
  -DYAML_VERSION_STRING="${VERSION}" \
  -I "${LIBYAML}/include" \
  ${LIBYAML}/src/api.c \
  ${LIBYAML}/src/reader.c \
  ${LIBYAML}/src/scanner.c \
  ${LIBYAML}/src/parser.c \
  ${LIBYAML}/src/loader.c \
  ${LIBYAML}/src/writer.c \
  ${LIBYAML}/src/emitter.c \
  ${LIBYAML}/src/dumper.c 
then
  echo "Build failed"
  exit 8
fi

if c89 \
  -D_XOPEN_SOURCE=600 \
  -DNOIBMHTTP=1 \
  -D_OPEN_THREADS=1 \
  -Wc,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,exp,list\(\),so\(\),off,xref \
  -Wl,dll \
  -I "${COMMON}/h" \
  -I "${COMMON}/platform/posix" \
  -I "${LIBYAML}/include" \
  -o "${COMMON}/bin/configmgr" \
  api.o \
  reader.o \
  scanner.o \
  parser.o \
  loader.o \
  writer.o \
  emitter.o \
  dumper.o \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/bpxskt.c \
  ${COMMON}/c/charsets.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/configmgr.c \
  ${COMMON}/c/json.c \
  ${COMMON}/c/jsonschema.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/platform/posix/psxregex.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/scheduling.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/xlate.c \
  ${COMMON}/c/yaml2json.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/zosfile.c
then
  echo "Build successful"
  exit 0
else
  # remove configmgr in case the linker had RC=4 and produced the binary
  rm -f "${COMMON}/bin/configmgr"
  echo "Build failed"
  exit 8
fi


# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.