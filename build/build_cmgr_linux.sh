#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.

# Build script for configmgr on Linux using GCC (or a CC-compatible compiler).
# This is a Linux companion to build_cmgr_macos.sh (macOS/clang) and
# build_cmgr_xlclang.sh (z/OS/xlclang).

WORKING_DIR=$(cd "$(dirname "$0")" && pwd)

# Loads project info: PROJECT, VERSION, QUICKJS, LIBYAML, etc.
. "$WORKING_DIR/configmgr.proj.env"

echo "********************************************************************************"
echo "Building $PROJECT for Linux..."

COMMON="$WORKING_DIR/.."

# Resolve and possibly clone the libyaml and quickjs dependencies.
. "$WORKING_DIR/dependencies.sh"
check_dependencies "${COMMON}" "$WORKING_DIR/configmgr.proj.env"
DEPS_DESTINATION=$(get_destination "${COMMON}" "${PROJECT}")

# Split the VERSION string (e.g. "3.5.0") into MAJOR / MINOR / PATCH.
OLDIFS=$IFS
IFS="."
MAJOR="" MINOR="" PATCH=""
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

# Output binary
OUTPUT="${COMMON}/bin/configmgr"
rm -f "${OUTPUT}"

LIBYAML_SRC="${DEPS_DESTINATION}/${LIBYAML}/src"
LIBYAML_INC="${DEPS_DESTINATION}/${LIBYAML}/include"
QJS="${DEPS_DESTINATION}/${QUICKJS}"

# Allow CC override; default to gcc.
CC="${CC:-gcc}"

# -------------------------------------------------------------------------------
# Compiler flags
# -------------------------------------------------------------------------------
# -DCMGRTEST=1          : enables the main() function in configmgr.c
# -DCONFIG_BIGNUM=1     : matches z/OS build; enables libbf inside QuickJS
# -DCONFIG_VERSION=...  : timestamp string embedded in the binary
# -DYAML_DECLARE_STATIC : link libyaml statically (no shared library needed)
# -D_GNU_SOURCE         : exposes glibc extensions (POSIX + GNU extensions)
#                         required for realpath(3), getline(3), etc.
# -------------------------------------------------------------------------------

CFLAGS="-std=gnu11 -Wall -Wno-unused-function -Wno-unused-variable"
CFLAGS="$CFLAGS -g -O2 -I${COMMON}/h -I${COMMON}/platform/posix"
CFLAGS="$CFLAGS -I${LIBYAML_INC} -I${QJS}"
CFLAGS="$CFLAGS -D_GNU_SOURCE"
CFLAGS="$CFLAGS -DCMGRTEST=1"

# Pre-include <sys/types.h> before quickjs.c so that ssize_t is already
# declared by the system headers.  The C11 standard allows compatible typedef
# redeclarations, so GCC in gnu11 mode will accept the internal quickjs typedef
# without error once the system type is already in scope.
QJS_EXTRA_CFLAGS="-include ${COMMON}/platform/posix/quickjs_linux_compat.h"

CFLAGS="$CFLAGS -DCONFIG_BIGNUM=1"
CFLAGS="$CFLAGS -DCONFIG_VERSION=\"2021-03-27\""
CFLAGS="$CFLAGS -DYAML_VERSION_MAJOR=${MAJOR}"
CFLAGS="$CFLAGS -DYAML_VERSION_MINOR=${MINOR}"
CFLAGS="$CFLAGS -DYAML_VERSION_PATCH=${PATCH}"
CFLAGS="$CFLAGS -DYAML_VERSION_STRING=\"${MAJOR}.${MINOR}.${PATCH}\""
CFLAGS="$CFLAGS -DYAML_DECLARE_STATIC=1"

# libyaml sources
LIBYAML_SRCS="
  ${LIBYAML_SRC}/api.c
  ${LIBYAML_SRC}/reader.c
  ${LIBYAML_SRC}/scanner.c
  ${LIBYAML_SRC}/parser.c
  ${LIBYAML_SRC}/loader.c
  ${LIBYAML_SRC}/writer.c
  ${LIBYAML_SRC}/emitter.c
  ${LIBYAML_SRC}/dumper.c
"

# QuickJS sources (portable fork; no z/OS polyfill needed on Linux)
QJS_SRCS="
  ${QJS}/cutils.c
  ${QJS}/quickjs.c
  ${QJS}/quickjs-libc.c
  ${QJS}/libunicode.c
  ${QJS}/libbf.c
  ${QJS}/libregexp.c
