

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef H_ISGENQ_H_
#define H_ISGENQ_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdint.h>
#else
#include <stdint.h>
#endif

typedef struct QName_tag {
  char value[8];
} QName;

typedef struct RName_tag {
  unsigned char length;
  char value[255];
} RName;

typedef struct ENQToken_tag {
  char token[32];
} ENQToken;

/* scope values can be found in SYS1.MACLIB(ISGYENQ) */
#define ISGENQ_SCOPE_STEP     1
#define ISGENQ_SCOPE_SYSTEM   2
#define ISGENQ_SCOPE_SYSTEMS  3
#define ISGENQ_SCOPE_SYSPLEX  4

#pragma map(isgenqGetExclusiveLockOrFail, "ENQGETXF")
#pragma map(isgenqGetExclusiveLock, "ENQGETX")
#pragma map(isgenqTestLock, "ENQTEST")
#pragma map(isgenqReleaseLock, "ENQREL")

int isgenqGetExclusiveLockOrFail(const QName *qname,
                                 const RName *rname,
                                 uint8_t scope,
                                 ENQToken *token,
                                 int *reasonCode);

int isgenqGetExclusiveLock(const QName *qname,
                           const RName *rname,
                           uint8_t scope,
                           ENQToken *token,
                           int *reasonCode);

int isgenqTestLock(const QName *qname,
                   const RName *rname,
                   uint8_t scope,
                   int *reasonCode);

int isgenqReleaseLock(ENQToken *token, int *reasonCode);

#define IS_ISGENQ_LOCK_OBTAINED($isgenqRC, $isgenqRSN) \
    ($isgenqRC <= 4 && ((unsigned)$isgenqRSN & 0xFFFF) != 0x0404)

#endif /* H_ISGENQ_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

