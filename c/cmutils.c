

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
#include "alloc.h"
#include "cmutils.h"
#include "zos.h"

static int getCallersKey() {

  int key = 0;
  __asm(
      ASM_PREFIX
      "         LA    7,1                                                      \n"
      "         ESTA  6,7                                                      \n"
      "         SRL   6,20                                                     \n"
      "         N     6,=X'0000000F'                                           \n"
      "         ST    6,%0                                                     \n"
      : "=m"(key)
      :
      : "r6", "r7"
  );

  return key;
}

#define IS_LE64 (!defined(METTLE) && defined(_LP64))

#if !IS_LE64

/* Disable these for LE 64-bit for now, the compile complains about GPR 4 */

static void copyWithDestinationKey(void *dest,
                                   unsigned int destKey,
                                   unsigned int destALET,
                                   const void *src,
                                   size_t size) {

  __asm(
      ASM_PREFIX
      /* input parameters */
      "         LA    2,0(,%0)            destination address                  \n"
      "         LA    4,0(,%1)            source address                       \n"
      "         LA    1,0(,%2)            destination key                      \n"
      "         SLL   1,4                 shift key to bits 24-27              \n"
      "         LA    5,0                 use r5 for 0 value                   \n"
      "         SAR   4,5                 source is primary (0)                \n"
      "         SAR   2,%3                destination address space type       \n"
      "         LA    5,0(,%4)            size                                 \n"
      /* establish addressability */
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "LCMCTBGN DS    0H                                                       \n"
      "         LARL  10,LCMCTBGN                                              \n"
      "         USING LCMCTBGN,10                                              \n"
      /* enable AR mode for MVCDK */
      "         SAC   512                 AR mode                              \n"
      /* copy data in a loop */
      "LCMTCPY1 DS    0H                                                       \n"
#ifdef _LP64
      "         LTGR  5,5                 size > 0?                            \n"
#else
      "         LTR   5,5                                                      \n"
#endif
      "         BNP   LCMTEXIT            no, leave                            \n"
      "         LA    0,256                                                    \n"
#ifdef _LP64
      "         CGR   5,0                 size left > 256?                     \n"
#else
      "         CR    5,0                                                      \n"
#endif
      "         BH    LCMTCPY2            yes, go subtract 256                 \n"
#ifdef _LP64
      "         LGR   0,5                 no, load bytes left to r0            \n"
#else
      "         LR    0,5                                                      \n"
#endif
      "LCMTCPY2 DS    0H                                                       \n"
#ifdef _LP64
      "         SGR   5,0                 subtract 256                         \n"
#else
      "         SR    5,0                                                      \n"
#endif
      "         BCTR  0,0                 bytes to copy - 1                    \n"
      "         MVCDK 0(2),0(4)           copy                                 \n"
      "         LA    2,256(,2)           bump destination address             \n"
      "         LA    4,256(,4)           bump source address                  \n"
      "         B     LCMTCPY1            repeat                               \n"
      /* go back into primary mode and leave */
      "LCMTEXIT DS    0H                                                       \n"
      "         SAC   0                   go back to primary mode              \n"
      "         POP   USING                                                    \n"
      :
      : "r"(dest), "r"(src), "r"(destKey), "r"(destALET), "r"(size)
      : "r0", "r1", "r2", "r4", "r5", "r10"
  );

}

