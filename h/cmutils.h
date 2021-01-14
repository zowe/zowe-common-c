

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

/**
 * @brief Copies data to an address in the secondary address space using
 * the PC routine caller's key.
 *
 * @attention This function must be used inside a stacking PC routine.
 *
 * @param dest Destination address in SASN.
 * @param src Source address in PASN.
 * @param size Size of the data.
 */
void cmCopyToSecondaryWithCallerKey(void *dest, const void *src, size_t size);

/**
 * @brief Copies data from an address in the secondary address space using
 * the PC routine caller's key.
 *
 * @attention This function must be used inside a stacking PC routine.
 *
 * @param dest Destination address in PASN.
 * @param src Source address in SASN.
 * @param size Size of the data.
 */
void cmCopyFromSecondaryWithCallerKey(void *dest, const void *src, size_t size);

/**
 * @brief Copies data to an address in the primary address space using
 * the PC routine caller's key.
 *
 * @attention This function must be used inside a stacking PC routine.
 *
 * @param dest Destination address in PASN.
 * @param src Source address in PASN.
 * @param size Size of the data.
 */
void cmCopyToPrimaryWithCallerKey(void *dest, const void *src, size_t size);

/**
 * @brief Copies data from an address in the primary address space using
 * the PC routine caller's key.
 *
 * @attention This function must be used inside a stacking PC routine.
 *
 * @param dest Destination address in PASN.
 * @param src Source address in PASN.
 * @param size Size of the data.
 */
void cmCopyFromPrimaryWithCallerKey(void *dest, const void *src, size_t size);

/**
 * @brief Copies data to an address in the home address space using
 * the PC routine caller's key.
 *
 * @attention This function must be used inside a stacking PC routine.
 *
 * @param dest Destination address in HASN.
 * @param src Source address in PASN.
 * @param size Size of the data.
 */
void cmCopyToHomeWithCallerKey(void *dest, const void *src, size_t size);

/**
 * @brief Copies data to an address in the home address space using
 * the PC routine caller's key.
 *
 * @attention This function must be used inside a stacking PC routine.
 *
 * @param dest Destination address in PASN.
 * @param src Source address in HASN.
 * @param size Size of the data.
 */
void cmCopyFromHomeWithCallerKey(void *dest, const void *src, size_t size);

/**
 * @brief Copies data from an address in the address space identified by the
 * specified ALET using the specified key.
 *
 * @param dest Destination address in PASN.
 * @param src Source address in the address space identified by the provided
 * ALET.
 * @param key Key of the source data.
 * @param alet ALET of the address space with the source data.
 * @param size Size of the data.
 */
void cmCopyWithSourceKeyAndALET(void *dest, const void *src, unsigned int key,
                                unsigned int alet, size_t size);

/**
 * @brief Returns the content and the address of the caller's address space
 * ACEE.
 *
 * @attention This function must be used inside a stacking PC routine.
 *
 * @param content Content of the ACEE.
 * @param address Address of the ACEE. This parameter can be NULL.
 */
void cmGetCallerAddressSpaceACEE(ACEE *content, ACEE **address);

/**
 * @brief Returns the content and the address of the caller's task ACEE.
 *
 * @attention This function must be used inside a stacking PC routine.
 *
 * @param content Content of the ACEE.
 * @param address Address of the ACEE. This parameter can be NULL.
 */
void cmGetCallerTaskACEE(ACEE *content, ACEE **address);

/**
 * @brief Allocates storage. The function can be used in cross-memory mode.
 *
 * @param size Size of the storage.
 * @param subpool Subpool of the storage.
 * @param key Key of the storage
 * @return The address of the storage in case of success, NULL if the request
 * has failed.
 */
void *cmAlloc(unsigned int size, int subpool, int key);

/**
 * @brief Releases storage. The call is unconditional, that is, it will ABEND
 * if bad storage is passed. The function can be used in cross-memory mode.
 *
 * @param data Storage to be released.
 * @param size Size of the storage.
 * @param subpool Subpool of the storage.
 * @param key Key of the storage.
 */
