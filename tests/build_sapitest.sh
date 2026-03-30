#!/bin/sh
set -e

echo "Building sapitest (standalone SAPI test)"

xlclang \
  -q64 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -I /usr/include/zos \
  -o sapitest \
  sapitest.c

rc=$?
if [ $rc -eq 0 ]; then
  echo "Build successful: ./sapitest"
  ls -la sapitest
else
  echo "Build failed rc=$rc"
fi
exit $rc
