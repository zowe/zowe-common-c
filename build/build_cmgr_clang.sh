#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.

# build_cmgr_clang.sh
#
# Modernized build of the Zowe configmgr for the ibm-clang64 / Open XL C/C++
# (Clang-based) compiler family. Successor to build_cmgr_xlclang.sh. The old
# xlclang script is preserved during migration; this one runs in parallel.
#
# Modes (auto-detected by uname -s; override with first positional arg):
#   zos         native z/OS 3.1+ build via ibm-clang64. Produces an executable.
#   zos-cross   cross-compile from Linux/WSL using upstream clang with
#               -target s390x-ibm-zos. Emits GOFF .o files. No link step (the
#               cross sysroot has no side-decks and no binder). Objects can be
#               shipped to z/OS and fed to the binder there.
#   linux       native Linux/WSL build. [PLACEHOLDER] Configmgr-on-Linux is a
#               long-standing Zowe backlog item; significant source porting is
#               required before a working binary exists. This mode currently
#               exits with a diagnostic and no artifacts.
#
# Env overrides (zos-cross specifically):
#   CC                 compiler binary (default: auto-detected; overrides all probes)
#   XLLVM_CLANG        path to a clang built from upstream llvm-project with
#                      s390x-ibm-zos target support. If unset, the script probes:
#                        1. "clang" on PATH (if it can emit GOFF .o)
#                        2. "$HOME/llvm-project/build/bin/clang"
#                      If none works, errors with instructions.
#   ZOS_SYSROOT        path to a z/OS sysroot containing usr/include.
#                      If unset, the script probes:
#                        1. "$HOME/zcross/sysroot"
#                        2. "$HOME/git2026/zcross/sysroot"
#                        3. "/opt/zos-sysroot"
#                      If none exists, errors with instructions.
#
# Env overrides (any mode):
#   ZWE_CLANG_FLAGS    extra flags appended to every compile command (parallel
#                      to ZWE_XLCLANG_FLAGS used by build_cmgr_xlclang.sh)
#   KEEP_TMP           set to any value to preserve the build directory after
#                      exit (useful on zos-cross for shipping objects to z/OS)

set -e

WORKING_DIR=$(cd $(dirname "$0") && pwd)
COMMON="$WORKING_DIR/.."

. $WORKING_DIR/configmgr.proj.env

# -- mode selection --
MODE="$1"
if [ -z "$MODE" ]; then
  case "$(uname -s 2>/dev/null)" in
    OS/390|z/OS)  MODE=zos ;;
    Linux|Darwin) MODE=zos-cross ;;
    *)            echo "Unknown uname '$(uname -s)'; pass mode explicitly: zos|zos-cross|linux" >&2; exit 1 ;;
  esac
fi

echo "********************************************************************************"
echo "Building $PROJECT (mode: $MODE)"

# -- mode-specific prereq discovery (runs before any dep clone so failure is fast) --
#
# zos: nothing to probe here we assume ibm-clang64 is on PATH on z/OS 3.1+.
#      If it isn't, the compile invocation will error cleanly.
# zos-cross: probe for a cross-capable clang and a z/OS sysroot.

zos_cross_probe_compiler() {
  # Returns 0 iff $1 is a clang capable of real s390x-ibm-zos cross compilation.
  # We require two things:
  #   (a) it accepts -fzos-extensions (the z/OS C dialect flag, added to
  #       upstream LLVM relatively recently; absent on e.g. Ubuntu clang-18),
  #   (b) it emits a true GOFF object (first two bytes 0x03 0xF0), not ELF.
  #       older clangs with partial SystemZ support still accept the triple
  #       but produce ELF for s390x-linux-style output.
  candidate="$1"
  [ -n "$candidate" ] || return 1
  command -v "$candidate" >/dev/null 2>&1 || [ -x "$candidate" ] || return 1
  tmpo=$(mktemp -t zoscross_probe.XXXXXX.o 2>/dev/null) || return 1
  if printf 'int _probe(void){return 0;}\n' | \
     "$candidate" -target s390x-ibm-zos -fzos-extensions -c -xc - -o "$tmpo" \
       >/dev/null 2>&1 && [ -s "$tmpo" ]; then
    magic=$(od -An -N2 -tx1 "$tmpo" 2>/dev/null | tr -d ' \n')
    rm -f "$tmpo"
    [ "$magic" = "03f0" ] && return 0
  fi
  rm -f "$tmpo"
  return 1
}

