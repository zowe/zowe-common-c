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
echo "Building configmgr for REXX"

# These paths assume that the build is run from /zss/deps/zowe-common-c/builds

# set -v

# Loads project info like name and version
. $WORKING_DIR/configmgr.proj.env

echo "********************************************************************************"
echo "Building $PROJECT..."

COMMON="$WORKING_DIR/.."

# Checks for and possibly downloads dependencies from env vars from above file
. $WORKING_DIR/dependencies.sh
check_dependencies "${COMMON}" "$WORKING_DIR/configmgr.proj.env"
DEPS_DESTINATION=$(get_destination "${COMMON}" "${PROJECT}")

rm -f "${COMMON}/bin/configmgr"

MAJOR=0
MINOR=2
PATCH=5
VERSION="\"${MAJOR}.${MINOR}.${PATCH}\""

GSKDIR=/usr/lpp/gskssl
GSKINC="${GSKDIR}/include"

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
  -DNEW_CAA_LOCATIONS=1 \
  -I "${DEPS_DESTINATION}/${LIBYAML}/include" \
  -I "${DEPS_DESTINATION}/${QUICKJS}" \
  ${DEPS_DESTINATION}/${LIBYAML}/src/api.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/reader.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/scanner.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/parser.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/loader.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/writer.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/emitter.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/dumper.c \
  ${DEPS_DESTINATION}/${QUICKJS}/cutils.c \
  ${DEPS_DESTINATION}/${QUICKJS}/quickjs.c \
  ${DEPS_DESTINATION}/${QUICKJS}/quickjs-libc.c \
  ${DEPS_DESTINATION}/${QUICKJS}/libunicode.c \
  ${DEPS_DESTINATION}/${QUICKJS}/libregexp.c \
  ${DEPS_DESTINATION}/${QUICKJS}/porting/debugutil.c \
  ${DEPS_DESTINATION}/${QUICKJS}/porting/polyfill.c
#then
#  echo "Done with qascii-compiled open-source parts"
#else
#  echo "Build failed"
#  exit 8
#fi

xlclang \
  -c \
  -q64 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  -DYAML_VERSION_MAJOR=${YAML_MAJOR} \
  -DYAML_VERSION_MINOR=${YAML_MINOR} \
  -DYAML_VERSION_PATCH=${YAML_PATCH} \
  -DYAML_VERSION_STRING="${YAML_VERSION}" \
  -DYAML_DECLARE_STATIC=1 \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DCONFIG_VERSION=\"2021-03-27\" \
  -I "${DEPS_DESTINATION}/${LIBYAML}/include" \
  -I "${DEPS_DESTINATION}/${QUICKJS}" \
  -I ${COMMON}/h \
  ${COMMON}/c/embeddedjs.c 

# Hacking around weird link issue:
ar x /usr/lpp/cbclib/lib/libibmcmp.a z_atomic.LIB64R.o

xlclang \
  -q64 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'),list()" \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DNEW_CAA_LOCATIONS=1 \
  -DUSE_ZOWE_TLS=1 \
  -I "${COMMON}/h" \
  -I "${COMMON}/platform/posix" \
  -I ${GSKINC} \
  -I "${DEPS_DESTINATION}/${LIBYAML}/include" \
  -I "${DEPS_DESTINATION}/${QUICKJS}" \
  -o "${COMMON}/bin/zwecfgle" \
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
  debugutil.o \
  embeddedjs.o \
  z_atomic.LIB64R.o \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/bpxskt.c \
  ${COMMON}/c/charsets.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/configmgr.c \
  ${COMMON}/c/embeddedjs.c \
  ${COMMON}/c/fdpoll.c \
  ${COMMON}/c/http.c \
  ${COMMON}/c/httpclient.c \
  ${COMMON}/c/json.c \
  ${COMMON}/c/jcsi.c \
  ${COMMON}/c/jsonschema.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/microjq.c \
  ${COMMON}/c/parsetools.c \
  ${COMMON}/c/pdsutil.c \
  ${COMMON}/c/qjsnet.c \
  ${COMMON}/c/qjszos.c \
  ${COMMON}/platform/posix/psxregex.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/rexxcmgr.c \
  ${COMMON}/c/scheduling.c \
  ${COMMON}/c/socketmgmt.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/tls.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/xlate.c \
  ${COMMON}/c/yaml2json.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/zosfile.c \
  ${GSKDIR}/lib/GSKSSL64.x \
  ${GSKDIR}/lib/GSKCMS64.x

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


# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.
