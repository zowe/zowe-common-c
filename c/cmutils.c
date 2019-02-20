

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

ZOWE_PRAGMA_PACK

typedef int32_t CPID;

typedef struct CPHeader_tag {
  char text[24];
} CPHeader;

ZOWE_PRAGMA_PACK_RESET

static CPID cellpoolBuild(unsigned int pCellCount,
                          unsigned int sCellCount,
                          unsigned int cellSize,
                          int subpool, int key,
                          const CPHeader *header) {

  CPID cpid = -1;

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

static void cellpoolDelete(CPID cellpoolID) {

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

static void *cellpoolGet(CPID cellpoolID, bool conditional) {

  uint64 callerGPRs[12] = {0};

  void * __ptr32 cell = NULL;

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
        : [gprs]"m"(callerGPRs), [cpid]"r"(cellpoolID)
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
        : [gprs]"m"(callerGPRs), [cpid]"r"(cellpoolID)
        : "r0", "r1", "r14", "r15"
    );
  }

  return cell;
}

static void cellpoolFree(CPID cellpoolID, void *cell) {

  uint64 callerGPRs[12] = {0};

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
      : [gprs]"m"(callerGPRs), [cpid]"r"(cellpoolID), [cell]"=NR:r1"(cell)
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

#define CM_MAP_SUBPOOL  228
#define CM_MAP_KEY      4

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

  CrossMemoryMap *map =
      cmAlloc(mapSize, CM_MAP_SUBPOOL, CM_MAP_KEY);
  if (map == NULL) {
    return NULL;
  }
  memset(map, 0, mapSize);
  memcpy(map->eyecatcher, CROSS_MEMORY_MAP_EYECATCHER, sizeof(map->eyecatcher));
  map->size = mapSize;
  map->keySize = keySize;
  map->bucketCount = CM_MAP_BUCKET_COUNT;

  CPHeader header = {
      .text = CM_MAP_HEADER,
  };

  if (keySize & 0x00000003) {
    keySize += (4 - (keySize % 4));
  }

  map->entrySize = sizeof(CrossMemoryMapEntry) + keySize;
  map->entryCellpool = cellpoolBuild(CM_MAP_PRIMARY_CELL_COUNT,
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
    cellpoolDelete(cellpoolToDelete);
  }
  cmFree2((void **)mapAddr, map->size, CM_MAP_SUBPOOL, CM_MAP_KEY);
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

  CrossMemoryMapEntry *entry = cellpoolGet(map->entryCellpool, FALSE);
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
  cellpoolFree(map->entryCellpool, entry);
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

  CrossMemoryMapEntry *chain = map->buckets[bucketID];
  CrossMemoryMapEntry *existingEntry = findEntry(chain, key, keySize);
  CrossMemoryMapEntry *newEntry = NULL;

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


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

