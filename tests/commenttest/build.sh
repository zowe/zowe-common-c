#!/bin/sh
#
# build.sh - build the YAML comment round-trip test under any of the
# three supported toolchains:
#
#   xlclang        z/OS xlclang. Two-phase: iconv libyaml ASCII->EBCDIC,
#                  compile with -qascii; then EBCDIC main + link.
#   clang          z/OS ibm-clang64 (Open XL). Two-phase: ISO8859-1
#                  libyaml direct, then EBCDIC main + link.
#   linux          Linux/WSL plain clang/gcc. No EBCDIC anywhere; drops
#                  z/OS-only TUs (bpxskt, le, recovery, scheduling, zos,
#                  zosfile). Links plain libyaml objects.
#
# Each toolchain produces its own binary:
#   commenttest_xlc      (xlclang)
#   commenttest_clang    (ibm-clang64)
#   commenttest_linux    (Linux/WSL)
#
# Usage:
#   cd tests/commenttest
#   sh build.sh            # auto-detect: both z/OS compilers on z/OS, linux on Linux
#   sh build.sh xlclang    # xlclang only
#   sh build.sh clang      # ibm-clang64 only
#   sh build.sh linux      # Linux/WSL only (uses $CC, default clang)
#   sh build.sh both       # both z/OS compilers (xlclang + clang)

WHICH="$1"
if [ -z "$WHICH" ]; then
  case "$(uname -s 2>/dev/null)" in
    OS/390|z/OS)  WHICH=both ;;
    Linux|Darwin) WHICH=linux ;;
    *)            echo "Unknown uname; pass mode explicitly: xlclang|clang|linux|both" >&2; exit 1 ;;
  esac
fi

WORKING_DIR=$(cd "$(dirname "$0")" && pwd)
COMMON="${WORKING_DIR}/../.."
LIBYAML_SRC="${COMMON}/deps/configmgr/libyaml"
H="${COMMON}/h"
PXSX="${COMMON}/platform/posix"

MAJOR=0
MINOR=2
PATCH=5
VERSION="\"${MAJOR}.${MINOR}.${PATCH}\""

YAML_DEFS="-DYAML_VERSION_MAJOR=${MAJOR} \
  -DYAML_VERSION_MINOR=${MINOR} \
  -DYAML_VERSION_PATCH=${PATCH} \
  -DYAML_VERSION_STRING=${VERSION} \
  -DYAML_DECLARE_STATIC=1"

NATIVE_DEFS="-D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1"

# Subset of configmgr's z/OS defines we need: enough to gate le.c's
# libraryFunctionTable into the form ibm-clang64 can compile. We
# deliberately drop USE_ZOWE_TLS because we do not link tls.c.
CMGR_DEFS="-D_EXT=1 -DNOIBMHTTP=1 -DNEW_CAA_LOCATIONS=1 -DCMGRTEST=1"

# libyaml TUs (same set as the original build.sh)
LIBYAML_UNITS="api reader scanner parser loader writer emitter dumper"

# zowe-common-c TUs the test transitively needs on z/OS (same set as the
# original build.sh)
COMMON_C_ZOS="
  ${COMMON}/c/alloc.c
  ${COMMON}/c/bpxskt.c
  ${COMMON}/c/charsets.c
  ${COMMON}/c/collections.c
  ${COMMON}/c/json.c
  ${COMMON}/c/jsonschema.c
  ${COMMON}/c/le.c
  ${COMMON}/c/logging.c
  ${COMMON}/platform/posix/psxregex.c
  ${COMMON}/c/recovery.c
  ${COMMON}/c/scheduling.c
  ${COMMON}/c/timeutls.c
  ${COMMON}/c/utils.c
  ${COMMON}/c/xlate.c
  ${COMMON}/c/yaml2json.c
  ${COMMON}/c/zos.c
  ${COMMON}/c/zosfile.c
"

# Trimmed list for Linux/WSL: drop z/OS-only TUs (bpxskt, le, recovery,
# scheduling, zos, zosfile). Mirrors what build_cmgr_clang.sh's linux
# mode drops from MAIN_SOURCES.
COMMON_C_LINUX="
  ${COMMON}/c/alloc.c
  ${COMMON}/c/charsets.c
  ${COMMON}/c/collections.c
  ${COMMON}/c/json.c
  ${COMMON}/c/jsonschema.c
  ${COMMON}/c/logging.c
  ${COMMON}/platform/posix/psxfile.c
  ${COMMON}/platform/posix/psxregex.c
  ${COMMON}/c/timeutls.c
  ${COMMON}/c/utils.c
  ${COMMON}/c/xlate.c
  ${COMMON}/c/yaml2json.c