if [ "$MODE" = "zos-cross" ]; then
  # 1) compiler discovery
  if [ -n "${CC:-}" ]; then
    zos_cross_probe_compiler "$CC" || {
      echo "zos-cross: \$CC='$CC' cannot cross-compile for s390x-ibm-zos." >&2
      exit 1
    }
  elif [ -n "${XLLVM_CLANG:-}" ] && zos_cross_probe_compiler "$XLLVM_CLANG"; then
    CC="$XLLVM_CLANG"
  elif zos_cross_probe_compiler "clang"; then
    CC="clang"
  elif [ -n "${HOME:-}" ] && zos_cross_probe_compiler "$HOME/llvm-project/build/bin/clang"; then
    CC="$HOME/llvm-project/build/bin/clang"
  else
    cat >&2 <<EOF
zos-cross: no cross-capable clang found.

Tried (in order):
  - \$CC (unset)
  - \$XLLVM_CLANG (${XLLVM_CLANG:-unset})
  - clang on PATH
  - \$HOME/llvm-project/build/bin/clang (${HOME:-?}/llvm-project/build/bin/clang)

You need a clang built from upstream llvm-project with the SystemZ / s390x
target enabled. Either install one on PATH or set
  export XLLVM_CLANG=/path/to/your/clang
and re-run.
EOF
    exit 1
  fi

  # 2) sysroot discovery
  if [ -z "${ZOS_SYSROOT:-}" ] && [ -n "${HOME:-}" ]; then
    for cand in "$HOME/zcross/sysroot" "$HOME/git2026/zcross/sysroot" "/opt/zos-sysroot"; do
      if [ -d "$cand/usr/include" ]; then
        ZOS_SYSROOT="$cand"
        break
      fi
    done
  fi
  if [ -z "${ZOS_SYSROOT:-}" ] || [ ! -d "${ZOS_SYSROOT}/usr/include" ]; then
    cat >&2 <<EOF
zos-cross: no z/OS sysroot found.

Tried:
  - \$ZOS_SYSROOT (${ZOS_SYSROOT:-unset})
  - \$HOME/zcross/sysroot
  - \$HOME/git2026/zcross/sysroot
  - /opt/zos-sysroot

Export ZOS_SYSROOT=<dir> where <dir>/usr/include mirrors a z/OS system's
/usr/include tree. A zcross-style sysroot checkout is the typical source.
EOF
    exit 1
  fi

  echo "zos-cross: CC=$CC"
  echo "zos-cross: ZOS_SYSROOT=$ZOS_SYSROOT"
fi

# -- dependency fetch (QuickJS, libyaml) --
. $WORKING_DIR/dependencies.sh
check_dependencies "${COMMON}" "$WORKING_DIR/configmgr.proj.env"
DEPS_DESTINATION=$(get_destination "${COMMON}" "${PROJECT}")

# -- version split for libyaml defines --
OLDIFS=$IFS
IFS="."
for part in ${VERSION}; do
  if   [ -z "$MAJOR" ]; then MAJOR=$part
  elif [ -z "$MINOR" ]; then MINOR=$part
  else                        PATCH=$part
  fi
done
IFS=$OLDIFS
VERSION="\"${VERSION}\""