"

# zowe-common-c sources required by configmgr on POSIX (non-z/OS) platforms.
# zosfile.c and zos.c are z/OS-only; we replace them with the POSIX file
# implementation (posixfile.c) and the z/OS module stubs (stub_zos_modules.c).
#
# pdsutil.c is guarded by #ifdef __ZOWE_OS_ZOS in configmgr.c so it is safe
# to omit entirely.
#
# qjszos.c and qjsnet.c use z/OS-specific headers that do not exist on Linux;
# stub_zos_modules.c provides no-op replacements for their module initializers.
#
# tls.c, http.c, httpclient.c, bpxskt.c, socketmgmt.c, fdpoll.c, jcsi.c
# introduce dependencies on TLS / GSKit / socket primitives that are not
# needed for configmgr's core YAML-reading / schema-validation path.  We
# therefore omit those modules; they can be re-enabled for future use along
# with the appropriate -lssl / -lz flags.
COMMON_SRCS="
  ${COMMON}/c/alloc.c
  ${COMMON}/c/charsets.c
  ${COMMON}/c/collections.c
  ${COMMON}/c/configmgr.c
  ${COMMON}/c/embeddedjs.c
  ${COMMON}/c/json.c
  ${COMMON}/c/jsonschema.c
  ${COMMON}/c/logging.c
  ${COMMON}/c/microjq.c
  ${COMMON}/c/parsetools.c
  ${COMMON}/c/timeutls.c
  ${COMMON}/c/utils.c
  ${COMMON}/c/xlate.c
  ${COMMON}/c/yaml2json.c
  ${COMMON}/platform/posix/psxregex.c
  ${COMMON}/platform/posix/posixfile.c
  ${COMMON}/platform/posix/stub_zos_modules.c
"

echo "Compiling configmgr..."

# Build a temporary directory for intermediate objects
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

# Compile QuickJS sources separately with the extra flags that pre-include
# <sys/types.h> to ensure ssize_t is already declared before quickjs.c's
# internal typedef.
QJS_OBJS=""
for src in ${QJS_SRCS}; do
  base=$(basename "$src" .c)
  obj="${TMP_DIR}/${base}.o"
  # shellcheck disable=SC2086
  ${CC} ${CFLAGS} ${QJS_EXTRA_CFLAGS} -c -o "${obj}" "${src}"
  rc=$?
  if [ "${rc}" -ne 0 ]; then
    echo "Failed to compile ${src} (rc=${rc})"
    exit ${rc}
  fi
  QJS_OBJS="${QJS_OBJS} ${obj}"
done

# Compile libyaml sources
YAML_OBJS=""
for src in ${LIBYAML_SRCS}; do
  base=$(basename "$src" .c)
  obj="${TMP_DIR}/${base}.o"
  # shellcheck disable=SC2086
  ${CC} ${CFLAGS} -c -o "${obj}" "${src}"
  rc=$?
  if [ "${rc}" -ne 0 ]; then
    echo "Failed to compile ${src} (rc=${rc})"
    exit ${rc}
  fi
  YAML_OBJS="${YAML_OBJS} ${obj}"
done

# Compile common zowe-common-c and configmgr sources
COMMON_OBJS=""
for src in ${COMMON_SRCS}; do
  base=$(basename "$src" .c)
  # Append PID to guard against name collisions across directories
  obj="${TMP_DIR}/${base}_$$.o"
  # shellcheck disable=SC2086
  ${CC} ${CFLAGS} -c -o "${obj}" "${src}"
  rc=$?
  if [ "${rc}" -ne 0 ]; then
    echo "Failed to compile ${src} (rc=${rc})"
    exit ${rc}
  fi
  COMMON_OBJS="${COMMON_OBJS} ${obj}"
done

echo "Linking..."
# Link with -lm for math functions used by QuickJS (libbf) and jsonschema.
# iconv is part of glibc on most Linux distributions and does not require a
# separate -liconv flag.  If you are building on a musl-libc system (e.g.
# Alpine Linux) and the link fails with undefined references to iconv_open,
# add -liconv here.
# shellcheck disable=SC2086
${CC} ${CFLAGS} -o "${OUTPUT}" ${QJS_OBJS} ${YAML_OBJS} ${COMMON_OBJS} -lm
rc=$?

if [ "${rc}" -eq 0 ]; then
  echo "Build successful: ${OUTPUT}"
else
  echo "Build failed (rc=${rc})"
fi

exit $rc
