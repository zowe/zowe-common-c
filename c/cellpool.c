

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "cellpool.h"
#include "zos.h"

unsigned int cellpoolGetDWordAlignedSize(unsigned int size) {

  unsigned rem = size % 8;

  if (rem == 0) {
    return size;
  }

  return size + 8 - rem;

}

CPID cellpoolBuild(unsigned int pCellCount,
                   unsigned int sCellCount,
                   unsigned int cellSize,
                   int subpool, int key,
                   const CPHeader *header) {

  CPID cpid = CPID_NULL;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      char parmList[64];
      CPHeader header;
    )
  );

  if (below2G == NULL) { /* This can only fail in LE 64-bit */
    return cpid;
  }

  below2G->header = *header;

  if (isCallerCrossMemory() || isCallerSRB()) {

    __asm(

        ASM_PREFIX
        "         SYSSTATE PUSH                                                  \n"
        "         SYSSTATE OSREL=ZOSV1R6                                         \n"
  #ifdef _LP64
        "         SAM31                                                          \n"
        "         SYSSTATE AMODE64=NO                                            \n"
  #endif

        "         CPOOL BUILD"
        ",PCELLCT=(%[pcell])"
        ",SCELLCT=(%[scell])"
        ",CSIZE=(%[csize])"
        ",SP=(%[sp])"
        ",KEY=(%[key])"
        ",TCB=0"
        ",LOC=(31,64)"
        ",CPID=(%[cpid])"
        ",HDR=%[header]"
        ",MF=(E,%[parmList])"
        "                                                                        \n"

  #ifdef _LP64
        "         SAM64                                                          \n"
  #endif
        "         SYSSTATE POP                                                   \n"

        : [cpid]"=NR:r0"(cpid)

        : [pcell]"r"(pCellCount),
          [scell]"r"(sCellCount),
          [csize]"r"(cellSize),
          [sp]"r"(subpool),
          [key]"r"(key),
          [header]"m"(below2G->header),
          [parmList]"m"(below2G->parmList)

        : "r0", "r1", "r14", "r15"

    );

  } else {

    __asm(

        ASM_PREFIX
        "         SYSSTATE PUSH                                                  \n"
        "         SYSSTATE OSREL=ZOSV1R6                                         \n"
  #ifdef _LP64
        "         SAM31                                                          \n"
        "         SYSSTATE AMODE64=NO                                            \n"
  #endif

        "         CPOOL BUILD"
        ",PCELLCT=(%[pcell])"
        ",SCELLCT=(%[scell])"
        ",CSIZE=(%[csize])"
        ",SP=(%[sp])"
        ",KEY=(%[key])"
        ",LOC=(31,64)"
        ",CPID=(%[cpid])"
        ",HDR=%[header]"
        ",MF=(E,%[parmList])"
        "                                                                        \n"

  #ifdef _LP64
        "         SAM64                                                          \n"
  #endif
        "         SYSSTATE POP                                                   \n"

        : [cpid]"=NR:r0"(cpid)

        : [pcell]"r"(pCellCount),
          [scell]"r"(sCellCount),
          [csize]"r"(cellSize),
          [sp]"r"(subpool),
          [key]"r"(key),
          [header]"m"(below2G->header),
          [parmList]"m"(below2G->parmList)

        : "r0", "r1", "r14", "r15"

    );

  }

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return cpid;
}

void cellpoolDelete(CPID cellpoolID) {

  __asm(

      ASM_PREFIX
      "         SYSSTATE PUSH                                                  \n"
      "         SYSSTATE OSREL=ZOSV1R6                                         \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         CPOOL DELETE,CPID=(%[cpid])                                    \n"

#ifdef _LP64
      "         SAM64                                                          \n"
#endif
      "         SYSSTATE POP                                                   \n"

      :
      : [cpid]"r"(cellpoolID)
      : "r0", "r1", "r14", "r15"
  );

}