static void copyWithSourceKey(void *dest,
                              const void *src,
                              unsigned int srcKey,
                              unsigned int srcALET,
                              size_t size) {

  __asm(
      ASM_PREFIX
      /* input parameters */
      "         LA    2,0(,%0)            destination address                  \n"
      "         LA    4,0(,%1)            source address                       \n"
      "         LA    1,0(,%2)            source key                           \n"
      "         SLL   1,4                 shift key to bits 24-27              \n"
      "         LA    5,0                 use r5 for 0 value                   \n"
      "         SAR   2,5                 destination is primary (0)           \n"
      "         SAR   4,%3                source address ALET                  \n"
      "         LA    5,0(,%4)            size                                 \n"
      /* establish addressability */
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "LCMCFBGN DS    0H                                                       \n"
      "         LARL  10,LCMCFBGN                                              \n"
      "         USING LCMCFBGN,10                                              \n"
      /* enable AR mode for MVCDK */
      "         SAC   512                 AR mode                              \n"
      /* copy data in a loop */
      "LCMFCPY1 DS    0H                                                       \n"
#ifdef _LP64
      "         LTGR  5,5                 size > 0?                            \n"
#else
      "         LTR   5,5                                                      \n"
#endif
      "         BNP   LCMFEXIT            no, leave                            \n"
      "         LA    0,256                                                    \n"
#ifdef _LP64
      "         CGR   5,0                 size left > 256?                     \n"
#else
      "         CR    5,0                                                      \n"
#endif
      "         BH    LCMFCPY2            yes, go subtract 256                 \n"
#ifdef _LP64
      "         LGR   0,5                 no, load bytes left to r0            \n"
#else
      "         LR    0,5                                                      \n"
#endif
      "LCMFCPY2 DS    0H                                                       \n"
#ifdef _LP64
      "         SGR   5,0                 subtract 256                         \n"
#else
      "         SR    5,0                                                      \n"
#endif
      "         BCTR  0,0                 bytes to copy - 1                    \n"
      "         MVCSK 0(2),0(4)           copy                                 \n"
      "         LA    2,256(,2)           bump destination address             \n"
      "         LA    4,256(,4)           bump source address                  \n"
      "         B     LCMFCPY1            repeat                               \n"
      /* go back into primary mode and leave */
      "LCMFEXIT DS    0H                                                       \n"
      "         SAC   0                   go back to primary mode              \n"
      "         POP   USING                                                    \n"
      :
      : "r"(dest), "r"(src), "r"(srcKey), "r"(srcALET), "r"(size)
      : "r0", "r1", "r2", "r4", "r5", "r10"
  );

}

void cmCopyToSecondaryWithCallerKey(void *dest, const void *src, size_t size) {
  copyWithDestinationKey(dest, getCallersKey(), CROSS_MEMORY_ALET_SASN, src, size);
}

void cmCopyFromSecondaryWithCallerKey(void *dest, const void *src, size_t size) {
  copyWithSourceKey(dest, src, getCallersKey(), CROSS_MEMORY_ALET_SASN, size);
}

void cmCopyToPrimaryWithCallerKey(void *dest, const void *src, size_t size) {
  copyWithDestinationKey(dest, getCallersKey(), CROSS_MEMORY_ALET_PASN, src, size);
}

void cmCopyFromPrimaryWithCallerKey(void *dest, const void *src, size_t size) {
  copyWithSourceKey(dest, src, getCallersKey(), CROSS_MEMORY_ALET_PASN, size);
}

void cmCopyToHomeWithCallerKey(void *dest, const void *src, size_t size) {
  copyWithDestinationKey(dest, getCallersKey(), CROSS_MEMORY_ALET_HASN, src, size);
}

void cmCopyFromHomeWithCallerKey(void *dest, const void *src, size_t size) {
  copyWithSourceKey(dest, src, getCallersKey(), CROSS_MEMORY_ALET_HASN, size);
}

void cmCopyWithSourceKeyAndALET(void *dest, const void *src, unsigned int key,
                                unsigned int alet, size_t size) {
  copyWithSourceKey(dest, src, key, alet, size);
}

#define KEY0 0

static void getCallerTCB(TCB *content, TCB **address) {

  TCB * __ptr32 callerTCBAddr = NULL;
  copyWithSourceKey(&callerTCBAddr, (void *)CURRENT_TCB, KEY0,
                    CROSS_MEMORY_ALET_HASN, 4);
  copyWithSourceKey(content, callerTCBAddr, KEY0,
                    CROSS_MEMORY_ALET_HASN, sizeof(TCB));

  if (address != NULL) {
    *address = callerTCBAddr;
  }

}

