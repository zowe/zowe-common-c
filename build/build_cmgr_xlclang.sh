#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

WORKING_DIR=$(dirname "$0")

# set -v

echo "********************************************************************************"
echo "Building configmgr..."

# These paths assume that the build is run from /zss/deps/zowe-common-c/builds

date_stamp=$(date +%Y%m%d)

TMP_DIR="${WORKING_DIR}/tmp-${date_stamp}"

mkdir -p "${TMP_DIR}" && cd "${TMP_DIR}"

COMMON="../.."

VERSION=$(cat ../configmgr_version.txt)

# Split version into parts
OLDIFS=$IFS
IFS="."
for part in ${VERSION}; do
  if [ -z "$MAJOR" ]; then
    MAJOR=$part
  elif [ -z "$MINOR" ]; then
    MINOR=$part
  else
    PATCH=$part
  fi
done
IFS=$OLDIFS


QUICKJS="${COMMON}/deps/configmgr/quickjs"
QUICKJS_LOCATION="git@github.com:joenemo/quickjs-portable.git"
QUICKJS_BRANCH="0.9.0"

LIBYAML="${COMMON}/deps/configmgr/libyaml"
LIBYAML_LOCATION="git@github.com:yaml/libyaml.git"
LIBYAML_BRANCH="0.2.5"

DEPS="QUICKJS LIBYAML"

OLDIFS=$IFS
IFS=" "
for dep in ${DEPS}; do
  eval directory="\$${dep}"
  echo "Check if dir exist=$directory"
  if [ ! -d "$directory" ]; then
    eval echo Clone: \$${dep}_LOCATION @ \$${dep}_BRANCH to \$${dep}
    eval git clone --branch "\$${dep}_BRANCH" "\$${dep}_LOCATION" "\$${dep}"
  fi
done
IFS=$OLDIFS

rm -f "${COMMON}/bin/configmgr"

# export _C89_ACCEPTABLE_RC=4

xlclang \
  -c \
  -q64 \
  -qascii \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  -DYAML_VERSION_MAJOR=${MAJOR} \
  -DYAML_VERSION_MINOR=${MINOR} \
  -DYAML_VERSION_PATCH=${PATCH} \
  -DYAML_VERSION_STRING="${VERSION}" \
  -DYAML_DECLARE_STATIC=1 \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DCONFIG_VERSION=\"2021-03-27\" \
  -I "${LIBYAML}/include" \
  -I "${QUICKJS}" \
  ${LIBYAML}/src/api.c \
  ${LIBYAML}/src/reader.c \
  ${LIBYAML}/src/scanner.c \
  ${LIBYAML}/src/parser.c \
  ${LIBYAML}/src/loader.c \
  ${LIBYAML}/src/writer.c \
  ${LIBYAML}/src/emitter.c \
  ${LIBYAML}/src/dumper.c \
  ${QUICKJS}/cutils.c \
  ${QUICKJS}/quickjs.c \
  ${QUICKJS}/quickjs-libc.c \
  ${QUICKJS}/libunicode.c \
  ${QUICKJS}/libregexp.c \
  ${QUICKJS}/porting/polyfill.c
#then
#  echo "Done with qascii-compiled open-source parts"
#else
#  echo "Build failed"
#  exit 8
#fi

xlclang \
  -q64 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DNOIBMHTTP=1 \
  -DCMGRTEST=1 \
  -I "${COMMON}/h" \
  -I "${COMMON}/platform/posix" \
  -I "${LIBYAML}/include" \
  -I "${QUICKJS}" \
  -o "${COMMON}/bin/configmgr" \
  api.o \
  reader.o \
  scanner.o \
  parser.o \
  loader.o \
  writer.o \
  emitter.o \
  dumper.o \
  cutils.o \
  quickjs.o \
  quickjs-libc.o \
  libunicode.o \
  libregexp.o \
  polyfill.o \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/bpxskt.c \
  ${COMMON}/c/charsets.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/configmgr.c \
  ${COMMON}/c/embeddedjs.c \
  ${COMMON}/c/json.c \
  ${COMMON}/c/jsonschema.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/microjq.c \
  ${COMMON}/c/parsetools.c \
  ${COMMON}/platform/posix/psxregex.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/scheduling.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/xlate.c \
  ${COMMON}/c/yaml2json.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/zosfile.c
#then
#  echo "Build successful"
#  ls -l "${COMMON}/bin"
#  exit 0
#else
#  # remove configmgr in case the linker had RC=4 and produced the binary
#  rm -f "${COMMON}/bin/configmgr"
#  echo "Build failed"
#  exit 8
#fi

rm -f "${TMP_DIR}"


# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.