void *cellpoolGet(CPID cellpoolID, bool conditional) {

  uint64_t callerGPRs[12] = {0};

  void * __ptr32 cell = NULL;

  /*
   * Notes about the use of callerGPRs:
   *
   * - The registers must be saved before switching to AMODE 31 and restored
   *   after switching back to AMODE 64, because the stack storage containing
   *   the callerGPRs may be above 2G.
   *
   * - Register 13 is being saved in callerGPRs, changed to point to callerGPRs,
   *   and then restored back to its original value when the registers are
   *   restored. All parameters must be passed in registers on the CPOOL request
   *   because of R13 being changed.
   *
   */

  if (conditional) {
    __asm(

        ASM_PREFIX
        "         STMG  2,13,%[gprs]                                             \n"
        "         LA    13,%[gprs]                                               \n"
  #ifdef _LP64
        "         SAM31                                                          \n"
  #endif

        "         CPOOL GET,C,CPID=(%[cpid]),REGS=USE                            \n"

  #ifdef _LP64
        "         SAM64                                                          \n"
  #endif
        "         LMG   2,13,0(13)                                               \n"

        : [cell]"=NR:r1"(cell)
        : [gprs]"m"(callerGPRs), [cpid]"NR:r1"(cellpoolID)
        : "r0", "r1", "r14", "r15"
    );
  } else {
    __asm(

        ASM_PREFIX
        "         STMG  2,13,%[gprs]                                             \n"
        "         LA    13,%[gprs]                                               \n"
  #ifdef _LP64
        "         SAM31                                                          \n"
  #endif

        "         CPOOL GET,U,CPID=(%[cpid]),REGS=USE                            \n"

  #ifdef _LP64
        "         SAM64                                                          \n"
  #endif
        "         LMG   2,13,0(13)                                               \n"

        : [cell]"=NR:r1"(cell)
        : [gprs]"m"(callerGPRs), [cpid]"NR:r1"(cellpoolID)
        : "r0", "r1", "r14", "r15"
    );
  }

  return cell;
}

void cellpoolFree(CPID cellpoolID, void *cell) {

  uint64_t callerGPRs[12] = {0};

  /*
   * Notes about the use of callerGPRs:
   *
   * - The registers must be saved before switching to AMODE 31 and restored
   *   after switching back to AMODE 64, because the stack storage containing
   *   the callerGPRs may be above 2G.
   *
   * - Register 13 is being saved in callerGPRs, changed to point to callerGPRs,
   *   and then restored back to its original value when the registers are
   *   restored. All parameters must be passed in registers on the CPOOL request
   *   because of R13 being changed.
   *
   */

  __asm(

      ASM_PREFIX
      "         STMG  2,13,%[gprs]                                             \n"
      "         LA    13,%[gprs]                                               \n"
#ifdef _LP64
      "         SAM31                                                          \n"
#endif

      "         CPOOL FREE,CPID=(%[cpid]),CELL=(%[cell]),REGS=USE              \n"

#ifdef _LP64
      "         SAM64                                                          \n"
#endif
      "         LMG   2,13,0(13)                                               \n"

      :
      : [gprs]"m"(callerGPRs), [cpid]"NR:r1"(cellpoolID), [cell]"NR:r0"(cell)
      : "r0", "r1", "r14", "r15"
  );

}

/* Tests (TODO move to a designated place)

LE:

xlc "-Wa,goff" \
"-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),LP64,XPLINK" \
-DCELLPOOL_TEST -I ../h -o cellpool \
alloc.c \
cellpool.c \
timeutls.c \
utils.c \
zos.c \

Metal:

CFLAGS=(-S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"'
-qreserved_reg=r12
-Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64"
-I ../h )

ASFLAGS=(-mgoff -mobject -mflag=nocont --TERM --RENT)

LDFLAGS=(-V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus)

xlc "${CFLAGS[@]}" -DCELLPOOL_TEST \
alloc.c \
cellpool.c \
metalio.c \
qsam.c \
timeutls.c \
utils.c \
zos.c \

as "${ASFLAGS[@]}" -aegimrsx=alloc.asm alloc.s
as "${ASFLAGS[@]}" -aegimrsx=cellpool.asm cellpool.s
as "${ASFLAGS[@]}" -aegimrsx=metalio.asm metalio.s
as "${ASFLAGS[@]}" -aegimrsx=qsam.asm qsam.s
as "${ASFLAGS[@]}" -aegimrsx=timeutls.asm timeutls.s
as "${ASFLAGS[@]}" -aegimrsx=utils.asm utils.s
as "${ASFLAGS[@]}" -aegimrsx=zos.asm zos.s

ld "${LDFLAGS[@]}" -e main \
-o "//'$USER.DEV.LOADLIB(CELLPOOL)'" \
alloc.o \
cellpool.o \
metalio.o \
qsam.o \
timeutls.o \
utils.o \
zos.o \
> CELLPOOL.link

*/

