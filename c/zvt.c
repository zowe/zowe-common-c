

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
#include <metal/stdlib.h>
#include <metal/string.h>
#else
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "zos.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

#include "zvt.h"

#define TOSTR(number) #number
#define INT_TO_STR(number) TOSTR(number

static void *allocateECSAStorage(uint16_t size,
                                 uint8_t key,
                                 uint8_t subpool) {

  void *result;

  __asm(
      ASM_PREFIX
      "         STORAGE OBTAIN"
      ",LENGTH=(%[size])"
      ",ADDR=(%[result])"
      ",KEY=(%[key])"
      ",SP=(%[sp])"
      ",OWNER=SYSTEM"
      ",BNDRY=PAGE"
      ",LOC=(31,64)"
      ",COND=NO"
      "                                                                        \n"
      : [result]"=r"(result)
      : [size]"NR:r0"(size), [key]"r"(key), [sp]"r"(subpool)
      : "r0", "r1", "r14", "r15"
  );

  return result;
}

static void freeECSAStorage(void *data,
                            uint16_t size,
                            uint8_t key,
                            uint8_t subpool) {

  __asm(
      ASM_PREFIX
      "         STORAGE RELEASE"
      ",LENGTH=(%[size])"
      ",ADDR=(%[data])"
      ",KEY=(%[key])"
      ",SP=(%[sp])"
      ",COND=NO"
      "                                                                        \n"
      :
      : [size]"NR:r0"(size), [data]"r"(data), [key]"r"(key), [sp]"r"(subpool)
      : "r0", "r1", "r14", "r15"
  );

}

static ZVT * __ptr32 * getZVTHandle() {

  CVT *cvt = *(CVT * __ptr32 *)0x10;
  ECVT *ecvt = cvt->cvtecvt;
  char *ecvtctbl = ecvt->ecvtctbl;

  return (ZVT * __ptr32 *)(ecvtctbl + ZVT_OFFSET);
}

static void getSTCK(uint64_t *stckValue) {

  __asm(" STCK 0(%0)" : : "r"(stckValue));

}

void zvtInit() {

  ZVT * __ptr32 * currentZVTHandle = getZVTHandle();
  ZVT * __ptr32 oldZVT = NULL;

  if (*currentZVTHandle != NULL) {
    return;
  }

  ZVT * __ptr32 newZVT = allocateECSAStorage(sizeof(ZVT), ZVT_KEY, ZVT_SUBPOOL);

  int wasProblemState = supervisorMode(TRUE);

  int originalKey = setKey(0);
  {

    memset(newZVT, 0, sizeof(ZVT));
    memcpy(newZVT->eyecatcher, ZVT_EYECATCHER, sizeof(newZVT->eyecatcher));
    newZVT->version = ZVT_VERSION;
    newZVT->key = ZVT_KEY;
    newZVT->subpool = ZVT_SUBPOOL;
    newZVT->size = sizeof(ZVT);
    getSTCK(&newZVT->creationTime);

    ASCB *ascb = getASCB();

    char *jobName = getASCBJobname(ascb);
    if (jobName) {
      memcpy(newZVT->jobName, jobName, sizeof(newZVT->jobName));
    } else {
      memset(newZVT->jobName, ' ', sizeof(newZVT->jobName));
    }

    newZVT->asid = ascb->ascbasid;

    cs((cs_t *)&oldZVT, (cs_t *)currentZVTHandle, (cs_t)newZVT);

  }
  setKey(originalKey);

  if (wasProblemState) {
    supervisorMode(FALSE);
  }

  if (oldZVT != NULL) {
    freeECSAStorage(newZVT, sizeof(ZVT), ZVT_KEY, ZVT_SUBPOOL);
    newZVT = NULL;
  }

}

ZVT *zvtGet() {

  return *getZVTHandle();

}

ZVTEntry *zvtAllocEntry() {

  ZVTEntry * __ptr32 zvte =
      allocateECSAStorage(sizeof(ZVTEntry), ZVTE_KEY, ZVTE_SUBPOOL);

  int wasProblemState = supervisorMode(TRUE);

  int originalKey = setKey(0);
  {

    memset(zvte, 0, sizeof(ZVTEntry));
    memcpy(zvte->eyecatcher, ZVTE_EYECATCHER, sizeof(zvte->eyecatcher));
    zvte->version = ZVTE_VERSION;
    zvte->key = ZVTE_KEY;
    zvte->subpool = ZVTE_SUBPOOL;
    zvte->size = sizeof(ZVTEntry);
    getSTCK(&zvte->creationTime);

    ASCB *ascb = getASCB();

    char *jobName = getASCBJobname(ascb);
    if (jobName) {
      memcpy(zvte->jobName, jobName, sizeof(zvte->jobName));
    } else {
      memset(zvte->jobName, ' ', sizeof(zvte->jobName));
    }

    zvte->asid = ascb->ascbasid;

  }
  setKey(originalKey);

  if (wasProblemState) {
    supervisorMode(FALSE);
  }

  return zvte;
}

void zvtFreeEntry(ZVTEntry *entry) {

  freeECSAStorage(entry, sizeof(ZVTEntry), ZVTE_KEY, ZVTE_SUBPOOL);
  entry = NULL;

}

void *zvtGetCMSLookupRoutineAnchor(ZVT *zvt) {
  return zvt->cmsLookupRoutine;
}

void *zvtSetCMSLookupRoutineAnchor(ZVT *zvt, void *anchor) {

  void *oldAnchor = zvt->cmsLookupRoutine;

  int wasProblemState = supervisorMode(TRUE);
  int originalKey = setKey(0);
  {
    if (cds((cds_t *)&oldAnchor, (cds_t *)&zvt->cmsLookupRoutine,
            *(cds_t *)&anchor)) {
    }
  }
  setKey(originalKey);
  if (wasProblemState) {
    supervisorMode(FALSE);
  }

  return oldAnchor;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

