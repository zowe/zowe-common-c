

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

void cmCopyWithDestinationKey(void *dest, unsigned destKey, unsigned destALET,
                              const void *src, size_t size) {

  copyWithDestinationKey(dest, destKey, destALET, src, size);

}

void cmCopyWithSourceKey(void *dest, const void *src, unsigned srcKey,
                         unsigned srcALET, size_t size) {

  copyWithSourceKey(dest, src, srcKey, srcALET, size);

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
      ",KEY=(%[key])"
      /* ",CALLRKY=YES" */
      ",LINKAGE=SYSTEM                                                         \n"
      :
      : [size]"NR:r0"(size), [storageAddr]"r"(localData),
        [sp]"r"(subpool), [key]"r"(key)
      : "r0", "r1", "r14", "r15"
  );

}

ZOWE_PRAGMA_PACK

typedef int32_t CPID;

typedef struct CMCPHeader_tag {
  char text[24];
} CMCPHeader;

ZOWE_PRAGMA_PACK_RESET

static CPID cmCellPoolBuild(unsigned int pCellCount,
                            unsigned int sCellCount,
                            unsigned int cellSize,
                            int subpool, int key,
                            const CMCPHeader *header) {

  CPID cpid = -1;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      char parmList[64];
      CMCPHeader header;
    )
  );

  if (below2G == NULL) { /* This can only fail in LE 64-bit */
    return cpid;
  }

  below2G->header = *header;

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
      : [pcell]"r"(pCellCount), [scell]"r"(sCellCount), [csize]"r"(cellSize),
        [sp]"r"(subpool), [key]"r"(key), [header]"m"(below2G->header),
        [parmList]"m"(below2G->parmList)
      : "r0", "r1", "r14", "r15"
  );

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return cpid;
}

