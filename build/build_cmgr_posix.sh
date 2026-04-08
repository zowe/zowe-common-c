#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.

# Build script for configmgr on POSIX platforms: macOS, Linux, and BSD.
#
# The platform is auto-detected via 'uname -s'.  Defaults can be overridden:
#
#   CC=<compiler>          Set the C compiler (e.g. CC=clang-18).
#   CXX=<compiler>         Set the C++ compiler (only used on macOS/BSD for
#                          the Windows regex wrapper; not needed here).
#   --platform <name>      Force a platform instead of auto-detecting.
#                          Accepted values: darwin, linux, freebsd, openbsd,
#                          netbsd  (case-insensitive).
#   --cc <compiler>        Same as setting CC= in the environment.
#   -h, --help             Show this message.
#
# Usage examples:
#   ./build_cmgr_posix.sh
#   CC=clang ./build_cmgr_posix.sh
#   ./build_cmgr_posix.sh --cc gcc-14
#   ./build_cmgr_posix.sh --platform linux   # force Linux mode, e.g. on WSL

# ---------------------------------------------------------------------------
# Parse arguments
# ---------------------------------------------------------------------------
FORCE_PLATFORM=""
FORCE_CC=""

while [ $# -gt 0 ]; do
  case "$1" in
    --platform) FORCE_PLATFORM="$2"; shift 2 ;;
    --cc)       FORCE_CC="$2";       shift 2 ;;
    -h|--help)
      echo "Usage: $0 [--platform darwin|linux|freebsd|openbsd|netbsd] [--cc <compiler>]"
      exit 0
      ;;
    *)
      echo "Unknown argument: $1"
      echo "Usage: $0 [--platform darwin|linux|freebsd|openbsd|netbsd] [--cc <compiler>]"
      exit 1
      ;;
  esac
done

WORKING_DIR=$(cd "$(dirname "$0")" && pwd)
COMMON="$WORKING_DIR/.."

# Load project info: PROJECT, VERSION, QUICKJS, LIBYAML, etc.
. "$WORKING_DIR/configmgr.proj.env"

# Resolve and possibly clone dependencies (libyaml, quickjs).
. "$WORKING_DIR/dependencies.sh"
check_dependencies "${COMMON}" "$WORKING_DIR/configmgr.proj.env"
DEPS_DESTINATION=$(get_destination "${COMMON}" "${PROJECT}")

# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------
UNAME=$(uname -s 2>/dev/null | tr '[:upper:]' '[:lower:]')
PLATFORM="${FORCE_PLATFORM:-$UNAME}"

case "$PLATFORM" in
  darwin|macos)
    PLATFORM_LABEL="macOS"
    CC="${FORCE_CC:-${CC:-clang}}"
    # Expose BSD/Darwin extensions required by some headers.
    PLATFORM_CFLAGS="-D_DARWIN_C_SOURCE"
    # iconv is a separate library on macOS.
    LINK_LIBS="-lm -liconv"
    ;;
  linux)
    PLATFORM_LABEL="Linux"
    CC="${FORCE_CC:-${CC:-gcc}}"
    # Expose glibc extensions required for realpath(3), getline(3), etc.
    # On musl-libc systems (Alpine, etc.) this is harmless; add -liconv to
    # LINK_LIBS if the link fails with undefined references to iconv_open.
    PLATFORM_CFLAGS="-D_GNU_SOURCE"
    LINK_LIBS="-lm"
    ;;
  freebsd|openbsd|netbsd|dragonfly)
    PLATFORM_LABEL="BSD (${PLATFORM})"
    CC="${FORCE_CC:-${CC:-clang}}"
    # BSD systems are POSIX by default; no extra -D needed.
    PLATFORM_CFLAGS=""
    # iconv is part of the base system on all major BSDs.
    LINK_LIBS="-lm"
    ;;
  *)
    echo "Unsupported platform: '${PLATFORM}'"
    echo "Use --platform darwin|linux|freebsd|openbsd|netbsd to specify manually."
    exit 1
    ;;
esac

# ---------------------------------------------------------------------------
# QuickJS ssize_t compatibility
#
# quickjs.c has a bare 'typedef int ssize_t' fallback that clang flags as a
# redefinition error when the type is already declared by the system headers.
# We pre-include platform/common/quickjs_posix_compat.h (which pulls in
# <sys/types.h>) so ssize_t is already in scope.  When the compiler is
# clang-family we also suppress the resulting compatible-redeclaration
# diagnostic; GCC accepts it silently in gnu11 mode.
# ---------------------------------------------------------------------------
QJS_EXTRA_CFLAGS="-include ${COMMON}/platform/posix/quickjs_posix_compat.h"
case "$CC" in
  *clang*)
    QJS_EXTRA_CFLAGS="$QJS_EXTRA_CFLAGS -Wno-typedef-redefinition -Wno-error=typedef-redefinition"
    ;;
esac

echo "********************************************************************************"
echo "Building $PROJECT for ${PLATFORM_LABEL} with CC=${CC}..."

