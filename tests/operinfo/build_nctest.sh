#!/bin/sh

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.

# POC: Build ncurses test with xlclang (EBCDIC) linking against zopen ncurses (ASCII).
# Follows the configmgr hybrid-link pattern from build_cmgr_xlclang.sh.

set -e

# Source zopen environment for ncurses paths
export ZOPEN_ROOTFS=$HOME/zopen
. $HOME/zopen/etc/zopen-config --override-zos-tools 2>/dev/null

COMMON="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"

# ncurses paths from zopen
NC_BASE="$ZOPEN_ROOTFS/usr/local"
NC_INC="${NC_BASE}/include/ncursesw"
NC_LIB="${NC_BASE}/lib"

echo "ncurses include: ${NC_INC}"
echo "ncurses lib:     ${NC_LIB}"
echo ""

# Verify ncurses is there
if [ ! -f "${NC_INC}/ncurses.h" ]; then
  echo "ERROR: ncurses.h not found at ${NC_INC}"
  exit 1
fi
if [ ! -f "${NC_LIB}/libncursesw.a" ]; then
  echo "ERROR: libncursesw.a not found at ${NC_LIB}"
  exit 1
fi

# Show what we're linking against
ls -la "${NC_LIB}/libncursesw.a" "${NC_LIB}/libpanelw.a"
echo ""

XLC_OPTS='-q64 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('"'"'CEE.SCEEMAC'"'"','"'"'SYS1.MACLIB'"'"','"'"'SYS1.MODGEN'"'"')"'
DEFINES="-D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1"

echo "=== Attempt 1: Direct link (xlclang EBCDIC main + ncurses ASCII .a) ==="
echo ""

eval xlclang \
  $XLC_OPTS \
  $DEFINES \
  -I "${NC_INC}" \
  -o nctest \
  nctest.c \
  -L "${NC_LIB}" \
  -lncursesw \
  -lpanelw \
  2>&1
rc=$?

if [ $rc -eq 0 ]; then
  echo ""
  echo "SUCCESS: Direct link worked!"
  ls -la nctest
  echo ""
  echo "Run with: ./nctest"
  exit 0
fi

echo ""
echo "Direct link failed (rc=$rc)."
echo ""
echo "=== Attempt 2: Compile our code as ASCII .o, then link everything ASCII ==="
echo ""

# If direct link fails, try compiling our code in ASCII mode too,
# handling EBCDIC conversion at runtime instead of compile time.
eval xlclang \
  $XLC_OPTS \
  -qascii \
  $DEFINES \
  -I "${NC_INC}" \
  -o nctest \
  nctest.c \
  -L "${NC_LIB}" \
  -lncursesw \
  -lpanelw \
  2>&1
rc=$?

if [ $rc -eq 0 ]; then
  echo ""
  echo "SUCCESS: ASCII-mode link worked!"
  echo "NOTE: Main program compiled in ASCII mode — string literal conversion is different."
  ls -la nctest
  echo ""
  echo "Run with: ./nctest"
  exit 0
fi

echo ""
echo "ASCII-mode link also failed (rc=$rc)."
echo ""
echo "=== Attempt 3: Compile nctest as ASCII .o, link with EBCDIC stub ==="
echo ""

# Compile nctest.c as ASCII .o
eval xlclang \
  -c \
  $XLC_OPTS \
  -qascii \
  $DEFINES \
  -I "${NC_INC}" \
  nctest.c \
  2>&1
rc=$?

if [ $rc -ne 0 ]; then
  echo "Compile of nctest.c as ASCII .o failed (rc=$rc)"
  exit $rc
fi

echo "nctest.o compiled (ASCII mode)"

# Link with xlclang (EBCDIC mode is the default link mode)
eval xlclang \
  -q64 \
  -o nctest \
  nctest.o \
  -L "${NC_LIB}" \
  -lncursesw \
  -lpanelw \
  2>&1
rc=$?

if [ $rc -eq 0 ]; then
  echo ""
  echo "SUCCESS: Two-phase build worked!"
  ls -la nctest
  echo ""
  echo "Run with: ./nctest"
  exit 0
fi

echo ""
echo "All attempts failed. Capturing diagnostics..."
echo ""
echo "ar t libncursesw.a (first 10):"
ar t "${NC_LIB}/libncursesw.a" | head -10
echo ""
echo "nm nctest.o | grep ncurses-related (first 20):"
nm nctest.o 2>/dev/null | grep -i "initscr\|endwin\|newwin\|wgetch\|wprintw\|waddstr\|panel\|start_color" | head -20
echo ""
echo "Unresolved externals in nctest.o:"
nm nctest.o 2>/dev/null | grep " U " | head -20

exit $rc