static void cmCellPoolDelete(CPID cellpoolID) {

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

static void *cmCellPoolGet(CPID cellpoolID, bool conditional) {

  uint64 callerGPRs[12] = {0};

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

static void cmCellPoolFree(CPID cellpoolID, void *cell) {

  uint64 callerGPRs[12] = {0};

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

ZOWE_PRAGMA_PACK

typedef struct CrossMemoryMapEntry_tag {
  char eyecatcher[4];
#define CM_MAP_ENTRY_EYECATCHER "CMME"
  struct CrossMemoryMapEntry_tag * __ptr32 next;
  PAD_LONG(0, void *value);
  char key[0];
} CrossMemoryMapEntry;

typedef struct CrossMemoryMap_tag {
  char eyecatcher[8];
#define CROSS_MEMORY_MAP_EYECATCHER "CMMAPEYE"
  unsigned int size;
  unsigned int keySize;
  unsigned int entrySize;
  CPID entryCellpool;
  unsigned int bucketCount;
  CrossMemoryMapEntry * __ptr32 buckets[0];
} CrossMemoryMap;

ZOWE_PRAGMA_PACK_RESET

#ifndef CMUTILS_TEST
#define CM_MAP_SUBPOOL  228
#define CM_MAP_KEY      4
#else
#define CM_MAP_SUBPOOL  132
#define CM_MAP_KEY      8
#endif

#define CM_MAP_MAX_KEY_SIZE 64
#define CM_MAP_BUCKET_COUNT 29

#define CM_MAP_PRIMARY_CELL_COUNT     16
#define CM_MAP_SECONDARY_CELL_COUNT   32

#define CM_MAP_HEADER "CMUTILS ECSA MAP        "

CrossMemoryMap *makeCrossMemoryMap(unsigned int keySize) {

  if (keySize > CM_MAP_MAX_KEY_SIZE) {
    return NULL;
  }

  unsigned int mapSize = sizeof(CrossMemoryMap) +
      sizeof(CrossMemoryMapEntry * __ptr32) * CM_MAP_BUCKET_COUNT;

#ifndef CMUTILS_TEST
  CrossMemoryMap *map =
      cmAlloc(mapSize, CM_MAP_SUBPOOL, CM_MAP_KEY);
#else
  CrossMemoryMap *map = (CrossMemoryMap *)safeMalloc(mapSize, "CM MAP");
#endif
  if (map == NULL) {
    return NULL;
  }
  memset(map, 0, mapSize);
  memcpy(map->eyecatcher, CROSS_MEMORY_MAP_EYECATCHER, sizeof(map->eyecatcher));
  map->size = mapSize;
  map->keySize = keySize;
  map->bucketCount = CM_MAP_BUCKET_COUNT;

  CMCPHeader header = {
      .text = CM_MAP_HEADER,
  };

  if (keySize & 0x00000007) {
    keySize += (8 - (keySize % 8));
  }

  map->entrySize = sizeof(CrossMemoryMapEntry) + keySize;
  map->entryCellpool = cmCellPoolBuild(CM_MAP_PRIMARY_CELL_COUNT,
                                       CM_MAP_SECONDARY_CELL_COUNT,
                                       map->entrySize,
                                       CM_MAP_SUBPOOL, CM_MAP_KEY,
                                       &header);

  return map;
}

/* This function is not thread-safe. */
void removeCrossMemoryMap(CrossMemoryMap **mapAddr) {
  CrossMemoryMap *map = *mapAddr;
  CPID cellpoolToDelete = map->entryCellpool;
  map->entryCellpool = -1;
  if (cellpoolToDelete != -1) {
    cmCellPoolDelete(cellpoolToDelete);
  }
#ifndef CMUTILS_TEST
  cmFree2((void **)mapAddr, map->size, CM_MAP_SUBPOOL, CM_MAP_KEY);
#else
  safeFree((char *)map, map->size);
#endif
}

static unsigned int calculateKeyHash(const void *key, unsigned int size) {
  const char *target = key;
  unsigned int hash = 0;
  /* TODO: make sure we cover the last (size % 4) bytes */
  for (unsigned int i = 0; i < size; i += sizeof(int)) {
    hash = hash ^ *((unsigned int *)(target + i));
  }
  return hash;
}

static CrossMemoryMapEntry *findEntry(CrossMemoryMapEntry *chain,
                                      const void *key, unsigned int keySize) {
  CrossMemoryMapEntry *currEntry = chain;
  while (currEntry != NULL) {
    if (memcmp(currEntry->key, key, keySize) == 0) {
      return currEntry;
    }
    currEntry = currEntry->next;
  }

  return NULL;
}

static CrossMemoryMapEntry *makeEntry(CrossMemoryMap *map,
                                      const void *key,
                                      void *value) {

  CrossMemoryMapEntry *entry = cmCellPoolGet(map->entryCellpool, FALSE);
  if (entry == NULL) {
    return NULL;
  }

  memset(entry, 0, map->entrySize);
  memcpy(entry->eyecatcher, CROSS_MEMORY_MAP_EYECATCHER,
         sizeof(entry->eyecatcher));
  memcpy(entry->key, key, map->keySize);
  entry->value = value;

  return entry;
}

static void removeEntry(CrossMemoryMap *map, CrossMemoryMapEntry *entry) {
  cmCellPoolFree(map->entryCellpool, entry);
}

/* Put a new key-value into the map:
 *  0 - success
 *  1 - entry already exists (the value is not updated)
 * -1 - fatal error
 *
 * This function is thread-safe.
 */
int crossMemoryMapPut(CrossMemoryMap *map, const void *key, void *value) {

  unsigned int keySize = map->keySize;
  unsigned int hash = calculateKeyHash(key, keySize);
  unsigned int bucketID = hash % map->bucketCount;

  CrossMemoryMapEntry * __ptr32 chain = map->buckets[bucketID];
  CrossMemoryMapEntry * __ptr32 existingEntry = findEntry(chain, key, keySize);
  CrossMemoryMapEntry * __ptr32 newEntry = NULL;

  if (existingEntry == NULL) {
    newEntry = makeEntry(map, key, value);
    if (newEntry == NULL) {
      return -1; /* fatal error */
    }
    newEntry->next = chain;
  } else {
    return 1;
  }

  /* We don't allow entry removal, so it should be safe to assume that when
   * a new entry is added to a chain, the chain head address always changes,
   * that is, there should not be the ABA problem. */
  while (cs((cs_t *)&chain, (cs_t *)&map->buckets[bucketID], (cs_t)newEntry)) {
    /* chain has been updated */
    if (findEntry(chain, key, keySize)) {
      removeEntry(map, newEntry);
      newEntry = NULL;
      return 1; /* the same key has been added */
    }
    newEntry->next = chain;
  }

  return 0;
}

/* Get the handle of the value with the specified key.
 *
 * This function is thread-safe. */
void **crossMemoryMapGetHandle(CrossMemoryMap *map, const void *key) {

  unsigned int keySize = map->keySize;
  unsigned int hash = calculateKeyHash(key, keySize);
  unsigned int bucketID = hash % map->bucketCount;

  CrossMemoryMapEntry *chain = map->buckets[bucketID];
  CrossMemoryMapEntry *entry = findEntry(chain, key, keySize);

  if (entry != NULL) {
    return &entry->value;
  }

  return NULL;
}

void *crossMemoryMapGet(CrossMemoryMap *map, const void *key) {
  void **valueHandle = crossMemoryMapGetHandle(map, key);
  return valueHandle ? *valueHandle : NULL;
}

/* Iterate a map.
 *
 * This function is thread-safe as long as the user handles the values properly.
 */
void crossMemoryMapIterate(CrossMemoryMap *map,
                           CrossMemoryMapVisitor *visitor,
                           void *visitorData) {

  for (unsigned int bucketID = 0; bucketID < map->bucketCount; bucketID++) {
    CrossMemoryMapEntry *entry = map->buckets[bucketID];
    while (entry != NULL) {
      if (visitor(entry->key, map->keySize, &entry->value, visitorData) != 0) {
        return;
      }
      entry = entry->next;
    }
  }

}

/* Tests (TODO move to a designated place)

LE:

xlc "-Wa,goff" \
"-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),LP64,XPLINK" \
-DCMUTILS_TEST -I ../h -o cmutils \
alloc.c \
cmutils.c \

Metal:

CFLAGS=(-S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"'
-qreserved_reg=r12
-Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64"
-I ../h )

ASFLAGS=(-mgoff -mobject -mflag=nocont --TERM --RENT)

LDFLAGS=(-V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus)

xlc "${CFLAGS[@]}" -DCMUTILS_TEST \
alloc.c \
cmutils.c \
metalio.c \
qsam.c \
timeutls.c \
utils.c \
zos.c \

as "${ASFLAGS[@]}" -aegimrsx=alloc.asm alloc.s
as "${ASFLAGS[@]}" -aegimrsx=cmutils.asm cmutils.s
as "${ASFLAGS[@]}" -aegimrsx=metalio.asm metalio.s
as "${ASFLAGS[@]}" -aegimrsx=qsam.asm qsam.s
as "${ASFLAGS[@]}" -aegimrsx=timeutls.asm timeutls.s
as "${ASFLAGS[@]}" -aegimrsx=utils.asm utils.s
as "${ASFLAGS[@]}" -aegimrsx=zos.asm zos.s

ld "${LDFLAGS[@]}" -e main \
-o "//'$USER.DEV.LOADLIB(CMUTILS)'" \
alloc.o \
cmutils.o \
metalio.o \
qsam.o \
timeutls.o \
utils.o \
zos.o \
> CMUTILS.link

*/

#define CMUTILS_TEST_STATUS_OK        0
#define CMUTILS_TEST_STATUS_FAILURE   8

static int testUnconditionalCellPoolGet(void) {

  unsigned psize = 10;
  unsigned ssize = 2;
  unsigned cellSize = 512;
  int sp = 132, key = 8;
  CMCPHeader header = {"TEST-CP-HEADER"};
  bool isConditional = false;

  CPID id = cmCellPoolBuild(psize, ssize, cellSize, sp, key, &header);
  if (id == -1) {
<<<<<<< HEAD
<<<<<<< HEAD
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_SEVERE, "error: cmCellPoolBuild failed\n");
=======
    printf("error: cmCellPoolBuild failed\n");
>>>>>>> e3a5b4d... Ensure there is no name collisions with cellpool.c in cmutils.c
=======
    printf("error: cmCellPoolBuild failed\n");
>>>>>>> c7398ac... Refactored logging for {COMMON}/c/[a-c]*.c
    return CMUTILS_TEST_STATUS_FAILURE;
  }

  int status = CMUTILS_TEST_STATUS_OK;

  for (int i = 0; i < 100; i++) {
    void *cell = cmCellPoolGet(id, isConditional);
    if (cell == NULL) {
<<<<<<< HEAD
<<<<<<< HEAD
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_SEVERE, "error: cmCellPoolGet(unconditional) test failed, cell #%d\n", i);
=======
      printf("error: cmCellPoolGet(unconditional) test failed, cell #%d\n", i);
>>>>>>> e3a5b4d... Ensure there is no name collisions with cellpool.c in cmutils.c
=======
      printf("error: cmCellPoolGet(unconditional) test failed, cell #%d\n", i);
>>>>>>> c7398ac... Refactored logging for {COMMON}/c/[a-c]*.c
      status = CMUTILS_TEST_STATUS_FAILURE;
      break;
    }
  }

  cmCellPoolDelete(id);

  return status;
}

static int testConditionalCellPoolGet(void) {

  unsigned psize = 10;
  unsigned ssize = 2;
  unsigned cellSize = 512;
  int sp = 132, key = 8;
  CMCPHeader header = {"TEST-CP-HEADER"};
  bool isConditional = true;

  CPID id = cmCellPoolBuild(psize, ssize, cellSize, sp, key, &header);
  if (id == -1) {
<<<<<<< HEAD
<<<<<<< HEAD
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_SEVERE, "error: cmCellPoolBuild failed\n");
=======
    printf("error: cmCellPoolBuild failed\n");
>>>>>>> e3a5b4d... Ensure there is no name collisions with cellpool.c in cmutils.c
=======
    printf("error: cmCellPoolBuild failed\n");
>>>>>>> c7398ac... Refactored logging for {COMMON}/c/[a-c]*.c
    return CMUTILS_TEST_STATUS_FAILURE;
  }

  int status = CMUTILS_TEST_STATUS_FAILURE;

  for (int i = 0; i < psize + 1; i++) {
    void *cell = cmCellPoolGet(id, isConditional);
    if (cell == NULL && i == psize) {
        status = CMUTILS_TEST_STATUS_OK;
        break;
    }
  }

  if (status != CMUTILS_TEST_STATUS_OK) {
<<<<<<< HEAD
<<<<<<< HEAD
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_SEVERE, "error: cmCellPoolGet(conditional) test failed\n");
=======
    printf("error: cmCellPoolGet(conditional) test failed\n");
>>>>>>> e3a5b4d... Ensure there is no name collisions with cellpool.c in cmutils.c
=======
    printf("error: cmCellPoolGet(conditional) test failed\n");
>>>>>>> c7398ac... Refactored logging for {COMMON}/c/[a-c]*.c
  }

  cmCellPoolDelete(id);

  return status;
}

