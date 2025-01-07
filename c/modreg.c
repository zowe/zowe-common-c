
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/string.h>

#include "zowetypes.h"
#include "lpa.h"
#include "isgenq.h"
#include "qsam.h"
#include "zos.h"
#include "zvt.h"

#include <csvexti.h>

#include "modreg.h"

#include "logging.h"

#ifndef _LP64
#error AMODE 31 is not supported
#endif

#ifndef METTLE
/* TODO LE support after c/lpa.c gets it */
#error LE is not supported
#endif

#define PACK_RC_RSN(rc32, rsn32) ((uint64_t) (rc32) << 32 | (rsn32))

#define LOG_DEBUG($fmt, ...) \
  zowelog(NULL, LOG_COMP_MODREG, ZOWE_LOG_DEBUG, \
          MODREG_LOG_DEBUG_MSG_ID" %s:%d: "$fmt, __FUNCTION__, __LINE__, \
          ##__VA_ARGS__)

#define DUMP_DEBUG($data, $size) \
  zowedump(NULL, LOG_COMP_MODREG, ZOWE_LOG_DEBUG, (char *) $data, $size)


#ifndef MODREG_LOG_DEBUG_MSG_ID
#define MODREG_LOG_DEBUG_MSG_ID ""
#endif

ZOWE_PRAGMA_PACK

/*
 * WARNING: THESE TWO VALUES MUST NEVER CHANGE!!!
 */
static const QName MODR_QNAME = {"ZWE    "};
static const RName MODR_RNAME = {8, "ISMODR  "};

typedef struct ModuleMark_tag {
  /* IMPORTANT: this must be kept in sync with the macro MODREG_MARK_MODULE. */
  char eyecatcher[8];
#define MODREG_MODULE_MARK_EYECATCHER "ZWECMOD:"
  uint16_t version;
#define MODREG_MODULE_MARK_VERSION    1
  uint16_t size;
#define MODREG_MODULE_MARK_SIZE       48
  char reserved0[4];
  char buildTime[26];
  char reserved1[6];
} ModuleMark;

typedef struct ModuleRegistryEntry_tag {

  char eyecatcher[8];
#define MODREG_ENTRY_EYECATCHER   "ZWEMODRE"
  uint8_t version;
#define MODREG_ENTRY_VERSION      1
  uint8_t key;
#define MODREG_ENTRY_KEY          0
  char reserved1[2];
  uint16_t size;
#define MODREG_ENTRY_SIZE         144
  int8_t flag;
  char reserved2[1];
  uint64_t creationTime;
  char jobName[8];
  uint16_t asid;
  char reserved3[6];

  struct ModuleRegistryEntry_tag *next;

  char moduleName[8];
  ModuleMark mark;

  LPMEA lpaInfo;

} ModuleRegistryEntry;

ZOWE_PRAGMA_PACK_RESET

// TODO move all these similar macros to utils.h
#define MODREG_STATIC_ASSERT($expr) typedef char p[($expr) ? 1 : -1]

MODREG_STATIC_ASSERT(sizeof(ModuleMark) == MODREG_MODULE_MARK_SIZE);
MODREG_STATIC_ASSERT(sizeof(ModuleRegistryEntry) == MODREG_ENTRY_SIZE);

static void *allocateCommonStorage(uint32_t size, uint8_t key,
                                   int *rc, int *rsn) {

  void *result = NULL;
  key <<= 4;

  __asm(
      ASM_PREFIX
      "         IARST64 REQUEST=GET"
      ",SIZE=%[size]"
      ",COMMON=YES"
      ",OWNER=SYSTEM"
      ",FPROT=NO"
      ",SENSITIVE=NO"
      ",TYPE=PAGEABLE"
      ",CALLERKEY=NO,KEY00TOF0=%[key]"
      ",FAILMODE=RC"
      ",REGS=SAVE"
      ",RETCODE=%[rc],RSNCODE=%[rsn]                                           \n"
      : "=NR:r1"(result), [rc]"=m"(*rc), [rsn]"=m"(*rsn)
      : [size]"m"(size), [key]"m"(key)
      : "r0", "r14", "r15"
  );

  if (*rc != 0) {
    result = NULL;
  }

  return result;
}

static void freeCommonStorage(void *data) {

  __asm(
      ASM_PREFIX
      "         IARST64 REQUEST=FREE,AREAADDR=%[data],REGS=SAVE                \n"
      :
      : [data]"m"(data)
      : "r0", "r1", "r14", "r15"
  );

}

static uint64_t getAdjustedSTCK(void) {
  // get store-clock value
  uint64_t stckValue;
  __asm(" STCK 0(%0)" : : "r"(&stckValue));
  // get leap seconds
  CVT * __ptr32 cvt = *(void * __ptr32 * __ptr32) 0x10;
  void * __ptr32 cvtext2 = cvt->cvtext2;
  int64_t *cvtlso = (int64_t * __ptr32) (cvtext2 + 0x50);
  // return adjusted value
  return stckValue - *cvtlso;
}

static ModuleRegistryEntry *allocEntry(uint64_t *rsn) {

  ModuleRegistryEntry *entry = NULL;

  int wasProblemState = supervisorMode(TRUE);

  int originalKey = setKey(0);
  {
    int allocRC, allocRSN;
    entry = allocateCommonStorage(sizeof(ModuleRegistryEntry), MODREG_ENTRY_KEY,
                                  &allocRC, &allocRSN);
    if (entry != NULL) {
      memset(entry, 0, sizeof(ModuleRegistryEntry));
      memcpy(entry->eyecatcher, MODREG_ENTRY_EYECATCHER,
             sizeof(entry->eyecatcher));
      entry->version = MODREG_ENTRY_VERSION;
      entry->key = MODREG_ENTRY_KEY;
      entry->size = sizeof(ModuleRegistryEntry);
      entry->creationTime = getAdjustedSTCK();
      memset(entry->moduleName, ' ', sizeof(entry->moduleName));

      ASCB *ascb = getASCB();
      char *jobName = getASCBJobname(ascb);
      if (jobName) {
        memcpy(entry->jobName, jobName, sizeof(entry->jobName));
      } else {
        memset(entry->jobName, ' ', sizeof(entry->jobName));
      }
      entry->asid = ascb->ascbasid;
    } else {
      *rsn = PACK_RC_RSN(allocRC, allocRSN);
    }

  }
  setKey(originalKey);

  if (wasProblemState) {
    supervisorMode(FALSE);
  }

  return entry;
}

static void freeEntry(ModuleRegistryEntry *entry) {

  int wasProblemState = supervisorMode(TRUE);

  int originalKey = setKey(0);
  {
    freeCommonStorage(entry);
  }
  setKey(originalKey);

  if (wasProblemState) {
    supervisorMode(FALSE);
  }

}

static bool isModuleEntryEligible(const ModuleRegistryEntry *entry) {
  LOG_DEBUG("validating module entry @ %p:", entry);
  DUMP_DEBUG(entry, sizeof(*entry));
  if (entry->version > MODREG_ENTRY_VERSION) {
    LOG_DEBUG("module entry has unsupported version %u", entry->version);
    return false;
  }
  if (entry->size > sizeof(*entry)) {
    LOG_DEBUG("module entry has unexpected size %u", entry->size);
    return false;
  }
  return true;
}

static const ModuleRegistryEntry *findModuleEntry(EightCharString module,
                                                  const ModuleMark *mark) {
  ZVT *zvt = zvtGet();
  LOG_DEBUG("ZVT address = %p", zvt);
  if (zvt == NULL) {
    return NULL;
  }

  LOG_DEBUG("looking for module \'%.8s\'; mark:", module.text);
  DUMP_DEBUG(mark, sizeof(*mark));

  ModuleRegistryEntry *entry = zvt->moduleRegistry;
  int entryCount;
  for (entryCount = 0; entryCount <= MODREG_MAX_REGISTRY_SIZE; entryCount++) {
    if (entry == NULL) {
      break;
    }
    if (isModuleEntryEligible(entry)) {
      if (!memcmp(module.text, entry->moduleName, sizeof(entry->moduleName)) &&
          !memcmp(mark, &entry->mark, sizeof(entry->mark))) {
        LOG_DEBUG("entry matched");
        return entry;
      }
    } else {
      LOG_DEBUG("module entry not eligible, skipping...");
    }
    entry = entry->next;
  }
  if (entryCount > MODREG_MAX_REGISTRY_SIZE) {
    LOG_DEBUG("too many entries encountered");
    __asm(" ABEND 777,REASON=108 ":::);
  }

  return NULL;
}

static int lockChain(ENQToken *lockToken, uint64_t *rsn) {

  *rsn = 0;

  int lockRSN = 0;
  int lockRC = isgenqGetExclusiveLock(&MODR_QNAME, &MODR_RNAME,
                                      ISGENQ_SCOPE_SYSTEM, lockToken, &lockRSN);
  if (lockRC > 4) {
    *rsn = PACK_RC_RSN(lockRC, lockRSN);
    return RC_MODREG_CHAIN_NOT_LOCKED;
  }

  return RC_MODREG_OK;
}

static int unlockChain(ENQToken *lockToken, uint64_t *rsn) {

  *rsn = 0;

  int unlockRSN = 0;
  int unlockRC = isgenqReleaseLock(lockToken, &unlockRSN);
  if (unlockRC > 4) {
    *rsn = PACK_RC_RSN(unlockRC, unlockRSN);
    return RC_MODREG_CHAIN_NOT_UNLOCKED;
  }

  return RC_MODREG_OK;
}

static void *loadModule(const void *dcb, const char name[8],
                        struct exti * __ptr32 moduleInfo,
                        int *rc, int *rsn) {

  void * __ptr32 entryPoint = NULL;
  char parmList[32] = {0};

  __asm(
      ASM_PREFIX
      "         LOAD  EPLOC=(%[name])"
      ",DCB=(%[dcb])"
      ",ERRET=LOADMERR"
      ",EXTINFO=(%[exti])"
      ",SF=(E,%[parm])                                                         \n"
      "         J     LOADMEND                                                 \n"
      "LOADMERR DS    0H                                                       \n"
      "         XGR   0,0                                                      \n"
      "LOADMEND DS    0H                                                       \n"
      : "=NR:r0"(entryPoint), "=NR:r1"(*rc), "=NR:r15"(*rsn)
      : [name]"r"(name), [dcb]"r"(dcb), [exti]"r"(moduleInfo),
        [parm]"m"(parmList)
      : "r14"
  );

  return (void *) ((int) entryPoint & 0x7ffffffe);
}

static int deleteModule(const char name[8]) {

  int rc = 0;

  __asm(
      ASM_PREFIX
      "         DELETE EPLOC=(%[name])                                         \n"
      : "=NR:r15"(rc)
      : [name]"r"(name)
      : "r0", "r1", "r14"
  );

  return rc;
}

static const void *findSequence(const void *buffer, size_t bufferSize,
                                const void *sequence, size_t sequenceSize) {
  if ((bufferSize < sequenceSize) || bufferSize == 0 || sequenceSize == 0) {
    return NULL;
  }
  const char *currPos = buffer;
  const char *endPos = (const char *) buffer + bufferSize - sequenceSize;
  while (currPos <= endPos) {
    currPos = memchr(currPos, *(const char *) sequence, endPos - currPos + 1);
    if (currPos == NULL) {
      return NULL;
    }
    if (!memcmp(currPos, sequence, sequenceSize)) {
      return currPos;
    }
    currPos++;
  }
  return NULL;
}

static bool isMarkEligible(const ModuleMark *mark, size_t markSpace) {
  size_t markStructSize = sizeof(*mark);
  LOG_DEBUG("validating mark @ %p:", mark);
  DUMP_DEBUG(mark, min(markSpace, markStructSize));
  if (markSpace < markStructSize) {
    LOG_DEBUG("mark space too small - %zu vs %zu", markSpace, markStructSize);
    return false;
  }
  if (mark->buildTime[0] != '2' ||
      mark->buildTime[1] != '0') {
    LOG_DEBUG("mark build time has unexpected values");
    return false;
  }
  if (mark->version > MODREG_MODULE_MARK_VERSION) {
    LOG_DEBUG("mark has unsupported version %u", mark->version);
    return false;
  }
  if (mark->size > MODREG_MODULE_MARK_SIZE) {
    LOG_DEBUG("mark has unexpected size %u", mark->size);
    return false;
  }
  return true;
}

static const ModuleMark *findMark(const void *buffer, size_t bufferSize) {
  ssize_t bufferOffset = 0;
  while (true) {
    LOG_DEBUG("looking for a mark in buffer %p (%zu) + %zd",
              buffer, bufferSize, bufferOffset);
    const char *markInModule =
        findSequence((char *) buffer + bufferOffset, bufferSize - bufferOffset,
                     MODREG_MODULE_MARK_EYECATCHER,
                     strlen(MODREG_MODULE_MARK_EYECATCHER));
    LOG_DEBUG("mark found @ %p", markInModule);
    if (markInModule) {
      size_t markSpace =
          (char *) buffer + bufferSize - (char *) markInModule;
      LOG_DEBUG("mark space = %zu", markSpace);
      const ModuleMark *candidate = (ModuleMark *) markInModule;
      if (isMarkEligible(candidate, markSpace)) {
        return candidate;
      } else {
        LOG_DEBUG("mark %p is not eligible", markInModule);
        bufferOffset += markInModule - (char *) buffer +
                        strlen(MODREG_MODULE_MARK_EYECATCHER);
      }
    } else {
      return NULL;
    }
  }
}

static int readModuleMark(EightCharString ddname, EightCharString module,
                          ModuleMark *mark, uint64_t *rsn) {

  void *dcb = openSAM(ddname.text, OPEN_CLOSE_INPUT, FALSE, 0, 0, 0);
  LOG_DEBUG("dcb = %p", dcb);
  if (!dcb) {
    return RC_MODREG_OPEN_SAM_ERROR;
  }

  int rc = RC_MODREG_OK;

  struct exti modInfo;
  int loadRC, loadRSN;
  const void *ep = loadModule(dcb, module.text, &modInfo, &loadRC, &loadRSN);
  LOG_DEBUG("load - name = \'%.8s\', ep = %p, rc = %d, rsn = 0x%08X",
            module.text, ep, loadRC, loadRSN);
  if (!ep) {
    *rsn = PACK_RC_RSN(loadRC, loadRSN);
    rc = RC_MODREG_MODULE_LOAD_ERROR;
    goto out_close_sam;
  }
  LOG_DEBUG("ext num = %d; mod info:", modInfo.exti_numextents);
  DUMP_DEBUG(&modInfo, sizeof(modInfo));

  rc = RC_MODREG_MARK_MISSING;
  for (unsigned i = 0; i < modInfo.exti_numextents; i++) {

    struct extixe *moduleExtent =
        (struct extixe *) &modInfo.exti_extent_area + i;
    void *extentAddress = *(void **) moduleExtent->extixe_addr;
    size_t extentSize = *(size_t *) moduleExtent->extixe_length;
    LOG_DEBUG("ext addr = %d, size = %zu", extentAddress, extentSize);
    /* Can a mark be in-between extents? It's a valid question. Although,
     * no proof has been found in any of the doc, it's reasonable to assume,
     * that that cannot happen based on how a mark is defined (several DC
     * declarations) because otherwise other programs using the same approach
     * to declare constants would break.*/
    const ModuleMark *candidate = findMark(extentAddress, extentSize);
    if (candidate) {
      *mark = *candidate;
      rc = RC_MODREG_OK;
      break;
    }

  }

  int delRC = deleteModule(module.text);
  LOG_DEBUG("delete - rc = %d", delRC);
  rc = rc ? rc : (delRC ? RC_MODREG_MODULE_DELETE_ERROR : RC_MODREG_OK);

out_close_sam:
  closeSAM(dcb, OPEN_CLOSE_INPUT);

  return rc;
}

static void initAndAddEntry(ZVT *zvt, ModuleRegistryEntry *entry,
                            EightCharString module,
                            const LPMEA *lpaInfo,
                            const ModuleMark *mark) {
  int wasProblemState = supervisorMode(TRUE);
  int originalKey = setKey(0);
  {
    *(LPMEA *) &entry->lpaInfo = *lpaInfo;
    memcpy(entry->moduleName, module.text, sizeof(entry->moduleName));
    *(ModuleMark *) &entry->mark = *mark;
    entry->next = zvt->moduleRegistry;
    zvt->moduleRegistry = entry;
  }
  setKey(originalKey);
  if (wasProblemState) {
    supervisorMode(FALSE);
  }
}

int modregRegister(EightCharString ddname, EightCharString module,
                   LPMEA *lpaInfo, uint64_t *rsn) {

  uint64_t localRSN;
  rsn = rsn ? rsn : &localRSN;

  ZVT *zvt = zvtGet();
  LOG_DEBUG("ZVT address = %p", zvt);
  if (zvt == NULL) {
    return RC_MODREG_ZVT_NULL;
  }

  ModuleMark mark;
  int readRC = readModuleMark(ddname, module, &mark, rsn);
  if (readRC) {
    return readRC;
  }

  int lockRC;
  uint64_t lockRSN;
  ENQToken lockToken;
  lockRC = lockChain(&lockToken, &lockRSN);
  LOG_DEBUG("lock rc = %d, rsn = 0x%08X", lockRC, lockRSN);
  if (lockRC) {
    return lockRC;
  }

  int rc = RC_MODREG_OK;

  const ModuleRegistryEntry *existingEntry = findModuleEntry(module, &mark);
  LOG_DEBUG("existing entry found @ %p", existingEntry);
  if (existingEntry) {
    *lpaInfo = existingEntry->lpaInfo;
    rc = RC_MODREG_ALREADY_REGISTERED;
    goto out_unlock;
  }

  ModuleRegistryEntry *newEntry = allocEntry(rsn);
  LOG_DEBUG("new entry allocated @ %p", newEntry);
  if (!newEntry) {
    rc = RC_MODREG_ALLOC_FAILED;
    goto out_unlock;
  }

  int lpaRC, lpaRSN;
  lpaRC = lpaAdd(lpaInfo, &ddname, &module, &lpaRSN);
  LOG_DEBUG("lpa add rc = %d, rsn = 0x%08X", lpaRC, lpaRSN);
  if (lpaRC) {
    *rsn = PACK_RC_RSN(lpaRC, lpaRSN);
    rc = RC_MODREG_LPA_ADD_FAILED;
    goto out_free_entry;
  }
  LOG_DEBUG("lpa info @ %p:", lpaInfo);
  DUMP_DEBUG(lpaInfo, sizeof(*lpaInfo));

  void *lpaModuleAddress = lpaInfo->outputInfo.stuff.successInfo.loadPointAddr;
  int lpaModuleSize = *(int *) lpaInfo->outputInfo.stuff.successInfo.modLen;
  LOG_DEBUG("lpaModuleAddress = %p, size = %d",
            lpaModuleAddress, lpaModuleSize);
  const ModuleMark *lpaMark = findMark(lpaModuleAddress, lpaModuleSize);
  if (!lpaMark) {
    LOG_DEBUG("mark not found in LPA module");
    rc = RC_MODREG_MARK_MISSING_LPA;
    goto out_free_entry;
  }

  initAndAddEntry(zvt, newEntry, module, lpaInfo, lpaMark);

out_free_entry:
  if (rc != RC_MODREG_OK) {
    freeEntry(newEntry);
  }

out_unlock:
  lockRC = unlockChain(&lockToken, &lockRSN);
  LOG_DEBUG("unlock rc = %d, rsn = 0x%08X", lockRC, lockRSN);
  rc = rc ? rc : lockRC;

  return rc;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