#define CELLPOOL_TEST_STATUS_OK        0
#define CELLPOOL_TEST_STATUS_FAILURE   8

static int testUnconditionalCellPoolGet(void) {

  unsigned psize = 10;
  unsigned ssize = 2;
  unsigned cellSize = 512;
  int sp = 132, key = 8;
  CPHeader header = {"TEST-CP-HEADER"};
  bool isConditional = false;

  CPID id = cellpoolBuild(psize, ssize, cellSize, sp, key, &header);
  if (id == CPID_NULL) {
    printf("error: cellpoolBuild failed\n");
    return CELLPOOL_TEST_STATUS_FAILURE;
  }

  int status = CELLPOOL_TEST_STATUS_OK;

  for (int i = 0; i < 100; i++) {
    void *cell = cellpoolGet(id, isConditional);
    if (cell == NULL) {
      printf("error: cellpoolGet(unconditional) test failed, cell #%d\n", i);
      status = CELLPOOL_TEST_STATUS_FAILURE;
      break;
    }
  }

  cellpoolDelete(id);

  return status;
}

static int testConditionalCellPoolGet(void) {

  unsigned psize = 10;
  unsigned ssize = 2;
  unsigned cellSize = 512;
  int sp = 132, key = 8;
  CPHeader header = {"TEST-CP-HEADER"};
  bool isConditional = true;

  CPID id = cellpoolBuild(psize, ssize, cellSize, sp, key, &header);
  if (id == CPID_NULL) {
    printf("error: cellpoolBuild failed\n");
    return CELLPOOL_TEST_STATUS_FAILURE;
  }

  int status = CELLPOOL_TEST_STATUS_FAILURE;

  for (int i = 0; i < psize + 1; i++) {
    void *cell = cellpoolGet(id, isConditional);
    if (cell == NULL && i == psize) {
        status = CELLPOOL_TEST_STATUS_OK;
        break;
    }
  }

  if (status != CELLPOOL_TEST_STATUS_OK) {
    printf("error: cellpoolGet(conditional) test failed\n");
  }

  cellpoolDelete(id);

  return status;
}

static int testCellPoolFree(void) {

  unsigned psize = 10;
  unsigned ssize = 2;
  unsigned cellSize = 512;
  int sp = 132, key = 8;
  CPHeader header = {"TEST-CP-HEADER"};
  bool isConditional = true;

  void *cells[10] = {0};

  CPID id = cellpoolBuild(psize, ssize, cellSize, sp, key, &header);
  if (id == CPID_NULL) {
    printf("error: cellpoolBuild failed\n");
    return CELLPOOL_TEST_STATUS_FAILURE;
  }

  int status = CELLPOOL_TEST_STATUS_OK;

  for (int i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
    cells[i] = cellpoolGet(id, isConditional);
    if (cells[i] == NULL) {
      printf("error: cellpoolFree test failed (alloc 1), cell #%d\n", i);
      status = CELLPOOL_TEST_STATUS_FAILURE;
      break;
    }
  }

  if (status == CELLPOOL_TEST_STATUS_OK) {

    for (int i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
      cellpoolFree(id, cells[i]);
      cells[i] = NULL;
    }

    for (int i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
      cells[i] = cellpoolGet(id, isConditional);
      if (cells[i] == NULL) {
        printf("error: cellpoolFree test failed (alloc 2), cell #%d\n", i);
        status = CELLPOOL_TEST_STATUS_FAILURE;
        break;
      }
    }

  }

  cellpoolDelete(id);

  return status;
}


static int testCellPool(void) {

  int status = CELLPOOL_TEST_STATUS_OK;

  if (status == CELLPOOL_TEST_STATUS_OK) {
    status = testUnconditionalCellPoolGet();
  }

  if (status == CELLPOOL_TEST_STATUS_OK) {
    status = testConditionalCellPoolGet();
  }

  if (status == CELLPOOL_TEST_STATUS_OK) {
    status = testCellPoolFree();
  }

  return status;
}

#ifdef CELLPOOL_TEST
int main() {
#else
static int notMain() {
#endif

  printf("info: starting cellpool test\n");

  int status = CELLPOOL_TEST_STATUS_OK;

  status = testCellPool();

  if (status == CELLPOOL_TEST_STATUS_OK) {
    printf("info: SUCCESS, tests have passed\n");
  } else {
    printf("error: FAILURE, some tests have failed\n");
  }

  return status;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