static void getCallerASCB(ASCB *content, ASCB **address) {

  ASCB * __ptr32 callerASCBAddr = NULL;
  copyWithSourceKey(&callerASCBAddr, (void *)CURRENT_ASCB, KEY0,
                    CROSS_MEMORY_ALET_HASN, sizeof(ASCB * __ptr32));
  copyWithSourceKey(content, callerASCBAddr, KEY0,
                    CROSS_MEMORY_ALET_HASN, sizeof(ASCB));

  if (address != NULL) {
    *address = callerASCBAddr;
  }

}

static void getCallerASXB(ASXB *content, ASXB **address) {

  ASCB callerASCB;
  getCallerASCB(&callerASCB, NULL);

  ASXB * __ptr32 callerASXBAddr = callerASCB.ascbasxb;
  copyWithSourceKey(content, callerASXBAddr, KEY0,
                    CROSS_MEMORY_ALET_HASN, sizeof(ASXB));

  if (address != NULL) {
    *address = callerASXBAddr;
  }

}


void cmGetCallerAddressSpaceACEE(ACEE *content, ACEE **address) {

  ASXB callerASXB;
  getCallerASXB(&callerASXB, NULL);

  ACEE *callerACEEAddr = callerASXB.asxbsenv;

  copyWithSourceKey(content, callerACEEAddr, KEY0,
                    CROSS_MEMORY_ALET_HASN, sizeof(ACEE));

  if (address != NULL) {
    *address = callerACEEAddr;
  }

}

void cmGetCallerTaskACEE(ACEE *content, ACEE **address) {

  TCB callerTCB;
  TCB *callerTCBAddress = NULL;
  getCallerTCB(&callerTCB, &callerTCBAddress);

  ACEE * __ptr32 callerACEEAddr = NULL;
  if (callerTCBAddress != NULL) {
    callerACEEAddr = callerTCB.tcbsenv;
  }

  if (callerACEEAddr != NULL) {
    copyWithSourceKey(content, callerACEEAddr, KEY0,
                      CROSS_MEMORY_ALET_HASN, sizeof(ACEE));
  }

  if (address != NULL) {
    *address = callerACEEAddr;
  }

}

#endif /* not LE 64-bit */

void *cmAlloc(unsigned int size, int subpool, int key) {
  void *data = NULL;
  cmAlloc2(size, subpool, key, &data);
  return data;
}

void cmFree(void *data, unsigned int size, int subpool, int key) {
  cmFree2(&data, size, subpool, key);
}

void cmAlloc2(unsigned int size, int subpool, int key, void **resultPtr) {

  *resultPtr = NULL;
  key <<= 4;

  // Note: Used ADDR= option so STORAGE macro stores the address (for use by
  // recovery) a few instructions sooner than the compiler would.
#ifndef _LP64
#define ROFF "0"
#else
#define ROFF "4"
#endif

  __asm(
      ASM_PREFIX
      "         STORAGE OBTAIN"
      ",COND=YES"
      ",LENGTH=(%[size])"
      ",ADDR="ROFF"(,%[resultPtr])"
      ",SP=(%[sp])"
      ",LOC=31"
      ",OWNER=SYSTEM"
      ",KEY=(%[key])"
      ",LINKAGE=SYSTEM                                                         \n"
      : [result]"=m"(*resultPtr)
      : [size]"NR:r0"(size), [sp]"r"(subpool), [key]"r"(key),
        [resultPtr]"r"(resultPtr)
      : "r0", "r1", "r14", "r15"
  );


}

void cmFree2(void **dataPtr, unsigned int size, int subpool, int key) {

  void *localData = *dataPtr;
  *dataPtr = NULL;
  key <<= 4;

  __asm(
      ASM_PREFIX
      "         STORAGE RELEASE"
      ",COND=NO"
      ",LENGTH=(%[size])"
      ",SP=(%[sp])"
      ",ADDR=(%[storageAddr])"
      ",CALLRKY=YES"
      ",LINKAGE=SYSTEM                                                         \n"
      :
      : [size]"NR:r0"(size), [storageAddr]"r"(localData),
        [sp]"r"(subpool), [key]"r"(key)
      : "r0", "r1", "r14", "r15"
  );

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