# -- per-mode compiler + flag matrix --
#
# Flag translation (xlclang -> ibm-clang64 / upstream clang with s390x-ibm-zos):
#   -q64                              -> -m64 (implicit on zos-cross; s390x is LP64)
#   -qascii                           -> -fzos-le-char-mode=ascii + -fexec-charset=ISO8859-1
#                                        (applied to libs stage only; see charset split below)
#   (default, no -qascii)             -> -fzos-le-char-mode=ebcdic + -fexec-charset=IBM-1047
#                                        (applied to main zowe-common-c stage)
#   -Wc,float(ieee)                   -> -mzos-float-kind=ieee (implicit on zos-cross)
#   -Wc,longname                      -> dropped; default in clang
#   -Wc,langlvl(extc99)               -> -std=gnu99 (+ -D_EXT via zos_cross_compat.h / below)
#   -Wc,gonum                         -> dropped; no upstream equivalent (-g covers DWARF)
#   -Wc,goff                          -> dropped; Open XL / cross always emit GOFF
#   -Wc,ASM                           -> -fasm
#   -Wc,asmlib('CEE.SCEEMAC',...)     -> repeated -mzos-asmlib=//'DATASET' flags
#                                        (Open XL 2.2 requires // dataset syntax and one flag per library;
#                                         comma-separated values are silently ignored with a warning)
#                                        (zos only; no upstream equivalent on cross, and
#                                         configmgr has no inline HLASM anyway)
#
# Charset staging: the xlclang build compiles the OSS deps (libyaml + QuickJS)
# with -qascii (ISO-8859-1 literals, ASCII LE char mode) and the zowe-common-c
# sources WITHOUT -qascii (IBM-1047 literals, EBCDIC LE char mode). That split
# is preserved here per stage via LIB_CHARSET_CFLAGS vs MAIN_CHARSET_CFLAGS.
#
# Upstream-clang caveat (zos-cross): the -fzos-le-char-mode driver option is
# NOT exposed by upstream LLVM; clang hardcodes the module flag to "ascii"
# (see llvm-project clang/lib/CodeGen/CodeGenModule.cpp). So on zos-cross,
# every TU's module flag is "ascii" regardless of our intent for stage 2.
# -fexec-charset still works, so literal encoding is correct. This makes
# zos-cross good for compile verification but unsuitable for producing .o
# files that should be linked with ibm-clang64 stage-2 EBCDIC objects on z/OS.
#
# Listing/diagnostic options (-qlist etc.) are intentionally not translated.
# Open XL 2.2's -mzos-listing=json tooling is weak; improvements expected in
# 2.3. Revisit then.

case "$MODE" in
  zos)
    CC="${CC:-ibm-clang64}"
    BASE_CFLAGS="-m64 \
      -mzos-float-kind=ieee \
      -std=gnu99 \
      -fasm \
      -mzos-asmlib=//'CEE.SCEEMAC' \
      -mzos-asmlib=//'SYS1.MACLIB' \
      -mzos-asmlib=//'SYS1.MODGEN' \
      -mzos-no-asm-implicit-clobber-reg \
      -Wno-trigraphs \
      -D_EXT=1"
    LIB_CHARSET_CFLAGS="-fzos-le-char-mode=ascii -fexec-charset=ISO8859-1"
    MAIN_CHARSET_CFLAGS="-fzos-le-char-mode=ebcdic -fexec-charset=IBM-1047"
    GSKDIR=/usr/lpp/gskssl
    GSKINC="${GSKDIR}/include"
    LINK_OBJS_SSL="${GSKDIR}/lib/GSKSSL64.x ${GSKDIR}/lib/GSKCMS64.x"
    DO_LINK=1
    ;;
  zos-cross)
    # CC and ZOS_SYSROOT were already discovered / validated above.
    SHIM="$WORKING_DIR/zos_cross_compat.h"
    BASE_CFLAGS="-target s390x-ibm-zos \
      -fzos-extensions \
      -mzos-sys-include=${ZOS_SYSROOT}/usr/include \
      -include ${SHIM} \
      -std=gnu99 \
      -fasm"
    # Upstream clang hardcodes LE char mode to "ascii" and -fexec-charset to
    # only accept UTF-8 (rejects ISO8859-1, IBM-1047, etc.). ibm-clang64 is
    # more flexible. On zos-cross we accept UTF-8 literals for both stages
    # since this mode is for compile verification, not for producing .o files
    # that should be binary-merged with z/OS ibm-clang64 output.
    LIB_CHARSET_CFLAGS="-fexec-charset=UTF-8"
    MAIN_CHARSET_CFLAGS="-fexec-charset=UTF-8"
    # Cross sysroot bundles the z/OS /usr/include layout, GSK headers included.
    GSKINC="${ZOS_SYSROOT}/usr/include"
    LINK_OBJS_SSL=""
    DO_LINK=0
    ;;
  linux)
    # Native Linux/WSL build. Produces an executable at ${COMMON}/bin/configmgr.
    # No z/OS-isms: no -fzos-*, no GSK SSL, no LE, no BPX. TLS is disabled in
    # this build (USE_ZOWE_TLS is NOT defined) until an OpenSSL backend lands.
    CC="${CC:-clang}"
    if ! command -v "$CC" >/dev/null 2>&1; then
      echo "linux: compiler not found: $CC" >&2
      echo "       set CC=<your clang or gcc> and retry." >&2
      exit 1
    fi
    # _GNU_SOURCE unlocks glibc extensions QuickJS depends on (environ,
    # sighandler_t, tm_gmtoff, st_{a,m,c}tim, etc).
    BASE_CFLAGS="-D__ZOWE_OS_LINUX=1 -D_GNU_SOURCE=1 -std=gnu99"
    # Charsets not needed on Linux source is UTF-8, execution charset is UTF-8.
    LIB_CHARSET_CFLAGS=""
    MAIN_CHARSET_CFLAGS=""
    GSKINC="/nonexistent"
    LINK_OBJS_SSL=""
    DO_LINK=1
    ;;
  *)
    echo "Unknown mode '$MODE'. Expected: zos | zos-cross | linux" >&2
    exit 1
    ;;
