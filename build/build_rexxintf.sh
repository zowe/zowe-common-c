#! /bin/sh

WORKING_DIR=$(dirname "$0")

echo "********************************************************************************"
echo "Building configmgr INTF for REXX"

COMMON="$WORKING_DIR/.."

# Step 0   Libraries

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'://'CBC.SCCNOBJ'"

# Step 1:  Build 31-bit program for REXX

xlc -qmetal -DMSGPREFIX=\"ZWE\" -DSUBPOOL=132 -DMETTLE=1 "-Wc,longname,langlvl(extc99),goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -I ${COMMON}/h -S ${COMMON}/c/zwecfg31.c ${COMMON}/c/metalio.c ${COMMON}/c/zos.c ${COMMON}/c/qsam.c ${COMMON}/c/timeutls.c ${COMMON}/c/utils.c ${COMMON}/c/alloc.c

as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zwecfg31.asm zwecfg31.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=alloc.asm alloc.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=utils.asm utils.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=timeutls.asm timeutls.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zos.asm zos.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=qsam.asm qsam.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=metalio.asm metalio.s

ld -b rent -b case=mixed -b map -b xref -b reus -e main -o ${COMMON}/bin/ZWECFG31 zwecfg31.o metalio.o qsam.o zos.o timeutls.o utils.o alloc.o

# Step 2: Build 64 Bit Metal program to be called from REXX to call LE64

xlc -qmetal -q64 -DMSGPREFIX=\"ZWE\" -DMETAL_PRINTF_TO_STDOUT=1 -DSUBPOOL=132 -DMETTLE=1 "-Wc,longname,langlvl(extc99),goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -I ${COMMON}/h -S ${COMMON}/c/rex2le64.c ${COMMON}/c/metalio.c ${COMMON}/c/zos.c ${COMMON}/c/qsam.c ${COMMON}/c/timeutls.c ${COMMON}/c/utils.c ${COMMON}/c/alloc.c

as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=rexle64.asm rex2le64.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=alloc.asm alloc.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=utils.asm utils.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=timeutls.asm timeutls.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=zos.asm zos.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=qsam.asm qsam.s
as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=metalio.asm metalio.s

ld -b rent -b case=mixed -b map -b xref -b reus -e main -o ${COMMON}/bin/ZWECFG64 rex2le64.o metalio.o qsam.o zos.o timeutls.o utils.o alloc.o