"

# --- xlclang ------------------------------------------------------------
build_xlclang() {
  echo
  echo "=========== xlclang ==========="
  TMP_DIR="${WORKING_DIR}/tmp-xlc-$(date +%Y%m%d%S)"
  mkdir -p "${TMP_DIR}/libyaml_src" "${TMP_DIR}/libyaml_inc"

  echo "Phase 0 (xlclang): iconv libyaml sources ASCII -> EBCDIC"
  for f in ${LIBYAML_SRC}/src/*.c ${LIBYAML_SRC}/src/*.h; do
    base=$(basename "$f")
    iconv -f ISO8859-1 -t IBM-1047 "$f" > "${TMP_DIR}/libyaml_src/$base"
  done
  for f in ${LIBYAML_SRC}/include/*.h; do
    base=$(basename "$f")
    iconv -f ISO8859-1 -t IBM-1047 "$f" > "${TMP_DIR}/libyaml_inc/$base"
  done

  echo "Phase 1 (xlclang): libyaml objects (-qascii)"
  cd "${TMP_DIR}"
  LIBYAML_SOURCES=""
  for n in $LIBYAML_UNITS; do
    LIBYAML_SOURCES="$LIBYAML_SOURCES libyaml_src/$n.c"
  done
  # shellcheck disable=SC2086
  xlclang -c -q64 -qascii \
    "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
    $YAML_DEFS $NATIVE_DEFS \
    -I libyaml_inc \
    $LIBYAML_SOURCES
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "xlclang Phase 1 FAILED (rc=$rc)"
    cd "${WORKING_DIR}"
    rm -rf "${TMP_DIR}"
    return $rc
  fi

  echo "Phase 2 (xlclang): test + native sources + link"
  LIBYAML_OBJS=""
  for n in $LIBYAML_UNITS; do
    LIBYAML_OBJS="$LIBYAML_OBJS $n.o"
  done
  # shellcheck disable=SC2086
  xlclang -q64 \
    "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
    $NATIVE_DEFS $CMGR_DEFS \
    -I "$H" -I "$PXSX" -I libyaml_inc \
    -o "${WORKING_DIR}/commenttest_xlc" \
    $LIBYAML_OBJS \
    "${WORKING_DIR}/commenttest.c" \
    $COMMON_C_ZOS
  rc=$?
  cd "${WORKING_DIR}"
  rm -rf "${TMP_DIR}"

  if [ "$rc" -eq 0 ]; then
    echo "xlclang OK -> ${WORKING_DIR}/commenttest_xlc"
  else
    echo "xlclang Phase 2 FAILED (rc=$rc)"
  fi
  return $rc
}

# --- ibm-clang64 --------------------------------------------------------
build_clang() {
  echo
  echo "=========== ibm-clang64 ==========="
  TMP_DIR="${WORKING_DIR}/tmp-clang-$(date +%Y%m%d%S)"
  mkdir -p "${TMP_DIR}"
  cd "${TMP_DIR}"

  IBC_BASE="-m64 \
    -mzos-float-kind=ieee \
    -std=gnu99 \
    -fasm \
    -mzos-asmlib=//'CEE.SCEEMAC' \
    -mzos-asmlib=//'SYS1.MACLIB' \
    -mzos-asmlib=//'SYS1.MODGEN' \
    -mzos-no-asm-implicit-clobber-reg \
    -Wno-trigraphs"
  LIB_CHARSET="-fzos-le-char-mode=ascii -fexec-charset=ISO8859-1"
  MAIN_CHARSET="-fzos-le-char-mode=ebcdic -fexec-charset=IBM-1047"

  echo "Phase 1 (ibm-clang64): libyaml objects (ASCII char mode, ISO8859-1 source direct)"
  LIBYAML_SOURCES=""
  for n in $LIBYAML_UNITS; do
    LIBYAML_SOURCES="$LIBYAML_SOURCES ${LIBYAML_SRC}/src/$n.c"
  done
  # shellcheck disable=SC2086
  ibm-clang64 -c $IBC_BASE $LIB_CHARSET \
    $YAML_DEFS $NATIVE_DEFS \
    -I "${LIBYAML_SRC}/include" \
    $LIBYAML_SOURCES
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ibm-clang64 Phase 1 FAILED (rc=$rc)"
    cd "${WORKING_DIR}"
    rm -rf "${TMP_DIR}"
    return $rc
  fi

  echo "Phase 2 (ibm-clang64): test + native sources + link (EBCDIC main)"
  LIBYAML_OBJS=""
  for n in $LIBYAML_UNITS; do
    LIBYAML_OBJS="$LIBYAML_OBJS $n.o"
  done
  # shellcheck disable=SC2086
  ibm-clang64 $IBC_BASE $MAIN_CHARSET \
    $NATIVE_DEFS $CMGR_DEFS \
    -I "$H" -I "$PXSX" -I "${LIBYAML_SRC}/include" \
    -o "${WORKING_DIR}/commenttest_clang" \
    $LIBYAML_OBJS \
    "${WORKING_DIR}/commenttest.c" \
    $COMMON_C_ZOS
  rc=$?
  cd "${WORKING_DIR}"
  rm -rf "${TMP_DIR}"

  if [ "$rc" -eq 0 ]; then
    echo "ibm-clang64 OK -> ${WORKING_DIR}/commenttest_clang"
  else
    echo "ibm-clang64 Phase 2 FAILED (rc=$rc)"
  fi
  return $rc
}

# --- linux / WSL --------------------------------------------------------
build_linux() {
  echo
  echo "=========== linux ==========="
  CC="${CC:-clang}"
  if ! command -v "$CC" >/dev/null 2>&1; then
    echo "linux: compiler not found: $CC. Set CC=<your clang or gcc> and retry."
    return 1
  fi
  TMP_DIR="${WORKING_DIR}/tmp-linux-$$"
  mkdir -p "${TMP_DIR}"
  cd "${TMP_DIR}"

  LINUX_BASE="-D__ZOWE_OS_LINUX=1 -D_GNU_SOURCE=1 -std=gnu99"

  echo "Phase 1 (linux): libyaml objects"
  LIBYAML_SOURCES=""
  for n in $LIBYAML_UNITS; do
    LIBYAML_SOURCES="$LIBYAML_SOURCES ${LIBYAML_SRC}/src/$n.c"
  done
  # shellcheck disable=SC2086
  $CC -c $LINUX_BASE $YAML_DEFS \
    -I "${LIBYAML_SRC}/include" \
    $LIBYAML_SOURCES
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "linux Phase 1 FAILED (rc=$rc)"
    cd "${WORKING_DIR}"
    rm -rf "${TMP_DIR}"
    return $rc
  fi

  echo "Phase 2 (linux): test + native sources + link"
  LIBYAML_OBJS=""
  for n in $LIBYAML_UNITS; do
    LIBYAML_OBJS="$LIBYAML_OBJS $n.o"
  done
  # shellcheck disable=SC2086
  $CC $LINUX_BASE \
    -I "$H" -I "$PXSX" -I "${LIBYAML_SRC}/include" \
    -o "${WORKING_DIR}/commenttest_linux" \
    $LIBYAML_OBJS \
    "${WORKING_DIR}/commenttest.c" \
    $COMMON_C_LINUX \
    -lpthread -lm -ldl
  rc=$?
  cd "${WORKING_DIR}"
  rm -rf "${TMP_DIR}"

  if [ "$rc" -eq 0 ]; then
    echo "linux OK -> ${WORKING_DIR}/commenttest_linux"
  else
    echo "linux Phase 2 FAILED (rc=$rc)"
  fi
  return $rc
}

# --- dispatch -----------------------------------------------------------
case "$WHICH" in
  xlclang)
    build_xlclang
    exit $?
    ;;
  clang)
    build_clang
    exit $?
    ;;
  linux)
    build_linux
    exit $?
    ;;
  both)
    build_xlclang; rc1=$?
    build_clang;   rc2=$?
    echo
    echo "=========== summary ==========="
    echo "  xlclang     rc=$rc1  ($([ $rc1 -eq 0 ] && echo OK || echo FAIL))"
    echo "  ibm-clang64 rc=$rc2  ($([ $rc2 -eq 0 ] && echo OK || echo FAIL))"
    [ "$rc1" -eq 0 ] && [ "$rc2" -eq 0 ]
    exit $?
    ;;
  *)
    echo "Usage: $0 [xlclang|clang|linux|both]" >&2
    exit 2
    ;;
esac