esac

# -- scratch directory for .o files --
date_stamp=$(date +%Y%m%d%S)
TMP_DIR="${WORKING_DIR}/tmp-${date_stamp}-${MODE}"
mkdir -p "${TMP_DIR}" && cd "${TMP_DIR}"
trap 'cd "${WORKING_DIR}"; [ -z "${KEEP_TMP:-}" ] && rm -rf "${TMP_DIR}"' EXIT

rm -f "${COMMON}/bin/configmgr" 2>/dev/null || true

# -- stage 1: compile libyaml + QuickJS objects --
LIB_DEFS="\
  -DYAML_VERSION_MAJOR=${MAJOR} \
  -DYAML_VERSION_MINOR=${MINOR} \
  -DYAML_VERSION_PATCH=${PATCH} \
  -DYAML_VERSION_STRING=${VERSION} \
  -DYAML_DECLARE_STATIC=1 \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -D_LONG_LONG=1 \
  -D__EXTENDED__=1 \
  -D_LARGE_TIME_API=1 \
  -DCONFIG_BIGNUM=1 \
  -DCONFIG_VERSION=\"2021-03-27\""

LIB_INCLUDES="\
  -I ${DEPS_DESTINATION}/${LIBYAML}/include \
  -I ${DEPS_DESTINATION}/${QUICKJS}"

LIB_SOURCES="\
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
  ${DEPS_DESTINATION}/${QUICKJS}/libbf.c \
  ${DEPS_DESTINATION}/${QUICKJS}/libregexp.c"

# QuickJS porting/polyfill.c is a z/OS polyfill layer; Linux doesn't need it.
if [ "$MODE" != "linux" ]; then
  LIB_SOURCES="$LIB_SOURCES ${DEPS_DESTINATION}/${QUICKJS}/porting/polyfill.c"
fi

echo "Compiling libraries with $CC (ASCII / ISO-8859-1)"
$CC -c \
  $BASE_CFLAGS \
  $LIB_CHARSET_CFLAGS \
  $LIB_DEFS \
  $LIB_INCLUDES \
  ${ZWE_CLANG_FLAGS:-} \
  $LIB_SOURCES
echo "Libraries compiled successfully"

