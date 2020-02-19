

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef H_CMUTILS_H_
#define H_CMUTILS_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#else
#include <stddef.h>
#endif

#include "zos.h"
#include "zowetypes.h"

#define CROSS_MEMORY_ALET_PASN  0
#define CROSS_MEMORY_ALET_SASN  1
#define CROSS_MEMORY_ALET_HASN  2

#ifndef __LONGNAME__

#define cmCopyWithDestinationKey CMCPWDK
#define cmCopyWithSourceKey CMCPWSK

#define cmCopyToSecondaryWithCallerKey CMCPTSSK
#define cmCopyFromSecondaryWithCallerKey CMCPFSSK
#define cmCopyToPrimaryWithCallerKey CMCPTPSK
#define cmCopyFromPrimaryWithCallerKey CMCPFPSK
#define cmCopyToHomeWithCallerKey CMCPTHSK
#define cmCopyFromHomeWithCallerKey CMCPFHSK
#define cmCopyWithSourceKeyAndALET CMCPYSKA

#define cmGetCallerAddressSpaceACEE CMGAACEE
#define cmGetCallerTaskACEE CMGTACEE

#define cmAlloc CMALLOC
#define cmFree CMFREE
#define cmAlloc2 CMALLOC2
#define cmFree2 CMFREE2

#define makeCrossMemoryMap CMUMKMAP
#define removeCrossMemoryMap CMURMMAP
#define crossMemoryMapPut CMUMAPP
#define crossMemoryMapGetHandle CMUMAPGH
#define crossMemoryMapGet CMUMAPGT
#define crossMemoryMapIterate CMUMAPIT

#endif

/**
 * @brief Copy data to another address space with the destination key.
 * @param dest Destination address.
 * @param destKey Destination storage key.
 * @param destALET Destination ALET.
 * @param src Source address.
 * @param size Size of the data to be copied.
 */
void cmCopyWithDestinationKey(void *dest, unsigned destKey, unsigned destALET,
                              const void *src, size_t size);

/**
 * @brief Copy data from another address space with the source key.
 * @param dest Destination address.
 * @param src Source address.
 * @param srcKey Source storage key.
 * @param srcALET Source ALET.
 * @param size Size of the data to be copied.
 */
void cmCopyWithSourceKey(void *dest, const void *src, unsigned srcKey,
                         unsigned srcALET, size_t size);

void cmCopyToSecondaryWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyFromSecondaryWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyToPrimaryWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyFromPrimaryWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyToHomeWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyFromHomeWithCallerKey(void *dest, const void *src, size_t size);
void cmCopyWithSourceKeyAndALET(void *dest, const void *src, unsigned int key,
                                unsigned int alet, size_t size);

void cmGetCallerAddressSpaceACEE(ACEE *content, ACEE **address);
void cmGetCallerTaskACEE(ACEE *content, ACEE **address);

void *cmAlloc(unsigned int size, int subpool, int key);
void cmFree(void *data, unsigned int size, int subpool, int key);
void cmAlloc2(unsigned int size, int subpool, int key, void **resultPtr);
void cmFree2(void **dataPtr, unsigned int size, int subpool, int key);

typedef struct CrossMemoryMap_tag CrossMemoryMap;

CrossMemoryMap *makeCrossMemoryMap(unsigned int keySize);
void removeCrossMemoryMap(CrossMemoryMap **mapAddr);
int crossMemoryMapPut(CrossMemoryMap *map, const void *key, void *value);
void **crossMemoryMapGetHandle(CrossMemoryMap *map, const void *key);
void *crossMemoryMapGet(CrossMemoryMap *map, const void *key);

typedef int (CrossMemoryMapVisitor)(const char *key, unsigned int keySize,
                                    void **valueHandle, void *visitorData);
void crossMemoryMapIterate(CrossMemoryMap *map,
                           CrossMemoryMapVisitor *visitor,
                           void *visitorData);

#endif /* H_CMUTILS_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

