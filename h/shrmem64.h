

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef SRC_SHRMEM64_H_
#define SRC_SHRMEM64_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#else
#include "stddef.h"
#include "stdint.h"
#endif

#include "zowetypes.h"

#ifndef __LONGNAME__

#define shrmem64GetAddressSpaceToken SHR64TKN

#define shrmem64Alloc SHR64ALC
#define shrmem64Alloc2 SHR64AL2
#define shrmem64CommonAlloc SHR64CAL
#define shrmem64CommonAlloc2 SHR64CA2
#define shrmem64Release SHR64REL
#define shrmem64ReleaseAll SHR64REA

#define shrmem64GetAccess SHR64GAC
#define shrmem64GetAccess SHR64GA2
#define shrmem64RemoveAccess SHR64RAC
#define shrmem64RemoveAccess2 SHR64RA2

#endif

typedef uint64_t MemObjToken;

/**
 * @brief Returns a unique token for the current address space.
 *
 * @return 8-byte memory object token that can be used in shrmem64 functions.
 */
MemObjToken shrmem64GetAddressSpaceToken(void);

/**
 * @brief Allocates 64-bit shared storage. Use shrmem64GetAccess to establish
 * addressability to this storage in your the current address space. See more
 * details in the IARV64 doc (REQUEST=GETSHARED).
 *
 * @param userToken The token this storage will be associated with. This token
 * must be used to free the storage. Read the full requirements in the IARV64 doc.
 * @param size The size of the storage to be allocated.
 * @param rsn The reason code returned in case of a failure.
 *
 * @return One of the RC_SHRMEM64_xx return codes.
 */
int shrmem64Alloc(MemObjToken userToken, size_t size, void **result, int *rsn);

/** This is the key, alet and fprot varying version of the above */
#define SHRMEM64_USE_CALLER_KEY -1
int shrmem64Alloc2(MemObjToken userToken, size_t size, int key, int aletValue,
                   bool fetchProtect, void **result, int *rsn);

/** Variant for common (shared with everybody!) 
 */
int shrmem64CommonAlloc(MemObjToken userToken, size_t size, void **result,
                        int *rsn);
int shrmem64CommonAlloc2(MemObjToken userToken, size_t size, int key,
                         void **result, int *rsn);

/**
 * @brief Releases 64-bit shared storage. The storage will still be accessible
 * in the address spaces that have issued shrmem64GetAccess. No new access can
 * be obtained after this call. See more details in the IARV64 doc
 * (REQUEST=DETACH,AFFINITY=SYSTEM,MATCH=SINGLE).
 *
 * @param userToken The token this storage was associated with during
 * the corresponding shrmem64Alloc call.
 * @param target The storage to be released.
 * @param rsn The reason code returned in case of a failure.
 *
 * @return One of the RC_SHRMEM64_xx return codes.
 */
int shrmem64Release(MemObjToken userToken, void *target, int *rsn);

/**
 * @brief Releases all 64-bit shared storage associated with the specified token.
 * The associated storage will still be accessible in the address spaces that
 * have issued shrmem64GetAccess. No new access can be obtained after this call.
 * See more details in the IARV64 doc
 * (REQUEST=DETACH,AFFINITY=SYSTEM,MATCH=MOTOKEN).
 *
 * @param userToken The token all storage to be released is associated with.
 * @param rsn The reason code returned in case of a failure.
 *
 * @return One of the RC_SHRMEM64_xx return codes.
 */
int shrmem64ReleaseAll(MemObjToken userToken, int *rsn);

/**
 * @brief Establishes addressability to the specified storage in the current
 * address space. See more details in the IARV64 doc (REQUEST=SHAREMEMOBJ).
 *
 * @param userToken The token this storage will be associated with. It mist be
 * used for the subsequent shrmem64RemoveAccess call. It does not have to be
 * the same as the token used in shrmem64Alloc. Read the full requirements in
 * the IARV64 doc.
 * @param target The storage to be accessed.
 * @param rsn The reason code returned in case of a failure.
 *
 * @return One of the RC_SHRMEM64_xx return codes.
 */
int shrmem64GetAccess(MemObjToken userToken, void *target, int *rsn);

/** 
  This is the beefed-up version of shrmem64GetAccess that lets the caller control
  whether primary or home space is used and whether the object will be made writable.
 */
int shrmem64GetAccess2(MemObjToken userToken, void *target, bool makeWritable,
                       int aletValue, uint64_t size, int *rsn);

/**
 * @brief Removes the addressability of the specified storage in the current
 * address space. See more details in the IARV64 doc
 * (REQUEST=DETACH,AFFINITY=LOCAL).
 *
 * @param userToken The token this storage was associated with during
 * the corresponding shrmem64GetAccess call.
 * @param target The storage to be detached.
 * @param rsn The reason code returned in case of a failure.
 *
 * @return One of the RC_SHRMEM64_xx return codes.
 */
int shrmem64RemoveAccess(MemObjToken userToken, void *target, int *rsn);
int shrmem64RemoveAccess2(MemObjToken userToken, void *target, int aletValue,
                          bool isOwner, int *rsn);

#define RC_SHRMEM64_OK                          0
#define RC_SHRMEM64_GETSHARED_FAILED            8
#define RC_SHRMEM64_SHAREMEMOBJ_FAILED          9
#define RC_SHRMEM64_ALL_SYS_DETACH_FAILED       10
#define RC_SHRMEM64_SINGLE_SYS_DETACH_FAILED    11
#define RC_SHRMEM64_DETACH_FAILED               12
#define RC_SHRMEM64_GETCOMMON_FAILED            13
#define RC_SHRMEM64_CHANGEACCESS_FAILED         14


#endif /* SRC_SHRMEM64_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
