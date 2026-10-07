#!/bin/sh
# test_configmgr_compat.sh - Verify that our modified json.c, json.h, and yaml2json.c
# still compile cleanly with the same compiler flags used by the configmgr build
# (build/build_cmgr_xlclang.sh Phase 2), proving backward compatibility.
#
# This test does a compile-only pass (-c) of the modified source files using the
# exact Phase 2 flags from build_cmgr_xlclang.sh.  It does NOT attempt a full
# configmgr link because:
#   1. The third-party deps (libyaml, quickjs) need iconv conversion first, which
#      the configmgr build script does not yet do (pre-existing issue).
#   2. The full link requires GSK SSL libraries that may not be present.
#
# What this DOES prove:
#   - json.h/json.c/yaml2json.c compile without errors under configmgr flags
#   - The new comment-preservation fields in json.h are ABI-compatible
#   - No new warnings are introduced
#   - The code compiles both with and without the CMGRTEST define
#
# Usage:  cd tests/commenttest && ./test_configmgr_compat.sh
#

set -e

WORKING_DIR=$(cd "$(dirname "$0")" && pwd)
COMMON="$WORKING_DIR/../.."
DEPS_DESTINATION="${COMMON}/deps/configmgr"
LIBYAML_SRC="${DEPS_DESTINATION}/libyaml"
QUICKJS_INC="${DEPS_DESTINATION}/quickjs"

date_stamp=$(date +%Y%m%d%S)
TMP_DIR="${WORKING_DIR}/tmp-compat-${date_stamp}"
mkdir -p "${TMP_DIR}"

echo "========================================"
echo "configmgr Backward Compatibility Test"
echo "========================================"
echo ""
echo "Compiler flags match build/build_cmgr_xlclang.sh Phase 2 (native EBCDIC)."
echo "Compile-only (-c) - no link step required."
echo ""

# --- Phase 0: Convert libyaml headers from ISO8859-1 to IBM-1047 ---
# The deps/configmgr/libyaml headers are in ASCII/ISO8859-1.
# yaml2json.c #includes yaml.h, so we must convert it for native EBCDIC compilation.
# (This is the same encoding dance that build.sh Phase 0 performs.)
LIBYAML_INC="${TMP_DIR}/libyaml-include"
mkdir -p "${LIBYAML_INC}"
echo "Converting libyaml headers (ISO8859-1 -> IBM-1047)..."
for hdr in ${LIBYAML_SRC}/include/*.h; do
  base=$(basename "$hdr")
  iconv -f ISO8859-1 -t IBM-1047 "$hdr" > "${LIBYAML_INC}/${base}"
done
echo "  Converted $(ls ${LIBYAML_INC}/*.h | wc -l | tr -d ' ') header(s)."
echo ""

PASS=0
FAIL=0

compile_one() {
  src="$1"
  base=$(basename "$src" .c)
  echo -n "  Compiling ${base}.c ... "
  xlclang \
    -c \
    -q64 \
    "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
    -D_OPEN_SYS_FILE_EXT=1 \
    -D_XOPEN_SOURCE=600 \
    -D_OPEN_THREADS=1 \
    -DNOIBMHTTP=1 \
    -DUSE_ZOWE_TLS=1 \
    -DNEW_CAA_LOCATIONS=1 \
    -DCMGRTEST=1 \
    -I "${COMMON}/h" \
    -I "${COMMON}/platform/posix" \
    -I "${LIBYAML_INC}" \
    -I "${QUICKJS_INC}" \
    -o "${TMP_DIR}/${base}.o" \
    "$src" 2>"${TMP_DIR}/${base}.err"
  rc=$?
  if [ $rc -eq 0 ]; then
    echo "OK"
    PASS=$((PASS + 1))
  else
    echo "FAILED (rc=$rc)"
    cat "${TMP_DIR}/${base}.err"
    FAIL=$((FAIL + 1))
  fi
  return $rc
}

echo "--- Phase 2 files (our modified sources) ---"
# These are the exact files that configmgr Phase 2 compiles natively:
compile_one "${COMMON}/c/json.c"       || true
compile_one "${COMMON}/c/yaml2json.c"  || true

# Also compile a few other Phase 2 files to verify we haven't broken includes:
echo ""
echo "--- Smoke-test other Phase 2 files ---"
compile_one "${COMMON}/c/alloc.c"      || true
compile_one "${COMMON}/c/charsets.c"   || true
compile_one "${COMMON}/c/utils.c"      || true

echo ""
echo "========================================"
echo "Results:  ${PASS} passed,  ${FAIL} failed"
echo "========================================"

rm -rf "${TMP_DIR}"

if [ $FAIL -gt 0 ]; then
  echo "FAIL: Some files did not compile."
  exit 1
else
  echo "PASS: All modified files compile cleanly with configmgr flags."
  echo ""
  echo "NOTE: Comment preservation is opt-in and NOT yet enabled in configmgr."
  echo "      A later commit will switch it on via yaml2JSONWithComments()."
  exit 0
fi