static int testCellPoolFree(void) {

  unsigned psize = 10;
  unsigned ssize = 2;
  unsigned cellSize = 512;
  int sp = 132, key = 8;
  CMCPHeader header = {"TEST-CP-HEADER"};
  bool isConditional = true;

  void *cells[10] = {0};

  CPID id = cmCellPoolBuild(psize, ssize, cellSize, sp, key, &header);
  if (id == -1) {
<<<<<<< HEAD
<<<<<<< HEAD
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_SEVERE, "error: cmCellPoolBuild failed\n");
=======
    printf("error: cmCellPoolBuild failed\n");
>>>>>>> e3a5b4d... Ensure there is no name collisions with cellpool.c in cmutils.c
=======
    printf("error: cmCellPoolBuild failed\n");
>>>>>>> c7398ac... Refactored logging for {COMMON}/c/[a-c]*.c
    return CMUTILS_TEST_STATUS_FAILURE;
  }

  int status = CMUTILS_TEST_STATUS_OK;

  for (int i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
    cells[i] = cmCellPoolGet(id, isConditional);
    if (cells[i] == NULL) {
<<<<<<< HEAD
<<<<<<< HEAD
      zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_SEVERE, "error: cmCellPoolFree test failed (alloc 1), cell #%d\n", i);
=======
      printf("error: cmCellPoolFree test failed (alloc 1), cell #%d\n", i);
>>>>>>> e3a5b4d... Ensure there is no name collisions with cellpool.c in cmutils.c
=======
      printf("error: cmCellPoolFree test failed (alloc 1), cell #%d\n", i);
>>>>>>> c7398ac... Refactored logging for {COMMON}/c/[a-c]*.c
      status = CMUTILS_TEST_STATUS_FAILURE;
      break;
    }
  }

  if (status == CMUTILS_TEST_STATUS_OK) {

    for (int i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
      cmCellPoolFree(id, cells[i]);
      cells[i] = NULL;
    }

    for (int i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
      cells[i] = cmCellPoolGet(id, isConditional);
      if (cells[i] == NULL) {
<<<<<<< HEAD
<<<<<<< HEAD
        zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_SEVERE, "error: cmCellPoolFree test failed (alloc 2), cell #%d\n", i);
=======
        printf("error: cmCellPoolFree test failed (alloc 2), cell #%d\n", i);
>>>>>>> e3a5b4d... Ensure there is no name collisions with cellpool.c in cmutils.c
=======
        printf("error: cmCellPoolFree test failed (alloc 2), cell #%d\n", i);
>>>>>>> c7398ac... Refactored logging for {COMMON}/c/[a-c]*.c
        status = CMUTILS_TEST_STATUS_FAILURE;
        break;
      }
    }

  }

  cmCellPoolDelete(id);

  return status;
}

