#!/bin/sh

# Build script for comment round-trip test
# Follows the two-phase approach from build_cmgr_xlclang.sh:
#   Phase 1: Convert ASCII libyaml sources to EBCDIC, compile with -qascii -> .o files
#   Phase 2: Compile native EBCDIC sources + test, link with libyaml .o files

WORKING_DIR=$(cd $(dirname "$0") && pwd)
COMMON="${WORKING_DIR}/../.."
LIBYAML_SRC="${COMMON}/deps/configmgr/libyaml"

MAJOR=0
MINOR=2
PATCH=5
VERSION="\"${MAJOR}.${MINOR}.${PATCH}\""

date_stamp=$(date +%Y%m%d%S)
TMP_DIR="${WORKING_DIR}/tmp-${date_stamp}"
mkdir -p "${TMP_DIR}/libyaml_src" "${TMP_DIR}/libyaml_inc" && cd "${TMP_DIR}"

echo "Phase 0: Converting libyaml sources from ASCII to EBCDIC"

for f in ${LIBYAML_SRC}/src/*.c ${LIBYAML_SRC}/src/*.h; do
  base=$(basename "$f")
  iconv -f ISO8859-1 -t IBM-1047 "$f" > "libyaml_src/$base"
done
for f in ${LIBYAML_SRC}/include/*.h; do
  base=$(basename "$f")
  iconv -f ISO8859-1 -t IBM-1047 "$f" > "libyaml_inc/$base"
done

echo "Phase 1: Compiling libyaml (ASCII mode, EBCDIC source)"

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
  -I libyaml_inc \
  libyaml_src/api.c \
  libyaml_src/reader.c \
  libyaml_src/scanner.c \
  libyaml_src/parser.c \
  libyaml_src/loader.c \
  libyaml_src/writer.c \
  libyaml_src/emitter.c \
  libyaml_src/dumper.c
rc=$?

if [ "${rc}" -ne 0 ]; then
  echo "Phase 1 FAILED (rc=$rc)"
  rm -rf "${TMP_DIR}"
  exit $rc
fi
echo "Phase 1 OK"

echo "Phase 2: Compiling test + native sources, linking"

xlclang \
  -q64 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -I "${COMMON}/h" \
  -I "${COMMON}/platform/posix" \
  -I libyaml_inc \
  -o "${WORKING_DIR}/commenttest" \
  api.o \
  reader.o \
  scanner.o \
  parser.o \
  loader.o \
  writer.o \
  emitter.o \
  dumper.o \
  ${WORKING_DIR}/commenttest.c \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/bpxskt.c \
  ${COMMON}/c/charsets.c \
  ${COMMON}/c/collections.c \
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
rc=$?

rm -rf "${TMP_DIR}"

if [ "${rc}" -eq 0 ]; then
  echo "Build successful: ${WORKING_DIR}/commenttest"
else
  echo "Phase 2 FAILED (rc=$rc)"
fi
exit $rc