void cmFree(void *data, unsigned int size, int subpool, int key);

/**
 * @brief Allocates storage. The function can be used in cross-memory mode.
 *
 * This version should be used in code which uses recovery as the window between
 * the actual allocation and returning the result is much smaller than in
 * cmAlloc.
 *
 * @param size Size of the storage.
 * @param subpool Subpool of the storage.
 * @param key Key of the storage.
 * @param resultPtr Pointer to the result address of the storage. The pointer is
 * set to NULL if the request fails.
 */
void cmAlloc2(unsigned int size, int subpool, int key, void **resultPtr);

/**
 * @brief Releases storage. The function can be used in cross-memory mode.
 *
 * The call is unconditional, that is, it will ABEND if bad storage is passed.
 * This version should be used in code which uses recovery as it first sets
 * the provided address to NULL and then releases the storage, whereby
 * helping to prevent double-free. If an ABEND happens in between setting
 * provided address to NULL and releasing the storage, the storage will be
 * leaked.
 *
 * @param dataPtr Pointer to the address of the storage to be released. It will
 * be set to NULL.
 * @param size Size of the storage.
 * @param subpool Subpool of the storage.
 * @param key Key of the storage.
 */
void cmFree2(void **dataPtr, unsigned int size, int subpool, int key);

typedef struct CrossMemoryMap_tag CrossMemoryMap;

/**
 * @brief Creates a simple map data structure in common storage. This function
 * can be used in cross-memory mode.
 *
 * @param keySize Size of a key.
 * @return Address of the map in common storage.
 */
CrossMemoryMap *makeCrossMemoryMap(unsigned int keySize);

/**
 * @brief Removes a common storage map. This function is not thread-safe. This
 * function can be used in cross-memory mode.
 *
 * @param mapAddr Pointer to the address of the map. It will be set to NULL by
 * the call.
 */
void removeCrossMemoryMap(CrossMemoryMap **mapAddr);

/**
 * @brief Puts a key-value pair to a common-storage map. This function is
 * thread-safe and can be used in cross-memory mode.
 *
 * The key is copied using the size provided in the makeCrossMemoryMap call,
 * the value is stored by address, thus it must be in common-storage.
 *
 * @param map Map to be used.
 * @param key Key to be used.
 * @param value Value associated with the key.
 * @return 0 in case of success, 1 in case of a duplicate (the value address is
 * updated), -1 in case of a fatal error (common storage allocation failure).
 */
int crossMemoryMapPut(CrossMemoryMap *map, const void *key, void *value);

/**
 * @brief Returns the address of the value address in the map identified by
 * the provided key. This function is thread-safe and it can be used in
 * cross-memory mode.
 *
 * @param map Map to be used.
 * @param key Key to be used.
 * @return Address of the value address or NULL if the key has not been found.
 */
void **crossMemoryMapGetHandle(CrossMemoryMap *map, const void *key);

/**
 * @brief Returns the value passed in the put call. This function is thread-safe
 * and it can be used in cross-memory mode.
 *
 * @param map Map to be used.
 * @param key Key to be used.
 * @return Address used in the put call or NULL if the key has not been found.
 */
void *crossMemoryMapGet(CrossMemoryMap *map, const void *key);

/**
 * @brief Function definition used for common storage map visitor function.
 *
 * @param key Key address of the current entry.
 * @param keySize Key size of the current entry.
 * @param valueHandle Address of the value address.
 * @param visitorData Data provided by the caller.
 * @return If a non-zero value is returned, the iteration is aborted.
 */
typedef int (CrossMemoryMapVisitor)(const char *key, unsigned int keySize,
                                    void **valueHandle, void *visitorData);

/**
 * @brief Iterates through a common storage map and calls the provided visitor
 * function for each entry. This function is thread-safe and it can be used
 * in cross-memory mode.
 *
 * The value pointed by valueHandle passed to the visitor function is not
 * serialized, the caller is responsible for that.
 *
 * @param map Map to be used.
 * @param visitor Visitor function.
 * @param visitorData Caller's data passed to the visitor function.
 */
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