static int testCMMap(void) {

  typedef struct KeyValuePair_tag {
    char key[20];
    char value[6];
  } KeyValuePair;

  const KeyValuePair pairs[28] = {
      {"key-number-1-0000001", "val-01",},
      {"key-number-2-0000002", "val-02",},
      {"key-number-3-0000003", "val-03",},
      {"key-number-4-0000004", "val-04",},
      {"key-number-5-0000005", "val-05",},
      {"key-number-6-0000006", "val-06",},
      {"key-number-7-0000007", "val-07",},
      {"key-number-8-0000008", "val-08",},
      {"key-number-9-0000009", "val-09",},
      {"key-number-a-0000010", "val-10",},
      {"key-number-b-0000011", "val-11",},
      {"key-number-c-0000012", "val-12",},
      {"key-number-d-0000013", "val-13",},
      {"key-number-e-0000014", "val-14",},
      {"key-number-f-0000015", "val-15",},
      {"key-number-g-0000016", "val-16",},
      {"key-number-h-0000017", "val-17",},
      {"key-number-i-0000018", "val-18",},
      {"key-number-j-0000019", "val-19",},
      {"key-number-k-0000020", "val-21",},
      {"key-number-l-0000021", "val-22",},
      {"key-number-m-0000022", "val-22",},
      {"key-number-o-0000023", "val-23",},
      {"key-number-p-0000024", "val-24",},
      {"key-number-q-0000025", "val-25",},
      {"key-number-r-0000026", "val-26",},
      {"key-number-s-0000027", "val-27",},
      {"key-number-t-0000028", "val-28",},
  };

  const KeyValuePair missingPairs[2] = {
      {"key-number-u-0000039", "val-29",},
      {"key-number-v-0000030", "val-30",},
  };

  CrossMemoryMap *map = makeCrossMemoryMap(sizeof(pairs[0].key));
  if (map == NULL) {
    printf("error: map not created\n");
    return CMUTILS_TEST_STATUS_FAILURE;
  }

  int status = CMUTILS_TEST_STATUS_OK;

  for (int i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
    int rc = crossMemoryMapPut(map, &pairs[i].key, &pairs[i].value);
    if (rc != 0) {
      status = CMUTILS_TEST_STATUS_FAILURE;
      printf("error: cm map put (1) failed, rc = %d, key = %s\n",
             rc, pairs[i].key);
      break;
    }
  }

  if (status == CMUTILS_TEST_STATUS_OK) {
    for (int i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
      int rc = crossMemoryMapPut(map, &pairs[i].key, &pairs[i].value);
      if (rc != 1) {
        status = CMUTILS_TEST_STATUS_FAILURE;
        printf("error: cm map put (2) failed, rc = %d, key = %s\n",
               rc, pairs[i].key);
        break;
      }
    }
  }

  if (status == CMUTILS_TEST_STATUS_OK) {
    for (int i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
      char *value = crossMemoryMapGet(map, &pairs[i].key);
      if (value == NULL) {
        status = CMUTILS_TEST_STATUS_FAILURE;
        printf("error: cm map get (1) failed, value not found for key = %s\n",
               pairs[i].key);
        break;
      }
      if (strcmp(value, pairs[i].value)) {
        status = CMUTILS_TEST_STATUS_FAILURE;
        printf("error: cm map get (1) failed, bad value found %s, expected %s\n",
               value, pairs[i].value);
      }
    }
  }

  if (status == CMUTILS_TEST_STATUS_OK) {
    for (int i = 0; i < sizeof(missingPairs) / sizeof(missingPairs[0]); i++) {
      char *value = crossMemoryMapGet(map, &missingPairs[i].key);
      if (value != NULL) {
        status = CMUTILS_TEST_STATUS_FAILURE;
        printf("error: cm map get (2) failed, key = %s, value = %s\n",
               missingPairs[i].key, value);
        break;
      }
    }
  }

  if (status == CMUTILS_TEST_STATUS_OK) {
    for (int i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
      void **valueHandle = crossMemoryMapGetHandle(map, &pairs[i].key);
      if (valueHandle == NULL) {
        status = CMUTILS_TEST_STATUS_FAILURE;
        printf("error: cm map get (3) failed, handle not found for key = %s\n",
               pairs[i].key);
        break;
      }
    }
  }

  removeCrossMemoryMap(&map);
  map = NULL;

  return status;

}


static int testCellPool(void) {

  int status = CMUTILS_TEST_STATUS_OK;

  if (status == CMUTILS_TEST_STATUS_OK) {
    status = testUnconditionalCellPoolGet();
  }

  if (status == CMUTILS_TEST_STATUS_OK) {
    status = testConditionalCellPoolGet();
  }

  if (status == CMUTILS_TEST_STATUS_OK) {
    status = testCellPoolFree();
  }

  if (status == CMUTILS_TEST_STATUS_OK) {
    status = testCMMap();
  }

  return status;
}


#ifdef CMUTILS_TEST
int main() {
#else
static int notMain() {
#endif

  printf("info: starting cmutils test\n");

  int status = CMUTILS_TEST_STATUS_OK;

  status = testCellPool();

  if (status == CMUTILS_TEST_STATUS_OK) {
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

