

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef H_ZVT_H_
#define H_ZVT_H_

#include "zowetypes.h"

ZOWE_PRAGMA_PACK

#define ZVTE_EYECATCHER   "ZOWEVTE "
#define ZVTE_VERSION      1
#define ZVTE_KEY          0
#define ZVTE_SUBPOOL      228

#define ZVT_OFFSET        0x023C
#define ZVT_EYECATCHER    "ZOWEVT  "
#define ZVT_VERSION       1
#define ZVT_KEY           0
#define ZVT_SUBPOOL       228

typedef struct ZVTEntry_tag {

  char eyecatcher[8];
  uint8_t version;
  uint8_t key;
  uint8_t subpool;
  char reserved1[1];
  uint16_t size;
  int8_t flag;
  char reserved2[1];
  uint64_t creationTime;
  char jobName[8];
  uint16_t asid;

  char reserved3[6];

  char instanceID[8];

  union {
    uint64_t lockWord64;
    struct {
      char padding[4];
      uint32_t lockWord31;
    };
  } lockWord;

  PAD_LONG(0, struct ZVTEntry_tag *prev);
  PAD_LONG(1, struct ZVTEntry_tag *next);

  PAD_LONG(2, void *productAnchor);

  char productFMID[8];
  uint16_t productVersion;
  uint16_t productRelease;
  uint16_t productMaint;
  char reserved4[2];
  char productDescription[32];
  char reserved5[128];

} ZVTEntry;

typedef struct ZVT_tag {

  char eyecatcher[8];
  uint8_t version;
  uint8_t key;
  uint8_t subpool;
  char reserved1[1];
  uint16_t size;
  char reserved2[2];
  /* Offset 0x10 */
  uint64_t creationTime;
  char jobName[8];
  /* Offset 0x20 */
  uint16_t asid;
  char reserved22[6];
  PAD_LONG(9, void *cmsLookupRoutine); /* points at another page in 31-common */
  char reserved3[104];

  struct {
    PAD_LONG(10, ZVTEntry *zis);
    PAD_LONG(11, ZVTEntry *zssp);
    PAD_LONG(12, ZVTEntry *reservedSlot2);
    PAD_LONG(13, ZVTEntry *reservedSlot3);
    PAD_LONG(14, ZVTEntry *reservedSlot4);
    PAD_LONG(15, ZVTEntry *reservedSlot5);
    PAD_LONG(16, ZVTEntry *reservedSlot6);
    PAD_LONG(17, ZVTEntry *reservedSlot7);
    PAD_LONG(18, ZVTEntry *reservedSlot8);
    PAD_LONG(19, ZVTEntry *reservedSlot9);
  } zvteSlots;

  char reserved4[3864];

} ZVT;

ZOWE_PRAGMA_PACK_RESET

#pragma map(zvtInit, "ZVTINIT")
#pragma map(zvtGet, "ZVTGET")
#pragma map(zvtAllocEntry, "ZVTAENTR")
#pragma map(zvtFreeEntry, "ZVTFENTR")
#pragma map(zvtGetCMSLookupRoutineAnchor, "ZVTGXMLR")
#pragma map(zvtSetCMSLookupRoutineAnchor, "ZVTSXMLR")

void zvtInit();
ZVT *zvtGet();
ZVTEntry *zvtAllocEntry();
void zvtFreeEntry(ZVTEntry *entry);

/**
 * The function returns the cross-memory server look up routine anchor address;
 * @param[in] zvt The ZVT to use.
 * @return The anchor is returned.
 */
void *zvtGetCMSLookupRoutineAnchor(ZVT *zvt);

/**
 * The function sets the cross-memory look up routine anchor field to
 * the provided value.
 * @param[in] zvt The ZVT to be used.
 * @param[in] anchor The anchor to be used.
 * @return The old anchor.
 */
void *zvtSetCMSLookupRoutineAnchor(ZVT *zvt, void *anchor);

#endif /* H_ZVT_H_ */



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