# ---------------------------------------------------------------------------
# Split VERSION (e.g. "3.5.0") into MAJOR / MINOR / PATCH
# ---------------------------------------------------------------------------
OLDIFS=$IFS; IFS="."
MAJOR=""; MINOR=""; PATCH=""
for part in ${VERSION}; do
  if   [ -z "$MAJOR" ]; then MAJOR=$part
  elif [ -z "$MINOR" ]; then MINOR=$part
  else                        PATCH=$part
  fi
done
IFS=$OLDIFS

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
OUTPUT="${COMMON}/bin/configmgr"
rm -f "${OUTPUT}"

LIBYAML_SRC="${DEPS_DESTINATION}/${LIBYAML}/src"
LIBYAML_INC="${DEPS_DESTINATION}/${LIBYAML}/include"
QJS="${DEPS_DESTINATION}/${QUICKJS}"

# ---------------------------------------------------------------------------
# Compiler flags
#
# -DCMGRTEST=1          enables the main() entry point in configmgr.c
# -DCONFIG_BIGNUM=1     enables libbf inside QuickJS (matches z/OS build)
# -DCONFIG_VERSION=...  timestamp string embedded in the binary
# -DYAML_DECLARE_STATIC links libyaml statically
# ---------------------------------------------------------------------------
CFLAGS="-std=gnu11 -Wall -Wno-unused-function -Wno-unused-variable"
CFLAGS="$CFLAGS -g -O2"
CFLAGS="$CFLAGS -I${COMMON}/h -I${COMMON}/platform/posix"
CFLAGS="$CFLAGS -I${LIBYAML_INC} -I${QJS}"
CFLAGS="$CFLAGS ${PLATFORM_CFLAGS}"
CFLAGS="$CFLAGS -DCMGRTEST=1"
CFLAGS="$CFLAGS -DCONFIG_BIGNUM=1"
CFLAGS="$CFLAGS -DCONFIG_VERSION=\"2021-03-27\""
CFLAGS="$CFLAGS -DYAML_VERSION_MAJOR=${MAJOR}"
CFLAGS="$CFLAGS -DYAML_VERSION_MINOR=${MINOR}"
CFLAGS="$CFLAGS -DYAML_VERSION_PATCH=${PATCH}"
CFLAGS="$CFLAGS -DYAML_VERSION_STRING=\"${MAJOR}.${MINOR}.${PATCH}\""
CFLAGS="$CFLAGS -DYAML_DECLARE_STATIC=1"

# ---------------------------------------------------------------------------
# Source lists
# ---------------------------------------------------------------------------
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

QJS_SRCS="
  ${QJS}/cutils.c
  ${QJS}/quickjs.c
  ${QJS}/quickjs-libc.c
  ${QJS}/libunicode.c
  ${QJS}/libbf.c
  ${QJS}/libregexp.c
"

# Omitted intentionally (see build_cmgr_xlclang.sh for the full z/OS list):
#   zosfile.c / zos.c / pdsutil.c  - z/OS only
#   qjszos.c / qjsnet.c            - ifdef'd out in embeddedjs.c via __ZOWE_OS_ZOS
#   tls.c / http*.c / bpxskt.c / socketmgmt.c / fdpoll.c / jcsi.c
#                                  - not needed for YAML/schema core
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
"

echo "Compiling configmgr..."

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

# Compile QuickJS (with the ssize_t compat pre-include and any clang suppressions)
QJS_OBJS=""
for src in ${QJS_SRCS}; do
  base=$(basename "$src" .c)
  obj="${TMP_DIR}/${base}.o"
  # shellcheck disable=SC2086
  ${CC} ${CFLAGS} ${QJS_EXTRA_CFLAGS} -c -o "${obj}" "${src}"
  rc=$?; [ $rc -ne 0 ] && echo "Failed: ${src} (rc=${rc})" && exit $rc
  QJS_OBJS="${QJS_OBJS} ${obj}"
done

# Compile libyaml
YAML_OBJS=""
for src in ${LIBYAML_SRCS}; do
  base=$(basename "$src" .c)
  obj="${TMP_DIR}/${base}.o"
  # shellcheck disable=SC2086
  ${CC} ${CFLAGS} -c -o "${obj}" "${src}"
  rc=$?; [ $rc -ne 0 ] && echo "Failed: ${src} (rc=${rc})" && exit $rc
  YAML_OBJS="${YAML_OBJS} ${obj}"
done

# Compile zowe-common-c / configmgr sources
COMMON_OBJS=""
for src in ${COMMON_SRCS}; do
  base=$(basename "$src" .c)
  obj="${TMP_DIR}/${base}_$$.o"   # PID suffix avoids name collisions
  # shellcheck disable=SC2086
  ${CC} ${CFLAGS} -c -o "${obj}" "${src}"
  rc=$?; [ $rc -ne 0 ] && echo "Failed: ${src} (rc=${rc})" && exit $rc
  COMMON_OBJS="${COMMON_OBJS} ${obj}"
done

echo "Linking..."
# shellcheck disable=SC2086
${CC} ${CFLAGS} -o "${OUTPUT}" ${QJS_OBJS} ${YAML_OBJS} ${COMMON_OBJS} ${LINK_LIBS}
rc=$?

if [ $rc -eq 0 ]; then
  echo "Build successful: ${OUTPUT}"
else
  echo "Build failed (rc=${rc})"
fi
exit $rc
