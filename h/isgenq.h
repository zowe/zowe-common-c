

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
#pragma map(isgenqGetSharedLockOrFail, "ENQGETSF")
#pragma map(isgenqGetSharedLock, "ENQGETS")
#pragma map(isgenqTestExclusiveLock, "ENQTSTX")
#pragma map(isgenqTestSharedLock, "ENQTSTS")
#pragma map(isgenqTestLock, "ENQTEST")
#pragma map(isgenqReleaseLock, "ENQREL")

/**
 * @brief The function acquires an exclusive lock or fails if a lock is already
 * being held for this QNAME and RNAME combination. See more details in the
 * ISGENQ documentation.
 *
 * @param qname Enqueue major name
 * @param rname Enqueue minor name
 * @param scope Scope of the enqueue (use one of ISGENQ_SCOPE_xxxx)
 * @param token Token used to release the enqueue
 * @param reasonCode Reason code from ISGENQ
 * @return Return code from ISGENQ
 */
int isgenqGetExclusiveLockOrFail(const QName *qname,
                                 const RName *rname,
                                 uint8_t scope,
                                 ENQToken *token,
                                 int *reasonCode);
/**
 * @brief The function acquires an exclusive lock. If a lock is already being
 * held for this QNAME and RNAME combination, the current task is suspended.
 * See more details in the ISGENQ documentation.
 *
 * @param qname Enqueue major name
 * @param rname Enqueue minor name
 * @param scope Scope of the enqueue (use one of ISGENQ_SCOPE_xxxx)
 * @param token Token used to release the enqueue
 * @param reasonCode Reason code from ISGENQ
 * @return Return code from ISGENQ
 */
int isgenqGetExclusiveLock(const QName *qname,
                           const RName *rname,
                           uint8_t scope,
                           ENQToken *token,
                           int *reasonCode);
/**
 * @brief The function acquires a shared lock or fails if a lock is already
 * being held for this QNAME and RNAME combination. See more details in the
 * ISGENQ documentation.
 *
 * @param qname Enqueue major name
 * @param rname Enqueue minor name
 * @param scope Scope of the enqueue (use one of ISGENQ_SCOPE_xxxx)
 * @param token Token used to release the enqueue
 * @param reasonCode Reason code from ISGENQ
 * @return Return code from ISGENQ
 */
int isgenqGetSharedLockOrFail(const QName *qname,
                              const RName *rname,
                              uint8_t scope,
                              ENQToken *token,
                              int *reasonCode);
/**
 * @brief The function acquires a shared lock. If an exclusive lock is already
 * being held for this QNAME and RNAME combination, the current task is
 * suspended. See more details in the ISGENQ documentation.
 *
 * @param qname Enqueue major name
 * @param rname Enqueue minor name
 * @param scope Scope of the enqueue (use one of ISGENQ_SCOPE_xxxx)
 * @param token Token used to release the enqueue
 * @param reasonCode Reason code from ISGENQ
 * @return Return code from ISGENQ
 */
int isgenqGetSharedLock(const QName *qname,
                        const RName *rname,
                        uint8_t scope,
                        ENQToken *token,
                        int *reasonCode);

/**
 * @brief The function does the same as isgenqGetExclusiveLockOrFail without
 * acquiring the actual lock in case of success. See more details in the ISGENQ
 * documentation.
 *
 * @param qname Enqueue major name
 * @param rname Enqueue minor name
 * @param scope Scope of the enqueue (use one of ISGENQ_SCOPE_xxxx)
 * @param token Token used to release the enqueue
 * @param reasonCode Reason code from ISGENQ
 * @return Return code from ISGENQ
 */
int isgenqTestExclusiveLock(const QName *qname,
                            const RName *rname,
                            uint8_t scope,
                            int *reasonCode);
/**
 * @brief The function does the same as isgenqGetSharedLockOrFail without
 * acquiring the actual lock in case of success. See more details in the ISGENQ
 * documentation.
 *
 * @param qname Enqueue major name
 * @param rname Enqueue minor name
 * @param scope Scope of the enqueue (use one of ISGENQ_SCOPE_xxxx)
 * @param token Token used to release the enqueue
 * @param reasonCode Reason code from ISGENQ
 * @return Return code from ISGENQ
 */
int isgenqTestSharedLock(const QName *qname,
                         const RName *rname,
                         uint8_t scope,
                         int *reasonCode);

/**
 * @brief The function does the same as isgenqTestExclusiveLock. See more
 * details in the ISGENQ documentation.
 *
 * @param qname Enqueue major name
 * @param rname Enqueue minor name
 * @param scope Scope of the enqueue (use one of ISGENQ_SCOPE_xxxx)
 * @param reasonCode Reason code from ISGENQ
 * @return Return code from ISGENQ
 */
int isgenqTestLock(const QName *qname,
                   const RName *rname,
                   uint8_t scope,
                   int *reasonCode);
/**
 * @brief The function releases a lock using the token obtain during the
 * corresponding get call. See more details in the ISGENQ documentation.
 *
 * @param token Token used to acquire the lock
 * @param reasonCode Reason code from ISGENQ
 * @return Return code from ISGENQ
 */
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