# -- stage 2: zowe-common-c translation units (+ link on zos) --
if [ "$MODE" = "linux" ]; then
  # On Linux: no GSK SSL (no TLS in this build yet), no LE, no BPX.
  MAIN_DEFS="\
    -DNOIBMHTTP=1 \
    -DCMGRTEST=1"
  # No GSKINC include on Linux.
  MAIN_INCLUDES="\
    -I ${COMMON}/h \
    -I ${COMMON}/platform/posix \
    -I ${DEPS_DESTINATION}/${LIBYAML}/include \
    -I ${DEPS_DESTINATION}/${QUICKJS}"
  # Drop z/OS-only translation units: bpxskt, jcsi, le, pdsutil, qjszos,
  # recovery, tls (GSK), zos, zosfile, scheduling (LE RLE tasks), qjsnet
  # (has an explicit #error on non-z/OS).
  MAIN_SOURCES="\
    ${COMMON}/c/alloc.c \
    ${COMMON}/c/charsets.c \
    ${COMMON}/c/collections.c \
    ${COMMON}/c/configmgr.c \
    ${COMMON}/c/embeddedjs.c \
    ${COMMON}/c/fdpoll.c \
    ${COMMON}/c/json.c \
    ${COMMON}/c/jsonschema.c \
    ${COMMON}/c/logging.c \
    ${COMMON}/c/microjq.c \
    ${COMMON}/c/parsetools.c \
    ${COMMON}/platform/posix/psxfile.c \
    ${COMMON}/platform/posix/psxregex.c \
    ${COMMON}/c/psxskt.c \
    ${COMMON}/c/socketmgmt.c \
    ${COMMON}/c/timeutls.c \
    ${COMMON}/c/utils.c \
    ${COMMON}/c/xlate.c \
    ${COMMON}/c/yaml2json.c"
else
  MAIN_DEFS="\
    -D_OPEN_SYS_FILE_EXT=1 \
    -D_XOPEN_SOURCE=600 \
    -D_OPEN_THREADS=1 \
    -DNOIBMHTTP=1 \
    -DUSE_ZOWE_TLS=1 \
    -DNEW_CAA_LOCATIONS=1 \
    -DCMGRTEST=1"
  MAIN_INCLUDES="\
    -I ${COMMON}/h \
    -I ${COMMON}/platform/posix \
    -I ${GSKINC} \
    -I ${DEPS_DESTINATION}/${LIBYAML}/include \
    -I ${DEPS_DESTINATION}/${QUICKJS}"
  MAIN_SOURCES="\
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
    ${COMMON}/c/scheduling.c \
    ${COMMON}/c/socketmgmt.c \
    ${COMMON}/c/timeutls.c \
    ${COMMON}/c/tls.c \
    ${COMMON}/c/utils.c \
    ${COMMON}/c/xlate.c \
    ${COMMON}/c/yaml2json.c \
    ${COMMON}/c/zos.c \
    ${COMMON}/c/zosfile.c"
fi

if [ "$DO_LINK" = "1" ]; then
  mkdir -p "${COMMON}/bin"
  # The libyaml/QuickJS objects glob covers both zos (includes polyfill.o) and
  # linux (no polyfill.o). *.o picks up whatever stage 1 produced.
  LIB_OBJS="*.o"
  case "$MODE" in
    linux)
      echo "Building configmgr (compile + link, linux) with $CC"
      $CC \
        $BASE_CFLAGS \
        $MAIN_CHARSET_CFLAGS \
        $MAIN_DEFS \
        $MAIN_INCLUDES \
        ${ZWE_CLANG_FLAGS:-} \
        -o "${COMMON}/bin/configmgr" \
        ${LIB_OBJS} \
        $MAIN_SOURCES \
        -lpthread -lm -ldl
      ;;
    *)
      echo "Building configmgr (compile + link, EBCDIC / IBM-1047) with $CC"
      $CC \
        $BASE_CFLAGS \
        $MAIN_CHARSET_CFLAGS \
        $MAIN_DEFS \
        $MAIN_INCLUDES \
        ${ZWE_CLANG_FLAGS:-} \
        -o "${COMMON}/bin/configmgr" \
        ${LIB_OBJS} \
        $MAIN_SOURCES \
        $LINK_OBJS_SSL
      ;;
  esac
else
  echo "Compiling configmgr translation units (mode $MODE, EBCDIC / IBM-1047, compile-only)"
  $CC -c \
    $BASE_CFLAGS \
    $MAIN_CHARSET_CFLAGS \
    $MAIN_DEFS \
    $MAIN_INCLUDES \
    ${ZWE_CLANG_FLAGS:-} \
    $MAIN_SOURCES
  obj_count=$(ls -1 *.o 2>/dev/null | wc -l | tr -d ' ')
  echo "Produced ${obj_count} GOFF object files in ${TMP_DIR}"
  echo "Set KEEP_TMP=1 to preserve them for shipping to z/OS."
fi

echo "Build successful"
