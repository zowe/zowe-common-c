#!/bin/sh

set -e

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"

xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX=\"ZWE\" \
  -qreserved_reg=r12 \
  -DHAVE_METALIO=1 \
  -Wc,langlvl\(extc99\),arch\(8\),agg,exp,list\(\),so\(\),off,xref,roconst,longname,lp64 \
  -I ../h -I /usr/include/zos \
  ../c/modreg.c \
  ../c/alloc.c \
  ../c/isgenq.c \
  ../c/lpa.c \
  ../c/qsam.c \
  ../c/metalio.c \
  ../c/utils.c \
  ../c/timeutls.c \
  ../c/zvt.c \
  ../c/zos.c \
  ./modregtest.c

for file in \
    modreg \
    alloc \
    isgenq \
    lpa \
    qsam \
    metalio \
    utils \
    timeutls \
    zvt \
    zos \
    modregtest
do
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=${file}.asm ${file}.s
done

ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e main \
-o "//'${USER}.LOADLIB(MODREG)'" \
modreg.o \
alloc.o \
isgenq.o \
lpa.o \
qsam.o \
metalio.o \
utils.o \
timeutls.o \
zvt.o \
zos.o \
modregtest.o > MODREG.lnk


