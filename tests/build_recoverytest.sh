#!/bin/sh

set -eu

cp recoverytest.c rcvrtest.c

xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 \
-DRCVR_CPOOL_STATES \
-Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname" -I ../h \
../c/alloc.c \
../c/cellpool.c \
../c/collections.c \
../c/le.c \
../c/logging.c \
../c/metalio.c \
../c/qsam.c \
../c/recovery.c \
../c/scheduling.c \
../c/timeutls.c \
../c/utils.c \
../c/zos.c \
srb_harness.c \
rcvrtest.c

as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=alloc.asm -o alloc.o alloc.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=cellpool.asm -o cellpool.o cellpool.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=collections.asm -o collections.o collections.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=le.asm -o le.o le.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=logging.asm -o logging.o logging.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=metalio.asm -o metalio.o metalio.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=qsam.asm -o qsam.o qsam.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=recovery.asm -o recovery.o recovery.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=scheduling.asm -o scheduling.o scheduling.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=timeutls.asm -o timeutls.o timeutls.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=utils.asm -o utils.o utils.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zos.asm -o zos.o zos.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=srb_harness.asm -o srb_harness.o srb_harness.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=rcvrtest.asm -o rcvrtest.o rcvrtest.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"
ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e main -o "//'$USER.DEV.LOADLIB(RCVRTST1)'" \
rcvrtest.o alloc.o cellpool.o collections.o le.o logging.o metalio.o qsam.o recovery.o scheduling.o timeutls.o utils.o zos.o srb_harness.o > RCVRTST1.link




xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"' -qreserved_reg=r12 \
-DRCVR_CPOOL_STATES \
-Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../h \
../c/alloc.c \
../c/cellpool.c \
../c/collections.c \
../c/le.c \
../c/logging.c \
../c/metalio.c \
../c/qsam.c \
../c/recovery.c \
../c/scheduling.c \
../c/timeutls.c \
../c/utils.c \
../c/zos.c \
srb_harness.c \
rcvrtest.c

as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=alloc.asm -o alloc.o alloc.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=cellpool.asm -o cellpool.o cellpool.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=collections.asm -o collections.o collections.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=le.asm -o le.o le.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=logging.asm -o logging.o logging.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=metalio.asm -o metalio.o metalio.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=qsam.asm -o qsam.o qsam.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=recovery.asm -o recovery.o recovery.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=scheduling.asm -o scheduling.o scheduling.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=timeutls.asm -o timeutls.o timeutls.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=utils.asm -o utils.o utils.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zos.asm -o zos.o zos.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=srb_harness.asm -o srb_harness.o srb_harness.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=rcvrtest.asm -o rcvrtest.o rcvrtest.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"
ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e main -o "//'$USER.DEV.LOADLIB(RCVRTST4)'" \
rcvrtest.o alloc.o cellpool.o collections.o le.o logging.o metalio.o qsam.o recovery.o scheduling.o timeutls.o utils.o zos.o srb_harness.o > RCVRTST4.link




xlc -D_OPEN_THREADS=1 "-Wa,goff" "-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),ASMLIB('CEE.SCEEMAC')" '-Wl,ac=1' \
-DRCVR_CPOOL_STATES \
-I ../h -o recoverytest31 recoverytest.c \
../c/alloc.c \
../c/cellpool.c \
../c/collections.c \
../c/le.c \
../c/logging.c \
../c/recovery.c \
../c/scheduling.c \
../c/timeutls.c \
../c/utils.c \
../c/zos.c


xlc -D_OPEN_THREADS=1 "-Wa,goff" "-Wc,LP64,XPLINK,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),ASMLIB('CEE.SCEEMAC')" '-Wl,ac=1' \
-DRCVR_CPOOL_STATES \
-I ../h -o recoverytest64 recoverytest.c \
../c/alloc.c \
../c/cellpool.c \
../c/collections.c \
../c/le.c \
../c/logging.c \
../c/recovery.c \
../c/scheduling.c \
../c/timeutls.c \
../c/utils.c \
../c/zos.c

